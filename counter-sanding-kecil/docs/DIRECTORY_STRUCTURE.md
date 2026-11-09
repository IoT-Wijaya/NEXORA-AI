# Struktur Direktori - SANDING KECIL

## Ikhtisar Proyek

SANDING KECIL adalah counter industri berbasis ESP32 yang menghitung dua jenis aktivitas: "hasil" (output) dan "modal" (input). Proyek ini menggunakan PlatformIO dengan Arduino framework, berjalan pada ESP32 NodeMCU-32S board, dan mengimplementasikan sistem pencacahan terdistribusi menggunakan FreeRTOS.

## Tata Letak Direktori Utama

```
counter-sanding-kecil/
├── .pio/                    # Direktori PlatformIO (diabaikan)
│   ├── build/              # Direktori build firmware
│   ├── libdeps/            # Library dependencies
│   └── ...
│
├── .vscode/                 # Konfigurasi VS Code
│   ├── .browse.c_cpp.db*   # Indeks penelusuran C/C++
│   ├── c_cpp_properties.json
│   └── launch.json         # Konfigurasi debugging GDB
│
├── docs/                    # Dokumentasi proyek (ini)
│   ├── README.md           # Dokumentasi ini dalam Bahasa Indonesia
│   └── DIRECTORY_STRUCTURE.md  # Struktur direktori
│
├── include/                 # Direktori header
│   ├── build_info.h        # Info build firmware
│   ├── ota_public_key.h    # Kunci publik OTA
│   ├── README             # README singkat include/
│   └── sanding_config.h    # Konfigurasi utama (pin, timing, topik)
│
├── lib/                     # Library aplikasi khusus
│   ├── command_parser/     # Parser perintah MQTT
│   ├── console_shell/      # Shell perintah serial
│   ├── counter_store/      # Penyimpanan counter thread-safe
│   ├── device_config/      # Manajemen konfigurasi perangkat
│   ├── device_identity/    # Identitas perangkat
│   ├── firmware_info/      # Info versi firmware
│   ├── http_client/        # Klien HTTP untuk verifikasi OTA
│   ├── lcd_view/           # Logika tampilan LCD
│   ├── mqtt_service/       # Layanan MQTT dengan callback
│   ├── net_link/           # Stack jaringan (Ethernet, MQTT)
│   ├── ota_core/           # Logika inti OTA
│   └── sensor_filter/      # Filter debounce sensor
│
├── src/                     # Direktori sumber utama
│   ├── main.cpp            # File sumber utama
│   ├── enc_driver.h        # Driver ENC28J60 Ethernet
│   ├── i2c_lcd_display.h   # Driver LCD I2C
│   ├── esp_mac_source.h    # Sumber MAC ESP32
│   └── ...                # File sumber lainnya
│
├── test/                    # Direktori test unit
│   ├── test_counter_store/      # Test untuk counter store
│   ├── test_mqtt_service/       # Test client MQTT
│   ├── test_sensor_filter/      # Test filtering sensor
│   └── ...                     # Test unit lainnya
│
├── tools/                   # Utility skript
│   ├── version.py          # Skript manajemen versi
│   └── ...                 # Utility lainnya
│
├── .gitignore               # Aturan gitignore
├── manifest.json            # Metadata firmware
├── ota_publik.pem            # Kunci publik OTA
├── ota_rahasia.pem            # Kunci pribadi OTA (RAHASIA)
└── platformio.ini           # Konfigurasi PlatformIO
```

## Penjelasan Direktori

### Root Directory
- **counter-sanding-kecil/**: Direktori root proyek

### .pio/ (Diabaikan)
- **build/**: Direktori sementara build firmware, berisi firmware yang dikompilasi dan file-file biner
- **libdeps/**: Dependencies library yang diunduh oleh PlatformIO untuk environment native dan target
- **files lainnya**: Direktori PlatformIO internal, tidak perlu di-commit

### .vscode/ (Diabaikan)
- **c_cpp_properties.json**: Konfigurasi properti IntelliSense
- **launch.json**: Konfigurasi debugging untuk GDB
- **files lainnya**: Konfigurasi VS Code, tidak perlu di-commit

### docs/ (Dokumentasi)
- **README.md**: Dokumentasi lengkap proyek dalam Bahasa Indonesia
- **DIRECTORY_STRUCTURE.md**: Dokumentasi struktur direktori (ini)
- **files lainnya**: File dokumentasi tambahan

### include/ (Header)
- **build_info.h**: Info build firmware (versi, timestamp)
- **ota_public_key.h**: Kunci publik untuk verifikasi OTA
- **README**: README singkat untuk direktori include/
- **sanding_config.h**: Konfigurasi utama (pin hardware, timing, topik MQTT)

### lib/ (Library Aplikasi Khusus)
Library aplikasi khusus proyek, bukan library eksternal:

- **command_parser/**: Parser perintah MQTT, mengimplementasikan parsing command MQTT
- **console_shell/**: Implementasi shell perintah serial untuk antarmuka UART
- **counter_store/**: Implementasi penyimpanan counter thread-safe dengan mutex
- **device_config/**: Manajemen konfigurasi perangkat dengan validasi
- **device_identity/**: Generator identitas perangkat (MAC, Client ID)
- **firmware_info/**: Manajer info versi firmware dan build
- **http_client/**: Klien HTTP untuk verifikasi OTA
- **lcd_view/**: Logika tampilan LCD dan manajemen halaman
- **mqtt_service/**: Implementasi layanan MQTT lengkap dengan callback
- **net_link/**: Stack jaringan terintegrasi (Ethernet + MQTT)
- **ota_core/**: Logika inti OTA (state machine, blob storage)
- **sensor_filter/**: Filter debounce untuk sensor fisik

### src/ (Sumber Utama)
- **main.cpp**: File sumber utama, setup tugas FreeRTOS dan loop utama
- **enc_driver.h**: Driver ENC28J60 Ethernet controller
- **i2c_lcd_display.h**: Driver untuk LCD display I2C 16x2
- **esp_mac_source.h**: Source MAC address ESP32
- **nvs_blob_store.h**: Penyimpanan blob non-volatile untuk OTA
- **nvs_config_store.h**: Penyimpanan konfigurasi non-volatile
- **esp_spin_lock.h**: Implementasi mutex spin lock untuk thread safety
- **partition_log.h**: Log partisi flash untuk debugging
- **files lainnya**: File sumber lainnya

### test/ (Testing)
- **test_counter_store/**: Test unit untuk thread-safe counter storage
- **test_mqtt_service/**: Test client dan service MQTT
- **test_sensor_filter/**: Test filtering sensor hardware debounce
- **test_***: Test unit untuk setiap komponen

### tools/ (Utility)
- **version.py**: Skript Python untuk manajemen versi firmware

## File Kunci Penting

### platformio.ini
```ini
; PlatformIO Project Configuration File
[platformio]
default_envs = sanding_kecil

[env:sanding_kecil]
platform = espressif32
board = nodemcu-32s
framework = arduino
monitor_speed = 115200
build_src_flags = -Wall -Wextra
build_flags = -Iinclude
lib_deps = knolleary/PubSubClient@^2.8, marcoschwartz/LiquidCrystal_I2C@^1.1.4
```

### sanding_config.h (Konfigurasi Utama)
```cpp
namespace config {
    constexpr char namaMesin[] = "SANDINGKCL";
    constexpr uint8_t LCD_ADDRESS = 0x27;
    constexpr uint8_t SENSOR_HASIL = 27;
    constexpr uint8_t SENSOR_MODAL = 32;
    constexpr unsigned long PRESS_STABLE_TIME = 100;
    constexpr unsigned long RELEASE_STABLE_TIME = 400;
    // ... lebih banyak konfigurasi
}
```

## Pola Penggunaan

### Library Implementasi
Library di `lib/` mengikuti pola standar:
- **Nama direktori**: Nama library (misal: `counter_store`)
- **File sumber**: `lib/counter_store/*.cpp/.h`
- **Header publik**: `include/` (dijelaskan di atas)

### File Test
Implementasi test unit:
- **Direktori**: `test/test_counter_store/`
- **File**: `test/test_counter_store/test.cpp`
- **Framework**: PlatformIO Unity

## Diagram Dependensi

```
src/
├── main.cpp
│   ├── include/sanding_config.h
│   ├── lib/command_parser/
│   ├── lib/counter_store/
│   ├── lib/mqtt_service/
│   ├── lib/console_shell/
│   ├── lib/ota_core/
│   └── ...
│
include/
├── sanding_config.h
└── ...

lib/
├── command_parser/     -> include/command_parser/
├── counter_store/      -> include/counter_store/
├── mqtt_service/       -> include/mqtt_service/
└── ...
```

## Catatan Pengembangan

### Direktori yang Diabaikan
- ***.pio/**: Direktori PlatformIO internal, diignore dalam .gitignore
- ***.vscode/**: Konfigurasi VS Code, diignore dalam .gitignore

### File Sensitif
- **ota_rahasia.pem**: Kunci pribadi OTA (RAHASIA), diignore dalam .gitignore

### Environment Build
- **platformio.ini**: Konfigurasi PlatformIO, commit dalam repository
- **CLAUDE.md**: Panduan Claude Code, commit dalam repository

## Ikhtisar

Proyek ini mengadopsi struktur yang jelas dan modular:
- **src/**: Implementasi aplikasi utama
- **include/**: API publik
- **lib/**: Library aplikasi khusus
- **test/**: Suite test unit
- **docs/**: Dokumentasi proyek

Struktur ini memudahkan pengembangan, testing, dan pemeliharaan kode, sambil mempertahankan pemisahan yang jelas antara implementasi dan API.