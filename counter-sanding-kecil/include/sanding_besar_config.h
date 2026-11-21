#pragma once
#include <stdint.h>

// Konfigurasi KHUSUS mesin Sanding Besar (ESP32 + W5500 + LCD 20x4).
// Nilai yang sama untuk semua mesin ada di general_config.h.
// BELUM DIBANGUN: semua nilai bertanda ISI harus dicek dari rangkaian asli.

// Slug mesin di Laravel HARUS sama dengan bagian ini.
#ifndef TOPIC_BASE
#define TOPIC_BASE "pabrik/veneer/" // ISI: samakan dengan slug di Laravel
#endif

// Pustaka Ethernet yang dipakai modul ini (dibaca eth_driver.h).
// Harus cocok dengan lib_deps env di platformio.ini.
// Folder rilis mesin ini di server: http://<OTA_HOST>/fw/sanding-besar/
// (nginx menyajikan /srv/firmware/sanding-besar/). Harus sama dengan slug di tools/ci_rilis.py.
#define OTA_DIR "/fw/sanding-besar/"

#define ETH_HEADER <Ethernet.h>

namespace config
{

    // ---- Identitas mesin ----
    constexpr char namaMesin[] = "SANDINGBSR"; // ISI
    static_assert(sizeof(namaMesin) - 1 <= 10, "namaMesin maksimal 10 karakter");
    constexpr char clientIdPrefix[] = "SandingBesar-";
    static_assert(sizeof(clientIdPrefix) - 1 + 12 < 32,
                  "prefix + 12 digit MAC harus muat di Client ID (31 karakter)");

    // ---- LCD 20x4 ----
    constexpr uint8_t LCD_ADDRESS = 0x27; // ISI: cek dengan I2C scanner
    constexpr uint8_t LCD_COLUMNS = 20;
    constexpr uint8_t LCD_ROWS = 4;

    // ---- Teks tampilan ----
    constexpr char judulMesin[] = "SANDING BESAR";
    constexpr char ringkasanPerangkat[] = "ESP32 + W5500";
    static_assert(sizeof(judulMesin) - 1 <= LCD_COLUMNS, "judulMesin harus muat di satu baris LCD");

    // ---- Ethernet (W5500) ----
    constexpr uint8_t ETH_CS_PIN = 5; // ISI
    constexpr uint8_t ETH_SCK = 18;   // ISI
    constexpr uint8_t ETH_MISO = 19;  // ISI
    constexpr uint8_t ETH_MOSI = 23;  // ISI

    // ---- Pin sensor dan tombol ----
    constexpr uint8_t SENSOR_HASIL = 27; // ISI
    constexpr uint8_t SENSOR_MODAL = 32; // ISI
    constexpr uint8_t RESET_BUTTON = 0;  // ISI
    constexpr bool SENSOR_ACTIVE = false;

    // ---- OTA ----
    constexpr const char *OTA_MANIFEST_PATH = OTA_DIR "manifest.signed";
    constexpr const char *OTA_FIRMWARE_PATH = OTA_DIR "firmware.bin";
    constexpr const char *OTA_DEVICE_TYPE = "sanding-besar";
    constexpr const char *OTA_BOARD = "nodemcu-32s";

} // namespace config