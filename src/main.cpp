// YWROBOT
// Compatible with the Arduino IDE 1.0
// Library version:1.1
#include <Wire.h>
#include <WiFiManager.h>
#include <PubSubClient.h> // Library untuk MQTT

#define SENSOR_PIN_1 35 // Sensor pertama (Modal Sanding)
#define SENSOR_PIN_2 32 // Sensor kedua (Hasil Sanding - Dengan filter anti-noise)

// Konfigurasi Broker MQTT Public
const char *mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

// Variabel Counter Sensor 1 & 2
unsigned long objectCount1 = 0;
unsigned long lastPrintedCount1 = 999999;
unsigned long objectCount2 = 0;
unsigned long lastPrintedCount2 = 999999;

// Variabel sensor & debounce Sensor 1
int lastSensorState1 = HIGH;
unsigned long lastDebounceTime1 = 0;
unsigned long debounceDelay1 = 150;

// Variabel sensor & debounce Sensor 2 (Dibuat lebih ketat untuk meredam noise)
int lastSensorState2 = HIGH;
unsigned long lastDebounceTime2 = 0;
unsigned long debounceDelay2 = 300;
bool counted2 = false;

// Variabel Deteksi Error Hardware Sensor dari ESP32
bool currentError1 = false;
bool currentError2 = false;
bool lastReportedError1 = false;
bool lastReportedError2 = false;
unsigned long lastErrorCheck = 0;

// Waktu Timing LCD
const unsigned long INTERVAL_MATI = 30 * 60 * 1000UL;
const unsigned long DURASI_NYALA = 5 * 60 * 1000UL;
unsigned long timerLCD = 0;
bool lcdAktif = true;

// Timing Pengecekan Sinyal & MQTT Reconnect Non-Blocking
unsigned long lastWiFiCheck = 0;
const unsigned long WIFI_CHECK_INTERVAL = 5000;
unsigned long lastReconnectAttempt = 0;
const unsigned long RECONNECT_INTERVAL = 5000;

// Deklarasi fungsi callback agar bisa dibaca PubSubClient
void callback(char *topic, byte *payload, unsigned int length);

// Fungsi Reconnect MQTT Non-Blocking dengan LWT (Last Will & Testament)
void reconnectMQTT()
{
  unsigned long now = millis();
  if (now - lastReconnectAttempt >= RECONNECT_INTERVAL)
  {
    lastReconnectAttempt = now;

    Serial.print("Menghubungkan ke MQTT Broker...");
    String clientId = "ESP32VeneerClient-PabrikWijaya-";
    clientId += String(random(0xffff), HEX);

    // Menggunakan LWT: jika ESP32 mati mendadak/mati lampu, broker otomatis kirim "0" ke topik status
    if (client.connect(clientId.c_str(), "", "", "pabrik/veneer/status", 0, true, "0"))
    {
      Serial.println("TERHUBUNG!");

      // Kirim status awal ONLINE (1)
      client.publish("pabrik/veneer/status", "1", true);

      // Kirim ulang data counter saat ini
      char countString1[10];
      dtostrf(objectCount1, 1, 0, countString1);
      client.publish("pabrik/veneer/jumlah", countString1);

      char countString2[10];
      dtostrf(objectCount2, 1, 0, countString2);
      client.publish("pabrik/veneer/jumlah2", countString2);

      String currentSSID = WiFi.SSID();
      client.publish("pabrik/veneer/ssid", currentSSID.c_str(), true);

      // Subscribe ke topik perintah dari Dashboard Filament
      client.subscribe("pabrik/veneer/perintah");
    }
    else
    {
      Serial.print("Gagal, rc=");
      Serial.print(client.state());
      Serial.println(" Coba lagi dalam 5 detik.");
    }
  }
}

// Fungsi Callback untuk Menerima Pesan Masuk (Perintah Reset, dll)
void callback(char *topic, byte *payload, unsigned int length)
{
  String message = "";
  for (unsigned int i = 0; i < length; i++)
  {
    message += (char)payload[i];
  }

  Serial.print("Pesan masuk pada topik [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  if (String(topic) == "pabrik/veneer/perintah")
  {
    if (message == "RESET_S1")
    {
      objectCount1 = 0;
      Serial.println("SENSOR 1 (MODAL) DI-RESET!");
      char countString1[10];
      dtostrf(objectCount1, 1, 0, countString1);
      client.publish("pabrik/veneer/jumlah", countString1);
    }
    else if (message == "RESET_S2")
    {
      objectCount2 = 0;
      Serial.println("SENSOR 2 (HASIL) DI-RESET!");
      char countString2[10];
      dtostrf(objectCount2, 1, 0, countString2);
      client.publish("pabrik/veneer/jumlah2", countString2);
    }
    else if (message == "RESET_ALL" || message == "RESET")
    {
      objectCount1 = 0;
      objectCount2 = 0;
      Serial.println("SEMUA COUNTER DI-RESET!");

      char countString1[10];
      dtostrf(objectCount1, 1, 0, countString1);
      client.publish("pabrik/veneer/jumlah", countString1);

      char countString2[10];
      dtostrf(objectCount2, 1, 0, countString2);
      client.publish("pabrik/veneer/jumlah2", countString2);
    }
  }
}

// Fungsi Evaluasi Teks Sinyal
String getSignalStatus(int rssi)
{
  if (WiFi.status() != WL_CONNECTED)
    return "Terputus";
  if (rssi >= -65)
    return "Kuat";
  else if (rssi >= -75)
    return "Sedang";
  else
    return "Lemah";
}

// Estimasi Kecepatan Link (Mbps) berdasarkan RSSI untuk Dashboard
int getEstimatedSpeedMbps(int rssi)
{
  if (WiFi.status() != WL_CONNECTED)
    return 0;
  if (rssi >= -55)
    return 72;
  else if (rssi >= -65)
    return 54;
  else if (rssi >= -75)
    return 24;
  else if (rssi >= -85)
    return 6;
  else
    return 1;
}

void setup()
{
  Serial.begin(115200);
  Wire.begin();

  pinMode(SENSOR_PIN_1, INPUT_PULLUP);
  pinMode(SENSOR_PIN_2, INPUT_PULLUP);

  // WiFiManager dengan Timeout
  WiFiManager wm;
  wm.setConnectTimeout(15);
  wm.setConfigPortalTimeout(60);

  bool res = wm.autoConnect("ESP32-Veneer-Counter");

  if (!res)
  {
    Serial.println("Gagal terhubung ke WiFi!");
  }
  else
  {
    Serial.println("Terhubung ke WiFi!");
  }

  // Set Broker MQTT & Buffer Size & Callback
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setBufferSize(512);

  timerLCD = millis();
}

void loop()
{
  unsigned long currentMillis = millis();

  // 1. PASTIKAN KONEKSI MQTT AKTIF
  if (WiFi.status() == WL_CONNECTED)
  {
    if (!client.connected())
    {
      reconnectMQTT();
    }
    else
    {
      client.loop();
    }
  }

  // 2. KONTROL OTOMATIS BACKLIGHT LCD
  if (lcdAktif)
  {
    if (currentMillis - timerLCD >= DURASI_NYALA)
    {
      lcdAktif = false;
      timerLCD = currentMillis;
    }
  }
  else
  {
    if (currentMillis - timerLCD >= INTERVAL_MATI)
    {
      lcdAktif = true;
      timerLCD = currentMillis;
    }
  }

  // 3. PENGECEKAN SINYAL WIFI & PUBLISH KE DASHBOARD
  if (currentMillis - lastWiFiCheck >= WIFI_CHECK_INTERVAL)
  {
    lastWiFiCheck = currentMillis;
    int rssi = WiFi.RSSI();
    String statusSinyal = getSignalStatus(rssi);

    if (client.connected())
    {
      int estSpeed = getEstimatedSpeedMbps(rssi);
      char speedStr[10];
      itoa(estSpeed, speedStr, 10);

      client.publish("pabrik/veneer/kecepatan", speedStr);
    }
  }

  // 4A. PENGHITUNGAN SENSOR PROXIMITY 1 (Pin 35)
  int currentReading1 = digitalRead(SENSOR_PIN_1);
  if (currentReading1 != lastSensorState1)
  {
    if ((currentMillis - lastDebounceTime1) > debounceDelay1)
    {
      if (currentReading1 == LOW)
      {
        objectCount1++;
      }
      lastDebounceTime1 = currentMillis;
      lastSensorState1 = currentReading1;
    }
  }

  // 4B. PENGHITUNGAN SENSOR PROXIMITY 2 (Pin 32) DENGAN FILTER ANTI-NOISE KETAT
  int currentReading2 = digitalRead(SENSOR_PIN_2);
  if (currentReading2 != lastSensorState2)
  {
    lastDebounceTime2 = currentMillis;
    lastSensorState2 = currentReading2;
  }

  if (currentReading2 == LOW)
  {
    if (!counted2 && (currentMillis - lastDebounceTime2) > debounceDelay2)
    {
      objectCount2++;
      counted2 = true;
    }
  }
  else
  {
    counted2 = false;
  }

  // 5. DIAGNOSTIK HARDWARE ERROR DARI SISI ESP32 (Pengecekan tiap 2 detik)
  if (currentMillis - lastErrorCheck >= 2000)
  {
    lastErrorCheck = currentMillis;

    // Set normal (false) secara default
    currentError1 = false;
    currentError2 = false;

    // Kirim status error ke broker MQTT jika terjadi perubahan status
    if (client.connected())
    {
      if (currentError1 != lastReportedError1)
      {
        lastReportedError1 = currentError1;
        client.publish("pabrik/veneer/s1_error", currentError1 ? "1" : "0", true);
      }
      if (currentError2 != lastReportedError2)
      {
        lastReportedError2 = currentError2;
        client.publish("pabrik/veneer/s2_error", currentError2 ? "1" : "0", true);
      }
    }
  }

  // 6A. UPDATE TAMPILAN JUMLAH & PUBLISH MQTT SENSOR 1
  if (objectCount1 != lastPrintedCount1)
  {
    lastPrintedCount1 = objectCount1;

    Serial.print("Sensor 1 Terdeteksi! Total: ");
    Serial.println(lastPrintedCount1);

    if (client.connected())
    {
      char countString1[10];
      dtostrf(lastPrintedCount1, 1, 0, countString1);
      client.publish("pabrik/veneer/jumlah", countString1);
    }
  }

  // 6B. UPDATE TAMPILAN JUMLAH & PUBLISH MQTT SENSOR 2
  if (objectCount2 != lastPrintedCount2)
  {
    lastPrintedCount2 = objectCount2;

    Serial.print("Sensor 2 (Anti-Noise) Terdeteksi! Total: ");
    Serial.println(lastPrintedCount2);

    if (client.connected())
    {
      char countString2[10];
      dtostrf(lastPrintedCount2, 1, 0, countString2);
      client.publish("pabrik/veneer/jumlah2", countString2);
    }
  }
}