#pragma once
#include <stdint.h>

// Slug mesin di Laravel HARUS sama dengan bagian ini ("sanding").
// Bisa ditimpa lewat build_flags untuk keperluan uji.
#ifndef TOPIC_BASE
#define TOPIC_BASE "pabrik/sanding/"
#endif

namespace config
{

    // ---- Identitas mesin ----
    constexpr char namaMesin[] = "SANDINGKCL";
    static_assert(sizeof(namaMesin) - 1 <= 10, "namaMesin maksimal 10 karakter agar muat di LCD");
    constexpr char clientIdPrefix[] = "SandingKecil-";
    static_assert(sizeof(clientIdPrefix) - 1 + 12 < 32,
                  "prefix + 12 digit MAC harus muat di Client ID (31 karakter)");

    // ---- LCD ----
    constexpr uint8_t LCD_ADDRESS = 0x27;
    constexpr uint8_t LCD_COLUMNS = 16;
    constexpr uint8_t LCD_ROWS = 2;

    // ---- ENC28J60 ----
    constexpr uint8_t ENC_CS_PIN = 5;
    constexpr uint8_t ENC_SCK = 18;
    constexpr uint8_t ENC_MISO = 19;
    constexpr uint8_t ENC_MOSI = 23;

    // ---- Pin sensor dan tombol ----
    constexpr uint8_t SENSOR_HASIL = 27;
    constexpr uint8_t SENSOR_MODAL = 32;
    constexpr uint8_t RESET_BUTTON = 0;   // GPIO0 = pin boot, jangan ditahan saat menyalakan
    constexpr bool SENSOR_ACTIVE = false; // aktif LOW

    // ---- Waktu (ms) ----
    constexpr uint16_t MQTT_DEFAULT_PORT = 1883;
    constexpr unsigned long PRESS_STABLE_TIME = 100;
    constexpr unsigned long RELEASE_STABLE_TIME = 400;
    constexpr unsigned long RESET_DEBOUNCE_TIME = 50;
    constexpr unsigned long LCD_PAGE_INTERVAL = 30000UL;
    constexpr unsigned long ETH_CHECK_INTERVAL = 3000UL;
    constexpr unsigned long DHCP_RETRY_INTERVAL = 15000UL;
    constexpr unsigned long MQTT_RECONNECT_INTERVAL = 5000UL;
    constexpr unsigned long HEARTBEAT_INTERVAL = 5000UL;
    constexpr uint16_t MQTT_SOCKET_TIMEOUT_S = 3;
    constexpr bool MQTT_USE_LAST_WILL = false;
    static_assert(HEARTBEAT_INTERVAL < 15000UL, "Laravel menganggap OFFLINE jika tidak ada pesan lebih dari 15 detik");

    // ---- OTA ----
    constexpr uint8_t OTA_MAX_PENDING_BOOTS = 3;               // boot tanpa konfirmasi sebelum rollback
    constexpr uint8_t OTA_MAX_REQUEST_ATTEMPTS = 3;            // percobaan unduh sebelum menyerah
    constexpr uint8_t OTA_MAX_ROLLBACK_ATTEMPTS = 3;           // percobaan rollback sebelum mengaku gagal
    constexpr unsigned long OTA_HEALTH_DEADLINE_MS = 180000UL; // 3 menit
    // Batas waktu pembuktian sehat harus antara 1 dan 10 menit`
    constexpr const char *OTA_HOST = "monitoring.wijayaplywoods.com";
    constexpr uint16_t OTA_PORT = 80;
    constexpr const char *OTA_MANIFEST_PATH = "/fw/manifest.signed";
    constexpr const char *OTA_FIRMWARE_PATH = "/fw/firmware.bin";
    constexpr const char *OTA_DEVICE_TYPE = "sanding-kecil";
    constexpr const char *OTA_BOARD = "nodemcu-32s";
    constexpr uint32_t OTA_OVERALL_TIMEOUT_MS = 600000UL;
    static_assert(OTA_HEALTH_DEADLINE_MS >= 60000UL && OTA_HEALTH_DEADLINE_MS <= 600000UL,
                  "batas waktu pembuktian sehat harus antara 1 dan 10 menit");
    // Task yang tidak melapor dalam batas ini membuat perangkat reset. Harus lebih lama dari
    // pemblokiran terpanjang networkTask (DHCP 8 dtk, sambung MQTT sampai ±20 dtk).
    constexpr uint32_t TASK_WDT_TIMEOUT_S = 30;
    static_assert(TASK_WDT_TIMEOUT_S * 1000UL * 2 <= OTA_HEALTH_DEADLINE_MS,
                  "watchdog harus jauh lebih pendek dari tenggat kesehatan OTA");

    // ---- Topic MQTT ----
    constexpr const char *topicStatus = TOPIC_BASE "status";
    constexpr const char *topicJumlah = TOPIC_BASE "jumlah";
    constexpr const char *topicJumlah2 = TOPIC_BASE "jumlah2";
    constexpr const char *topicIP = TOPIC_BASE "ip";
    constexpr const char *topicPerintah = TOPIC_BASE "perintah";
    constexpr const char *topicAck = TOPIC_BASE "ack";
    constexpr const char *topicVersi = TOPIC_BASE "versi";
    constexpr const char *topicMode = TOPIC_BASE "mode";

    // OTA MQTT TOPIC
    constexpr const char *topicOta = TOPIC_BASE "ota_perintah";
    constexpr const char *topicOtaStatus = TOPIC_BASE "ota_status";

    constexpr unsigned long DHCP_TIMEOUT_MS = 8000;
    constexpr unsigned long DHCP_RESPONSE_TIMEOUT_MS = 2000;
    constexpr unsigned long MQTT_OTA_HOLDOFF_MS = 3000;
    constexpr uint32_t OTA_REPORT_RETRY_MS = 5000;

} // namespace config