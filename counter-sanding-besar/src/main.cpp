#include <Wire.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <esp_system.h>

#include "sanding_config.h"
#include "build_info.h"
#include "secrets.h"
#include "sensor_filter.h"
#include "counter_store.h"
#include "esp_spin_lock.h"
#include "esp_task_watchdog.h"
#include "command_parser.h"
#include "net_monitor.h"
#include "w5500_driver.h"
#include "pubsub_mqtt_client.h"
#include "mqtt_service.h"
#include "lcd_view.h"
#include "i2c_lcd_display.h"
#include "device_identity.h"
#include "fixed_mac_source.h"
#include "device_config.h"
#include "console_shell.h"
#include "nvs_config_store.h"
#include "firmware_info.h"

using namespace config;

// =====================================================
// SANDING BESAR - ESP32 + W5500 + LCD 20x4 + MQTT
// -----------------------------------------------------
// sensorTask  (core 0): HANYA menghitung sensor & tombol reset.
//                       Tidak menyentuh MQTT, LCD, atau flash.
// networkTask (core 1): SATU-SATUNYA pemilik MQTT, LCD, dan flash.
//
// Dua task tidak boleh memakai objek yang sama
// (client MQTT, LCD) karena library-nya tidak thread-safe.
// Komunikasi antar task lewat CounterStore (dilindungi mutex).
//
// LAPISAN KESTABILAN:
//  1. Backoff      : jeda konek ulang MQTT 2s -> 4s -> ... -> 60s
//  2. Reset chip   : MQTT/IP bermasalah > 5 menit -> reset W5500
//  3. Restart total: MQTT/IP bermasalah > 10 menit -> ESP.restart()
//  4. Watchdog     : task macet > 60 detik -> restart otomatis
//  5. Counter disimpan di flash, jadi restart tidak menghapus hitungan
//  6. Satu fungsi status (hitungNetState) dipakai LCD dan laporan diagnosa
// =====================================================

// =====================================================
// KOMPONEN GLOBAL (konstruktor tidak berbuat banyak,
// inisialisasi nyata terjadi di setup())
// =====================================================

// Konfigurasi runtime (MQTT host/port/user/pass, nama mesin) via NVS.
NvsConfigStore configStore;
ConfigManager configManager(configStore, config::namaMesin, config::MQTT_DEFAULT_PORT);
DeviceConfig deviceConfig; // diisi sekali di setup(), sesudahnya hanya dibaca

ConsoleShell serialConsole(
    configManager,
    [](const char *baris)
    { Serial.println(baris); },
    []()
    { delay(300); ESP.restart(); });

// LCD I2C 20x4 dan logika halamannya.
I2cLcdDisplay lcdDisplay;
LcdView lcdView(lcdDisplay, deviceConfig.machineName, LCD_PAGE_INTERVAL);

// MAC address dari include/secrets.h, lalu Client ID MQTT.
FixedMacSource macSource;
DeviceIdentity identity(macSource, config::clientIdPrefix);

// Jaringan W5500 + monitoring DHCP/link.
W5500Driver ethDriver(identity);
NetMonitor netMonitor(ethDriver, DHCP_RETRY_INTERVAL);

// Klien MQTT (PubSubClient dibungkus agar sesuai antarmuka IMqttClient).
EthernetClient ethClient;
PubSubMqttClient mqttClient(ethClient);
MqttService mqtt(mqttClient);

// =====================================================
// COUNTER (dibagi antar task, selalu diakses lewat mutex)
// =====================================================
EspSpinLock counterLock;
CounterStore counters(counterLock);

// Versi firmware (dari tools/version.py via build_info.h).
FirmwareInfo firmwareInfo(FW_VERSION, FW_BUILD);

// =====================================================
// STATUS PENCACAHAN (hanya diubah oleh networkTask,
// kecuali hitungAktif juga dibaca oleh sensorTask)
// =====================================================
volatile bool hitungAktif = HITUNG_OTOMATIS;
volatile uint8_t cmdTertunda = 0;
const char *ackPesan = nullptr; // "START_OK", "STOP_OK", "RESET_OK"

// =====================================================
// FLASH (NVS): simpan & muat counter
// Hanya dipanggil dari setup() dan networkTask.
// =====================================================
Preferences prefs;
bool perluSimpan = false;
unsigned long lastSave = 0;

void simpanCounter()
{
    if (!SIMPAN_COUNTER_FLASH)
    {
        return;
    }

    const CounterSnapshot c = counters.snapshot();

    prefs.begin("counter", false);
    prefs.putULong("hasil", c.hasil);
    prefs.putULong("modal", c.modal);
    prefs.end();
}

void muatCounter()
{
    if (!SIMPAN_COUNTER_FLASH)
    {
        return;
    }

    prefs.begin("counter", true);
    const uint32_t hasil = prefs.getULong("hasil", 0);
    const uint32_t modal = prefs.getULong("modal", 0);
    prefs.end();

    counters.set(hasil, modal);

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

    if (mqtt.isConnected())
    {
        ethClient.publish(topicStatus, STATUS_OFFLINE, true);
        mqttClient.disconnect();
    }

    delay(300);
    ESP.restart();
}

// =====================================================
// WATCHDOG (menggunakan EspTaskWatchdog dari src/)
// =====================================================
void aktifkanWatchdog()
{
    if (!EspTaskWatchdog::begin(WDT_TIMEOUT_S))
    {
        Serial.println("PERINGATAN: watchdog tugas gagal diatur.");
    }
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
enum NetState : uint8_t
{
    NS_KABEL_PUTUS = 0, // paling parah
    NS_TANPA_IP = 1,
    NS_MQTT_NG = 2,
    NS_ONLINE = 3
};

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
        return "ONLINE";
    }
}

NetState hitungNetState(bool linkUp, bool hasIP, bool mqttOk)
{
    if (!linkUp)
        return NS_KABEL_PUTUS;
    if (!hasIP)
        return NS_TANPA_IP;
    if (!mqttOk)
        return NS_MQTT_NG;
    return NS_ONLINE;
}

// MAC W5500 (unik di jaringan, sama untuk driver dan pemulihan chip).
uint8_t mac[6] = DEVICE_MAC;

// Riwayat gangguan (dilaporkan ke dashboard setelah koneksi pulih)
bool pernahOnline = false;
unsigned long outageStart = 0;
NetState outageCause = NS_ONLINE;
unsigned long lastOutageSec = 0;
NetState lastOutageCause = NS_ONLINE;
int lastErrSeen = 0;         // kode error MQTT terakhir yang bukan 0
bool diagSegera = false;     // minta kirim laporan diag secepatnya
unsigned long lastDiag = 0;

uint32_t lanLostCount = 0;
uint32_t mqttLostCount = 0;
uint8_t linkOffSamples = 0;

unsigned long lastEthCheck = 0;

// Pelacak lama masalah (untuk pemulihan bertingkat)
unsigned long problemSince = 0;
bool sudahResetEth = false;

unsigned long lastReport = 0;
NetState lastLcdState = NS_ONLINE;

// =====================================================
// LAPORAN DIAGNOSA (JSON, retained, ke topik diag)
// =====================================================
void publishDiag()
{
    if (!mqtt.isConnected())
    {
        return;
    }

    const Ip4 ip = ethDriver.localIP();

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
        ip.b[0], ip.b[1], ip.b[2], ip.b[3],
        firmwareInfo.version());

    mqtt.publishDiag(buf);
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

// =====================================================
// LCD 20x4 (hanya dipanggil dari networkTask dan setup)
// =====================================================
void updateLCD()
{
    const CounterSnapshot c = counters.snapshot();

    char ipText[21] = "";
    if (ethDriver.hasIP())
    {
        formatIp(ethDriver.localIP(), ipText, sizeof(ipText));
    }

    LcdState state;
    state.hasil = c.hasil;
    state.modal = c.modal;
    state.counting = hitungAktif;
    state.lanOk = netMonitor.isOnline();
    state.mqttOk = mqtt.isConnected();
    state.ip = ipText;
    state.modeAktif = hitungAktif;

    lcdView.render(state);
}

// =====================================================
// MQTT PUBLISH (hanya dipanggil dari networkTask)
// =====================================================
void publishKeadaanAwal()
{
    const CounterSnapshot c = counters.snapshot();

    char ip[21] = "";
    if (ethDriver.hasIP())
    {
        formatIp(ethDriver.localIP(), ip, sizeof(ip));
    }

    // Setiap MQTT tersambung dimulai putaran bukaan baru: publish jumlah,
    // jumlah2, mode, dan IP (semua retained).
    mqtt.publishOnConnect(c.hasil, c.modal, hitungAktif, ethDriver.hasIP() ? ip : nullptr);
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
        // Tandai permintaan START; networkTask yang melaksanakannya
        // (memakai HITUNG_OTOMATIS dan backoff bila perlu).
        cmdTertunda = 1;
        Serial.println("START: permintaan ditandai");
        break;

    case Command::Stop:
        cmdTertunda = 2;
        Serial.println("STOP: permintaan ditandai");
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
        restartDevice("perintah MQTT");
        break;

    case Command::Unknown:
        break;
    }
}

// =====================================================
// W5500 / ETHERNET
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
    pinMode(W5500_RST_PIN, OUTPUT);
    digitalWrite(W5500_RST_PIN, LOW);
    delay(50);
    digitalWrite(W5500_RST_PIN, HIGH);
    delay(500);
#endif

    Ethernet.init(W5500_CS_PIN);

    if (Ethernet.begin(mac, 8000, 2000) != 0)
    {
        Serial.print("Reset berhasil, IP: ");
        Serial.println(Ethernet.localIP());
    }
    else
    {
        Serial.println("Reset selesai, tapi DHCP belum berhasil.");
    }
}

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
// TOMBOL RESET LOKAL (tombol BOOT)
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
// Hanya menghitung. Tidak menyentuh MQTT, LCD, atau flash.
// =====================================================
void sensorTask(void *parameter)
{
    Serial.println("Sensor Task START");

    if (!EspTaskWatchdog::subscribeCurrentTask())
        Serial.println("PERINGATAN: task tidak terpasang ke watchdog.");

    SensorFilter filterHasilLocal(SENSOR_ACTIVE, PRESS_STABLE_TIME, RELEASE_STABLE_TIME);
    SensorFilter filterModalLocal(SENSOR_ACTIVE, PRESS_STABLE_TIME, RELEASE_STABLE_TIME);

    filterHasilLocal.begin(digitalRead(SENSOR_HASIL), millis());
    filterModalLocal.begin(digitalRead(SENSOR_MODAL), millis());

    for (;;)
    {
        EspTaskWatchdog::feed();

        // Filter SELALU dijalankan agar status sensor tetap sinkron,
        // walau saklar OFF. Hasilnya baru dipakai kalau saklar ON.
        bool hitH = filterHasilLocal.update(digitalRead(SENSOR_HASIL), millis());
        if (hitH && hitungAktif)
        {
            counters.incrementHasil();
            const CounterSnapshot c = counters.snapshot();
            Serial.print("HASIL +1 = ");
            Serial.println(c.hasil);
        }

        bool hitM = filterModalLocal.update(digitalRead(SENSOR_MODAL), millis());
        if (hitM && hitungAktif)
        {
            counters.incrementModal();
            const CounterSnapshot c = counters.snapshot();
            Serial.print("MODAL +1 = ");
            Serial.println(c.modal);
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

    if (!EspTaskWatchdog::subscribeCurrentTask())
        Serial.println("PERINGATAN: task tidak terpasang ke watchdog.");

    bool prevMqttOK = false;

    for (;;)
    {
        EspTaskWatchdog::feed();

        unsigned long now = millis();

        // -------------------------------------------------
        // Cek Ethernet berkala
        // -------------------------------------------------
        if (now - lastEthCheck >= ETH_CHECK_INTERVAL)
        {
            lastEthCheck = now;

            const bool linkUpSebelum = ethDriver.linkUp();
            const NetEvent ev = netMonitor.update(now);
            logNetEvent(ev);

            const bool linkUp = ethDriver.linkUp();
            const bool hasIP = ethDriver.hasIP();

            if (linkUpSebelum && !linkUp)
            {
                lanLostCount++;
                Serial.println("KABEL LAN PUTUS");
            }
            if (linkUp && !linkUpSebelum)
            {
                Serial.println("Kabel LAN terdeteksi");
            }

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
            lastErrSeen = mqtt.lastState();
            Serial.print("MQTT gagal, rc=");
            Serial.print(lastErrSeen);
            Serial.print(" (");
            Serial.print(namaErrorMQTT(lastErrSeen));
            Serial.println("), coba lagi dengan backoff");
            updateLCD();
            break;

        case MqttEvent::None:
            break;
        }

        // Hitung berapa kali MQTT putus setelah sempat online
        const bool mqttOK = mqtt.isConnected();
        if (prevMqttOK && !mqttOK)
        {
            mqttLostCount++;
            Serial.println("MQTT PUTUS");
        }
        prevMqttOK = mqttOK;

        // -------------------------------------------------
        // Catat riwayat gangguan, dilaporkan setelah koneksi pulih
        // -------------------------------------------------
        const bool linkUp = ethDriver.linkUp();
        const bool hasIP = ethDriver.hasIP();
        const NetState ns = hitungNetState(linkUp, hasIP, mqtt.isConnected());

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

        // LCD segera diperbarui saat status berubah
        if (ns != lastLcdState)
        {
            lastLcdState = ns;
            updateLCD();
        }

        // -------------------------------------------------
        // Kirim laporan diag: segera setelah konek/pulih, lalu tiap 60 detik
        // -------------------------------------------------
        if (mqtt.isConnected() && (diagSegera || now - lastDiag >= DIAG_INTERVAL))
        {
            lastDiag = now;
            diagSegera = false;
            publishDiag();
        }

        // -------------------------------------------------
        // Perintah START / STOP tertunda dari MQTT (atau Serial)
        // -------------------------------------------------
        uint8_t c = cmdTertunda;
        cmdTertunda = 0;

        if (!HITUNG_OTOMATIS)
        {
            c = 0; // mode otomatis: START/STOP dari HMI diabaikan
        }

        if (c == 1) // START
        {
            counters.reset(true, true);
            hitungAktif = true;
            if (mqtt.isConnected())
            {
                const CounterSnapshot s = counters.snapshot();
                mqtt.publishChange(s.hasil, s.modal, true, nullptr);
                mqttClient.publish(topicMode, "ON", true);
                if (KIRIM_ACK)
                {
                    mqttClient.publish(topicAck, "START_OK", false);
                }
            }
        }
        else if (c == 2) // STOP
        {
            hitungAktif = false;
            counters.reset(true, true);
            if (mqtt.isConnected())
            {
                const CounterSnapshot s = counters.snapshot();
                mqtt.publishChange(s.hasil, s.modal, false, nullptr);
                mqttClient.publish(topicMode, "OFF", true);
                if (KIRIM_ACK)
                {
                    mqttClient.publish(topicAck, "STOP_OK", false);
                }
            }
        }

        // -------------------------------------------------
        // Ada perubahan counter dari sensorTask / reset / START / STOP
        // -------------------------------------------------
        if (counters.takeDirty())
        {
            // Bendera dimatikan SEBELUM membaca counter, jadi
            // hitungan yang masuk setelah ini tidak ada yang terlewat.
            perluSimpan = true;

            if (mqtt.isConnected())
            {
                const CounterSnapshot c = counters.snapshot();
                mqtt.publishChange(c.hasil, c.modal, hitungAktif, ackPesan);
            }

            ackPesan = nullptr;
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
        // Pemulihan bertingkat
        // Dihitung hanya jika kabel terpasang tapi MQTT belum OK.
        // Jika kabel yang putus, restart tidak akan membantu.
        // -------------------------------------------------
        bool bermasalah = linkUp && !mqtt.isConnected();

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
            const unsigned long lama = now - problemSince;

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
            Serial.print(hasIP ? "OK" : "NO");
            Serial.print(" | MQTT=");
            Serial.print(mqtt.isConnected() ? "OK" : "NO");
            Serial.print(" | LAN putus=");
            Serial.print(lanLostCount);
            Serial.print(" | MQTT putus=");
            Serial.print(mqttLostCount);
            Serial.print(" | Heap=");
            Serial.println(ESP.getFreeHeap());
        }

        // -------------------------------------------------
        // Ganti halaman LCD (baris 2 bergantian)
        // -------------------------------------------------
        if (lcdView.advancePageIfDue(now))
        {
            updateLCD();
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
    const bool hasilAwal = digitalRead(SENSOR_HASIL);
    const bool modalAwal = digitalRead(SENSOR_MODAL);

    // Muat counter terakhir dari flash (jika fitur aktif)
    muatCounter();

    // LCD
    lcdView.begin();
    lcdView.showMessage("SANDING BESAR", "STARTING...");
    delay(1500);

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

    // MAC & Client ID
    if (!identity.begin())
    {
        Serial.println("PERINGATAN: MAC perangkat tidak valid, jaringan mungkin gagal.");
    }
    Serial.print("MAC: ");
    Serial.println(identity.macText());
    Serial.print("Client ID: ");
    Serial.println(identity.clientId());

    memcpy(mac, identity.mac(), sizeof(mac));

    delay(500);

    Serial.print("Versi: ");
    Serial.print(firmwareInfo.version());
    Serial.print(" (build ");
    Serial.print(firmwareInfo.build());
    Serial.println(")");

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
    mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
    mqttClient.setKeepAlive(MQTT_KEEPALIVE_S);
    mqttClient.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);

    MqttSettings mqttSettings{};
    snprintf(mqttSettings.clientId, sizeof(mqttSettings.clientId), "%s", identity.clientId());
    mqttSettings.user = deviceConfig.mqttUser;
    mqttSettings.pass = deviceConfig.mqttPass;
    mqttSettings.useLastWill = true;
    mqttSettings.enabled = ConfigManager::isComplete(deviceConfig);
    mqtt.begin(mqttSettings);
    mqtt.onCommand(handleCommand);

    if (!mqttSettings.enabled)
    {
        Serial.println("MQTT dinonaktifkan: mqtt_host belum diisi. Ketik 'help' di Serial Monitor.");
    }

    // LCD awal
    updateLCD();

    // Watchdog harus aktif sebelum task didaftarkan
    aktifkanWatchdog();

    // Task sensor di core 0
    xTaskCreatePinnedToCore(
        sensorTask,
        "SensorTask",
        SENSOR_TASK_STACK,
        NULL,
        SENSOR_TASK_PRIORITY,
        NULL,
        SENSOR_TASK_CORE);

    // Task network di core 1
    xTaskCreatePinnedToCore(
        networkTask,
        "NetworkTask",
        NETWORK_TASK_STACK,
        NULL,
        NETWORK_TASK_PRIORITY,
        NULL,
        NETWORK_TASK_CORE);

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
