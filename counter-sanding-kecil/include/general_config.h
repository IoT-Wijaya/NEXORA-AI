#pragma once
#include <stdint.h>

// Nilai yang SAMA untuk semua mesin. Dipasang sesudah berkas mesin
// (lihat sanding_config.h) karena topik memakai TOPIC_BASE milik mesin.

namespace config
{

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
    // Batas waktu pembuktian sehat harus antara 1 dan 10 menit
    constexpr const char *OTA_HOST = "monitoring.wijayaplywoods.com";
    constexpr uint16_t OTA_PORT = 80;
    constexpr uint32_t OTA_OVERALL_TIMEOUT_MS = 600000UL;
    static_assert(OTA_HEALTH_DEADLINE_MS >= 60000UL && OTA_HEALTH_DEADLINE_MS <= 600000UL,
                  "batas waktu pembuktian sehat harus antara 1 dan 10 menit");
    // Task yang tidak melapor dalam batas ini membuat perangkat reset. Harus lebih lama dari
    // pemblokiran terpanjang networkTask (DHCP 8 dtk, sambung MQTT sampai ±20 dtk).
    constexpr uint32_t TASK_WDT_TIMEOUT_S = 30;
    static_assert(TASK_WDT_TIMEOUT_S * 1000UL * 2 <= OTA_HEALTH_DEADLINE_MS,
                  "watchdog harus jauh lebih pendek dari tenggat kesehatan OTA");

    // ---- Topic MQTT (TOPIC_BASE dari berkas mesin) ----
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