// ============================================================================
// VENEER / PLYWOOD COUNTER - ESP32 + W5500 + LCD 20x4 + MQTT VPS
// INDIKATOR STATUS LAN & MQTT DETAIL DI LCD
// ============================================================================

#include <Wire.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>

// ===================== PIN =====================
#define SENSOR_PIN_1     27  // Sensor Hasil Sanding Besar
#define SENSOR_PIN_2     32  // Sensor Modal Sanding Otomatis
#define RESET_BUTTON_PIN  0  // Tombol Reset ESP32 (BOOT)

// Pin SPI Hardware ESP32 untuk W5500
#define W5500_CS_PIN   5
#define W5500_SCK_PIN  18
#define W5500_MISO_PIN 19
#define W5500_MOSI_PIN 23

// ===================== MQTT VPS CONFIG =====================
const char *mqtt_server = "IP_PUBLIC_VPS"; // <-- Ganti dengan IP Public VPS Anda
const uint16_t mqtt_port = 1883;

#define TOPIK_STATUS   "pabrik/veneer/status"
#define TOPIK_JUMLAH1  "pabrik/veneer/jumlah"
#define TOPIK_JUMLAH2  "pabrik/veneer/jumlah2"
#define TOPIK_IP       "pabrik/veneer/ip"
#define TOPIK_PERINTAH "pabrik/veneer/perintah"

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x01};

// ===================== OBJEK =====================
EthernetClient ethClient;
PubSubClient client(ethClient);
LiquidCrystal_I2C lcd(0x27, 20, 4); // LCD 20x4

// ===================== COUNTER & PRODUKSI =====================
portMUX_TYPE countMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t objectCount1 = 0; // Hasil Sanding (Sensor 1)
volatile uint32_t objectCount2 = 0; // Modal Sanding (Sensor 2)

uint32_t lastPublishedCount1 = UINT32_MAX;
uint32_t lastPublishedCount2 = UINT32_MAX;

// ===================== FILTER SENSOR =====================
const uint32_t S1_PRESS_STABLE_MS   = 100;
const uint32_t S1_RELEASE_STABLE_MS = 400;
const uint32_t S2_PRESS_STABLE_MS   = 100;
const uint32_t S2_RELEASE_STABLE_MS = 400;

struct SensorFilter {
  uint8_t pin;
  uint32_t pressStableMs;
  uint32_t releaseStableMs;
  int lastRaw;
  uint32_t lastChange;
  bool armed;
  const char *nama;
};

SensorFilter sensor1 = {
  SENSOR_PIN_1, S1_PRESS_STABLE_MS, S1_RELEASE_STABLE_MS,
  HIGH, 0, true, "S1"
};

SensorFilter sensor2 = {
  SENSOR_PIN_2, S2_PRESS_STABLE_MS, S2_RELEASE_STABLE_MS,
  HIGH, 0, true, "S2"
};

// ===================== STATUS NETWORK =====================
enum NetStatus : uint8_t {
  NET_MEMULAI,
  NET_LAN_GAGAL,
  NET_LINK_OFF,
  NET_MQTT_BELUM,
  NET_ONLINE
};

volatile NetStatus netStatus = NET_MEMULAI;
volatile int mqttStateCode = -1;
volatile uint32_t lanLostCount = 0;
volatile uint32_t mqttLostCount = 0;
volatile bool gRestartRequested = false;

char ipText[16] = "-";

// ===================== LCD & TIMER =====================
uint32_t lastLCDUpdate = 0;
const uint32_t LCD_UPDATE_INTERVAL = 300;

unsigned long lastNetReportInterval = 0;
const uint32_t NET_REPORT_INTERVAL = 60000; 

// ===================== NETWORK TASK STATE =====================
bool ethernetSiap = false;
bool lastLinkState = false;
bool lastMqttState = false;
uint32_t lastReconnectAttempt = 0;
const uint32_t RECONNECT_INTERVAL = 5000;
uint32_t lastDhcpRenewCheck = 0;
const uint32_t DHCP_RENEW_INTERVAL = 1000;
uint32_t lastHeartbeat = 0;
const uint32_t HEARTBEAT_INTERVAL = 15000;

uint8_t linkOffSamples = 0;
const uint8_t LINK_OFF_CONFIRM_SAMPLES = 2;

// ============================================================================
// ATOMIC COUNTER HELPERS
// ============================================================================
uint32_t readCount1() {
  uint32_t value;
  portENTER_CRITICAL(&countMux);
  value = objectCount1;
  portEXIT_CRITICAL(&countMux);
  return value;
}

uint32_t readCount2() {
  uint32_t value;
  portENTER_CRITICAL(&countMux);
  value = objectCount2;
  portEXIT_CRITICAL(&countMux);
  return value;
}

void incrementCount1() {
  portENTER_CRITICAL(&countMux);
  objectCount1++;
  portEXIT_CRITICAL(&countMux);
}

void incrementCount2() {
  portENTER_CRITICAL(&countMux);
  objectCount2++;
  portEXIT_CRITICAL(&countMux);
}

void resetCounters(bool s1, bool s2) {
  portENTER_CRITICAL(&countMux);
  if (s1) objectCount1 = 0;
  if (s2) objectCount2 = 0;
  portEXIT_CRITICAL(&countMux);
}

// ============================================================================
// SENSOR TASK (CORE 0)
// ============================================================================
void initSensor(SensorFilter &s) {
  s.lastRaw = digitalRead(s.pin);
  s.armed = (s.lastRaw == HIGH);
  s.lastChange = millis();
}

bool updateSensor(SensorFilter &s, uint32_t now) {
  int raw = digitalRead(s.pin);

  if (raw != s.lastRaw) {
    s.lastRaw = raw;
    s.lastChange = now;
  }

  uint32_t stableFor = now - s.lastChange;

  if (s.armed) {
    if (raw == LOW && stableFor >= s.pressStableMs) {
      s.armed = false;
      return true;
    }
  } else {
    if (raw == HIGH && stableFor >= s.releaseStableMs) {
      s.armed = true;
    }
  }
  return false;
}

void sensorTask(void *param) {
  (void)param;
  for (;;) {
    uint32_t now = millis();
    if (updateSensor(sensor1, now)) incrementCount1();
    if (updateSensor(sensor2, now)) incrementCount2();
    vTaskDelay(pdMS_TO_TICKS(1)); 
  }
}

// ============================================================================
// NETWORK HELPERS
// ============================================================================
void catatIP() {
  IPAddress ip = Ethernet.localIP();
  snprintf(ipText, sizeof(ipText), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

bool publishCount(const char *topic, uint32_t value) {
  if (!client.connected()) return false;
  char buf[11];
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)value);
  return client.publish(topic, buf, true);
}

void publishS1() {
  uint32_t c = readCount1();
  if (publishCount(TOPIK_JUMLAH1, c)) lastPublishedCount1 = c;
}

void publishS2() {
  uint32_t c = readCount2();
  if (publishCount(TOPIK_JUMLAH2, c)) lastPublishedCount2 = c;
}

void restartDevice() {
  Serial.println("Merestart ESP32...");
  if (client.connected()) {
    client.publish(TOPIK_STATUS, "0", true);
    client.disconnect();
  }
  delay(300);
  ESP.restart();
}

// ============================================================================
// MQTT CALLBACK
// ============================================================================
void callback(char *topic, byte *payload, unsigned int length) {
  char message[64];
  unsigned int n = (length < sizeof(message) - 1) ? length : sizeof(message) - 1;
  memcpy(message, payload, n);
  message[n] = '\0';

  Serial.print("MQTT [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  if (strcmp(topic, TOPIK_PERINTAH) != 0) return;

  if (strcmp(message, "RESET_S1") == 0) {
    resetCounters(true, false);
    publishS1();
  }
  else if (strcmp(message, "RESET_S2") == 0) {
    resetCounters(false, true);
    publishS2();
  }
  else if (strcmp(message, "RESET_ALL") == 0 || strcmp(message, "RESET") == 0) {
    resetCounters(true, true);
    publishS1();
    publishS2();
  }
  else if (strcmp(message, "RESTART_DEVICE") == 0) {
    restartDevice();
  }
}

void reconnectMQTT() {
  uint32_t now = millis();
  if (now - lastReconnectAttempt < RECONNECT_INTERVAL) return;
  lastReconnectAttempt = now;

  if (!ethernetSiap || !lastLinkState) return;

  char clientId[64];
  snprintf(clientId, sizeof(clientId),
           "ESP32VeneerClient-%02X%02X%02X%02X%02X%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  if (client.connect(clientId, TOPIK_STATUS, 0, true, "0")) {
    client.publish(TOPIK_STATUS, "1", true);
    client.subscribe(TOPIK_PERINTAH);
    publishS1();
    publishS2();

    IPAddress ip = Ethernet.localIP();
    char ipStr[16];
    snprintf(ipStr, sizeof(ipStr), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
    client.publish(TOPIK_IP, ipStr, true);

    lastHeartbeat = millis();
  } else {
    mqttStateCode = client.state();
  }
}

// ============================================================================
// NETWORK TASK (CORE 1)
// ============================================================================
void networkTask(void *param) {
  (void)param;
  Ethernet.init(W5500_CS_PIN);
  if (Ethernet.begin(mac) == 0) {
    ethernetSiap = false;
    netStatus = NET_LAN_GAGAL;
  } else {
    ethernetSiap = true;
    catatIP();
  }

  lastLinkState = (Ethernet.linkStatus() == LinkON);
  uint32_t lastLinkCheck = 0;

  for (;;) {
    uint32_t now = millis();

    if (gRestartRequested) {
      gRestartRequested = false;
      restartDevice();
    }

    if (now - lastLinkCheck >= 500) {
      lastLinkCheck = now;
      EthernetLinkStatus rawLink = Ethernet.linkStatus();

      if (rawLink == LinkON) {
        linkOffSamples = 0;
        if (!lastLinkState) lastLinkState = true;
      } else if (rawLink == LinkOFF) {
        if (linkOffSamples < 255) linkOffSamples++;
        if (linkOffSamples >= LINK_OFF_CONFIRM_SAMPLES && lastLinkState) {
          lastLinkState = false;
          lanLostCount++;
          if (client.connected()) client.disconnect();
          lastMqttState = false;
        }
      }
    }

    if (ethernetSiap && lastLinkState) {
      if (!client.connected()) {
        if (lastMqttState) {
          mqttLostCount++;
          lastMqttState = false;
        }
        reconnectMQTT();
        if (client.connected()) lastMqttState = true;
      } else {
        client.loop();
        lastMqttState = true;
      }
    } else {
      lastMqttState = false;
    }

    if (ethernetSiap && now - lastDhcpRenewCheck >= DHCP_RENEW_INTERVAL) {
      lastDhcpRenewCheck = now;
      Ethernet.maintain();
    }

    if (client.connected()) {
      uint32_t c1 = readCount1();
      uint32_t c2 = readCount2();
      
      if (c1 != lastPublishedCount1) {
        if (publishCount(TOPIK_JUMLAH1, c1)) lastPublishedCount1 = c1;
      }
      if (c2 != lastPublishedCount2) {
        if (publishCount(TOPIK_JUMLAH2, c2)) lastPublishedCount2 = c2;
      }

      if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
        lastHeartbeat = now;
        client.publish(TOPIK_STATUS, "1", true);
      }
    }

    if (!ethernetSiap) netStatus = NET_LAN_GAGAL;
    else if (!lastLinkState) netStatus = NET_LINK_OFF;
    else if (!client.connected()) netStatus = NET_MQTT_BELUM;
    else netStatus = NET_ONLINE;

    if (now - lastNetReportInterval >= NET_REPORT_INTERVAL) {
      lastNetReportInterval = now;
      Serial.print("[LAPORAN 1 MENIT] Status LAN: ");
      Serial.print(lastLinkState ? "OK (Link ON)" : "PUTUS");
      Serial.print(" | MQTT Broker: ");
      Serial.println(client.connected() ? "Terhubung" : "Terputus");
    }

    mqttStateCode = client.state();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ============================================================================
// SETUP & LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(100);

  Wire.begin();
  lcd.init();
  lcd.backlight(); 
  lcd.setCursor(0, 0);
  lcd.print("Veneer Counter 20x4");
  lcd.setCursor(0, 1);
  lcd.print("Inisialisasi Sistem.");

  pinMode(SENSOR_PIN_1, INPUT_PULLUP);
  pinMode(SENSOR_PIN_2, INPUT_PULLUP);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  initSensor(sensor1);
  initSensor(sensor2);

  SPI.begin(W5500_SCK_PIN, W5500_MISO_PIN, W5500_MOSI_PIN, W5500_CS_PIN);

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setBufferSize(512);
  client.setKeepAlive(30);
  client.setSocketTimeout(10);

  xTaskCreatePinnedToCore(sensorTask, "sensorTask", 4096, NULL, 3, NULL, 0);
  xTaskCreatePinnedToCore(networkTask, "networkTask", 12288, NULL, 1, NULL, 1);

  delay(1500);
  lcd.clear();
}

void loop() {
  uint32_t currentMillis = millis();
  uint32_t c1 = readCount1();
  uint32_t c2 = readCount2();

  // Update Tampilan LCD 20x4 Secara Kontinu dengan Status Terperinci
  if (currentMillis - lastLCDUpdate >= LCD_UPDATE_INTERVAL) {
    lastLCDUpdate = currentMillis;

    char baris0[21];
    char baris1[21];
    char baris2[21];
    char baris3[21];

    // Logika Pemetaan Teks Status di Baris 0
    if (!ethernetSiap || netStatus == NET_LAN_GAGAL) {
      snprintf(baris0, sizeof(baris0), "CEK KABEL/LAN: FAIL ");
    } else if (!lastLinkState || netStatus == NET_LINK_OFF) {
      snprintf(baris0, sizeof(baris0), "LAN: OFF  MQTT: NG  ");
    } else if (netStatus == NET_MQTT_BELUM) {
      snprintf(baris0, sizeof(baris0), "LAN: OK   MQTT: NG  ");
    } else {
      snprintf(baris0, sizeof(baris0), "LAN: OK   MQTT: OK  ");
    }

    snprintf(baris1, sizeof(baris1), "SANDING BESAR       ");
    snprintf(baris2, sizeof(baris2), "Modal  : %-11lu", (unsigned long)c2);
    snprintf(baris3, sizeof(baris3), "Hasil  : %-11lu", (unsigned long)c1);

    lcd.setCursor(0, 0); lcd.print(baris0);
    lcd.setCursor(0, 1); lcd.print(baris1);
    lcd.setCursor(0, 2); lcd.print(baris2);
    lcd.setCursor(0, 3); lcd.print(baris3);
  }

  delay(2);
}
