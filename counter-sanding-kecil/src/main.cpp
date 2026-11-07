#include "sanding_config.h"
#include "sensor_filter.h"
#include "counter_store.h"
#include "esp_spin_lock.h"
#include "command_parser.h"
#include "net_monitor.h"
#include "enc_driver.h"
#include "pubsub_mqtt_client.h"
#include "mqtt_service.h"
#include "lcd_view.h"
#include "i2c_lcd_display.h"
#include "device_identity.h"
#include "esp_mac_source.h"
#include "device_config.h"
#include "console_shell.h"
#include "nvs_config_store.h"
#include "firmware_info.h"
#include "build_info.h"

using namespace config;

// NVS Configuration store (dipakai oleh console shell)
NvsConfigStore configStore;
ConfigManager configManager(configStore, config::namaMesin, config::MQTT_DEFAULT_PORT);
DeviceConfig deviceConfig; // diisi sekali di setup(), sesudahnya hanya dibaca

ConsoleShell serialConsole(
    configManager,
    [](const char *baris)
    { Serial.println(baris); },
    []()
    { delay(300); ESP.restart(); });

I2cLcdDisplay lcdDisplay;
LcdView lcdView(lcdDisplay, deviceConfig.machineName, LCD_PAGE_INTERVAL);

EspMacSource macSource;
DeviceIdentity identity(macSource, config::clientIdPrefix);
EncDriver encDriver(identity);
NetMonitor netMonitor(encDriver, DHCP_RETRY_INTERVAL);

EthernetClient ethClient;
PubSubMqttClient mqttClient(ethClient);
MqttService mqtt(mqttClient);

// =====================================================
// COUNTER (dibagi antar task, selalu diakses lewat mutex)
// =====================================================
EspSpinLock counterLock;
CounterStore counters(counterLock);

// Firmware version (dipakai di MQTT Last Will)
FirmwareInfo firmwareInfo(FW_VERSION, FW_BUILD);

// =====================================================
// SAKLAR HITUNG
// false saat boot: sensor tidak menghitung sampai HMI mengirim START.
// Filter sensor tetap berjalan saat OFF (agar status sinkron),
// hanya hasil hitungannya yang dibuang.
// =====================================================
volatile bool hitungAktif = false;

// Balasan yang menunggu dikirim ke HMI ("START_OK", "STOP_OK", "RESET_OK").
// Diisi dan dibaca HANYA oleh networkTask (callback MQTT berjalan di
// dalam client.loop()), jadi tidak perlu volatile/mutex.
const char *ackPesan = nullptr;

// =====================================================
// STATUS JARINGAN & LCD (hanya diubah oleh networkTask)
// =====================================================

unsigned long lastEthCheck = 0;

SensorFilter filterHasil(SENSOR_ACTIVE, PRESS_STABLE_TIME, RELEASE_STABLE_TIME);
SensorFilter filterModal(SENSOR_ACTIVE, PRESS_STABLE_TIME, RELEASE_STABLE_TIME);

// =====================================================
// LCD (hanya dipanggil dari networkTask dan setup)
// =====================================================

void updateLCD()
{
  const CounterSnapshot c = counters.snapshot();

  LcdState state;
  state.hasil = c.hasil;
  state.modal = c.modal;
  state.counting = hitungAktif;
  state.lanOk = netMonitor.isOnline();
  state.mqttOk = mqtt.isConnected();

  lcdView.render(state);
}

void handleLCD()
{
  if (lcdView.advancePageIfDue(millis()))
  {
    updateLCD();
  }
}

// =====================================================
// MQTT CALLBACK (berjalan di dalam client.loop() -> networkTask)
// Memakai char array, bukan String, agar memori tidak terfragmentasi.
// =====================================================
void handleCommand(Command command)
{
  Serial.print("MQTT Command: ");
  Serial.println(CommandParser::name(command));

  switch (command)
  {
  case Command::Start:
    // Nol-kan DULU, baru aktifkan: hitungan setelah ini tidak ikut terhapus
    counters.reset(true, true);
    hitungAktif = true;
    ackPesan = "START_OK";
    Serial.println("START: mulai menghitung dari 0");
    break;

  case Command::Stop:
    // Matikan DULU, baru nol-kan: tidak ada hitungan nyelip setelah nol
    hitungAktif = false;
    counters.reset(true, true);
    ackPesan = "STOP_OK";
    Serial.println("STOP: berhenti menghitung, angka 0");
    break;

  case Command::ResetHasil:
    counters.reset(true, false);
    ackPesan = "RESET_OK";
    Serial.println("RESET HASIL");
    break;

  case Command::ResetModal:
    counters.reset(false, true);
    ackPesan = "RESET_OK";
    Serial.println("RESET MODAL");
    break;

  case Command::ResetAll:
    counters.reset(true, true);
    ackPesan = "RESET_OK";
    Serial.println("RESET SEMUA");
    break;

  case Command::RestartDevice:
    Serial.println("RESTART ESP32...");
    delay(500);
    ESP.restart();
    break;

  case Command::Unknown:
    break;
  }
}

// Dipanggil sekali setiap MQTT berhasil tersambung.
void publishKeadaanAwal()
{
  const CounterSnapshot c = counters.snapshot();

  char ip[20];
  formatIp(netMonitor.localIp(), ip, sizeof(ip));

  // Setelah restart, mode "OFF" -> HMI tahu sensor perlu START ulang.
  mqtt.publishOnConnect(c.hasil, c.modal, hitungAktif, ip);

  // Versi firmware sebagai JSON, retained. Dipanggil sekali setiap MQTT tersambung.
  char versi[FirmwareInfo::JSON_SIZE];
  firmwareInfo.toJson(versi, sizeof(versi));
  mqtt.publishVersion(versi);
}

// =====================================================
// ETHERNET
// lanOK = kabel terpasang DAN sudah punya IP
// (kode lama hanya mengecek kabel)
// =====================================================

void logNetEvent(NetEvent event)
{
  if (event == NetEvent::DhcpRetrySucceeded)
  {
    Serial.print("DHCP diulang, berhasil. IP: ");
    Serial.println(Ethernet.localIP());
  }
  else if (event == NetEvent::DhcpRetryFailed)
  {
    Serial.println("DHCP diulang, gagal.");
  }
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
      counters.reset(true, true);
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
    // Filter SELALU dijalankan agar status sensor tetap sinkron,
    // walau saklar OFF. Hasilnya baru dipakai kalau saklar ON.
    bool hasilBaru = filterHasil.update(digitalRead(SENSOR_HASIL), millis());
    bool modalBaru = filterModal.update(digitalRead(SENSOR_MODAL), millis());

    if (hitungAktif)
    {
      if (hasilBaru)
      {
        counters.incrementHasil();
        Serial.print("HASIL +1 = ");
        Serial.println(counters.snapshot().hasil);
      }

      if (modalBaru)
      {
        counters.incrementModal();
        Serial.print("MODAL +1 = ");
        Serial.println(counters.snapshot().modal);
      }
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
      logNetEvent(netMonitor.update(now));
      updateLCD();
    }

    // -------------------------------------------------
    // MQTT
    // -------------------------------------------------
    switch (mqtt.update(netMonitor.isOnline(), now))
    {
    case MqttEvent::Connected:
      Serial.println("MQTT CONNECTED");
      publishKeadaanAwal();
      updateLCD();
      break;

    case MqttEvent::ConnectFailed:
      Serial.print("MQTT gagal, rc=");
      Serial.println(mqtt.lastState());
      updateLCD();
      break;

    case MqttEvent::None:
      break;
    }

    // -------------------------------------------------
    // Ada perubahan counter dari sensorTask / reset / START / STOP
    // -------------------------------------------------
    if (counters.takeDirty())
    {
      if (mqtt.isConnected())
      {
        const CounterSnapshot c = counters.snapshot();
        mqtt.publishChange(c.hasil, c.modal, hitungAktif, ackPesan);
      }

      ackPesan = nullptr;
      updateLCD();
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

  configStore.begin();
  const int tidakValid = configManager.load(deviceConfig);
  if (tidakValid > 0)
  {
    Serial.print("PERINGATAN: ");
    Serial.print(tidakValid);
    Serial.println(" nilai konfigurasi tidak valid, memakai nilai bawaan.");
  }

  // MAC & Client ID
  if (!identity.begin())
  {
    Serial.println("PERINGATAN: MAC perangkat tidak valid, jaringan mungkin gagal.");
  }
  Serial.print("MAC: ");
  Serial.println(identity.macText());
  Serial.print("Client ID: ");
  Serial.println(identity.clientId());

  delay(500);

  Serial.println();
  Serial.println("==============================");
  Serial.println("SANDING KECIL");
  Serial.println("ESP32 + ENC28J60");
  Serial.println("==============================");

  // Firmware version
  Serial.print("Versi: ");
  Serial.print(firmwareInfo.version());
  Serial.print(" (build ");
  Serial.print(firmwareInfo.build());
  Serial.println(")");

  // Pin sensor & tombol
  pinMode(SENSOR_HASIL, INPUT_PULLUP);
  pinMode(SENSOR_MODAL, INPUT_PULLUP);
  pinMode(RESET_BUTTON, INPUT_PULLUP);

  // Kondisi awal sensor
  filterHasil.begin(digitalRead(SENSOR_HASIL), millis());
  filterModal.begin(digitalRead(SENSOR_MODAL), millis());

  // LCD
  lcdView.begin();
  lcdView.showMessage("SANDING KECIL", "STARTING...");
  delay(1500);

  // SPI + ENC28J60
  SPI.begin(ENC_SCK, ENC_MISO, ENC_MOSI, ENC_CS_PIN);
  Ethernet.init(ENC_CS_PIN);

  // DHCP (jika gagal, networkTask akan mengulang otomatis)
  Serial.println("Memulai DHCP...");

  if (netMonitor.start())
  {
    Serial.print("DHCP berhasil, IP: ");
    Serial.println(Ethernet.localIP());
  }
  else
  {
    Serial.println("DHCP gagal, akan dicoba lagi otomatis.");
  }

  // MQTT
  mqttClient.setServer(deviceConfig.mqttHost, deviceConfig.mqttPort);

  MqttSettings mqttSettings{};
  snprintf(mqttSettings.clientId, sizeof(mqttSettings.clientId), "%s", identity.clientId());
  mqttSettings.user = deviceConfig.mqttUser;
  mqttSettings.pass = deviceConfig.mqttPass;
  mqttSettings.useLastWill = MQTT_USE_LAST_WILL;
  mqttSettings.enabled = ConfigManager::isComplete(deviceConfig);
  mqtt.begin(mqttSettings);
  mqtt.onCommand(handleCommand);

  if (!mqttSettings.enabled)
  {
    Serial.println("MQTT dinonaktifkan: mqtt_host belum diisi. Ketik 'help' di Serial Monitor.");
  }

  // LCD awal
  lcdView.restartPageTimer(millis());
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
  while (Serial.available() > 0)
  {
    serialConsole.feed((char)Serial.read());
  }
  vTaskDelay(pdMS_TO_TICKS(20));
}
