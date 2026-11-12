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
#include "partition_log.h"
#include "ota_state_machine.h"
#include "blob_ota_store.h"
#include "nvs_blob_store.h"
#include "ethernet_http_transport.h"
#include "esp_ota_flash.h"
#include "arduino_clock.h"
#include "ota_runner.h"
#include "ota_public_key.h"
#include "line_router.h"
#include "ota_command.h"
#include "server_proof.h"
#include "rollback.h"
#include "esp_boot_slots.h"
#include "esp_task_watchdog.h"
#include "ota_reporter.h"

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
// OTA STATE MACHINE (dibagi antar task, selalu diakses lewat mutex)
// =====================================================
NvsBlobStore otaBlobStore("ota");
BlobOtaStore otaStore(otaBlobStore, "state");
OtaStateMachine otaMachine(
    otaStore,
    OtaConfig{OTA_MAX_PENDING_BOOTS, OTA_MAX_REQUEST_ATTEMPTS, OTA_MAX_ROLLBACK_ATTEMPTS,
              OTA_HEALTH_DEADLINE_MS});

// Mengirim hasil OTA yang belum dilaporkan; dipakai HANYA oleh networkTask.
OtaReporter otaReporter(
    otaMachine,
    [](const char *json)
    { return mqtt.publishOtaReport(json); },
    OTA_REPORT_RETRY_MS);

BootAction aksiBootOta = BootAction::Normal; // hasil onBoot(), dibaca setup() dan networkTask
bool otaMengawasi = false;                   // true: firmware baru berstatus Pending, menunggu terbukti sehat
volatile uint32_t otaTargetDiminta = 0;      // diisi loop() dari perintah Serial, dilayani networkTask

// Bukti bahwa broker menerima keadaan awal boot ini. Ditulis dan dibaca HANYA oleh networkTask
// (publishKeadaanAwal dan awasiKesehatanOta), jadi tidak perlu mutex.
ServerProof serverProof;

// Baris Serial ditawarkan dulu ke penangan OTA; yang bukan perintah OTA diteruskan ke konsol.
LineRouter serialRouter([](char c)
                        { serialConsole.feed(c); });

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

  // Setiap MQTT tersambung dimulai putaran bukti baru: server dianggap sehat bila
  // SEMUA publish keadaan awal diterima (dipakai pengawas kesehatan OTA).
  serverProof.begin();

  // Setelah restart, mode "OFF" -> HMI tahu sensor perlu START ulang.
  serverProof.record(mqtt.publishOnConnect(c.hasil, c.modal, hitungAktif, ip));

  // Versi firmware sebagai JSON, retained. Dipanggil sekali setiap MQTT tersambung.
  char versi[FirmwareInfo::JSON_SIZE];
  firmwareInfo.toJson(versi, sizeof(versi));
  serverProof.record(mqtt.publishVersion(versi));

  serverProof.finish();
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

  // Task ini harus terpasang ke watchdog agar perangkat reset bila macet.
  if (!EspTaskWatchdog::subscribeCurrentTask())
    Serial.println("PERINGATAN: task tidak terpasang ke watchdog.");

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
// OTA
// Pemasangan sungguhan berjalan pada BOOT BERSIH: di setup(), sebelum task
// dibuat, sehingga hanya satu thread yang memakai Ethernet. Permintaannya
// datang dari perintah Serial "ota <build>" (SEMENTARA; kelak dari MQTT) dan
// dilayani networkTask, pemilik tunggal otaMachine.
// =====================================================
OtaRunOutcome jalankanOtaRunner(bool dryRun, uint32_t buildDiminta)
{
  EthernetHttpTransport transport;
  EspOtaFlash flash;
  ArduinoClock jam;
  OtaRunner runner(transport, flash, jam, [](const char *baris)
                   { Serial.println(baris); });

  const esp_partition_t *slotBerikut = esp_ota_get_next_update_partition(nullptr);

  OtaRunConfig cfg;
  cfg.host = OTA_HOST;
  cfg.port = OTA_PORT;
  cfg.manifestPath = OTA_MANIFEST_PATH;
  cfg.firmwarePath = OTA_FIRMWARE_PATH;
  cfg.publicKey = ota::SIGNING_PUBLIC_KEY;
  cfg.dryRun = dryRun;
  cfg.expectedBuild = buildDiminta;
  cfg.profile = DeviceProfile{OTA_DEVICE_TYPE, OTA_BOARD, static_cast<uint32_t>(firmwareInfo.build()),
                              slotBerikut != nullptr ? slotBerikut->size : 0};
  cfg.overallTimeoutMs = OTA_OVERALL_TIMEOUT_MS;
  return runner.run(cfg);
}

void cetakHasilOta(const OtaRunOutcome &hasil)
{
  Serial.print("OTA: hasil=");
  Serial.print(otaRunResultCode(hasil.result));
  Serial.print(" - ");
  Serial.println(hasil.detail);
}

// Dipanggil dari setup() bila onBoot() menjawab RunOta.
void prosesOtaBootBersih()
{
  Serial.println("OTA: boot bersih, memulai pemasangan.");
  lcdView.showMessage("UPDATE FIRMWARE", "JANGAN DIMATIKAN");

  OtaRunOutcome hasil;
  if (netMonitor.isOnline())
  {
    hasil = jalankanOtaRunner(false, otaMachine.record().targetBuild);
  }
  else
  {
    hasil.result = OtaRunResult::ManifestDownloadFailed;
    hasil.detail = "LAN belum online";
  }
  cetakHasilOta(hasil);

  if (hasil.result == OtaRunResult::Installed)
  {
    otaMachine.otaInstalled(); // bila gagal menyimpan, onBoot() berikutnya memperbaikinya
    Serial.println("OTA: terpasang. Memulai ulang ke firmware baru...");
    delay(500);
    ESP.restart();
  }

  otaMachine.otaFailed(otaResultFor(hasil.result));
  Serial.println("OTA: gagal. Perangkat lanjut dengan firmware yang sekarang.");
}

// Dipanggil dari setup() bila onBoot() menjawab Rollback. Tidak kembali: perangkat restart.
// Bila gagal, restart tetap dilakukan; state machine membatasi percobaan (OTA_MAX_ROLLBACK_ATTEMPTS).
void prosesRollbackBoot()
{
  Serial.println("OTA: ROLLBACK ke firmware lama.");
  lcdView.showMessage("ROLLBACK", "JANGAN DIMATIKAN");

  EspBootSlots slots;
  const RollbackResult hasil = rollbackToPrevious(slots);
  Serial.print("OTA: rollback=");
  Serial.println(rollbackResultCode(hasil));
  if (hasil != RollbackResult::Switched)
  {
    Serial.println(slots.lastError());
  }
  delay(500);
  ESP.restart();
}

#ifdef OTA_DRY_RUN_ON_BOOT
// SEMENTARA: uji kering tanpa mengalihkan slot boot.
void ujiKeringOta()
{
  if (!netMonitor.isOnline())
  {
    Serial.println("OTA DRY RUN dilewati: LAN belum online.");
    return;
  }
  cetakHasilOta(jalankanOtaRunner(true, 0));
}
#endif

// Penangan baris Serial: "ota <build>". Hanya mencatat permintaan; networkTask yang melayaninya.
bool tanganiPerintahOta(const char *baris)
{
  uint32_t target = 0;
  switch (OtaCommand::parse(baris, target))
  {
  case OtaCommandResult::NotOta:
    return false;
  case OtaCommandResult::BadArgument:
    Serial.println("Pemakaian: ota <nomor build>, contoh: ota 2");
    return true;
  case OtaCommandResult::Ok:
    otaTargetDiminta = target;
    return true;
  }
  return false;
}

// Perintah OTA dari MQTT. Berjalan di networkTask; hanya mencatat permintaan,
// pelaksanaannya oleh prosesPermintaanOta() seperti pada perintah Serial.
void handleOtaMqtt(OtaMessage pesan, uint32_t build)
{
  switch (pesan)
  {
  case OtaMessage::Valid:
    Serial.print("MQTT: perintah OTA diterima, build ");
    Serial.println(build);
    otaTargetDiminta = build;
    break;
  case OtaMessage::TooEarly:
    Serial.println("MQTT: perintah OTA diabaikan (tiba tepat setelah tersambung, mungkin retained).");
    break;
  case OtaMessage::Malformed:
    Serial.println("MQTT: perintah OTA diabaikan (format salah, harus 'ota <build>').");
    break;
  }
}

// networkTask: melayani permintaan "ota <build>".
void prosesPermintaanOta()
{
  const uint32_t target = otaTargetDiminta;
  if (target == 0)
    return;
  otaTargetDiminta = 0;

  // Permintaan OTA hanya diterima bila firmware baru lebih baru dari yang sekarang.
  if (target <= static_cast<uint32_t>(firmwareInfo.build()))
  {
    Serial.print("OTA: permintaan diabaikan, build target ");
    Serial.print(target);
    Serial.println(" tidak lebih baru dari build sekarang.");
    return;
  }

  if (!otaMachine.requestUpdate(target, static_cast<uint32_t>(firmwareInfo.build())))
  {
    Serial.print("OTA: permintaan ditolak, state=");
    Serial.println(otaStateCode(otaMachine.state()));
    return;
  }
  Serial.print("OTA: update ke build ");
  Serial.print(target);
  Serial.println(" diminta. Memulai ulang ke boot bersih...");
  vTaskDelay(pdMS_TO_TICKS(300));
  ESP.restart();
}

// networkTask: firmware baru (Pending) harus membuktikan diri sehat.
// Sehat = LAN online, MQTT tersambung, DAN broker menerima semua publish keadaan awal
// boot ini (serverProof). Rollback belum dijalankan (diisi di 20c).
void awasiKesehatanOta(unsigned long now)
{
  constexpr unsigned long INTERVAL_MS = 1000;
  static unsigned long terakhir = 0;
  if (!otaMengawasi || now - terakhir < INTERVAL_MS)
    return;
  terakhir = now;

  const HealthReport laporan{netMonitor.isOnline(), mqtt.isConnected(), serverProof.ok()};
  switch (otaMachine.tick(now, laporan))
  {
  case HealthAction::Confirmed:
    Serial.println("OTA: firmware baru terkonfirmasi sehat.");
    otaMengawasi = false;
    break;
  case HealthAction::Rollback:
    Serial.println("OTA: batas waktu habis, rollback. Memulai ulang...");
    otaMengawasi = false;
    vTaskDelay(pdMS_TO_TICKS(300));
    ESP.restart();
    break;
  case HealthAction::NotNeeded:
    otaMengawasi = false;
    break;
  case HealthAction::Waiting:
    break;
  }
}

// networkTask: melaporkan hasil OTA terakhir ke website sampai berhasil.
void laporkanHasilOta(unsigned long now)
{
  switch (otaReporter.service(now, mqtt.isConnected(), static_cast<uint32_t>(firmwareInfo.build())))
  {
  case OtaReportEvent::Sent:
    Serial.println("OTA: hasil dilaporkan ke server.");
    break;
  case OtaReportEvent::SendFailed:
    Serial.println("OTA: gagal mengirim laporan, dicoba lagi.");
    break;
  case OtaReportEvent::NotMarked:
    Serial.println("OTA: laporan terkirim tetapi gagal ditandai, akan dikirim ulang.");
    break;
  case OtaReportEvent::Nothing:
    break;
  }
}

// =====================================================
// TASK NETWORK (CORE 1)
// Satu-satunya pemilik MQTT dan LCD.
// =====================================================
void networkTask(void *parameter)
{
  Serial.println("Network Task START");

  // Task ini harus terpasang ke watchdog agar perangkat reset bila macet.
  if (!EspTaskWatchdog::subscribeCurrentTask())
    Serial.println("PERINGATAN: task tidak terpasang ke watchdog.");

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
    // OTA: permintaan dari Serial dan pembuktian sehat firmware baru
    // -------------------------------------------------
    prosesPermintaanOta();
    awasiKesehatanOta(now);
    laporkanHasilOta(now);

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

  serialRouter.addHandler(tanganiPerintahOta);

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

  logPartitions();

  // Catatan OTA
  otaBlobStore.begin();
  OtaRecord catatanOta;
  Serial.println(otaStore.load(catatanOta) ? "OTA: catatan ditemukan"
                                           : "OTA: catatan belum ada atau rusak, dibuat baru");

  const BootAction aksiBoot = otaMachine.onBoot(firmwareInfo.build(), millis());
  aksiBootOta = aksiBoot;
  otaMengawasi = (aksiBoot == BootAction::WatchHealth);
  Serial.print("OTA: state=");
  Serial.print(otaStateCode(otaMachine.state()));
  Serial.print(", aksi boot=");
  Serial.println(bootActionCode(aksiBoot));
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

  // Rollback: pindahkan slot boot ke firmware lama, lalu restart. Tidak kembali ke setup().
  if (aksiBootOta == BootAction::Rollback)
  {
    prosesRollbackBoot();
  }

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

  if (aksiBootOta == BootAction::RunOta)
  {
    prosesOtaBootBersih(); // bila berhasil, perangkat restart dan tidak kembali ke sini
  }
#ifdef OTA_DRY_RUN_ON_BOOT
  else
  {
    ujiKeringOta();
  }
#endif

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
  // MQTT handling topic for OTA command
  mqtt.onOtaCommand(handleOtaMqtt);

  if (!mqttSettings.enabled)
  {
    Serial.println("MQTT dinonaktifkan: mqtt_host belum diisi. Ketik 'help' di Serial Monitor.");
  }

  // LCD awal
  lcdView.restartPageTimer(millis());
  updateLCD();

  // Watchdog task: reset bila networkTask tidak melapor dalam batas waktu.
  if (!EspTaskWatchdog::begin(TASK_WDT_TIMEOUT_S))
  {
    Serial.println("PERINGATAN: watchdog tugas gagal diatur.");
  }

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
    serialRouter.feed((char)Serial.read());
  }
  vTaskDelay(pdMS_TO_TICKS(20));
}