#pragma once
#include <stdint.h>

/*
    Slug mesin di laravel harus sama dengan bagian ini
    bisa ditimpa lewat build_flags di platformio.ini untuk keperluan uji
*/
#ifndef TOPIC_BASE
#define TOPIC_BASE "pabrik/hmi_sanding/"
#endif

namespace config
{
    // ---- Identitas mesin ----
    constexpr char namaMesin[] = "HMI SANDING";
    static_assert(sizeof(namaMesin) - 1 <= 12, "namaMesin maksimal 12 karakter agar muat di LCD");
    constexpr char clientIdPrefix[] = "HmiSanding-";
    static_assert(sizeof(clientIdPrefix) - 1 + 12 < 32,
                  "prefix + 12 digit MAC harus muat di Client ID (31 karakter)");

    // ---- LCD ----
    constexpr int LCD_I2C_ADDR = 0x27;
    constexpr int LCD_COLUMN = 20;
    constexpr int LCD_ROW = 4;

    // ---- W5500 ----
    constexpr uint8_t CS_PIN = 16;
    constexpr uint8_t SCK = 18;
    constexpr uint8_t MISO = 23;
    constexpr uint8_t MOSI = 19;

    // ---- Rotary Encoder ----
    constexpr uint8_t PIN_CLK = 32;
    constexpr uint8_t PIN_DT = 35;     // GPIO35 input-only, TIDAK punya pullup-internal: pasang 10k ke 3V3
    constexpr uint8_t PIN_SW = 5;      // Rotary Encoder Button (OK)
    constexpr uint8_t PIN_BTN_PIN = 4; // Tombol fisik tunggal (BACK)

    // ---- Waktu (ms) ----
    constexpr uint16_t MQTT_DEFAULT_PORT = 1883;
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
    static_assert(HEARTBEAT_INTERVAL < 15000UL, "Laravel menganggap OFFLINE jika tidak ada pesan lebih dari 15 detik");

    // ---- OTA -----

    // ---- Subscribe Topic MQTT Sanding Besar ----
    constexpr char *topicMasuk = "pabrik/veneer/jumlah";
    constexpr char *topicKeluar = "pabrik/veneer/jumlah2";
    constexpr char *topicRefresh = "pabrik/veneer/refresh";
    constexpr char *topicPerintah = "pabrik/veneer/perintah";
    constexpr char *topicAck = "pabrik/veneer/ack";
    constexpr char *topicMode = "pabrik/veneer/mode";

    // ---- MQTT HMI ----
    constexpr char *topicHmiStatus = TOPIC_BASE "status";
    constexpr char *topicHmiIP = TOPIC_BASE "ip";
    constexpr char *topicHmiDiag = TOPIC_BASE "diag";
    constexpr char *topicHmiPerintah = TOPIC_BASE "perintah";
    constexpr char *topicHmiOta = TOPIC_BASE "ota";
    constexpr char *topicHmiAktif = TOPIC_BASE "aktif";
}