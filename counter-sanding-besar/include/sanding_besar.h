#pragma once
#include <stdint.h>

// Topic dashboard sama dengan kode lama (dashboard tidak perlu diubah).
// Bisa ditimpa lewat build_flags untuk keperluan uji.
#ifndef TOPIC_BASE
#define TOPIC_BASE "pabrik/veneer/"
#endif

namespace config
{

    // ---- Identitas mesin ----
    constexpr char namaMesin[] = "SANDING BESAR";
    constexpr char clientIdPrefix[] = "SandingBesar-";
    // Client ID = prefix + 8 digit hex dari eFuse MAC
    static_assert(sizeof(clientIdPrefix) - 1 + 8 < 40,
                  "prefix + 8 digit harus muat di buffer clientID (40 karakter)");

    // ---- LCD ----
    constexpr uint8_t LCD_ADDRESS = 0x27;
    constexpr uint8_t LCD_COLUMNS = 20;
    constexpr uint8_t LCD_ROWS = 4;
    static_assert(sizeof(namaMesin) - 1 <= LCD_COLUMNS, "namaMesin harus muat di satu baris LCD");

    // ---- W5500 ----
    constexpr uint8_t W5500_CS_PIN = 5;
    constexpr uint8_t W5500_SCK_PIN = 18;
    constexpr uint8_t W5500_MISO_PIN = 19;
    constexpr uint8_t W5500_MOSI_PIN = 23;
    // Pin RST modul W5500 (opsional). Isi nomor GPIO (misalnya 4) jika kabel RST
    // disambung, atau biarkan -1 jika tidak (hanya init ulang lewat kode).
    constexpr int W5500_RST_PIN = -1;

    // ---- Pin sensor dan tombol ----
    constexpr uint8_t SENSOR_HASIL = 27;  // Sensor Hasil Sanding Besar
    constexpr uint8_t SENSOR_MODAL = 32;  // Sensor Modal Sanding Otomatis
    constexpr uint8_t RESET_BUTTON = 0;   // GPIO0 = tombol BOOT, jangan ditahan saat menyalakan
    constexpr bool SENSOR_ACTIVE = false; // aktif LOW

    // ---- Waktu filter sensor (ms) ----
    constexpr unsigned long PRESS_STABLE_TIME = 100;
    constexpr unsigned long RELEASE_STABLE_TIME = 400;
    constexpr unsigned long RESET_DEBOUNCE_TIME = 50;

    // ---- Waktu jaringan (ms) ----
    constexpr unsigned long ETH_CHECK_INTERVAL = 3000UL;
    constexpr unsigned long DHCP_RETRY_INTERVAL = 15000UL;
    constexpr unsigned long DHCP_TIMEOUT_MS = 8000;
    constexpr unsigned long DHCP_RESPONSE_TIMEOUT_MS = 2000;

    // Backoff reconnect MQTT: mulai 2 detik, dobel tiap gagal, maks 60 detik
    constexpr unsigned long MQTT_RETRY_MIN = 2000UL;
    constexpr unsigned long MQTT_RETRY_MAX = 60000UL;
    static_assert(MQTT_RETRY_MIN <= MQTT_RETRY_MAX, "MQTT_RETRY_MIN tidak boleh lebih besar dari MQTT_RETRY_MAX");

    // Harus JAUH di bawah 15 detik: dashboard menganggap OFFLINE
    // jika tidak ada pesan lebih dari 15 detik.
    constexpr unsigned long HEARTBEAT_INTERVAL = 5000UL;
    static_assert(HEARTBEAT_INTERVAL < 15000UL, "Dashboard menganggap OFFLINE jika tidak ada pesan lebih dari 15 detik");

    // Kabel dianggap putus setelah N pengecekan berturut-turut (N x ETH_CHECK_INTERVAL)
    constexpr uint8_t LINK_OFF_CONFIRM = 2;

    // ---- Pemulihan bertingkat (dihitung sejak kabel ON tapi MQTT belum OK) ----
    constexpr unsigned long RESET_ETH_AFTER = 5UL * 60UL * 1000UL;
    constexpr unsigned long RESTART_AFTER = 10UL * 60UL * 1000UL;
    static_assert(RESET_ETH_AFTER < RESTART_AFTER, "Reset chip harus terjadi SEBELUM restart total");

    // ---- Laporan ----
    constexpr unsigned long REPORT_INTERVAL = 60000UL; // ke Serial Monitor
    constexpr unsigned long DIAG_INTERVAL = 60000UL;   // ke dashboard (topik diag)

    // ---- Watchdog: task tidak "lapor hidup" selama ini -> ESP32 restart ----
    constexpr uint32_t WDT_TIMEOUT_S = 60;

    // ---- Penyimpanan counter ke flash ----
    // true  = hitungan dilanjutkan setelah restart / mati listrik
    // false = hitungan selalu mulai dari 0 setelah restart
    constexpr bool SIMPAN_COUNTER_FLASH = true;
    constexpr unsigned long SAVE_INTERVAL = 10000UL; // simpan maks tiap 10 detik

    // ---- Pengaturan MQTT ----
    constexpr uint16_t MQTT_BUFFER_SIZE = 512;    // default 256 byte tidak cukup untuk JSON diag
    constexpr uint16_t MQTT_KEEPALIVE_S = 15;     // menjaga koneksi tetap "terlihat hidup" di NAT router
    constexpr uint16_t MQTT_SOCKET_TIMEOUT_S = 3; // agar koneksi gagal tidak menahan task lama
    static_assert(MQTT_KEEPALIVE_S < 15 + 1, "Keepalive terlalu besar untuk batas OFFLINE dashboard");

    // Isi topik status
    constexpr const char *STATUS_ONLINE = "1";
    constexpr const char *STATUS_OFFLINE = "0";

    // ---- Topic MQTT ----
    constexpr const char *topicStatus = TOPIC_BASE "status";
    constexpr const char *topicJumlah = TOPIC_BASE "jumlah";   // Hasil
    constexpr const char *topicJumlah2 = TOPIC_BASE "jumlah2"; // Modal
    constexpr const char *topicIP = TOPIC_BASE "ip";
    constexpr const char *topicPerintah = TOPIC_BASE "perintah";
    constexpr const char *topicDiag = TOPIC_BASE "diag"; // laporan diagnosa lengkap (JSON)
    constexpr const char *topicAck = TOPIC_BASE "ack";
    constexpr const char *topicMode = TOPIC_BASE "mode";

    // ---- FreeRTOS task ----
    constexpr uint32_t SENSOR_TASK_STACK = 4096;
    constexpr uint32_t NETWORK_TASK_STACK = 12288;
    constexpr uint8_t SENSOR_TASK_PRIORITY = 2;
    constexpr uint8_t NETWORK_TASK_PRIORITY = 1;
    constexpr uint8_t SENSOR_TASK_CORE = 0;
    constexpr uint8_t NETWORK_TASK_CORE = 1;

} // namespace config