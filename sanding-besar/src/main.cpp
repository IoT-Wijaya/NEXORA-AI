#include <Wire.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <esp_system.h>

// =====================================================
// SANDING BESAR - ESP32 + W5500 + LCD 20x4 + MQTT VPS
// -----------------------------------------------------
// sensorTask  (core 0): HANYA menghitung sensor & tombol reset.
//                       Tidak menyentuh MQTT, LCD, atau flash.
// networkTask (core 1): SATU-SATUNYA pemilik MQTT, LCD, dan flash.
//
// Dua task tidak boleh memakai objek yang sama
// (client MQTT, LCD) karena library-nya tidak thread-safe.
// Komunikasi antar task lewat counter + bendera counterDirty.
//
// LAPISAN KESTABILAN:
//  1. Backoff      : jeda konek ulang MQTT 2s -> 4s -> ... -> 60s
//  2. Reset chip   : MQTT/IP bermasalah > 5 menit -> reset W5500
//  3. Restart total: MQTT/IP bermasalah > 10 menit -> ESP.restart()
//  4. Watchdog     : task macet > 60 detik -> restart otomatis
//  5. Counter disimpan di flash, jadi restart tidak menghapus hitungan
//  6. Satu fungsi status (hitungNetState) dipakai LCD DAN dashboard
// =====================================================

// =====================================================
// KONFIGURASI LCD 20x4
// =====================================================
#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 20
#define LCD_ROWS 4

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

// =====================================================
// KONFIGURASI W5500
// =====================================================
#define W5500_CS_PIN 5
#define W5500_SCK_PIN 18
#define W5500_MISO_PIN 19
#define W5500_MOSI_PIN 23

// Pin RST modul W5500 (opsional). Isi nomor GPIO (misalnya 4) jika kabel
// RST disambung, atau biarkan -1 jika tidak (hanya init ulang lewat kode).
#define W5500_RST_PIN -1

// MAC harus UNIK di jaringan. Sanding Kecil memakai ...FE:02.
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x01};

EthernetClient ethClient;
PubSubClient client(ethClient);

// =====================================================
// MQTT
// =====================================================
const char *mqtt_server = "103.27.206.23"; // ganti jika IP VPS berbeda
const int mqtt_port = 1883;

// Kosongkan jika broker masih anonim. Isi jika sudah memakai password.
const char *mqtt_user = "";
const char *mqtt_pass = "";

// Topik sama dengan kode lama (dashboard tidak perlu diubah)
#define TOPIC_BASE "pabrik/veneer/"

const char *topicStatus = TOPIC_BASE "status";
const char *topicJumlah = TOPIC_BASE "jumlah";   // Hasil
const char *topicJumlah2 = TOPIC_BASE "jumlah2"; // Modal
const char *topicIP = TOPIC_BASE "ip";
const char *topicPerintah = TOPIC_BASE "perintah";
const char *topicDiag = TOPIC_BASE "diag"; // laporan diagnosa lengkap (JSON)

// Isi topik status (sama dengan kode lama)
#define STATUS_ONLINE "1"
#define STATUS_OFFLINE "0"

#define FW_VERSION "2026.10.17"

// =====================================================
// PIN
// =====================================================
#define SENSOR_HASIL 27 // Sensor Hasil Sanding Besar
#define SENSOR_MODAL 32 // Sensor Modal Sanding Otomatis
#define RESET_BUTTON 0  // GPIO0 = tombol BOOT, jangan ditahan saat menyalakan

// Sensor aktif LOW
#define SENSOR_ACTIVE LOW

// =====================================================
// WAKTU (ms)
// =====================================================
const unsigned long PRESS_STABLE_TIME = 100;
const unsigned long RELEASE_STABLE_TIME = 400;
const unsigned long RESET_DEBOUNCE_TIME = 50;

const unsigned long ETH_CHECK_INTERVAL = 3000UL;
const unsigned long DHCP_RETRY_INTERVAL = 15000UL;

// Backoff reconnect MQTT: mulai 2 detik, dobel tiap gagal, maks 60 detik
const unsigned long MQTT_RETRY_MIN = 2000UL;
const unsigned long MQTT_RETRY_MAX = 60000UL;

// Harus JAUH di bawah 15 detik: dashboard menganggap OFFLINE
// jika tidak ada pesan lebih dari 15 detik.
const unsigned long HEARTBEAT_INTERVAL = 5000UL;

// Pemulihan bertingkat (dihitung sejak kabel ON tapi MQTT belum OK)
const unsigned long RESET_ETH_AFTER = 5UL * 60UL * 1000UL;
const unsigned long RESTART_AFTER = 10UL * 60UL * 1000UL;

// Kabel dianggap putus setelah N pengecekan berturut-turut (N x 3 detik)
const uint8_t LINK_OFF_CONFIRM = 2;

// Laporan ke Serial Monitor dan ke dashboard (topik diag)
const unsigned long REPORT_INTERVAL = 60000UL;
const unsigned long DIAG_INTERVAL = 60000UL;

// Watchdog: task yang tidak "lapor hidup" selama ini -> ESP32 restart
const uint32_t WDT_TIMEOUT_S = 60;

// =====================================================
// PENYIMPANAN COUNTER KE FLASH
// true  = hitungan dilanjutkan setelah restart / mati listrik
// false = hitungan selalu mulai dari 0 setelah restart
// =====================================================
const bool SIMPAN_COUNTER_FLASH = true;
const unsigned long SAVE_INTERVAL = 10000UL; // simpan maks tiap 10 detik

Preferences prefs;
bool perluSimpan = false;
unsigned long lastSave = 0;

// =====================================================
// COUNTER (dibagi antar task, selalu diakses lewat mutex)
// =====================================================
volatile unsigned long jumlahHasil = 0; // Sensor 1
volatile unsigned long jumlahModal = 0; // Sensor 2

// Bendera: "ada perubahan counter, tolong publish & update LCD"
volatile bool counterDirty = false;

portMUX_TYPE counterMux = portMUX_INITIALIZER_UNLOCKED;

// =====================================================
// STATUS JARINGAN (hanya diubah oleh networkTask)
// =====================================================
volatile bool linkUp = false; // kabel terpasang
volatile bool lanOK = false;  // kabel terpasang DAN punya IP
volatile bool mqttOK = false;
volatile int mqttErr = -1; // hasil client.state() terakhir

// -----------------------------------------------------
// SATU SUMBER KEBENARAN STATUS
// Fungsi hitungNetState() dipakai oleh LCD DAN laporan ke dashboard,
// jadi keduanya pasti sama.
// -----------------------------------------------------
enum NetState : uint8_t
{
  NS_KABEL_PUTUS = 0, // paling parah
  NS_TANPA_IP = 1,
  NS_MQTT_NG = 2,
  NS_ONLINE = 3
};

// Riwayat gangguan (dilaporkan ke dashboard setelah koneksi pulih)
bool pernahOnline = false;
unsigned long outageStart = 0;
NetState outageCause = NS_ONLINE;
unsigned long lastOutageSec = 0;
NetState lastOutageCause = NS_ONLINE;
int lastErrSeen = 0;     // kode error MQTT terakhir yang bukan 0
bool diagSegera = false; // minta kirim laporan diag secepatnya
unsigned long lastDiag = 0;

uint32_t lanLostCount = 0;
uint32_t mqttLostCount = 0;
uint8_t linkOffSamples = 0;

unsigned long lastEthCheck = 0;
unsigned long lastDhcpTry = 0;
unsigned long lastMQTTReconnect = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastReport = 0;

unsigned long mqttRetryDelay = MQTT_RETRY_MIN;

// Pelacak lama masalah (untuk pemulihan bertingkat)
unsigned long problemSince = 0;
bool sudahResetEth = false;

// =====================================================
// DATA FILTER SENSOR
// Satu struct dipakai untuk semua sensor (tidak ditulis ulang)
// =====================================================
struct Sensor
{
  uint8_t pin;
  bool state;   // status stabil setelah filter
  bool lastRaw; // pembacaan mentah terakhir
  unsigned long changeTime;
};

Sensor sensorHasil = {SENSOR_HASIL, HIGH, HIGH, 0};
Sensor sensorModal = {SENSOR_MODAL, HIGH, HIGH, 0};

// =====================================================
// HELPER COUNTER
// =====================================================
void bacaCounter(unsigned long &hasil, unsigned long &modal)
{
  portENTER_CRITICAL(&counterMux);
  hasil = jumlahHasil;
  modal = jumlahModal;
  portEXIT_CRITICAL(&counterMux);
}

void resetCounters(bool resetHasil, bool resetModal)
{
  portENTER_CRITICAL(&counterMux);
  if (resetHasil)
  {
    jumlahHasil = 0;
  }
  if (resetModal)
  {
    jumlahModal = 0;
  }
  portEXIT_CRITICAL(&counterMux);

  counterDirty = true;
}

// =====================================================
// FLASH (NVS): simpan & muat counter
// Hanya dipanggil dari setup() dan networkTask.
// =====================================================
void simpanCounter()
{
  if (!SIMPAN_COUNTER_FLASH)
  {
    return;
  }

  unsigned long hasil;
  unsigned long modal;
  bacaCounter(hasil, modal);

  prefs.begin("counter", false);
  prefs.putULong("hasil", hasil);
  prefs.putULong("modal", modal);
  prefs.end();
}

void muatCounter()
{
  if (!SIMPAN_COUNTER_FLASH)
  {
    return;
  }

  prefs.begin("counter", true);
  unsigned long hasil = prefs.getULong("hasil", 0);
  unsigned long modal = prefs.getULong("modal", 0);
  prefs.end();

  portENTER_CRITICAL(&counterMux);
  jumlahHasil = hasil;
  jumlahModal = modal;
  portEXIT_CRITICAL(&counterMux);

  Serial.print("Counter dimuat dari flash: HASIL=");
  Serial.print(hasil);
  Serial.print(" MODAL=");
  Serial.println(modal);
}

// =====================================================
// RESTART AMAN: simpan counter dulu, pamit ke broker, baru restart
// =====================================================
void restartDevice(const char *alasan)
{
  Serial.print("RESTART ESP32, alasan: ");
  Serial.println(alasan);

  simpanCounter();

  if (client.connected())
  {
    client.publish(topicStatus, STATUS_OFFLINE, true);
    client.disconnect();
  }

  delay(300);
  ESP.restart();
}

// =====================================================
// WATCHDOG
// Mendukung Arduino-ESP32 versi 2.x dan 3.x
// =====================================================
void aktifkanWatchdog()
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t cfg = {
      .timeout_ms = WDT_TIMEOUT_S * 1000,
      .idle_core_mask = 0,
      .trigger_panic = true};

  if (esp_task_wdt_reconfigure(&cfg) != ESP_OK)
  {
    esp_task_wdt_init(&cfg);
  }
#else
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
#endif
}

// =====================================================
// TERJEMAHAN KODE ERROR MQTT (maks 7 huruf agar muat di LCD)
// =====================================================
const char *namaErrorMQTT(int kode)
{
  switch (kode)
  {
  case -4:
    return "TIMEOUT";
  case -3:
    return "LOST";
  case -2:
    return "GAGAL";
  case -1:
    return "PUTUS";
  case 0:
    return "OK";
  case 1:
    return "PROTOK";
  case 2:
    return "CLIENT";
  case 3:
    return "BUSY";
  case 4:
    return "PASSWD";
  case 5:
    return "NOAKSES";
  default:
    return "???";
  }
}

// =====================================================
// STATUS JARINGAN (satu fungsi untuk LCD dan dashboard)
// =====================================================
NetState hitungNetState()
{
  if (!linkUp)
  {
    return NS_KABEL_PUTUS;
  }
  if (!lanOK)
  {
    return NS_TANPA_IP;
  }
  if (!mqttOK)
  {
    return NS_MQTT_NG;
  }
  return NS_ONLINE;
}

const char *namaNetState(NetState ns)
{
  switch (ns)
  {
  case NS_KABEL_PUTUS:
    return "KABEL_PUTUS";
  case NS_TANPA_IP:
    return "TANPA_IP";
  case NS_MQTT_NG:
    return "MQTT_NG";
  default:
    return "NONE";
  }
}

// Penyebab ESP32 terakhir menyala/restart
const char *namaResetReason()
{
  switch (esp_reset_reason())
  {
  case ESP_RST_POWERON:
    return "POWER";
  case ESP_RST_SW:
    return "SOFTWARE";
  case ESP_RST_PANIC:
    return "PANIC";
  case ESP_RST_INT_WDT:
  case ESP_RST_TASK_WDT:
  case ESP_RST_WDT:
    return "WATCHDOG";
  case ESP_RST_BROWNOUT:
    return "BROWNOUT";
  default:
    return "LAINNYA";
  }
}

// Laporan lengkap ke dashboard. Hanya bisa terkirim saat MQTT hidup,
// jadi isinya adalah RIWAYAT (apa yang terjadi saat putus tadi).
void publishDiag()
{
  if (!client.connected())
  {
    return;
  }

  IPAddress ip = Ethernet.localIP();

  char buf[400];
  snprintf(
      buf,
      sizeof(buf),
      "{\"last_outage_s\":%lu,\"last_outage_cause\":\"%s\","
      "\"last_err\":%d,\"last_err_name\":\"%s\","
      "\"lan_lost\":%lu,\"mqtt_lost\":%lu,"
      "\"reset\":\"%s\",\"up_min\":%lu,\"heap\":%lu,"
      "\"ip\":\"%u.%u.%u.%u\",\"fw\":\"%s\"}",
      lastOutageSec,
      namaNetState(lastOutageCause),
      lastErrSeen,
      namaErrorMQTT(lastErrSeen),
      (unsigned long)lanLostCount,
      (unsigned long)mqttLostCount,
      namaResetReason(),
      millis() / 60000UL,
      (unsigned long)ESP.getFreeHeap(),
      ip[0], ip[1], ip[2], ip[3],
      FW_VERSION);

  client.publish(topicDiag, buf, true);
}

// =====================================================
// LCD 20x4 (hanya dipanggil dari networkTask dan setup)
// =====================================================
void lcdTulisBaris(uint8_t baris, const char *teks)
{
  char buf[LCD_COLUMNS + 1];

  // "%-20s" = rata kiri, sisa kolom diisi spasi (menghapus sisa karakter lama)
  snprintf(buf, sizeof(buf), "%-20s", teks);

  lcd.setCursor(0, baris);
  lcd.print(buf);
}

void updateLCD()
{
  unsigned long hasil;
  unsigned long modal;
  bacaCounter(hasil, modal);

  char baris0[LCD_COLUMNS + 1];
  char baris1[LCD_COLUMNS + 1];
  char baris2[LCD_COLUMNS + 1];
  char baris3[LCD_COLUMNS + 1];

  // Baris 0 & 1: status dihitung oleh fungsi yang SAMA dengan dashboard
  switch (hitungNetState())
  {
  case NS_KABEL_PUTUS:
    snprintf(baris0, sizeof(baris0), "LAN:NG (KABEL PUTUS)");
    snprintf(baris1, sizeof(baris1), "SANDING BESAR");
    break;
  case NS_TANPA_IP:
    snprintf(baris0, sizeof(baris0), "LAN:NG (TANPA IP)");
    snprintf(baris1, sizeof(baris1), "SANDING BESAR");
    break;
  case NS_MQTT_NG:
    // Contoh: "LAN:OK MQTT:NG e=-2" dan "ERR: GAGAL"
    snprintf(baris0, sizeof(baris0), "LAN:OK MQTT:NG e=%d", mqttErr);
    snprintf(baris1, sizeof(baris1), "ERR: %s", namaErrorMQTT(mqttErr));
    break;
  default:
    snprintf(baris0, sizeof(baris0), "LAN:OK   MQTT:OK");
    snprintf(baris1, sizeof(baris1), "SANDING BESAR");
    break;
  }

  snprintf(baris2, sizeof(baris2), "Modal  : %lu", modal);
  snprintf(baris3, sizeof(baris3), "Hasil  : %lu", hasil);

  lcdTulisBaris(0, baris0);
  lcdTulisBaris(1, baris1);
  lcdTulisBaris(2, baris2);
  lcdTulisBaris(3, baris3);
}

// =====================================================
// MQTT PUBLISH (hanya dipanggil dari networkTask)
// =====================================================
void publishCounters()
{
  unsigned long hasil;
  unsigned long modal;
  bacaCounter(hasil, modal);

  char buffer[16];

  snprintf(buffer, sizeof(buffer), "%lu", hasil);
  client.publish(topicJumlah, buffer, true);

  snprintf(buffer, sizeof(buffer), "%lu", modal);
  client.publish(topicJumlah2, buffer, true);
}

void publishIP()
{
  IPAddress ip = Ethernet.localIP();

  char ipBuffer[20];
  snprintf(
      ipBuffer,
      sizeof(ipBuffer),
      "%u.%u.%u.%u",
      ip[0],
      ip[1],
      ip[2],
      ip[3]);

  client.publish(topicIP, ipBuffer, true);
}

// =====================================================
// MQTT CALLBACK (berjalan di dalam client.loop() -> networkTask)
// Memakai char array, bukan String, agar memori tidak terfragmentasi.
// =====================================================
void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  char command[24];

  unsigned int n = length < (sizeof(command) - 1) ? length : (sizeof(command) - 1);
  memcpy(command, payload, n);
  command[n] = '\0';

  // Buang spasi/enter di ujung
  while (n > 0 && isspace((unsigned char)command[n - 1]))
  {
    command[--n] = '\0';
  }

  Serial.print("MQTT Command: ");
  Serial.println(command);

  if (strcmp(command, "RESET_S1") == 0)
  {
    resetCounters(true, false);
    Serial.println("RESET HASIL");
  }
  else if (strcmp(command, "RESET_S2") == 0)
  {
    resetCounters(false, true);
    Serial.println("RESET MODAL");
  }
  else if (strcmp(command, "RESET_ALL") == 0 || strcmp(command, "RESET") == 0)
  {
    resetCounters(true, true);
    Serial.println("RESET SEMUA");
  }
  else if (strcmp(command, "RESTART_DEVICE") == 0)
  {
    restartDevice("perintah MQTT");
  }
}

// =====================================================
// MQTT CONNECT
// =====================================================
bool connectMQTT()
{
  Serial.println("Menghubungkan MQTT...");

  // Tutup socket lama agar tidak menggantung (socket W5500 jumlahnya terbatas)
  ethClient.stop();

  char clientID[40];
  snprintf(
      clientID,
      sizeof(clientID),
      "SandingBesar-%08lX",
      (unsigned long)ESP.getEfuseMac());

  bool connected;

  // Last Will: jika alat putus mendadak, broker otomatis menulis "0"
  if (strlen(mqtt_user) > 0)
  {
    connected = client.connect(clientID, mqtt_user, mqtt_pass,
                               topicStatus, 0, true, STATUS_OFFLINE);
  }
  else
  {
    connected = client.connect(clientID, topicStatus, 0, true, STATUS_OFFLINE);
  }

  if (connected)
  {
    Serial.println("MQTT CONNECTED");

    mqttOK = true;
    mqttErr = 0;
    mqttRetryDelay = MQTT_RETRY_MIN; // berhasil: jeda dikembalikan ke awal

    client.subscribe(topicPerintah);
    client.publish(topicStatus, STATUS_ONLINE, true);

    publishCounters();
    publishIP();
    diagSegera = true; // laporan diag dikirim oleh networkTask

    lastHeartbeat = millis();

    updateLCD();
    return true;
  }

  mqttErr = client.state();
  if (mqttErr != 0)
  {
    lastErrSeen = mqttErr;
  }

  Serial.print("MQTT gagal, rc=");
  Serial.print(mqttErr);
  Serial.print(" (");
  Serial.print(namaErrorMQTT(mqttErr));
  Serial.print("), coba lagi dalam ");
  Serial.print(mqttRetryDelay / 1000);
  Serial.println(" detik");

  // Gagal: jeda berikutnya digandakan, maksimal 60 detik
  mqttRetryDelay = min(mqttRetryDelay * 2, MQTT_RETRY_MAX);

  mqttOK = false;
  updateLCD();
  return false;
}

// =====================================================
// ETHERNET
// lanOK = kabel terpasang DAN sudah punya IP
// =====================================================
bool adaIP()
{
  return (uint32_t)Ethernet.localIP() != 0;
}

// Reset chip W5500 (hard reset jika pin RST disambung), lalu minta IP lagi.
// Dipakai sebagai langkah pemulihan sebelum restart total.
void resetEthernet()
{
  Serial.println("RESET MODUL ETHERNET (W5500)...");

  ethClient.stop();

#if W5500_RST_PIN >= 0
  digitalWrite(W5500_RST_PIN, LOW);
  delay(50);
  digitalWrite(W5500_RST_PIN, HIGH);
  delay(500); // beri waktu chip bangun
#endif

  Ethernet.init(W5500_CS_PIN);

  if (Ethernet.begin(mac, 8000, 2000) != 0)
  {
    Serial.print("Reset berhasil, IP: ");
    Serial.println(Ethernet.localIP());
    lanOK = true;
  }
  else
  {
    Serial.println("Reset selesai, tapi DHCP belum berhasil.");
    lanOK = false;
  }
}

void cekEthernet()
{
  if (Ethernet.linkStatus() == LinkOFF)
  {
    // Kabel dianggap putus hanya jika terdeteksi beberapa kali berturut-turut
    if (linkOffSamples < 255)
    {
      linkOffSamples++;
    }

    if (linkOffSamples >= LINK_OFF_CONFIRM)
    {
      if (linkUp)
      {
        lanLostCount++;
        Serial.println("KABEL LAN PUTUS");
      }
      linkUp = false;
      lanOK = false;
    }
    return;
  }

  // LinkON (atau Unknown): anggap kabel terpasang
  linkOffSamples = 0;

  if (!linkUp)
  {
    Serial.println("Kabel LAN terdeteksi");
  }
  linkUp = true;

  if (!adaIP())
  {
    // Kabel terpasang tapi belum dapat IP: ulangi DHCP secara berkala
    lanOK = false;

    unsigned long now = millis();

    if (now - lastDhcpTry >= DHCP_RETRY_INTERVAL)
    {
      lastDhcpTry = now;

      Serial.println("Mengulang DHCP...");

      // timeout dipendekkan agar LCD & MQTT tidak lama terhenti
      if (Ethernet.begin(mac, 8000, 2000) != 0)
      {
        Serial.print("DHCP berhasil, IP: ");
        Serial.println(Ethernet.localIP());
        lanOK = true;
      }
    }
    return;
  }

  Ethernet.maintain();
  lanOK = true;
}

// =====================================================
// FILTER SENSOR
// Mengembalikan true SATU KALI setiap ada hitungan baru.
// =====================================================
bool prosesSensor(Sensor &s)
{
  bool raw = digitalRead(s.pin);
  unsigned long now = millis();

  // Pembacaan mentah berubah: mulai hitung ulang waktu stabil
  if (raw != s.lastRaw)
  {
    s.lastRaw = raw;
    s.changeTime = now;
  }

  // Sensor aktif dan stabil cukup lama: hitung 1
  if (raw == SENSOR_ACTIVE && s.state != SENSOR_ACTIVE)
  {
    if (now - s.changeTime >= PRESS_STABLE_TIME)
    {
      s.state = SENSOR_ACTIVE;
      return true;
    }
  }
  // Sensor lepas dan stabil cukup lama: siap menghitung lagi
  else if (raw != SENSOR_ACTIVE && s.state == SENSOR_ACTIVE)
  {
    if (now - s.changeTime >= RELEASE_STABLE_TIME)
    {
      s.state = raw;
    }
  }

  return false;
}

// =====================================================
// TOMBOL RESET LOKAL (tombol BOOT)
// Tanpa delay: tidak menghentikan pembacaan sensor.
// =====================================================
void prosesTombolReset()
{
  static bool lastRaw = HIGH;
  static bool sudahDitangani = false;
  static unsigned long changeTime = 0;

  bool raw = digitalRead(RESET_BUTTON);
  unsigned long now = millis();

  if (raw != lastRaw)
  {
    lastRaw = raw;
    changeTime = now;
  }

  if (now - changeTime >= RESET_DEBOUNCE_TIME)
  {
    if (raw == LOW && !sudahDitangani)
    {
      sudahDitangani = true;
      Serial.println("RESET BUTTON");
      resetCounters(true, true);
    }
    else if (raw == HIGH && sudahDitangani)
    {
      sudahDitangani = false;
    }
  }
}

// =====================================================
// TASK SENSOR (CORE 0)
// Hanya menghitung. Tidak menyentuh MQTT, LCD, atau flash.
// =====================================================
void sensorTask(void *parameter)
{
  Serial.println("Sensor Task START");

  esp_task_wdt_add(NULL); // daftarkan task ini ke watchdog

  for (;;)
  {
    esp_task_wdt_reset(); // "saya masih hidup"

    if (prosesSensor(sensorHasil))
    {
      portENTER_CRITICAL(&counterMux);
      jumlahHasil++;
      portEXIT_CRITICAL(&counterMux);

      counterDirty = true;

      // Serial dipanggil DI LUAR critical section
      unsigned long hasil, modal;
      bacaCounter(hasil, modal);
      Serial.print("HASIL +1 = ");
      Serial.println(hasil);
    }

    if (prosesSensor(sensorModal))
    {
      portENTER_CRITICAL(&counterMux);
      jumlahModal++;
      portEXIT_CRITICAL(&counterMux);

      counterDirty = true;

      unsigned long hasil, modal;
      bacaCounter(hasil, modal);
      Serial.print("MODAL +1 = ");
      Serial.println(modal);
    }

    prosesTombolReset();

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// =====================================================
// TASK NETWORK (CORE 1)
// Satu-satunya pemilik MQTT, LCD, dan flash.
// =====================================================
void networkTask(void *parameter)
{
  Serial.println("Network Task START");

  esp_task_wdt_add(NULL); // daftarkan task ini ke watchdog

  bool prevMqttOK = false;
  NetState lastLcdState = NS_ONLINE;

  for (;;)
  {
    esp_task_wdt_reset(); // "saya masih hidup"

    unsigned long now = millis();

    // -------------------------------------------------
    // Cek Ethernet berkala
    // -------------------------------------------------
    if (now - lastEthCheck >= ETH_CHECK_INTERVAL)
    {
      lastEthCheck = now;
      cekEthernet();
      updateLCD();
    }

    // -------------------------------------------------
    // MQTT
    // -------------------------------------------------
    if (lanOK)
    {
      if (!client.connected())
      {
        mqttOK = false;

        // Jeda mengikuti backoff (2s -> 60s)
        if (now - lastMQTTReconnect >= mqttRetryDelay)
        {
          lastMQTTReconnect = now;
          connectMQTT();
        }
      }
      else
      {
        mqttOK = true;
        client.loop();
      }
    }
    else
    {
      mqttOK = false;

      // Jaringan putus: tutup koneksi lama agar tidak menggantung
      if (client.connected())
      {
        client.stop();
      }
    }

    // Hitung berapa kali MQTT putus setelah sempat online
    if (prevMqttOK && !mqttOK)
    {
      mqttLostCount++;
      Serial.println("MQTT PUTUS");
    }
    prevMqttOK = mqttOK;

    // -------------------------------------------------
    // Catat riwayat gangguan, dilaporkan setelah koneksi pulih
    // -------------------------------------------------
    NetState ns = hitungNetState();

    if (ns == NS_ONLINE)
    {
      if (outageStart != 0)
      {
        lastOutageSec = (now - outageStart) / 1000UL;
        lastOutageCause = outageCause;
        outageStart = 0;
        diagSegera = true;
      }
      pernahOnline = true;
    }
    else if (pernahOnline)
    {
      if (outageStart == 0)
      {
        outageStart = now;
        outageCause = ns;
      }
      else if (ns < outageCause)
      {
        outageCause = ns; // simpan penyebab yang paling parah
      }
    }

    // LCD langsung diperbarui saat status berubah (tanpa menunggu 3 detik)
    if (ns != lastLcdState)
    {
      lastLcdState = ns;
      updateLCD();
    }

    // Kirim laporan diag: segera setelah konek/pulih, lalu tiap 60 detik
    if (client.connected() && (diagSegera || now - lastDiag >= DIAG_INTERVAL))
    {
      lastDiag = now;
      diagSegera = false;
      publishDiag();
    }

    // -------------------------------------------------
    // Ada perubahan counter dari sensorTask / reset
    // -------------------------------------------------
    if (counterDirty)
    {
      // Bendera dimatikan SEBELUM membaca counter, jadi
      // hitungan yang masuk setelah ini tidak ada yang terlewat.
      counterDirty = false;
      perluSimpan = true;

      if (client.connected())
      {
        publishCounters();
      }

      updateLCD();
    }

    // -------------------------------------------------
    // Simpan counter ke flash (dibatasi tiap 10 detik)
    // -------------------------------------------------
    if (perluSimpan && (now - lastSave >= SAVE_INTERVAL))
    {
      lastSave = now;
      perluSimpan = false;
      simpanCounter();
    }

    // -------------------------------------------------
    // Heartbeat (timer sendiri, terpisah dari cek Ethernet)
    // -------------------------------------------------
    if (client.connected() && (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL))
    {
      lastHeartbeat = millis();
      client.publish(topicStatus, STATUS_ONLINE, true);
    }

    // -------------------------------------------------
    // Pemulihan bertingkat
    // Dihitung hanya jika kabel terpasang tapi MQTT belum OK.
    // Jika kabel yang putus, restart tidak akan membantu.
    // -------------------------------------------------
    bool bermasalah = linkUp && !mqttOK;

    if (!bermasalah)
    {
      problemSince = 0;
      sudahResetEth = false;
    }
    else if (problemSince == 0)
    {
      problemSince = now;
    }
    else
    {
      unsigned long lama = now - problemSince;

      if (!sudahResetEth && lama > RESET_ETH_AFTER)
      {
        sudahResetEth = true;
        resetEthernet(); // langkah 1: reset chip W5500
      }
      else if (lama > RESTART_AFTER)
      {
        restartDevice("MQTT/IP bermasalah > 10 menit"); // langkah 2
      }
    }

    // -------------------------------------------------
    // Laporan berkala ke Serial Monitor
    // -------------------------------------------------
    if (now - lastReport >= REPORT_INTERVAL)
    {
      lastReport = now;

      Serial.print("[LAPORAN] Uptime=");
      Serial.print(now / 60000UL);
      Serial.print("m | Kabel=");
      Serial.print(linkUp ? "ON" : "OFF");
      Serial.print(" | IP=");
      Serial.print(lanOK ? "OK" : "NO");
      Serial.print(" | MQTT=");
      Serial.print(mqttOK ? "OK" : "NO");
      Serial.print(" | LAN putus=");
      Serial.print(lanLostCount);
      Serial.print(" | MQTT putus=");
      Serial.print(mqttLostCount);
      Serial.print(" | Heap=");
      Serial.println(ESP.getFreeHeap());
    }

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

// =====================================================
// SETUP
// =====================================================
void setup()
{
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("==============================");
  Serial.println("SANDING BESAR");
  Serial.println("ESP32 + W5500 + LCD 20x4");
  Serial.println("==============================");

  // Pin sensor & tombol
  pinMode(SENSOR_HASIL, INPUT_PULLUP);
  pinMode(SENSOR_MODAL, INPUT_PULLUP);
  pinMode(RESET_BUTTON, INPUT_PULLUP);

  // Kondisi awal sensor
  sensorHasil.state = digitalRead(SENSOR_HASIL);
  sensorHasil.lastRaw = sensorHasil.state;

  sensorModal.state = digitalRead(SENSOR_MODAL);
  sensorModal.lastRaw = sensorModal.state;

  // Muat counter terakhir dari flash (jika fitur aktif)
  muatCounter();

  // LCD
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();

  lcdTulisBaris(0, "Veneer Counter 20x4");
  lcdTulisBaris(1, "SANDING BESAR");
  lcdTulisBaris(2, "Inisialisasi...");
  delay(1500);
  lcd.clear();

  // Pin RST W5500 (hanya jika dipakai): reset chip sekali saat boot
#if W5500_RST_PIN >= 0
  pinMode(W5500_RST_PIN, OUTPUT);
  digitalWrite(W5500_RST_PIN, LOW);
  delay(50);
  digitalWrite(W5500_RST_PIN, HIGH);
  delay(500);
#endif

  // SPI + W5500
  SPI.begin(W5500_SCK_PIN, W5500_MISO_PIN, W5500_MOSI_PIN, W5500_CS_PIN);
  Ethernet.init(W5500_CS_PIN);

  // DHCP (jika gagal, networkTask akan mengulang otomatis)
  Serial.println("Memulai DHCP...");

  if (Ethernet.begin(mac, 8000, 2000) == 0)
  {
    Serial.println("DHCP gagal, akan dicoba lagi otomatis.");
    lanOK = false;
  }
  else
  {
    lanOK = true;
    Serial.print("DHCP berhasil, IP: ");
    Serial.println(Ethernet.localIP());
  }

  linkUp = (Ethernet.linkStatus() != LinkOFF);

  // MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
  client.setBufferSize(512); // default 256 byte tidak cukup untuk JSON diag
  client.setKeepAlive(15);   // detik; menjaga koneksi tetap "terlihat hidup" di NAT router
  client.setSocketTimeout(3); // detik; agar koneksi gagal tidak menahan task lama

  // LCD awal
  updateLCD();

  // Watchdog harus aktif sebelum task didaftarkan
  aktifkanWatchdog();

  // Task sensor di core 0
  xTaskCreatePinnedToCore(
      sensorTask,
      "SensorTask",
      4096,
      NULL,
      2,
      NULL,
      0);

  // Task network di core 1
  xTaskCreatePinnedToCore(
      networkTask,
      "NetworkTask",
      12288,
      NULL,
      1,
      NULL,
      1);

  Serial.println("Semua task berjalan.");
}

// =====================================================
// LOOP
// Semua pekerjaan dilakukan oleh FreeRTOS Task.
// =====================================================
void loop()
{
  vTaskDelay(pdMS_TO_TICKS(1000));
}
