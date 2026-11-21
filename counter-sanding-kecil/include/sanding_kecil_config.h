#pragma once
#include <stdint.h>

// Konfigurasi KHUSUS mesin Sanding Kecil (ESP32 + ENC28J60 + LCD 16x2).
// Nilai yang sama untuk semua mesin ada di general_config.h.

// Slug mesin di Laravel HARUS sama dengan bagian ini ("sanding").
// Bisa ditimpa lewat build_flags untuk keperluan uji.
#ifndef TOPIC_BASE
#define TOPIC_BASE "pabrik/sanding/"
#endif

// Pustaka Ethernet yang dipakai modul ini (dibaca eth_driver.h).
// Harus cocok dengan lib_deps env di platformio.ini.
// Folder rilis mesin ini di server: http://<OTA_HOST>/fw/sanding-kecil/
// (nginx menyajikan /srv/firmware/sanding-kecil/). Harus sama dengan slug di tools/ci_rilis.py.
#define OTA_DIR "/fw/sanding-kecil/"

#define ETH_HEADER <EthernetENC.h>

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

    // ---- Teks tampilan ----
    constexpr char judulMesin[] = "SANDING KECIL";            // LCD dan log boot
    constexpr char ringkasanPerangkat[] = "ESP32 + ENC28J60"; // log boot
    static_assert(sizeof(judulMesin) - 1 <= LCD_COLUMNS, "judulMesin harus muat di satu baris LCD");

    // ---- Ethernet (ENC28J60) ----
    constexpr uint8_t ETH_CS_PIN = 5;
    constexpr uint8_t ETH_SCK = 18;
    constexpr uint8_t ETH_MISO = 19;
    constexpr uint8_t ETH_MOSI = 23;

    // ---- Pin sensor dan tombol ----
    constexpr uint8_t SENSOR_HASIL = 27;
    constexpr uint8_t SENSOR_MODAL = 32;
    constexpr uint8_t RESET_BUTTON = 0;   // GPIO0 = pin boot, jangan ditahan saat menyalakan
    constexpr bool SENSOR_ACTIVE = false; // aktif LOW

    // ---- OTA ----
    constexpr const char *OTA_MANIFEST_PATH = OTA_DIR "manifest.signed";
    constexpr const char *OTA_FIRMWARE_PATH = OTA_DIR "firmware.bin";
    constexpr const char *OTA_DEVICE_TYPE = "sanding-kecil";
    constexpr const char *OTA_BOARD = "nodemcu-32s";

} // namespace config