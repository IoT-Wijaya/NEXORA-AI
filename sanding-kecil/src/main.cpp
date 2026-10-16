#include <Wire.h>
#include <SPI.h>
#include <EthernetENC.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>

// =====================================================
// ATURAN UTAMA KODE INI
// -----------------------------------------------------
// sensorTask  (core 0): HANYA menghitung sensor & tombol reset.
//                       Tidak menyentuh MQTT atau LCD.
// networkTask (core 1): SATU-SATUNYA pemilik MQTT dan LCD.
//
// Dua task tidak boleh memakai objek yang sama
// (client MQTT, LCD) karena library-nya tidak thread-safe.
// Komunikasi antar task lewat counter + bendera counterDirty.
// =====================================================

// =====================================================
// KONFIGURASI LCD
// =====================================================
#define LCD_ADDRESS 0x27
#define LCD_COLUMNS 16
#define LCD_ROWS 2

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

// =====================================================
// KONFIGURASI ENC28J60
// =====================================================
#define ENC_CS_PIN 5
#define ENC_SCK 18
#define ENC_MISO 19
#define ENC_MOSI 23

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x01};

EthernetClient ethClient;
PubSubClient client(ethClient);

// =====================================================
// MQTT
// =====================================================
const char *mqtt_server = "IP_VPS_PRIBADI";
const int mqtt_port = 1883;

const char *mqtt_user = "";
const char *mqtt_pass = "";

// Slug mesin di Laravel HARUS sama dengan bagian ini ("sanding")
#define TOPIC_BASE "pabrik/sanding/"

const char *topicStatus = TOPIC_BASE "status";
const char *topicJumlah = TOPIC_BASE "jumlah";
const char *topicJumlah2 = TOPIC_BASE "jumlah2";
const char *topicIP = TOPIC_BASE "ip";
const char *topicPerintah = TOPIC_BASE "perintah";

// =====================================================
// NAMA MESIN (maksimal 10 karakter agar muat di LCD)
// =====================================================
const char *namaMesin = "SANDINGKCL";

// =====================================================
// PIN
// =====================================================
#define SENSOR_HASIL 27
#define SENSOR_MODAL 32
#define RESET_BUTTON 0 // GPIO0 = pin boot, jangan ditahan saat menyalakan

// Sensor aktif LOW
#define SENSOR_ACTIVE LOW

// =====================================================
// WAKTU (ms)
// =====================================================
const unsigned long PRESS_STABLE_TIME = 100;
const unsigned long RELEASE_STABLE_TIME = 400;
const unsigned long RESET_DEBOUNCE_TIME = 50;

const unsigned long LCD_PAGE_INTERVAL = 30000UL;
const unsigned long ETH_CHECK_INTERVAL = 3000UL;
const unsigned long DHCP_RETRY_INTERVAL = 15000UL;
const unsigned long MQTT_RECONNECT_INTERVAL = 5000UL;

// Harus JAUH di bawah 15 detik: Laravel menganggap OFFLINE
// jika tidak ada pesan lebih dari 15 detik.
const unsigned long HEARTBEAT_INTERVAL = 5000UL;

// =====================================================
// COUNTER (dibagi antar task, selalu diakses lewat mutex)
// =====================================================
volatile unsigned long jumlahHasil = 0;
volatile unsigned long jumlahModal = 0;

// Bendera: "ada perubahan counter, tolong publish & update LCD"
volatile bool counterDirty = false;

portMUX_TYPE counterMux = portMUX_INITIALIZER_UNLOCKED;

// =====================================================
// STATUS JARINGAN & LCD (hanya diubah oleh networkTask)
// =====================================================
volatile bool lanOK = false;
volatile bool mqttOK = false;

bool lcdMachinePage = false;
unsigned long lcdLastChange = 0;

unsigned long lastEthCheck = 0;
unsigned long lastDhcpTry = 0;
unsigned long lastMQTTReconnect = 0;
unsigned long lastHeartbeat = 0;

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
// LCD (hanya dipanggil dari networkTask dan setup)
// =====================================================
void lcdTulisBaris(uint8_t baris, const char *teks)
{
  char buf[LCD_COLUMNS + 1];

  // "%-16s" = rata kiri, sisa kolom diisi spasi (menghapus sisa karakter lama)
  snprintf(buf, sizeof(buf), "%-16s", teks);

  lcd.setCursor(0, baris);
  lcd.print(buf);
}

void updateLCD()
{
  unsigned long hasil;
  unsigned long modal;
  bacaCounter(hasil, modal);

  // Baris 1: selalu tampil, tidak berkedip
  char baris1[LCD_COLUMNS + 1];
  snprintf(baris1, sizeof(baris1), "M:%5lu H:%5lu", modal, hasil);
  lcdTulisBaris(0, baris1);

  // Baris 2: bergantian antara nama mesin dan status jaringan
  char baris2[LCD_COLUMNS + 1];

  if (lcdMachinePage)
  {
    snprintf(baris2, sizeof(baris2), "MESIN:%s", namaMesin);
  }
  else
  {
    snprintf(
        baris2,
        sizeof(baris2),
        "LAN:%s MQTT:%s",
        lanOK ? "OK" : "NO",
        mqttOK ? "OK" : "NO");
  }

  lcdTulisBaris(1, baris2);
}

void handleLCD()
{
  unsigned long now = millis();

  if (now - lcdLastChange >= LCD_PAGE_INTERVAL)
  {
    lcdLastChange = now;
    lcdMachinePage = !lcdMachinePage;
    updateLCD();
  }
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
    Serial.println("RESTART ESP32...");
    delay(500);
    ESP.restart();
  }
}

// =====================================================
// MQTT CONNECT
// =====================================================
bool connectMQTT()
{
  Serial.println("Menghubungkan MQTT...");

  char clientID[32];
  snprintf(
      clientID,
      sizeof(clientID),
      "SandingKecil-%08lX",
      (unsigned long)ESP.getEfuseMac());

  bool connected;

  if (strlen(mqtt_user) > 0)
  {
    connected = client.connect(clientID, mqtt_user, mqtt_pass);
  }
  else
  {
    connected = client.connect(clientID);
  }

  if (connected)
  {
    Serial.println("MQTT CONNECTED");

    mqttOK = true;

    client.subscribe(topicPerintah);
    client.publish(topicStatus, "ONLINE", true);

    publishCounters();
    publishIP();

    lastHeartbeat = millis();

    updateLCD();
    return true;
  }

  Serial.print("MQTT gagal, rc=");
  Serial.println(client.state());

  mqttOK = false;
  updateLCD();
  return false;
}

// =====================================================
// ETHERNET
// lanOK = kabel terpasang DAN sudah punya IP
// (kode lama hanya mengecek kabel)
// =====================================================
bool adaIP()
{
  return (uint32_t)Ethernet.localIP() != 0;
}

void cekEthernet()
{
  if (Ethernet.linkStatus() != LinkON)
  {
    lanOK = false;
    return;
  }

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
// TOMBOL RESET LOKAL
// Tanpa delay: tidak lagi menghentikan pembacaan sensor.
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
// Hanya menghitung. Tidak menyentuh MQTT atau LCD.
// =====================================================
void sensorTask(void *parameter)
{
  Serial.println("Sensor Task START");

  for (;;)
  {
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
// Satu-satunya pemilik MQTT dan LCD.
// =====================================================
void networkTask(void *parameter)
{
  Serial.println("Network Task START");

  for (;;)
  {
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

        if (now - lastMQTTReconnect >= MQTT_RECONNECT_INTERVAL)
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

    // -------------------------------------------------
    // Ada perubahan counter dari sensorTask / reset
    // -------------------------------------------------
    if (counterDirty)
    {
      // Bendera dimatikan SEBELUM membaca counter, jadi
      // hitungan yang masuk setelah ini tidak ada yang terlewat.
      counterDirty = false;

      if (client.connected())
      {
        publishCounters();
      }

      updateLCD();
    }

    // -------------------------------------------------
    // Heartbeat (timer sendiri, terpisah dari cek Ethernet)
    // -------------------------------------------------
    if (client.connected() && (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL))
    {
      lastHeartbeat = millis();
      client.publish(topicStatus, "ONLINE", true);
    }

    // -------------------------------------------------
    // Ganti halaman LCD
    // -------------------------------------------------
    handleLCD();

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
  Serial.println("SANDING KECIL");
  Serial.println("ESP32 + ENC28J60");
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

  // LCD
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();

  lcdTulisBaris(0, "SANDING KECIL");
  lcdTulisBaris(1, "STARTING...");
  delay(1500);

  // SPI + ENC28J60
  SPI.begin(ENC_SCK, ENC_MISO, ENC_MOSI, ENC_CS_PIN);
  Ethernet.init(ENC_CS_PIN);

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

  // MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(mqttCallback);
  client.setSocketTimeout(3); // detik; agar koneksi gagal tidak menahan task lama

  // LCD awal
  lcdMachinePage = false;
  lcdLastChange = millis();
  updateLCD();

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
      8192,
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
