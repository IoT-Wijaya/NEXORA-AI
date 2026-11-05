#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include "config.h"
#include <Preferences.h>
#include <Update.h>
#include "mbedtls/sha256.h"
#include "esp_task_wdt.h"
#include "esp_system.h"
#include <Dns.h>
#include <SSLClient.h>
#include <NTPClient.h>
#include "trust_anchors.h" // dibuat dengan pycert_bearssl.py: array TAs dan konstanta TAs_NUM

volatile bool otaDiminta = false;

const char *topicMasuk = "pabrik/veneer/jumlah";
const char *topicKeluar = "pabrik/veneer/jumlah2";
const char *topicRefresh = "pabrik/veneer/refresh";
const char *topicPerintah = "pabrik/veneer/perintah";
const char *topicAck = "pabrik/veneer/ack";
const char *topicMode = "pabrik/veneer/mode";

// =====================================================
//  KONFIGURASI ETHERNET W5500 (SPI: SCK=18, MISO=23, MOSI=19, CS=16)
// =====================================================
byte mac[6];

const int ETHERNET_CS_PIN = 16;

// =====================================================
//  PENGATURAN AUTO UPDATE
//  Polling dijarangkan: MQTT topik "refresh" sudah memicu ambil data.
// =====================================================
const unsigned long POLL_PILIH_MS = 300000; // 5 menit (sebelumnya 2 menit)
const unsigned long POLL_KOSONG_MS = 8000;
const unsigned long IDLE_SEBELUM_POLL_MS = 5000; // operator harus diam 5 dtk (sebelumnya 2 dtk)

// Timeout satu request HTTPS (sebelumnya bawaan SSLClient 30 dtk)
const unsigned int SSL_TIMEOUT_MS = 8000;

const int MAX_PUTARAN_PER_SISI = 2;

const int ERR_ETH_PUTUS = -100;
const int ERR_DATA_TIDAK_VALID = -101;

EthernetClient ethClient;
PubSubClient client(ethClient);

// =====================================================
//  HTTP / HTTPS
//  - httpPlain : HTTP biasa (server berupa IP lokal)
//  - httpSSL   : HTTPS lewat SSLClient (BearSSL) di atas httpBase
//  PIN_ENTROPI harus pin analog yang TIDAK tersambung ke apa pun.
// =====================================================
const int PIN_ENTROPI = 34;

EthernetClient httpPlain;
EthernetClient httpBase;
SSLClient httpSSL(httpBase, TAs, (size_t)TAs_NUM, PIN_ENTROPI);

// Sinkronisasi waktu (NTP) untuk memeriksa masa berlaku sertifikat
EthernetUDP ntpUdp;
NTPClient ntp(ntpUdp, "pool.ntp.org", 0);
unsigned long ntpEpoch = 0; // detik UNIX saat sinkron terakhir (0 = belum pernah)
unsigned long ntpMs = 0;    // millis() saat sinkron terakhir
unsigned long ntpCobaMs = 0;

// =====================================================
//  LCD I2C (20x4)
// =====================================================
LiquidCrystal_I2C lcd(0x27, 20, 4);

// =====================================================
//  DEFINISI PIN HARDWARE
// =====================================================
const int PIN_CLK = 32;
const int PIN_DT = 35;      // GPIO35 input-only, TIDAK punya pull-up internal: pasang 10k ke 3V3
const int PIN_SW = 5;       // Rotary Encoder Button (OK)
const int PIN_BTN_PIN4 = 4; // Tombol fisik tunggal (BACK)

// =====================================================
//  ENCODER (HARDWARE PCNT)
//  Putaran dihitung perangkat keras, jadi tidak hilang walau loop() tertahan
//  oleh HTTP/TLS/LCD. ambilLangkahEncoder() mengembalikan jumlah "klik"
//  sejak pemanggilan terakhir (+ = searah jarum jam).
// =====================================================
// =====================================================
//  ENCODER (POLLING + DEBOUNCE)
//  ambilLangkahEncoder() mengembalikan:
//    +1 = putar searah jarum jam, -1 = berlawanan, 0 = tidak ada putaran
//  Tepi CLK yang datang < ENC_DEBOUNCE_US setelah klik sebelumnya
//  dianggap pantulan kontak dan diabaikan.
// =====================================================
const unsigned long ENC_DEBOUNCE_US = 1500;
uint8_t encPrevCLK = HIGH;
unsigned long encLastEdgeUs = 0;

int ambilLangkahEncoder()
{
  int langkah = 0;
  uint8_t clk = digitalRead(PIN_CLK);

  if (clk != encPrevCLK) // ada perubahan level di CLK
  {
    unsigned long now = micros();

    // Hanya tepi turun (HIGH -> LOW) yang dihitung sebagai 1 klik
    if (clk == LOW && (now - encLastEdgeUs) > ENC_DEBOUNCE_US)
    {
      langkah = (digitalRead(PIN_DT) == HIGH) ? 1 : -1;
      encLastEdgeUs = now;
    }
    encPrevCLK = clk;
  }
  return langkah;
}

// =====================================================
//  TIPE DATA
// =====================================================
enum State
{
  STATE_FETCH_API,
  STATE_PILIH_DATA,
  STATE_PILIH_SISI,
  STATE_SANDING_RUNNING,
  STATE_KOREKSI_MANUAL,
  STATE_VALIDASI_SELISIH,
  STATE_KIRIM_DATA,
  STATE_KONFIRMASI_LAGI
};
State currentState = STATE_FETCH_API;

enum JenisHasil
{
  HASIL_SESUAI,
  HASIL_SUSUT,
  HASIL_ANOMALI
};

struct ErrInfo
{
  const char *judul;
  const char *saran;
  const char *detail;
};

// Hasil dari satu request HTTP:
// - status: kode HTTP (200, 401, ...) ATAU kode error negatif buatan kita
// - body  : isi balasan server (JSON)
struct HttpHasil
{
  int status;
  String body;
};

HttpHasil httpRequest(const char *method, const String &path, const String &body,
                      const char *namaHeader = nullptr, const String &nilaiHeader = "");

HttpHasil httpRequestMentah(const char *method, const String &path, const String &body,
                            const char *namaHeader, const String &nilaiHeader);
void logMqttPercobaan(bool ok, int state, unsigned long durMs);
void logMqttPutus(int state);
void logMqttMasuk(const char *topic, const byte *p, unsigned int len);
void logPerubahanNet();
unsigned long backoffBerikutnya(unsigned long sekarang);
bool mqttPublishAman(const char *topic, const char *payload, bool retain);
bool perintahDiagnostik(const String &m);
void perintahSerialDiagnostik(String line);

// =====================================================
//  PROTOTIPE FUNGSI
// =====================================================
ErrInfo infoError(int code);
String macString();
void siapkanMac();
void resetHitungan();
bool fetchDataFromAPI(bool silent);
void selesaikanSesi();

void publishIP();
void publishDiag();
void publishAktif(bool aktif);
bool sedangSanding();
void restartDevice(const char *alasan);

void handleFetchApi();
void handlePilihDataMenu();
void handlePilihSisiMenu();
void handleSandingRunning();
void handleKoreksiManual();
void handleValidasiMenu();
void handleKonfirmasiLagi();
void kirimDataKeServer();

void updateDisplayKosong();
void updateDisplayPilihData();
void updateDisplayPilihSisi();
void updateDisplayKoreksi();
void updateDisplayValidasi();
void updateDisplayKonfirmasi();

// Alamat server dipecah SEKALI di setup, tidak diulang-ulang
IPAddress serverAddr;   // hanya dipakai untuk HTTP biasa
String serverHost = ""; // nama host tanpa skema dan tanpa port
int serverPortNum = 80;
bool serverTls = false;        // true = HTTPS
String serverHostRuntime = ""; // yang tersimpan di NVS: IP/domain, boleh disertai skema dan :port

// EthernetServer asli tidak memenuhi kontrak Server::begin(uint16_t) milik
// core ESP32, jadi dianggap abstract. Class ini menambal kekurangan itu.
class ConfigServer : public EthernetServer
{
public:
  ConfigServer(uint16_t port) : EthernetServer(port) {}

  void begin(uint16_t port = 0) override
  {
    (void)port; // port sudah ditentukan lewat konstruktor
    EthernetServer::begin();
  }
};

ConfigServer configServer(8080); // halaman konfigurasi lokal ESP32

// =====================================================
//  DATA MASTER DARI API
// =====================================================
const int MAX_DATA = 20;
int apiIds[MAX_DATA];
int apiIdProduksiSanding[MAX_DATA];
int apiNoPalet[MAX_DATA];
String apiTanggal = "-";
String apiMesin = "-";
String apiShift = "PAGI";
String masterBaris1[MAX_DATA];
int masterModal[MAX_DATA];
int totalDataMaster = 0;

int indexData = 0;
String selectedSisi = "FACE";
int lastStateCLK; // tidak dipakai lagi oleh aplikasi; masih dirujuk diagnostik.h

// =====================================================
//  ANGKA SENSOR vs ANGKA KERJA
// =====================================================
int sensorMasuk = 0;
int sensorKeluar = 0;
int jumlahMasuk = 0;
int jumlahKeluar = 0;

// =====================================================
//  VARIABEL SESI
// =====================================================
bool sandingLagi = true;
int putaranFace = 0;
int putaranBack = 0;
bool autoKirimDiblokir = false;
bool adaKoreksiManual = false;
int koreksiField = 0;

// =====================================================
//  VARIABEL AUTO UPDATE
// =====================================================
int lastFetchCode = 0;
uint32_t lastPayloadHash = 0;
unsigned long lastPollMs = 0;
unsigned long lastInteractionMs = 0;
volatile bool refreshRequested = false;
bool layarKosongTergambar = false;
int lastKosongCode = -999;

// =====================================================
//  PELACAK DISPLAY
// =====================================================
int lastModal = -1, lastMasuk = -1, lastKeluar = -1, lastStatus = -1;
String lastSisi = "";

// =====================================================
//  DATA ALASAN PENYESUAIAN
// =====================================================
int alasanIndex = 0;
String opsiAlasan[] = {"Hilang", "S Error", "H Error"};
const int totalAlasan = 3;
const int ALASAN_S_ERROR = 1;

// =====================================================
//  MQTT
// =====================================================
unsigned long lastMqttAttempt = 0;

// Komunikasi dengan sensor
String ackTerakhir = ""; // balasan terakhir dari sensor ("START_OK", "STOP_OK", ...)
String sensorMode = "";  // "ON" = sensor sedang menghitung, "OFF" = tidak menghitung

// Pembatalan darurat di layar Sanding Running (BACK biasa terkunci).
const bool IZINKAN_BATAL_TAHAN = true;
const unsigned long TAHAN_BATAL_MS = 3000; // tahan BACK sekian ms untuk batal
unsigned long backTahanMulai = 0;

// =====================================================
//  PEMANTAUAN JARINGAN (LAN, MQTT, HTTP) KE DASHBOARD
// =====================================================
#define TOPIC_HMI "pabrik/hmi_sanding/" // ganti sesuai kebutuhan
const char *topicHmiStatus = TOPIC_HMI "status";
const char *topicHmiIP = TOPIC_HMI "ip";
const char *topicHmiDiag = TOPIC_HMI "diag";
const char *topicHmiPerintah = TOPIC_HMI "perintah";
const char *topicHmiOta = TOPIC_HMI "ota";
const char *topicHmiAktif = TOPIC_HMI "aktif"; // item yang sedang di-sanding (retained; kosong = tidak ada)
bool otaManual = false;

const unsigned long ETH_CHECK_INTERVAL = 3000UL;
const unsigned long DHCP_RETRY_INTERVAL = 15000UL;
const unsigned long MQTT_RETRY_MIN = 2000UL;
const unsigned long MQTT_RETRY_MAX = 60000UL;
const unsigned long HEARTBEAT_INTERVAL = 5000UL; // dashboard anggap OFFLINE jika > 15 detik
const unsigned long DIAG_INTERVAL = 60000UL;
const unsigned long RESET_ETH_AFTER = 5UL * 60UL * 1000UL;
const unsigned long RESTART_AFTER = 10UL * 60UL * 1000UL;
const uint8_t LINK_OFF_CONFIRM = 2;
const uint32_t WDT_TIMEOUT_S = 60;

enum NetState : uint8_t
{
  NS_KABEL_PUTUS = 0,
  NS_TANPA_IP = 1,
  NS_MQTT_NG = 2,
  NS_ONLINE = 3
};

bool linkUp = false, lanOK = false, mqttOK = false;
int mqttErr = -1, lastErrSeen = 0;
uint8_t linkOffSamples = 0;
uint32_t lanLostCount = 0, mqttLostCount = 0;
unsigned long lastEthCheck = 0, lastDhcpTry = 0, lastHeartbeat = 0, lastDiag = 0;
unsigned long mqttRetryDelay = MQTT_RETRY_MIN;
unsigned long problemSince = 0;
bool sudahResetEth = false;
bool diagSegera = false;
bool restartDiminta = false;

// riwayat gangguan
bool pernahOnline = false;
unsigned long outageStart = 0, lastOutageSec = 0;
NetState outageCause = NS_ONLINE, lastOutageCause = NS_ONLINE;

// statistik HTTP
int lastHttpCode = 0;
uint32_t httpOkCount = 0, httpFailCount = 0;
uint8_t httpFailStreak = 0;
unsigned long lastHttpOkMs = 0;

NetState hitungNetState()
{
  if (!linkUp)
    return NS_KABEL_PUTUS;
  if (!lanOK)
    return NS_TANPA_IP;
  if (!mqttOK)
    return NS_MQTT_NG;
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

const char *namaErrorMQTT(int k)
{
  switch (k)
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

bool statusIdle()
{
  return currentState == STATE_FETCH_API || currentState == STATE_PILIH_DATA;
}

bool adaIP() { return (uint32_t)Ethernet.localIP() != 0; }

void publishIP()
{
  IPAddress ip = Ethernet.localIP();
  char b[20];
  snprintf(b, sizeof(b), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  client.publish(topicHmiIP, b, true);
}

void publishDiag()
{
  if (!client.connected())
    return;
  IPAddress ip = Ethernet.localIP();
  long httpAgo = (lastHttpOkMs == 0) ? -1 : (long)((millis() - lastHttpOkMs) / 1000UL);

  char buf[480];
  snprintf(buf, sizeof(buf),
           "{\"last_outage_s\":%lu,\"last_outage_cause\":\"%s\","
           "\"last_err\":%d,\"last_err_name\":\"%s\","
           "\"lan_lost\":%lu,\"mqtt_lost\":%lu,"
           "\"http_code\":%d,\"http_ok\":%lu,\"http_fail\":%lu,"
           "\"http_streak\":%u,\"http_ok_ago_s\":%ld,"
           "\"sensor\":\"%s\",\"state\":%d,"
           "\"reset\":\"%s\",\"up_min\":%lu,\"heap\":%lu,"
           "\"ip\":\"%u.%u.%u.%u\",\"fw\":\"%s\"}",
           lastOutageSec, namaNetState(lastOutageCause),
           lastErrSeen, namaErrorMQTT(lastErrSeen),
           (unsigned long)lanLostCount, (unsigned long)mqttLostCount,
           lastHttpCode, (unsigned long)httpOkCount, (unsigned long)httpFailCount,
           (unsigned)httpFailStreak, httpAgo,
           sensorMode.c_str(), (int)currentState,
           namaResetReason(), millis() / 60000UL, (unsigned long)ESP.getFreeHeap(),
           ip[0], ip[1], ip[2], ip[3], FW_VERSION);
  client.publish(topicHmiDiag, buf, true);
}

void restartDevice(const char *alasan)
{
  Serial.println(String("RESTART ESP32, alasan: ") + alasan);
  if (client.connected())
  {
    client.publish(topicHmiStatus, "0", true);
    client.disconnect();
  }
  delay(300);
  ESP.restart();
}

void aktifkanWatchdog()
{
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t cfg = {
      .timeout_ms = WDT_TIMEOUT_S * 1000,
      .idle_core_mask = 0,
      .trigger_panic = true};
  if (esp_task_wdt_reconfigure(&cfg) != ESP_OK)
    esp_task_wdt_init(&cfg);
#else
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
#endif
  esp_task_wdt_add(NULL); // task loop() ikut diawasi
}

void resetEthernet()
{
  Serial.println("RESET MODUL ETHERNET (W5500)...");
  ethClient.stop();
  httpSSL.stop();
  httpBase.stop();
  httpPlain.stop();
  Ethernet.init(ETHERNET_CS_PIN);
  lanOK = (Ethernet.begin(mac, 8000, 2000) != 0);
}

void cekEthernet()
{
  if (Ethernet.linkStatus() == LinkOFF)
  {
    if (linkOffSamples < 255)
      linkOffSamples++;
    if (linkOffSamples >= LINK_OFF_CONFIRM)
    {
      if (linkUp)
      {
        lanLostCount++;
        Serial.println("KABEL LAN PUTUS");
        ethClient.stop(); // tutup socket lama supaya tidak menunggu timeout keepalive
      }
      linkUp = false;
      lanOK = false;
    }
    return;
  }

  linkOffSamples = 0;
  bool baruTersambung = !linkUp;
  linkUp = true;
  if (baruTersambung)
  {
    Serial.println("KABEL LAN TERSAMBUNG");
    mqttRetryDelay = MQTT_RETRY_MIN; // langsung coba sambung MQTT lagi
    lastMqttAttempt = 0;
  }

  if (!adaIP())
  {
    lanOK = false;
    unsigned long now = millis();
    if (now - lastDhcpTry >= DHCP_RETRY_INTERVAL)
    {
      lastDhcpTry = now;
      if (Ethernet.begin(mac, 8000, 2000) != 0)
        lanOK = true;
    }
    return;
  }

  Ethernet.maintain(); // perpanjang sewa DHCP
  lanOK = true;
}

// Dipanggil setiap putaran loop()
void pantauJaringan()
{
  static bool prevMqttOK = false;
  unsigned long now = millis();

  if (now - lastEthCheck >= ETH_CHECK_INTERVAL)
  {
    lastEthCheck = now;
    cekEthernet();
  }

  mqttOK = client.connected();
  if (prevMqttOK && !mqttOK)
  {
    mqttLostCount++;
    logMqttPutus(client.state());
  }
  prevMqttOK = mqttOK;

  // riwayat gangguan
  NetState ns = hitungNetState();
  logPerubahanNet();
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
      outageCause = ns;
  }

  // heartbeat & diag
  if (client.connected())
  {
    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL)
    {
      lastHeartbeat = now;
      mqttPublishAman(topicHmiStatus, "1", true);
    }
    if (diagSegera || now - lastDiag >= DIAG_INTERVAL)
    {
      lastDiag = now;
      diagSegera = false;
      publishDiag();
    }
  }

  // pemulihan bertingkat (hanya jika kabel ada tapi MQTT belum OK)
  bool bermasalah = linkUp && !mqttOK;
  if (!bermasalah)
  {
    problemSince = 0;
    sudahResetEth = false;
  }
  else if (problemSince == 0)
    problemSince = now;
  else
  {
    unsigned long lama = now - problemSince;
    if (!sudahResetEth && lama > RESET_ETH_AFTER)
    {
      sudahResetEth = true;
      resetEthernet(); // langkah 1: reset chip W5500
    }
    else if (lama > RESTART_AFTER && statusIdle())
    {
      restartDevice("MQTT bermasalah > 10 menit"); // langkah 2, hanya saat idle
    }
  }

  if (restartDiminta && statusIdle())
    restartDevice("perintah MQTT");
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  logMqttMasuk(topic, payload, length);

  String message = "";
  for (unsigned int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }

  String t = String(topic);
  if (t == topicMasuk)
  {
    sensorMasuk = message.toInt();
  }
  else if (t == topicKeluar)
  {
    sensorKeluar = message.toInt();
  }
  else if (t == topicRefresh)
  {
    refreshRequested = true;
  }
  else if (t == topicAck)
  {
    ackTerakhir = message;
  }
  else if (t == topicMode)
  {
    sensorMode = message;
  }
  else if (t == topicHmiPerintah)
  {
    message.trim();
    if (message == "RESTART_DEVICE")
      restartDiminta = true; // dieksekusi di pantauJaringan() saat HMI idle
    else if (message == "CEK_OTA")
      otaDiminta = true; // hanya bendera, unduhan dijalankan di loop()
    else
      perintahDiagnostik(message); // HWTEST_ON/OFF, STATUS, MQTT_RECONNECT, DEBUG_n
  }
}

bool mqttTryConnect()
{
  ethClient.stop();
  String clientId = "ESP32Sanding-" + macString();
  unsigned long t0 = millis();
  bool ok = client.connect(clientId.c_str(), topicHmiStatus, 0, true, "0");
  unsigned long dur = millis() - t0;

  if (ok)
  {
    mqttOK = true;
    mqttErr = 0;
    mqttRetryDelay = MQTT_RETRY_MIN;
    client.subscribe(topicMasuk);
    client.subscribe(topicKeluar);
    client.subscribe(topicRefresh);
    client.subscribe(topicAck);
    client.subscribe(topicMode);
    client.subscribe(topicHmiPerintah);
    client.publish(topicHmiStatus, "1", true);
    publishIP();
    // Setelah boot/sambung ulang: hapus pesan aktif basi, atau kirim ulang jika memang sedang sanding
    publishAktif(sedangSanding());
    diagSegera = true;
    lastHeartbeat = millis();
    logMqttPercobaan(true, 0, dur);
    return true;
  }

  mqttErr = client.state();
  if (mqttErr != 0)
    lastErrSeen = mqttErr;
  mqttRetryDelay = backoffBerikutnya(mqttRetryDelay);
  mqttOK = false;
  logMqttPercobaan(false, mqttErr, dur);
  return false;
}

void reconnectMQTT()
{
  if (!lanOK)
    return; // tanpa jaringan, percuma mencoba
  if (millis() - lastMqttAttempt < mqttRetryDelay)
    return;
  lastMqttAttempt = millis();
  mqttTryConnect();
}

// =====================================================
//  PERINTAH KE SENSOR (dengan konfirmasi + ulang 3x)
// =====================================================
bool kirimPerintahSensor(const char *perintah, const char *ackDiharapkan)
{
  for (int percobaan = 1; percobaan <= 3; percobaan++)
  {
    if (!client.connected())
    {
      if (!mqttTryConnect())
      {
        delay(500);
        continue;
      }
    }

    ackTerakhir = "";
    // retain = false (PENTING): kalau di-retain, sensor akan menjalankan
    // perintah yang sama lagi setiap kali reconnect ke broker.
    client.publish(topicPerintah, perintah, false);

    unsigned long mulai = millis();
    while (millis() - mulai < 2000)
    {
      esp_task_wdt_reset();
      client.loop(); // wajib, supaya balasan bisa diterima
      if (ackTerakhir == ackDiharapkan)
      {
        Serial.println("[SENSOR] " + String(perintah) + " OK (percobaan " + String(percobaan) + ")");
        return true;
      }
      delay(10);
    }
    Serial.println("[SENSOR] " + String(perintah) + " tidak dibalas, percobaan " + String(percobaan));
  }
  return false;
}

// START: sensor nol-kan angka lalu mulai menghitung
bool mulaiHitungSensor()
{
  bool ok = kirimPerintahSensor("START", "START_OK");
  if (ok)
  {
    sensorMode = "ON";
    publishAktif(true); // dashboard: item ini sedang di-sanding
  }
  return ok;
}

// STOP: sensor berhenti menghitung lalu nol-kan angka
bool hentikanHitungSensor()
{
  publishAktif(false); // dipakai batal darurat dan kirim sukses: sanding berakhir apa pun balasan sensor
  bool ok = kirimPerintahSensor("STOP", "STOP_OK");
  if (ok)
    sensorMode = "OFF";
  return ok;
}

// Pembatalan darurat (BACK ditahan lama di layar Sanding Running)
void batalkanPutaran()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("BATAL SANDING...");
  hentikanHitungSensor();

  // Tunggu tombol dilepas supaya tidak "tembus" ke layar berikutnya
  while (digitalRead(PIN_BTN_PIN4) == LOW)
  {
    esp_task_wdt_reset();
    if (client.connected())
      client.loop();
    delay(10);
  }
  backTahanMulai = 0;

  resetHitungan();
  lastModal = -1;
  currentState = STATE_PILIH_SISI;
  updateDisplayPilihSisi();
}

// =====================================================
//  INFO & TAMPIL ERROR
// =====================================================
ErrInfo infoError(int code)
{
  switch (code)
  {
  case -1:
    return {"Tdk bisa konek", "Cek server/TLS/NTP", "Koneksi gagal."};
  case ERR_ETH_PUTUS:
    return {"Kabel LAN lepas", "Cek koneksi W5500", "Ethernet terputus."};
  case ERR_DATA_TIDAK_VALID:
    return {"Data aneh", "Balasan bukan JSON", "Format salah."};
  case 301:
  case 302:
  case 307:
  case 308:
    return {"Dialihkan server", "Pakai https:// di cfg", "Redirect."};
  case 401:
    return {"API key salah", "Cek X-API-KEY", "API key ditolak."};
  case 404:
    return {"Tdk ditemukan", "Cek URL/ID palet", "Data tidak ada."};
  case 422:
    return {"Data ditolak", "Validasi Laravel", "Gagal validasi."};
  default:
    return {"HTTP error", "Lihat Serial Mon.", "Error umum."};
  }
}

void logError(const char *tag, int code)
{
  ErrInfo e = infoError(code);
  Serial.println(String("[") + tag + "] Kode " + String(code) + " - " + e.judul);
}

void tampilkanError(int code, int baris)
{
  ErrInfo e = infoError(code);
  lcd.setCursor(0, baris);
  lcd.print("E" + String(code) + " " + String(e.judul));
  lcd.setCursor(0, baris + 1);
  lcd.print(e.saran);
}

// =====================================================
//  LOGIKA HASIL
// =====================================================
bool hasilSesuai()
{
  int target = masterModal[indexData];
  return (target > 0 && jumlahMasuk == target && jumlahKeluar == target);
}

JenisHasil klasifikasiHasil()
{
  int target = masterModal[indexData];
  if (jumlahMasuk > target || jumlahKeluar > jumlahMasuk)
    return HASIL_ANOMALI;
  if (hasilSesuai())
    return HASIL_SESUAI;
  return HASIL_SUSUT;
}

void resetHitungan()
{
  // sensorMasuk / sensorKeluar SENGAJA tidak di-nol-kan di sini.
  jumlahMasuk = 0;
  jumlahKeluar = 0;
  alasanIndex = 0;
  autoKirimDiblokir = false;
  adaKoreksiManual = false;
  koreksiField = 0;
}

void siapkanMac()
{
  // MAC bawaan chip ESP32 (unik, tidak berubah)
  uint64_t chipId = ESP.getEfuseMac();
  for (int i = 0; i < 6; i++)
    mac[i] = (chipId >> (8 * (5 - i))) & 0xFF;

  // Bit 1 di byte pertama = "locally administered"
  mac[0] = (mac[0] | 0x02) & 0xFE;
}

String macString()
{
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

void kembaliKeMonitoring()
{
  adaKoreksiManual = false;
  koreksiField = 0;
  lastModal = -1;
  currentState = STATE_SANDING_RUNNING;
}

uint32_t hashString(const String &s)
{
  uint32_t h = 5381;
  for (size_t i = 0; i < s.length(); i++)
  {
    h = ((h << 5) + h) + (uint8_t)s[i];
  }
  return h;
}

int putaranSisiTerpilih()
{
  return (selectedSisi == "FACE") ? putaranFace : putaranBack;
}

// =====================================================
//  ITEM AKTIF KE DASHBOARD (topik pabrik/hmi_sanding/aktif, retained)
//  Berisi barang yang sedang di-sanding. Payload kosong = tidak ada sanding berjalan.
// =====================================================
// Sanding dianggap berjalan sejak sensor START sampai data terkirim / dibatalkan.
bool sedangSanding()
{
  return currentState == STATE_SANDING_RUNNING || currentState == STATE_KOREKSI_MANUAL ||
         currentState == STATE_VALIDASI_SELISIH || currentState == STATE_KIRIM_DATA;
}

void publishAktif(bool aktif)
{
  if (!client.connected())
    return;
  if (!aktif || totalDataMaster == 0)
  {
    client.publish(topicHmiAktif, "", true); // payload kosong menghapus pesan retained
    return;
  }

  JsonDocument d;
  d["id"] = apiIds[indexData];
  d["barang"] = masterBaris1[indexData];
  d["no_palet"] = apiNoPalet[indexData];
  d["target"] = masterModal[indexData];
  d["sisi"] = selectedSisi;
  d["putaran_ke"] = putaranSisiTerpilih() + 1;
  d["mesin"] = apiMesin;
  d["shift"] = apiShift;

  char b[320];
  size_t n = serializeJson(d, b, sizeof(b));
  if (n == 0 || n >= sizeof(b))
  {
    Serial.println("[AKTIF] payload terpotong, tidak dikirim");
    return;
  }
  client.publish(topicHmiAktif, b, true);
}

// =====================================================
//  ALAMAT SERVER (HTTP / HTTPS)
//  Format yang diterima:
//    192.168.1.10:8000              -> HTTP
//    server.contoh.com              -> HTTPS port 443
//    https://server.contoh.com      -> HTTPS port 443
//    https://server.contoh.com:8443 -> HTTPS port 8443
//    http://server.contoh.com       -> HTTP port 80
// =====================================================
bool hostValid(const String &h)
{
  if (h.length() < 1 || h.length() > 100)
    return false;
  for (size_t i = 0; i < h.length(); i++)
  {
    char c = h[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '.' || c == '-'))
      return false;
  }
  return true;
}

bool uraiAlamat(String s, String &host, int &port, bool &tls)
{
  s.trim();
  bool adaSkema = false;
  tls = false;
  if (s.startsWith("https://"))
  {
    tls = true;
    adaSkema = true;
    s = s.substring(8);
  }
  else if (s.startsWith("http://"))
  {
    adaSkema = true;
    s = s.substring(7);
  }
  int garis = s.indexOf('/');
  if (garis >= 0)
    s = s.substring(0, garis);

  bool adaPort = false;
  port = tls ? 443 : 80;
  int dua = s.indexOf(':');
  if (dua >= 0)
  {
    port = s.substring(dua + 1).toInt();
    adaPort = true;
    s = s.substring(0, dua);
  }
  host = s;

  // Tanpa skema: domain -> HTTPS, IP -> HTTP
  if (!adaSkema)
  {
    IPAddress uji;
    if (adaPort)
      tls = (port == 443);
    else if (!uji.fromString(host))
    {
      tls = true;
      port = 443;
    }
  }
  return hostValid(host) && port >= 1 && port <= 65535;
}

bool resolveServer()
{
  if (!uraiAlamat(serverHostRuntime, serverHost, serverPortNum, serverTls))
    return false;
  if (serverTls)
    return true; // SSLClient mencari IP sendiri lewat nama domain

  if (serverAddr.fromString(serverHost))
    return true;

  DNSClient dns;
  dns.begin(Ethernet.dnsServerIP());
  IPAddress hasil;
  if (dns.getHostByName(serverHost.c_str(), hasil) == 1)
  {
    serverAddr = hasil;
    Serial.println("[CFG] " + serverHost + " -> " + hasil.toString());
    return true;
  }
  Serial.println("[CFG] DNS gagal untuk " + serverHost);
  return false;
}

// Header Host: port tidak ditulis jika port bawaan skema
String hostHeader()
{
  bool bawaan = (serverTls && serverPortNum == 443) || (!serverTls && serverPortNum == 80);
  return bawaan ? serverHost : serverHost + ":" + String(serverPortNum);
}

bool siapkanAlamatServer()
{
  if (serverHostRuntime.length() == 0)
    serverHostRuntime = String(serverIp);

  bool ok = resolveServer();
  if (!ok)
  {
    Serial.println("[CFG] Alamat server gagal, pakai cadangan config.h");
    serverHostRuntime = String(serverIp);
    resolveServer();
  }
  Serial.println("[CFG] Server Laravel: " + serverHostRuntime + (serverTls ? " (HTTPS)" : " (HTTP)"));
  return ok;
}

// =====================================================
//  WAKTU (NTP) - dibutuhkan untuk memeriksa masa berlaku sertifikat
// =====================================================
void sinkronWaktu()
{
  ntp.begin();
  if (ntp.forceUpdate())
  {
    ntpEpoch = ntp.getEpochTime();
    ntpMs = millis();
    Serial.println("[NTP] epoch " + String(ntpEpoch));
  }
  else
  {
    Serial.println("[NTP] gagal sinkron");
  }
  ntp.end();
}

// Dipanggil setiap putaran loop(). Hanya saat idle agar tidak mengganggu operator.
void jagaWaktu()
{
  if (!lanOK || !statusIdle())
    return;
  unsigned long now = millis();
  unsigned long batas = (ntpEpoch == 0) ? 30000UL : 21600000UL; // 30 dtk jika belum pernah, 6 jam jika sudah
  if (ntpCobaMs != 0 && now - ntpCobaMs < batas)
    return;
  ntpCobaMs = now;
  sinkronWaktu();
}

// Beri tahu BearSSL jam sekarang (hari sejak tahun 0, detik sejak tengah malam)
void setWaktuTls()
{
  if (ntpEpoch == 0)
    return;
  uint32_t e = ntpEpoch + (millis() - ntpMs) / 1000UL;
  httpSSL.setVerificationTime(e / 86400UL + 719528UL, e % 86400UL);
}

// =====================================================
//  KUNCI (disimpan di NVS, TIDAK ada di firmware)
//  Isi lewat Serial Monitor:
//    setkey <API key sanding>
//    setotakey <key dari: php artisan device:key MAC>
// =====================================================
Preferences prefs;
String apiKeyRuntime = "";
String otaKeyRuntime = "";

void muatKunci()
{
  prefs.begin("cfg", false);
  apiKeyRuntime = prefs.getString("apikey", "");
  otaKeyRuntime = prefs.getString("otakey", "");
  serverHostRuntime = prefs.getString("server", String(serverIp));
  prefs.end();
  if (apiKeyRuntime.length() == 0)
    Serial.println("[CFG] apikey belum diset. Ketik: setkey <KEY>");
  if (otaKeyRuntime.length() == 0)
    Serial.println("[CFG] otakey belum diset. Ketik: setotakey <KEY>");
}

void cekPerintahSerial()
{
  if (!Serial.available())
    return;
  String line = Serial.readStringUntil('\n');
  line.trim();

  if (line.startsWith("setkey "))
  {
    apiKeyRuntime = line.substring(7);
    apiKeyRuntime.trim();
    prefs.begin("cfg", false);
    prefs.putString("apikey", apiKeyRuntime);
    prefs.end();
    Serial.println("[CFG] apikey tersimpan.");
  }
  else if (line.startsWith("setotakey "))
  {
    otaKeyRuntime = line.substring(10);
    otaKeyRuntime.trim();
    prefs.begin("cfg", false);
    prefs.putString("otakey", otaKeyRuntime);
    prefs.end();
    Serial.println("[CFG] otakey tersimpan.");
  }
  else
  {
    perintahSerialDiagnostik(line);
  }
}

// =====================================================
//  KONFIGURASI MELALUI BROWSER (LAN PORT 8080)
//  Buka: http://IP-ESP32:8080
// =====================================================
String decodeFormValue(const String &v)
{
  String out;
  out.reserve(v.length());
  for (size_t i = 0; i < v.length(); i++)
  {
    if (v[i] == '+')
      out += ' ';
    else if (v[i] == '%' && i + 2 < v.length())
    {
      char hex[3] = {v[i + 1], v[i + 2], 0};
      out += (char)strtol(hex, nullptr, 16);
      i += 2;
    }
    else
      out += v[i];
  }
  return out;
}

String ambilForm(const String &body, const String &nama)
{
  String cari = nama + "=";
  int mulai = body.indexOf(cari);
  if (mulai < 0)
    return "";
  mulai += cari.length();
  int akhir = body.indexOf('&', mulai);
  if (akhir < 0)
    akhir = body.length();
  return decodeFormValue(body.substring(mulai, akhir));
}

void kirimHalamanKonfigurasi(EthernetClient &c, const String &pesan = "")
{
  String html = "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<meta charset='utf-8'><title>Konfigurasi HMI</title>"
                "<style>body{font-family:Arial;max-width:520px;margin:24px auto;padding:0 14px;background:#f4f5f7}"
                ".card{background:white;padding:20px;border-radius:12px}input{box-sizing:border-box;width:100%;padding:12px;margin:7px 0 15px}"
                "button{padding:12px 18px;background:#1769aa;color:white;border:0;border-radius:7px}small{color:#555}</style></head><body><div class='card'>"
                "<h2>Konfigurasi HMI Sanding</h2>";
  if (pesan.length())
    html += "<p><b>" + pesan + "</b></p>";
  html += "<form method='POST' action='/save'>"
          "<label>IP atau domain Laravel (contoh https://server.contoh.com atau 192.168.1.10:8000)</label>"
          "<input name='server' required value='" +
          serverHostRuntime + "'>"
                              "<label>API Key Laravel</label><input name='apikey' type='password' placeholder='Kosongkan jika tidak diubah'>"
                              "<label>OTA Device Key</label><input name='otakey' type='password' placeholder='Kosongkan jika tidak diubah'>"
                              "<button type='submit'>Simpan Konfigurasi</button></form>"
                              "<small>API Key dan OTA Key tidak ditampilkan kembali. Perangkat akan memakai nilai yang tersimpan setelah konfigurasi disimpan.</small>"
                              "<p>Mode saat ini: " +
          String(serverTls ? "HTTPS" : "HTTP") + "</p>"
                                                 "<p>IP ESP32: " +
          Ethernet.localIP().toString() + "</p></div></body></html>";
  c.println("HTTP/1.1 200 OK");
  c.println("Content-Type: text/html; charset=utf-8");
  c.println("Connection: close");
  c.println("Content-Length: " + String(html.length()));
  c.println();
  c.print(html);
}

void prosesWebKonfigurasi()
{
  EthernetClient web = configServer.available();
  if (!web)
    return;

  unsigned long mulai = millis();
  while (!web.available() && millis() - mulai < 1500)
    delay(1);
  if (!web.available())
  {
    web.stop();
    return;
  }

  String requestLine = web.readStringUntil('\n');
  requestLine.trim();
  bool isPost = requestLine.startsWith("POST ");
  int contentLength = 0;
  while (web.connected())
  {
    String h = web.readStringUntil('\n');
    if (h == "\r" || h.length() == 0)
      break;
    h.trim();
    String lower = h;
    lower.toLowerCase();
    if (lower.startsWith("content-length:"))
      contentLength = lower.substring(15).toInt();
  }

  String body;
  if (isPost && contentLength > 0 && contentLength < 2048)
  {
    mulai = millis();
    while ((int)body.length() < contentLength && millis() - mulai < 3000)
    {
      while (web.available() && (int)body.length() < contentLength)
        body += (char)web.read();
      delay(1);
    }
  }

  if (!isPost)
  {
    kirimHalamanKonfigurasi(web);
  }
  else if (requestLine.startsWith("POST /save ") && (int)body.length() == contentLength)
  {
    String host = ambilForm(body, "server");
    String api = ambilForm(body, "apikey");
    String ota = ambilForm(body, "otakey");
    host.trim();
    api.trim();
    ota.trim();

    String hTmp;
    int pTmp;
    bool tTmp;
    bool valid = uraiAlamat(host, hTmp, pTmp, tTmp);

    if (!valid)
    {
      kirimHalamanKonfigurasi(web, "Format server tidak valid. Contoh: https://server.contoh.com, server.contoh.com, 192.168.1.10:8000.");
    }
    else
    {
      prefs.begin("cfg", false);
      prefs.putString("server", host);
      if (api.length())
        prefs.putString("apikey", api);
      if (ota.length())
        prefs.putString("otakey", ota);
      prefs.end();

      serverHostRuntime = host;
      if (api.length())
        apiKeyRuntime = api;
      if (ota.length())
        otaKeyRuntime = ota;
      siapkanAlamatServer();
      kirimHalamanKonfigurasi(web, "Konfigurasi berhasil disimpan.");
      Serial.println("[CFG] Konfigurasi browser disimpan (nilai kunci tidak ditampilkan).");
    }
  }
  else
  {
    web.println("HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\nRequest tidak valid");
  }

  delay(2);
  web.stop();
}

// =====================================================
//  OTA
//  CATATAN: pengecekan versi memakai httpRequest() sehingga ikut HTTPS,
//  tetapi unduhan firmware (unduhDanPasang) masih HTTP biasa. Selama
//  server memakai HTTPS, OTA dilewati agar layar tidak terganggu.
// =====================================================
const unsigned long OTA_CEK_PERTAMA_MS = 30000UL;   // 30 detik setelah boot
const unsigned long OTA_CEK_INTERVAL_MS = 600000UL; // lalu tiap 10 menit
const unsigned long OTA_IDLE_MIN_MS = 10000UL;      // jangan ganggu operator
unsigned long otaBerikutnyaMs = OTA_CEK_PERTAMA_MS;

String hexDari(const uint8_t *d, size_t n)
{
  static const char *h = "0123456789abcdef";
  String s;
  s.reserve(n * 2);
  for (size_t i = 0; i < n; i++)
  {
    s += h[d[i] >> 4];
    s += h[d[i] & 0x0F];
  }
  return s;
}

bool laporOta(const String &versi, const char *status, const String &pesan)
{
  JsonDocument doc;
  doc["mac"] = macString();
  doc["version"] = versi;
  doc["status"] = status;
  doc["message"] = pesan.substring(0, 200);
  String body;
  serializeJson(doc, body);

  HttpHasil r = httpRequest("POST", "/api/device/firmware/report", body, "X-Device-Key", otaKeyRuntime);
  Serial.println("[OTA] lapor " + String(status) + " -> HTTP " + String(r.status));
  return r.status >= 200 && r.status < 300;
}

// Dipanggil sekali setelah boot: lapor hasil update yang tertunda
void laporHasilBoot()
{
  prefs.begin("cfg", false);
  String pending = prefs.isKey("pending") ? prefs.getString("pending", "") : ""; // hindari log error NVS NOT_FOUND
  prefs.end();
  if (pending.length() == 0 || otaKeyRuntime.length() == 0)
    return;

  bool sukses = (pending == String(FW_VERSION));
  String pesan = sukses ? String("") : (String("Boot dengan versi ") + FW_VERSION);

  if (laporOta(pending, sukses ? "success" : "failed", pesan))
  {
    prefs.begin("cfg", false);
    prefs.remove("pending");
    prefs.end();
  }
}

void laporProgresOta(const char *state, int pct, const char *msg)
{
  if (!client.connected())
    return;
  char b[200];
  if (pct >= 0)
    snprintf(b, sizeof(b), "{\"state\":\"%s\",\"pct\":%d,\"msg\":\"%s\"}", state, pct, msg);
  else
    snprintf(b, sizeof(b), "{\"state\":\"%s\",\"msg\":\"%s\"}", state, msg);
  client.publish(topicHmiOta, b, false); // retain = false
  client.loop();
}

// Unduh dari Laravel, tulis ke flash, hitung SHA-256 selama unduh
// (masih HTTP biasa, belum mendukung HTTPS)
bool unduhDanPasang(const String &versi, const String &checksumHex, String &alasan)
{
  if (Ethernet.linkStatus() == LinkOFF)
  {
    alasan = "Kabel LAN lepas";
    return false;
  }

  EthernetClient dl; // koneksi terpisah dari MQTT dan HTTP biasa
  if (!dl.connect(serverAddr, serverPortNum))
  {
    alasan = "Gagal konek server";
    return false;
  }

  dl.println("GET /api/device/firmware/download?mac=" + macString() + "&device=" + DEVICE_TYPE + "&version=" + versi + " HTTP/1.0");
  dl.println("Host: " + hostHeader());
  dl.println("X-Device-Key: " + otaKeyRuntime);
  dl.println("Connection: close");
  dl.println();

  unsigned long mulai = millis();
  while (dl.connected() && !dl.available())
  {
    if (millis() - mulai > 10000)
    {
      dl.stop();
      alasan = "Server tidak menjawab";
      return false;
    }
    delay(10);
  }

  String statusLine = dl.readStringUntil('\n');
  int spasi = statusLine.indexOf(' ');
  int kode = (spasi < 0) ? 0 : statusLine.substring(spasi + 1, spasi + 4).toInt();
  if (kode != 200)
  {
    dl.stop();
    alasan = "Download HTTP " + String(kode);
    return false;
  }

  long panjang = -1;
  while (dl.connected() || dl.available())
  {
    String line = dl.readStringUntil('\n');
    if (line == "\r" || line.length() == 0)
      break;
    line.toLowerCase();
    if (line.startsWith("content-length:"))
      panjang = line.substring(15).toInt();
  }

  if (panjang <= 0)
  {
    dl.stop();
    alasan = "Content-Length tidak valid";
    return false;
  }
  if (!Update.begin(panjang))
  {
    dl.stop();
    alasan = String("Update.begin: ") + Update.errorString();
    return false;
  }

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);

  uint8_t buf[1024];
  long diterima = 0;
  int persenTerakhir = -1;
  bool gagal = false;
  unsigned long terakhir = millis();

  while (diterima < panjang)
  {
    esp_task_wdt_reset(); // unduhan bisa > 60 detik
    client.loop();
    int tersedia = dl.available();
    if (tersedia > 0)
    {
      long sisa = panjang - diterima;
      int minta = tersedia;
      if (minta > (int)sizeof(buf))
        minta = sizeof(buf);
      if (minta > sisa)
        minta = (int)sisa;

      int n = dl.read(buf, minta);
      if (n <= 0)
        continue;

      if (Update.write(buf, n) != (size_t)n)
      {
        alasan = String("Tulis flash: ") + Update.errorString();
        gagal = true;
        break;
      }
      mbedtls_sha256_update(&sha, buf, n);
      diterima += n;
      terakhir = millis();

      int persen = (int)((diterima * 100L) / panjang);
      if (persen != persenTerakhir && persen % 5 == 0)
      {
        persenTerakhir = persen;
        lcd.setCursor(0, 2);
        lcd.print("Unduh: " + String(persen) + "%    ");
        laporProgresOta("downloading", persen, "Mengunduh firmware");
      }
    }
    else
    {
      if (!dl.connected() || millis() - terakhir > 15000)
        break;
      delay(1);
    }
  }

  uint8_t digest[32];
  mbedtls_sha256_finish(&sha, digest);
  mbedtls_sha256_free(&sha);
  dl.stop();

  if (gagal)
  {
    Update.abort();
    return false;
  }
  if (diterima != panjang)
  {
    Update.abort();
    alasan = "Unduhan tidak lengkap";
    return false;
  }
  if (!hexDari(digest, 32).equalsIgnoreCase(checksumHex))
  {
    Update.abort();
    alasan = "Checksum tidak cocok";
    return false;
  }
  if (!Update.end(true))
  {
    alasan = String("Update.end: ") + Update.errorString();
    return false;
  }

  return true;
}

void cekUpdateFirmware()
{
  if (otaKeyRuntime.length() == 0)
  {
    Serial.println("[OTA] otakey belum diset, dilewati");
    if (otaManual)
      laporProgresOta("failed", -1, "OTA key belum diset di HMI");
    return;
  }

  // Unduhan firmware belum mendukung HTTPS: lewati sampai OTA dimigrasikan
  if (serverTls)
  {
    Serial.println("[OTA] dilewati: unduhan firmware belum mendukung HTTPS");
    if (otaManual)
      laporProgresOta("failed", -1, "OTA belum mendukung HTTPS");
    return;
  }

  String path = "/api/device/firmware/latest?mac=" + macString() +
                "&device=" + DEVICE_TYPE + "&current_version=" + FW_VERSION;
  HttpHasil r = httpRequest("GET", path, "", "X-Device-Key", otaKeyRuntime);

  // 1. Server harus menjawab 200 dulu
  if (r.status != 200)
  {
    Serial.println("[OTA] cek gagal, HTTP " + String(r.status));
    if (otaManual)
      laporProgresOta("failed", -1, ("Server firmware HTTP " + String(r.status)).c_str());
    return;
  }

  // 2. Baca JSON, cek ada update atau tidak
  JsonDocument doc;
  if (deserializeJson(doc, r.body) || !doc["update_available"].as<bool>())
  {
    if (otaManual)
      laporProgresOta("uptodate", -1, "Firmware sudah terbaru");
    return;
  }

  // 3. Baru sekarang versi dan checksum boleh dipakai
  String versi = doc["latest"]["version"].as<String>();
  String checksum = doc["latest"]["checksum"].as<String>();
  if (versi.length() == 0 || checksum.length() != 64)
  {
    if (otaManual)
      laporProgresOta("failed", -1, "Data rilis dari server tidak valid");
    return;
  }

  Serial.println("[OTA] versi baru: " + versi);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("UPDATE FIRMWARE");
  lcd.setCursor(0, 1);
  lcd.print(versi);
  lcd.setCursor(0, 3);
  lcd.print("Jangan matikan alat!");

  String alasan;
  if (unduhDanPasang(versi, checksum, alasan))
  {
    prefs.begin("cfg", false);
    prefs.putString("pending", versi); // dilaporkan setelah boot
    prefs.end();
    lcd.setCursor(0, 2);
    lcd.print("Selesai, restart... ");
    laporProgresOta("installing", 100, "Terpasang, HMI restart");
    for (int i = 0; i < 10; i++) // beri waktu pesan terkirim
    {
      client.loop();
      delay(50);
    }
    delay(1000);
    ESP.restart();
  }

  Serial.println("[OTA] GAGAL: " + alasan);
  laporOta(versi, "failed", alasan);
  laporProgresOta("failed", -1, alasan.c_str());
  lcd.setCursor(0, 2);
  lcd.print("Update gagal        ");
  delay(2500);

  // Kembalikan tampilan layar sebelumnya
  lastModal = -1;
  if (currentState == STATE_PILIH_DATA)
    updateDisplayPilihData();
  else
    layarKosongTergambar = false;
}

void cekOtaBerkala()
{
  // Hanya saat idle. TIDAK PERNAH saat sanding berjalan
  if (currentState != STATE_FETCH_API && currentState != STATE_PILIH_DATA)
    return;
  if ((long)(millis() - otaBerikutnyaMs) < 0)
    return;
  if (millis() - lastInteractionMs < OTA_IDLE_MIN_MS)
    return;

  otaBerikutnyaMs = millis() + OTA_CEK_INTERVAL_MS;
  cekUpdateFirmware();
}

// =====================================================
//  SETUP & LOOP
// =====================================================
#include "diagnostik.h"
void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("========== BOOT DIAGNOSTIC ==========");
  Serial.printf("Reset reason: %d\n", esp_reset_reason());
  Serial.printf("Free heap: %lu\n", (unsigned long)ESP.getFreeHeap());
  Serial.println("=====================================");
  muatKunci();
  Serial.println("\n--- HMI SANDING IOT (ETHERNET W5500 + NEW PINS) ---");

  // Tombol OK dan BACK tetap dibaca biasa. CLK/DT dikelola ESP32Encoder (PCNT).
  pinMode(PIN_SW, INPUT_PULLUP);
  pinMode(PIN_BTN_PIN4, INPUT_PULLUP);
  pinMode(PIN_CLK, INPUT_PULLUP);
  pinMode(PIN_DT, INPUT_PULLUP); // GPIO35 tidak punya pull-up internal: tetap pakai 10k eksternal
  encPrevCLK = digitalRead(PIN_CLK);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("MENYIAPKAN ETHERNET...");

  // Inisialisasi SPI W5500 (SCK=18, MISO=23, MOSI=19, SS=16)
  SPI.begin(18, 23, 19, ETHERNET_CS_PIN);
  Ethernet.init(ETHERNET_CS_PIN);

  siapkanMac();
  Serial.println("MAC perangkat: " + macString());

  if (Ethernet.begin(mac) == 0)
  {
    IPAddress ip(192, 168, 1, 177);
    Ethernet.begin(mac, ip);
  }

  delay(1500);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ETH Terhubung!");
  lcd.setCursor(0, 1);
  lcd.print(Ethernet.localIP().toString());
  delay(2000);

  linkUp = (Ethernet.linkStatus() != LinkOFF);
  lanOK = adaIP();

  httpSSL.setTimeout(SSL_TIMEOUT_MS); // sebelumnya 30 dtk (bawaan SSLClient)

  client.setServer(mqttServer, mqttPort);
  client.setCallback(mqttCallback);
  client.setBufferSize(640);  // default 256 byte tidak cukup untuk JSON diag
  client.setKeepAlive(60);    // sebelumnya 15: toleran terhadap loop() yang tertahan beberapa detik
  client.setSocketTimeout(3); // koneksi gagal tidak menahan UI lama
  mqttTryConnect();
  lastMqttAttempt = millis();
  aktifkanWatchdog();

  siapkanAlamatServer(); // gunakan alamat server dari NVS atau config.h
  sinkronWaktu();        // jam harus benar sebelum HTTPS pertama
  ntpCobaMs = millis();
  configServer.begin();
  Serial.println("[CFG] Halaman konfigurasi: http://" + Ethernet.localIP().toString() + ":8080");
  Serial.println("[CFG] Ketik 'help' untuk daftar perintah diagnostik");
  laporHasilBoot();
  lastStateCLK = digitalRead(PIN_CLK);
  fetchDataFromAPI(false);
  lastPollMs = millis();
}

void loop()
{
  esp_task_wdt_reset();
  ukurLoop();
  cekPerintahSerial();
  prosesWebKonfigurasi();
  if (!client.connected())
  {
    reconnectMQTT();
  }
  else
  {
    client.loop();
  }
  pantauJaringan();
  if (!hwTestAktif)
    cekOtaBerkala();
  jagaWaktu();
  tickDiagnostik();

  if (otaDiminta)
  {
    otaDiminta = false;
    if (statusIdle())
    {
      otaManual = true;
      laporProgresOta("checking", -1, "Memeriksa update");
      cekUpdateFirmware();
      otaManual = false;
    }
    else
    {
      laporProgresOta("failed", -1, "Mesin sedang bekerja, coba saat idle");
    }
  }

  // Buang putaran encoder yang tidak boleh "bocor" ke layar lain:
  // saat tes hardware, saat pindah state, dan di state yang tidak memakai encoder.
  static State stateSebelumnya = currentState;
  if (hwTestAktif || currentState != stateSebelumnya ||
      currentState == STATE_SANDING_RUNNING || currentState == STATE_KIRIM_DATA)
  {
    ambilLangkahEncoder();
    stateSebelumnya = currentState;
  }

  // Mode tes hardware: aksi tombol tidak diteruskan ke aplikasi
  if (hwTestAktif)
  {
    hwTestLoop();
    return;
  }

  switch (currentState)
  {
  case STATE_FETCH_API:
    handleFetchApi();
    break;
  case STATE_PILIH_DATA:
    handlePilihDataMenu();
    break;
  case STATE_PILIH_SISI:
    handlePilihSisiMenu();
    break;
  case STATE_SANDING_RUNNING:
    handleSandingRunning();
    break;
  case STATE_KOREKSI_MANUAL:
    handleKoreksiManual();
    break;
  case STATE_VALIDASI_SELISIH:
    handleValidasiMenu();
    break;
  case STATE_KIRIM_DATA:
    kirimDataKeServer();
    break;
  case STATE_KONFIRMASI_LAGI:
    handleKonfirmasiLagi();
    break;
  }
}

// =====================================================
//  FUNGSI HTTP/HTTPS UTAMA (dipakai GET & semua POST)
//  Mengembalikan status code HTTP asli + body balasan.
//  Kode negatif = error buatan kita (-1, -100, -101).
// =====================================================

void tutupKoneksiHTTP()
{
  if (serverTls)
  {
    httpSSL.stop();
    httpBase.stop();
  }
  else
  {
    httpPlain.stop();
  }
}

bool bukaKoneksi()
{
  // Bersihkan koneksi lama sebelum membuka koneksi baru
  tutupKoneksiHTTP();

  if (serverTls)
  {
    // HTTPS membutuhkan waktu yang valid untuk verifikasi sertifikat
    if (ntpEpoch == 0)
    {
      Serial.println("[HTTP] GAGAL: waktu NTP belum tersedia");
      return false;
    }

    setWaktuTls();

    unsigned long mulai = millis();
    bool berhasil = httpSSL.connect(serverHost.c_str(), serverPortNum);

    Serial.printf("[HTTP] HTTPS connect: %lu ms, hasil=%s\n",
                  millis() - mulai, berhasil ? "OK" : "GAGAL");

    if (!berhasil)
      tutupKoneksiHTTP();

    return berhasil;
  }

  unsigned long mulai = millis();
  bool berhasil = httpPlain.connect(serverAddr, serverPortNum);

  Serial.printf("[HTTP] HTTP connect: %lu ms, hasil=%s\n",
                millis() - mulai, berhasil ? "OK" : "GAGAL");

  if (!berhasil)
    tutupKoneksiHTTP();

  return berhasil;
}

// Membaca satu baris. available() hanya dipanggil selagi koneksi masih terbuka
// sehingga pesan "Cannot operate on a closed SSL connection" tidak muncul lagi.
bool bacaBarisHTTP(Client &c, String &baris, unsigned long deadline)
{
  baris = "";

  while ((long)(deadline - millis()) > 0)
  {
    esp_task_wdt_reset();

    if (client.connected())
      client.loop();

    if (!c.connected())
      return false; // sesi sudah tertutup dan buffer kosong

    while (c.available())
    {
      char ch = (char)c.read();

      if (ch == '\n')
        return true;

      if (ch != '\r')
      {
        if (baris.length() >= 512)
          return false;

        baris += ch;
      }
    }

    delay(1);
  }

  return false;
}

HttpHasil httpRequestMentah(const char *method, const String &path, const String &body,
                            const char *namaHeader, const String &nilaiHeader)
{
  HttpHasil hasil = {0, ""};

  const unsigned long HTTP_TIMEOUT_MS = 10000;
  const unsigned long HTTP_BODY_MAX = 8192;

  if (Ethernet.linkStatus() == LinkOFF)
  {
    hasil.status = ERR_ETH_PUTUS;
    return hasil;
  }

  if (serverTls && ntpEpoch == 0)
  {
    Serial.println("[HTTP] HTTPS dibatalkan: waktu NTP belum tersedia");
    hasil.status = -1;
    return hasil;
  }

  if (!bukaKoneksi())
  {
    Serial.println("[HTTP] Koneksi gagal");
    hasil.status = -1;
    return hasil;
  }

  // Deadline dihitung SETELAH connect, supaya waktu connect tidak menghabiskan jatah baca.
  unsigned long deadline = millis() + HTTP_TIMEOUT_MS;

  Client &c = serverTls ? (Client &)httpSSL : (Client &)httpPlain;

  c.println(String(method) + " " + path + " HTTP/1.0");
  c.println("Host: " + hostHeader());
  c.println("Accept: application/json");

  if (namaHeader != nullptr)
  {
    if (nilaiHeader.length() > 0)
      c.println(String(namaHeader) + ": " + nilaiHeader);
  }
  else if (apiKeyRuntime.length() > 0)
  {
    c.println("X-API-KEY: " + apiKeyRuntime);
  }

  c.println("Connection: close");

  if (body.length() > 0)
  {
    c.println("Content-Type: application/json");
    c.println("Content-Length: " + String(body.length()));
  }

  c.println();

  if (body.length() > 0)
    c.print(body);

  // 1. Baca status HTTP.
  String statusLine;

  if (!bacaBarisHTTP(c, statusLine, deadline))
  {
    Serial.println("[HTTP] timeout membaca status");
    c.stop();
    hasil.status = -1;
    return hasil;
  }

  int spasi = statusLine.indexOf(' ');

  if (spasi < 0)
  {
    c.stop();
    hasil.status = ERR_DATA_TIDAK_VALID;
    return hasil;
  }

  hasil.status = statusLine.substring(spasi + 1, spasi + 4).toInt();

  if (hasil.status == 0)
  {
    c.stop();
    hasil.status = ERR_DATA_TIDAK_VALID;
    return hasil;
  }

  // 2. Baca seluruh header sampai baris kosong, catat Content-Length.
  bool headerSelesai = false;
  long panjangBody = -1;

  while ((long)(deadline - millis()) > 0)
  {
    String header;

    if (!bacaBarisHTTP(c, header, deadline))
      break;

    if (header.length() == 0)
    {
      headerSelesai = true;
      break;
    }

    header.toLowerCase();
    if (header.startsWith("content-length:"))
      panjangBody = header.substring(15).toInt();
  }

  if (!headerSelesai)
  {
    Serial.println("[HTTP] timeout membaca header");
    c.stop();
    hasil.status = -1;
    return hasil;
  }

  // 3. Baca body. Berhenti begitu body LENGKAP, tanpa menunggu server menutup sesi TLS.
  //    available() pada SSLClient memblokir sampai timeout bila dipanggil setelah data
  //    habis dan server belum menutup sesi, jadi tidak boleh dipanggil lagi setelah lengkap.
  //    Lengkap = panjang sama dengan Content-Length; tanpa Content-Length = kurung JSON tertutup.
  int kedalaman = 0;
  bool dalamString = false, escape = false, mulaiJson = false;

  while ((long)(deadline - millis()) > 0)
  {
    esp_task_wdt_reset();

    if (client.connected())
      client.loop();

    while ((panjangBody < 0 || (long)hasil.body.length() < panjangBody) && c.available())
    {
      if (hasil.body.length() >= HTTP_BODY_MAX)
      {
        Serial.println("[HTTP] body terlalu besar");
        c.stop();
        hasil.status = ERR_DATA_TIDAK_VALID;
        hasil.body = "";
        return hasil;
      }

      char ch = (char)c.read();
      hasil.body += ch;

      if (panjangBody < 0) // tanpa Content-Length: deteksi akhir JSON
      {
        if (dalamString)
        {
          if (escape)
            escape = false;
          else if (ch == '\\')
            escape = true;
          else if (ch == '"')
            dalamString = false;
        }
        else if (ch == '"')
          dalamString = true;
        else if (ch == '{' || ch == '[')
        {
          kedalaman++;
          mulaiJson = true;
        }
        else if (ch == '}' || ch == ']')
          kedalaman--;

        if (mulaiJson && kedalaman <= 0)
        {
          c.stop();
          return hasil;
        }
      }
    }

    if (panjangBody >= 0 && (long)hasil.body.length() >= panjangBody)
    {
      c.stop();
      return hasil;
    }

    if (!c.connected())
    {
      c.stop();
      return hasil; // server menutup sesi = body sudah lengkap
    }

    delay(1);
  }

  // Deadline tercapai sebelum respons selesai.
  Serial.println("[HTTP] timeout membaca body");
  c.stop();
  hasil.status = -1;
  hasil.body = "";

  return hasil;
}

// =====================================================
//  HTTP GET: AMBIL DAFTAR DATA
// =====================================================
bool fetchDataFromAPI(bool silent)
{
  if (!silent)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MENGAMBIL DATA API");
  }

  HttpHasil r = httpRequest("GET", "/api/sanding?tipe=" + String(sandingType), "");
  lastFetchCode = r.status; // kode HTTP asli tercatat (401, 500, dst.)

  if (r.status != 200)
  {
    Serial.println("[DEBUG] GET gagal, kode: " + String(r.status));
    return false;
  }

  String payload = r.body;
  Serial.println("[DEBUG] Payload: " + payload);

  uint32_t h = hashString(payload);
  if (h == lastPayloadHash && totalDataMaster > 0)
  {
    return false; // data sama persis dengan sebelumnya, tidak ada yang berubah
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error || !doc["data"].is<JsonArray>())
  {
    Serial.println("[DEBUG] Gagal parsing JSON");
    lastFetchCode = ERR_DATA_TIDAK_VALID;
    return false;
  }

  int idPilihan = (totalDataMaster > 0) ? apiIds[indexData] : -1;
  int indexLama = indexData;

  JsonArray items = doc["data"];
  totalDataMaster = 0;

  for (JsonObject item : items)
  {
    if (totalDataMaster < MAX_DATA)
    {
      apiIds[totalDataMaster] = item["id"];
      apiIdProduksiSanding[totalDataMaster] = item["id_produksi_sanding"].as<int>();
      String rawDate = item["tanggal"].as<String>();
      apiTanggal = (rawDate.length() >= 10) ? rawDate.substring(0, 10) : rawDate;
      apiMesin = item["mesin"].as<String>();
      apiShift = item["shift"].as<String>();
      apiNoPalet[totalDataMaster] = item["no_palet"].as<int>();
      masterBaris1[totalDataMaster] = item["nama_barang"].as<String>();
      masterModal[totalDataMaster] = item["target"].as<int>();
      totalDataMaster++;
    }
  }

  // Pertahankan item yang sedang dipilih operator (kalau masih ada di daftar)
  indexData = 0;
  if (idPilihan != -1 && totalDataMaster > 0)
  {
    bool ketemu = false;
    for (int i = 0; i < totalDataMaster; i++)
    {
      if (apiIds[i] == idPilihan)
      {
        indexData = i;
        ketemu = true;
        break;
      }
    }
    if (!ketemu)
      indexData = (indexLama < totalDataMaster) ? indexLama : totalDataMaster - 1;
  }

  lastPayloadHash = h;
  return true;
}

void handleFetchApi()
{
  if (totalDataMaster > 0)
  {
    currentState = STATE_PILIH_DATA;
    lastModal = -1;
    lastPollMs = millis();
    lastInteractionMs = millis();
    updateDisplayPilihData();
    return;
  }

  if (!layarKosongTergambar || lastKosongCode != lastFetchCode)
  {
    updateDisplayKosong();
    layarKosongTergambar = true;
    lastKosongCode = lastFetchCode;
  }

  if (refreshRequested || (millis() - lastPollMs >= POLL_KOSONG_MS))
  {
    refreshRequested = false;
    lastPollMs = millis();
    fetchDataFromAPI(true);
  }
}

void updateDisplayKosong()
{
  lcd.clear();
  if (lastFetchCode == 200)
  {
    lcd.setCursor(0, 0);
    lcd.print("Belum ada modal");
    lcd.setCursor(0, 1);
    lcd.print("Menunggu input...");
  }
  else
  {
    lcd.setCursor(0, 0);
    lcd.print("GAGAL AMBIL DATA");
    tampilkanError(lastFetchCode, 1);
  }
}

void cekAutoUpdateDaftar()
{
  unsigned long now = millis();
  if (now - lastInteractionMs < IDLE_SEBELUM_POLL_MS)
    return;

  bool waktunya = (now - lastPollMs) >= POLL_PILIH_MS;
  if (!refreshRequested && !waktunya)
    return;

  refreshRequested = false;
  lastPollMs = now;

  int kodeLama = lastFetchCode;
  bool berubah = fetchDataFromAPI(true);

  if (totalDataMaster == 0)
  {
    currentState = STATE_FETCH_API;
    layarKosongTergambar = false;
    return;
  }

  if (berubah || kodeLama != lastFetchCode)
    updateDisplayPilihData();
}

// =====================================================
//  MENU: PILIH DATA
// =====================================================
void handlePilihDataMenu()
{
  cekAutoUpdateDaftar();
  if (currentState != STATE_PILIH_DATA)
    return;

  int d = ambilLangkahEncoder();
  if (d != 0)
  {
    lastInteractionMs = millis();
    indexData = ((indexData + d) % totalDataMaster + totalDataMaster) % totalDataMaster;
    updateDisplayPilihData();
  }

  if (digitalRead(PIN_SW) == LOW)
  {
    delay(300);
    putaranFace = 0;
    putaranBack = 0;
    resetHitungan();
    currentState = STATE_PILIH_SISI;
    lastModal = -1;
    updateDisplayPilihSisi();
  }
}

void updateDisplayPilihData()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(masterBaris1[indexData]);
  lcd.setCursor(0, 1);
  lcd.print(apiTanggal + " " + apiShift);
  lcd.setCursor(0, 2);
  lcd.print("modal : " + String(masterModal[indexData]));
  lcd.setCursor(0, 3);
  lcd.print(String(indexData + 1) + "/" + String(totalDataMaster));
  lcd.setCursor(17, 3);
  lcd.print("OK>");
}

// =====================================================
//  MENU: PILIH SISI
// =====================================================
void handlePilihSisiMenu()
{
  if (ambilLangkahEncoder() != 0)
  {
    selectedSisi = (selectedSisi == "FACE") ? "BACK" : "FACE";
    updateDisplayPilihSisi();
  }

  if (digitalRead(PIN_BTN_PIN4) == LOW)
  {
    delay(300);
    currentState = STATE_PILIH_DATA;
    lastModal = -1;
    lastInteractionMs = millis();
    refreshRequested = true;
    updateDisplayPilihData();
    return;
  }

  if (digitalRead(PIN_SW) == LOW)
  {
    delay(300);
    if (putaranSisiTerpilih() >= MAX_PUTARAN_PER_SISI)
    {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Sisi " + selectedSisi + " penuh");
      delay(1500);
      updateDisplayPilihSisi();
      return;
    }
    // Perintah MULAI: sensor nol-kan angka lalu mulai menghitung.
    // Kalau sensor tidak membalas, jangan masuk ke layar hitung.
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Memulai sensor...");
    if (!mulaiHitungSensor())
    {
      lcd.setCursor(0, 1);
      lcd.print("GAGAL! Cek sensor");
      lcd.setCursor(0, 2);
      lcd.print("& koneksi MQTT");
      delay(2000);
      updateDisplayPilihSisi();
      return;
    }
    resetHitungan();
    backTahanMulai = 0;
    currentState = STATE_SANDING_RUNNING;
    lastModal = -1;
  }
}

void updateDisplayPilihSisi()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Pilih Sisi (STANDBY)"); // sensor belum menghitung
  lcd.setCursor(0, 1);
  lcd.print(masterBaris1[indexData]);
  lcd.setCursor(0, 2);
  lcd.print("Sisi: " + selectedSisi + " (" + String(putaranSisiTerpilih()) + "x)");
  lcd.setCursor(0, 3);
  lcd.print("<back");
  lcd.setCursor(12, 3);
  lcd.print("OK=MULAI");
}

// =====================================================
//  STATE: SANDING RUNNING
// =====================================================
void handleSandingRunning()
{
  jumlahMasuk = sensorMasuk;
  jumlahKeluar = sensorKeluar;

  JenisHasil jenis = klasifikasiHasil();
  // Status untuk tampilan: 0 = susut (tanpa label), 1 = anomali, 2 = tepat,
  // 3 = sensor tidak sedang menghitung (mis. sensor restart) -> data tidak valid
  int statusTampil = 0;
  if (!client.connected() || sensorMode != "ON")
    statusTampil = 3;
  else if (jenis == HASIL_ANOMALI)
    statusTampil = 1;
  else if (jenis == HASIL_SESUAI)
    statusTampil = 2;

  if (masterModal[indexData] != lastModal || jumlahMasuk != lastMasuk || jumlahKeluar != lastKeluar || selectedSisi != lastSisi || statusTampil != lastStatus || lastModal == -1)
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(masterBaris1[indexData]);
    lcd.setCursor(0, 1);
    lcd.print("M : " + String(masterModal[indexData]));
    lcd.setCursor(0, 2);
    lcd.print("IN:" + String(jumlahMasuk) + " OUT:" + String(jumlahKeluar));
    // BACK terkunci di layar ini, jadi tidak ada label "<back"
    if (statusTampil == 1)
    {
      lcd.setCursor(0, 3);
      lcd.print("!ANOMALI");
    }
    if (statusTampil == 2)
    {
      lcd.setCursor(0, 3);
      lcd.print("TEPAT");
    }
    if (statusTampil == 3)
    {
      lcd.setCursor(0, 3);
      lcd.print("SENSOR OFF!");
    }
    else
    {
      lcd.setCursor(17, 3);
      lcd.print("OK>");
    }

    lastModal = masterModal[indexData];
    lastMasuk = jumlahMasuk;
    lastKeluar = jumlahKeluar;
    lastSisi = selectedSisi;
    lastStatus = statusTampil;
  }

  // ---- BACK DIKUNCI di layar ini ----
  // Pembatalan darurat hanya lewat tahan lama (TAHAN_BATAL_MS).
  if (IZINKAN_BATAL_TAHAN)
  {
    if (digitalRead(PIN_BTN_PIN4) == LOW)
    {
      if (backTahanMulai == 0)
        backTahanMulai = millis();
      if (millis() - backTahanMulai >= TAHAN_BATAL_MS)
      {
        batalkanPutaran();
        return;
      }
    }
    else
    {
      backTahanMulai = 0;
    }
  }

  if (digitalRead(PIN_SW) == LOW)
  {
    delay(300);
    if (statusTampil == 3)
      return; // sensor tidak menghitung: hasil tidak valid, jangan diproses
    switch (jenis)
    {
    case HASIL_SESUAI:
      currentState = STATE_KIRIM_DATA;
      break;
    case HASIL_SUSUT:
      currentState = STATE_VALIDASI_SELISIH;
      lastModal = -1;
      updateDisplayValidasi();
      break;
    case HASIL_ANOMALI:
      koreksiField = 0;
      currentState = STATE_KOREKSI_MANUAL;
      updateDisplayKoreksi();
      break;
    }
  }
}

// =====================================================
//  STATE: KOREKSI MANUAL
// =====================================================
void handleKoreksiManual()
{
  int d = ambilLangkahEncoder();
  if (d != 0)
  {
    if (koreksiField == 0)
    {
      jumlahMasuk += d;
      if (jumlahMasuk < 0)
        jumlahMasuk = 0;
    }
    else
    {
      jumlahKeluar += d;
      if (jumlahKeluar < 0)
        jumlahKeluar = 0;
    }
    updateDisplayKoreksi();
  }

  if (digitalRead(PIN_BTN_PIN4) == LOW)
  {
    delay(300);
    if (koreksiField == 1)
    {
      koreksiField = 0;
      updateDisplayKoreksi();
    }
    else
    {
      kembaliKeMonitoring();
    }
    return;
  }

  if (digitalRead(PIN_SW) == LOW)
  {
    delay(300);
    if (koreksiField == 0)
    {
      koreksiField = 1;
      updateDisplayKoreksi();
    }
    else
    {
      if (jumlahKeluar > jumlahMasuk)
      {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("OUT > IN!");
        delay(1500);
        updateDisplayKoreksi();
        return;
      }
      adaKoreksiManual = true;
      alasanIndex = ALASAN_S_ERROR;
      currentState = STATE_VALIDASI_SELISIH;
      updateDisplayValidasi();
    }
  }
}

void updateDisplayKoreksi()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CEK FISIK! M:" + String(masterModal[indexData]));
  lcd.setCursor(0, 1);
  lcd.print(String(koreksiField == 0 ? ">" : " ") + "IN : " + String(jumlahMasuk));
  lcd.setCursor(0, 2);
  lcd.print(String(koreksiField == 1 ? ">" : " ") + "OUT: " + String(jumlahKeluar));
  lcd.setCursor(0, 3);
  lcd.print("<back");
  lcd.setCursor(15, 3);
  lcd.print(koreksiField == 0 ? "NEXT>" : "  OK>");
}

// =====================================================
//  STATE: VALIDASI SELISIH
// =====================================================
void handleValidasiMenu()
{
  int d = ambilLangkahEncoder();
  if (d != 0)
  {
    alasanIndex = ((alasanIndex + d) % totalAlasan + totalAlasan) % totalAlasan;
    updateDisplayValidasi();
  }

  if (digitalRead(PIN_BTN_PIN4) == LOW)
  {
    delay(300);
    if (adaKoreksiManual)
    {
      koreksiField = 1;
      currentState = STATE_KOREKSI_MANUAL;
      updateDisplayKoreksi();
    }
    else
    {
      kembaliKeMonitoring();
    }
    return;
  }

  if (digitalRead(PIN_SW) == LOW)
  {
    delay(300);
    currentState = STATE_KIRIM_DATA;
  }
}

void updateDisplayValidasi()
{
  int sel = masterModal[indexData] - jumlahKeluar;
  String info = (sel > 0) ? ("Kurang " + String(sel)) : ((sel < 0) ? ("Lebih " + String(-sel)) : "Pas");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("M:" + String(masterModal[indexData]) + " " + info);
  lcd.setCursor(0, 1);
  lcd.print("IN:" + String(jumlahMasuk) + " OUT:" + String(jumlahKeluar));
  lcd.setCursor(0, 2);
  lcd.print("Alasan:" + opsiAlasan[alasanIndex]);
  lcd.setCursor(0, 3);
  lcd.print("<Back");
  lcd.setCursor(15, 3);
  lcd.print("KIRIM>");
}

// =====================================================
//  HTTP POST: KIRIM DATA PUTARAN
//  Sukses HANYA jika server membalas 2xx.
// =====================================================
void kirimDataKeServer()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MENGIRIM DATA...");

  bool sisiFace = (selectedSisi == "FACE");
  bool pakaiNormal = (klasifikasiHasil() == HASIL_SESUAI && !adaKoreksiManual);

  int putaranKe = (sisiFace ? putaranFace : putaranBack) + 1;
  if (putaranKe > MAX_PUTARAN_PER_SISI)
    putaranKe = MAX_PUTARAN_PER_SISI;

  JsonDocument doc;
  doc["sisi"] = sisiFace ? "face" : "back";
  doc["putaran_ke"] = putaranKe;
  doc["modal"] = masterModal[indexData];
  doc["masuk"] = jumlahMasuk;
  doc["keluar"] = jumlahKeluar;
  doc["masuk_sensor"] = sensorMasuk;
  doc["keluar_sensor"] = sensorKeluar;
  doc["machine_id"] = apiMesin;
  doc["alasan"] = pakaiNormal ? "normal" : opsiAlasan[alasanIndex];
  doc["id_produksi_sanding"] = apiIdProduksiSanding[indexData];

  String requestBody;
  serializeJson(doc, requestBody);

  String path = "/api/sanding/" + String(apiIds[indexData]) + "/putaran";
  HttpHasil r = httpRequest("POST", path, requestBody);

  // Berhasil HANYA jika server menjawab 2xx (200, 201, ...)
  bool berhasil = (r.status >= 200 && r.status < 300);
  int kodeError = berhasil ? 0 : r.status;

  if (berhasil)
  {
    if (sisiFace)
      putaranFace++;
    else
      putaranBack++;
    lcd.setCursor(0, 1);
    lcd.print("Kirim Berhasil! ");

    // Data sudah aman di server -> sensor berhenti hitung & kembali ke 0.
    lcd.setCursor(0, 2);
    lcd.print("Stop sensor...");
    bool stopOk = hentikanHitungSensor();
    lcd.setCursor(0, 2);
    lcd.print(stopOk ? "Sensor reset OK   " : "SENSOR TDK MERESPON");
    delay(1500);

    lastModal = -1;
    currentState = STATE_KONFIRMASI_LAGI;

    if (putaranFace >= MAX_PUTARAN_PER_SISI && putaranBack >= MAX_PUTARAN_PER_SISI)
    {
      sandingLagi = false;
      selesaikanSesi();
    }
    else
    {
      sandingLagi = true;
      updateDisplayKonfirmasi();
    }
  }
  else
  {
    logError("POST", kodeError);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("GAGAL KIRIM!");
    tampilkanError(kodeError, 1);
    delay(3500);

    autoKirimDiblokir = true;
    lastModal = -1;
    if (pakaiNormal)
      currentState = STATE_SANDING_RUNNING;
    else
    {
      currentState = STATE_VALIDASI_SELISIH;
      updateDisplayValidasi();
    }
  }
}

// =====================================================
//  STATE: KONFIRMASI SANDING LAGI
// =====================================================
void handleKonfirmasiLagi()
{
  bool semuaPenuh = (putaranFace >= MAX_PUTARAN_PER_SISI && putaranBack >= MAX_PUTARAN_PER_SISI);

  if (ambilLangkahEncoder() != 0 && !semuaPenuh)
  {
    sandingLagi = !sandingLagi;
    updateDisplayKonfirmasi();
  }

  if (digitalRead(PIN_SW) == LOW)
  {
    delay(300);
    if (sandingLagi && !semuaPenuh)
    {
      resetHitungan();
      lastModal = -1;
      currentState = STATE_PILIH_SISI;
      updateDisplayPilihSisi();
    }
    else
    {
      selesaikanSesi();
    }
  }
}

void updateDisplayKonfirmasi()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sanding lagi?");
  lcd.setCursor(0, 1);
  lcd.print(masterBaris1[indexData]);
  lcd.setCursor(0, 2);
  lcd.print(sandingLagi ? ">YA    TIDAK" : " YA    >TIDAK");
  lcd.setCursor(0, 3);
  lcd.print("M: " + String(masterModal[indexData]));
  lcd.setCursor(9, 3);
  lcd.print("F:" + String(putaranFace) + " B:" + String(putaranBack));
  lcd.setCursor(17, 3);
  lcd.print("OK>");
}

// =====================================================
//  HTTP POST: TUTUP PALET
//  Return 0 = sukses, selain itu = kode error.
// =====================================================
int tutupPaletDiServer()
{
  String path = "/api/sanding/" + String(apiIds[indexData]) + "/selesai";
  HttpHasil r = httpRequest("POST", path, "{}");

  if (r.status >= 200 && r.status < 300)
    return 0;
  return r.status;
}

void selesaikanSesi()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("MENYELESAIKAN...");

  int kode = tutupPaletDiServer();
  if (kode != 0)
  {
    logError("SELESAI", kode);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("GAGAL SELESAIKAN!");
    tampilkanError(kode, 1);
    delay(3500);

    lastModal = -1;
    currentState = STATE_KONFIRMASI_LAGI;
    updateDisplayKonfirmasi();
    return;
  }

  putaranFace = 0;
  putaranBack = 0;
  resetHitungan();
  lastModal = -1;

  totalDataMaster = 0;
  layarKosongTergambar = false;
  lastKosongCode = -999;
  fetchDataFromAPI(false);
  lastPollMs = millis();
  currentState = STATE_FETCH_API;
}