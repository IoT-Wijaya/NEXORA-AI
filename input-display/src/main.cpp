#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include "secrets.h"
#include "config.h"

const char *topicMasuk = "pabrik/sanding/jumlah";
const char *topicKeluar = "pabrik/sanding/jumlah2";
const char *topicRefresh = "pabrik/sanding/refresh";
const char *topicPerintah = "pabrik/sanding/perintah";
const char *topicAck = "pabrik/sanding/ack";
const char *topicMode = "pabrik/sanding/mode";

// =====================================================
//  KONFIGURASI ETHERNET W5500 (SPI: SCK=18, MISO=23, MOSI=19, CS=16)
// =====================================================
byte mac[6];

const int ETHERNET_CS_PIN = 16;

// =====================================================
//  PENGATURAN AUTO UPDATE
// =====================================================
const unsigned long POLL_PILIH_MS = 15000;
const unsigned long POLL_KOSONG_MS = 8000;
const unsigned long IDLE_SEBELUM_POLL_MS = 2000;

const int MAX_PUTARAN_PER_SISI = 2;

const int ERR_ETH_PUTUS = -100;
const int ERR_DATA_TIDAK_VALID = -101;

EthernetClient ethClient;
PubSubClient client(ethClient);

// =====================================================
//  LCD I2C (20x4)
// =====================================================
LiquidCrystal_I2C lcd(0x27, 20, 4);

// =====================================================
//  DEFINISI PIN HARDWARE (ENCODER DIPINDAH KE 32 & 35)
// =====================================================
const int PIN_CLK = 32;     // Dipindah ke 32 (aman dari SPI 18)
const int PIN_DT = 35;      // Dipindah ke 35 (aman dari SPI 19)
const int PIN_SW = 5;       // Rotary Encoder Button (OK)
const int PIN_BTN_PIN4 = 4; // Tombol fisik tunggal (BACK)

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
ErrInfo infoError(int code);

// Hasil dari satu request HTTP:
// - status: kode HTTP (200, 401, ...) ATAU kode error negatif buatan kita
// - body  : isi balasan server (JSON)
struct HttpHasil
{
  int status;
  String body;
};

// Alamat server dipecah SEKALI di setup, tidak diulang-ulang
IPAddress serverAddr;
int serverPortNum = 80;

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
int lastStateCLK;

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
// Set IZINKAN_BATAL_TAHAN = false untuk mengunci BACK sepenuhnya.
const bool IZINKAN_BATAL_TAHAN = true;
const unsigned long TAHAN_BATAL_MS = 3000; // tahan BACK sekian ms untuk batal
unsigned long backTahanMulai = 0;

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
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
}

bool mqttTryConnect()
{
  String clientId = "ESP32Sanding-" + String(random(0xffff), HEX);
  if (client.connect(clientId.c_str()))
  {
    client.subscribe(topicMasuk);
    client.subscribe(topicKeluar);
    client.subscribe(topicRefresh);
    client.subscribe(topicAck);
    client.subscribe(topicMode);
    return true;
  }
  return false;
}

void reconnectMQTT()
{
  if (millis() - lastMqttAttempt < 5000)
    return;
  lastMqttAttempt = millis();
  mqttTryConnect();
}

// =====================================================
//  PERINTAH KE SENSOR (dengan konfirmasi + ulang 3x)
//  Return true jika sensor membalas persis ackDiharapkan.
//  Pesan MQTT bisa hilang di jalan, jadi kita tidak percaya
//  begitu saja: tunggu balasan, kalau tidak datang kirim ulang.
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
    sensorMode = "ON";
  return ok;
}

// STOP: sensor berhenti menghitung lalu nol-kan angka
bool hentikanHitungSensor()
{
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
    return {"Tdk bisa konek", "Cek IP/server/FW", "Koneksi ditolak."};
  case ERR_ETH_PUTUS:
    return {"Kabel LAN lepas", "Cek koneksi W5500", "Ethernet terputus."};
  case ERR_DATA_TIDAK_VALID:
    return {"Data aneh", "Balasan bukan JSON", "Format salah."};
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
  // Sumber kebenarannya sensor; reset dilakukan lewat resetSensorRemote().
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

  // Bit 1 di byte pertama = "locally administered", dipakai untuk MAC buatan
  // pada antarmuka W5500, supaya tidak bentrok dengan MAC WiFi chip yang sama.
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
//  [TAHAP 1] PEMECAH ALAMAT SERVER
//  "192.168.1.23:8000" -> serverAddr = 192.168.1.23, serverPortNum = 8000
// =====================================================
void siapkanAlamatServer()
{
  String s = String(serverIp);
  int titikDua = s.indexOf(':');
  String ipOnly = (titikDua >= 0) ? s.substring(0, titikDua) : s;
  serverPortNum = (titikDua >= 0) ? s.substring(titikDua + 1).toInt() : 80;
  serverAddr.fromString(ipOnly);
}

// =====================================================
//  SETUP & LOOP
// =====================================================
void setup()
{
  Serial.begin(115200);
  Serial.println("\n--- HMI SANDING IOT (ETHERNET W5500 + NEW PINS) ---");

  pinMode(PIN_CLK, INPUT_PULLUP);
  pinMode(PIN_DT, INPUT_PULLUP);
  pinMode(PIN_SW, INPUT_PULLUP);
  pinMode(PIN_BTN_PIN4, INPUT_PULLUP);

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

  client.setServer(mqttServer, mqttPort);
  client.setCallback(mqttCallback);
  mqttTryConnect();
  lastMqttAttempt = millis();

  siapkanAlamatServer(); // [TAHAP 1] pecah IP:port sekali saja
  lastStateCLK = digitalRead(PIN_CLK);
  fetchDataFromAPI(false);
  lastPollMs = millis();
}

void loop()
{
  if (!client.connected())
  {
    reconnectMQTT();
  }
  else
  {
    client.loop();
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
//  [TAHAP 1] FUNGSI HTTP UTAMA (dipakai GET & semua POST)
//  Mengembalikan status code HTTP asli + body balasan.
//  Kode negatif = error buatan kita (-1, -100, -101).
// =====================================================
HttpHasil httpRequest(const char *method, const String &path, const String &body)
{
  HttpHasil hasil = {0, ""};

  // 1. Cek kabel LAN
  if (Ethernet.linkStatus() == LinkOFF)
  {
    hasil.status = ERR_ETH_PUTUS;
    return hasil;
  }

  // 2. Konek ke server
  if (!ethClient.connect(serverAddr, serverPortNum))
  {
    hasil.status = -1;
    return hasil;
  }

  // 3. Kirim request. println() = akhiri baris dengan \r\n (aturan HTTP)
  ethClient.println(String(method) + " " + path + " HTTP/1.0");
  ethClient.println("Host: " + String(serverIp));
  ethClient.println("Accept: application/json");
  // ethClient.println("X-API-KEY: " + String(apiKey));
  ethClient.println("Connection: close");
  if (body.length() > 0)
  {
    ethClient.println("Content-Type: application/json");
    ethClient.println("Content-Length: " + String(body.length()));
  }
  ethClient.println(); // baris kosong = akhir header

  // Body pakai print(), BUKAN println(), supaya panjangnya pas
  if (body.length() > 0)
    ethClient.print(body);

  // 4. Tunggu server mulai menjawab (maks 5 detik)
  unsigned long mulai = millis();
  while (ethClient.connected() && !ethClient.available())
  {
    if (millis() - mulai > 5000)
    {
      ethClient.stop();
      hasil.status = -1;
      return hasil;
    }
    delay(10);
  }

  // 5. Baris pertama balasan, contoh: "HTTP/1.0 200 OK" -> ambil 200
  String statusLine = ethClient.readStringUntil('\n');
  int spasi = statusLine.indexOf(' ');
  if (spasi < 0)
  {
    ethClient.stop();
    hasil.status = ERR_DATA_TIDAK_VALID;
    return hasil;
  }
  hasil.status = statusLine.substring(spasi + 1, spasi + 4).toInt();
  if (hasil.status == 0)
    hasil.status = ERR_DATA_TIDAK_VALID;

  // 6. Lewati header sampai ketemu baris kosong
  while (ethClient.connected() || ethClient.available())
  {
    String line = ethClient.readStringUntil('\n');
    if (line == "\r" || line.length() == 0)
      break;
  }

  // 7. Baca body selama koneksi hidup (batas diam 3 detik)
  unsigned long terakhir = millis();
  while ((ethClient.connected() || ethClient.available()) && millis() - terakhir < 3000)
  {
    while (ethClient.available())
    {
      hasil.body += (char)ethClient.read();
      terakhir = millis();
    }
    delay(1);
  }

  ethClient.stop();
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

  DynamicJsonDocument doc(8192);
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

  int currentStateCLK = digitalRead(PIN_CLK);
  if (currentStateCLK != lastStateCLK && currentStateCLK == LOW)
  {
    lastInteractionMs = millis();
    if (digitalRead(PIN_DT) == HIGH)
    {
      indexData = (indexData + 1) % totalDataMaster;
    }
    else
    {
      indexData = (indexData - 1 + totalDataMaster) % totalDataMaster;
    }
    updateDisplayPilihData();
  }
  lastStateCLK = currentStateCLK;

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
  int currentStateCLK = digitalRead(PIN_CLK);
  if (currentStateCLK != lastStateCLK && currentStateCLK == LOW)
  {
    selectedSisi = (selectedSisi == "FACE") ? "BACK" : "FACE";
    updateDisplayPilihSisi();
  }
  lastStateCLK = currentStateCLK;

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
    // Sebelum titik ini sensor TIDAK menghitung apa pun (noise diabaikan).
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
  if (sensorMode != "ON")
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

  // CATATAN: tidak ada lagi kirim otomatis. Pengawas yang menekan OK.

  // ---- BACK DIKUNCI di layar ini ----
  // Menekan BACK sebentar tidak berbuat apa-apa karena perhitungan dan
  // pencocokan sedang berjalan. Pembatalan darurat hanya lewat tahan lama
  // (TAHAN_BATAL_MS) agar tidak terjadi tidak sengaja.
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
  int currentStateCLK = digitalRead(PIN_CLK);
  if (currentStateCLK != lastStateCLK && currentStateCLK == LOW)
  {
    int delta = (digitalRead(PIN_DT) == HIGH) ? 1 : -1;
    if (koreksiField == 0)
    {
      jumlahMasuk += delta;
      if (jumlahMasuk < 0)
        jumlahMasuk = 0;
    }
    else
    {
      jumlahKeluar += delta;
      if (jumlahKeluar < 0)
        jumlahKeluar = 0;
    }
    updateDisplayKoreksi();
  }
  lastStateCLK = currentStateCLK;

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
  int currentStateCLK = digitalRead(PIN_CLK);
  if (currentStateCLK != lastStateCLK && currentStateCLK == LOW)
  {
    if (digitalRead(PIN_DT) == HIGH)
      alasanIndex = (alasanIndex + 1) % totalAlasan;
    else
      alasanIndex = (alasanIndex - 1 + totalAlasan) % totalAlasan;
    updateDisplayValidasi();
  }
  lastStateCLK = currentStateCLK;

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
//  [TAHAP 1] HTTP POST: KIRIM DATA PUTARAN
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

  DynamicJsonDocument doc(512);
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
    // Kalau gagal, tidak fatal: putaran berikutnya diawali START yang
    // meng-nol-kan angka lagi, dan START wajib terkonfirmasi.
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

  int currentStateCLK = digitalRead(PIN_CLK);
  if (currentStateCLK != lastStateCLK && currentStateCLK == LOW)
  {
    if (!semuaPenuh)
    {
      sandingLagi = !sandingLagi;
      updateDisplayKonfirmasi();
    }
  }
  lastStateCLK = currentStateCLK;

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
//  [TAHAP 1] HTTP POST: TUTUP PALET
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
