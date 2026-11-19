# Struktur Direktori - SANDING BESAR

## Ikhtisar Proyek

SANDING BESAR adalah counter industri berbasis ESP32 yang menghitung dua jenis aktivitas: "hasil" (output) dan "modal" (input). Proyek ini menggunakan PlatformIO dengan Arduino framework, berjalan pada ESP32 NodeMCU-32S board dengan modul Ethernet W5500 dan LCD I2C 20x4, dan mengimplementasikan sistem pencacakan menggunakan FreeRTOS dengan dua inti:

- **Core 0**: Sensor task yang membaca input fisik dan memperbarui counter
- **Core 1**: Network task yang menangani komunikasi MQTT, tampilan LCD, dan koordinasi simpan counter

Struktur ini meniru `counter-sanding-kecil` (standar emas) dengan penyesuaian pada modul jaringan (W5500, bukan ENC28J60) dan ukuran LCD (20x4, bukan 16x2).

## Pohon Lengkap Direktori dan File

```
counter-sanding-besar/
├── .gitignore                # Aturan file yang diabaikan Git (.pio, .vscode, build_info.h, secrets.h)
├── CLAUDE.md                 # Panduan Claude Code untuk pengembang AI
├── platformio.ini            # Konfigurasi PlatformIO: board nodemcu-32s, framework arduino, library dependencies
├── partitions.csv            # Tabel partisi flash ESP32 (kosong: pakai default bawaan Arduino ESP32)
│
├── docs/                     # Dokumentasi proyek
│   ├── README.md             # Dokumentasi lengkap proyek dalam Bahasa Indonesia
│   └── DIRECTORY_STRUCTURE.md  # Dokumentasi struktur direktori ini
│
├── include/                  # Header konfigurasi global yang dibaca di seluruh proyek
│   ├── README                # Penjelasan singkat tujuan direktori include/
│   ├── build_info.h          # Dihasilkan otomatis oleh tools/version.py saat build (FW_VERSION, FW_BUILD). JANGAN diedit atau di-commit.
│   ├── sanding_config.h       # Konfigurasi utama: pin W5500, LCD 20x4, timing, topik MQTT (namespace config)
│   ├── secrets.example.h     # Contoh kunci rahasia (MAC address W5500). Aman di-commit.
│   └── secrets.h             # MAC address unik perangkat (RAHASIA, diabaikan Git)
│
├── lib/                      # Library aplikasi khusus (bukan library eksternal). Setiap direktori adalah satu modul terpisah.
│   ├── README                # Penjelasan singkat tujuan direktori lib/
│   ├── command_parser/       # Parser perintah MQTT (START, STOP, RESET_S1, RESET_S2, RESET_ALL, RESTART_DEVICE)
│   │   ├── command_parser.cpp
│   │   └── command_parser.h
│   ├── counter_store/        # Penyimpanan counter thread-safe (hasil + modal) dengan mutex EspSpinLock + pemulihan dari flash
│   │   ├── counter_store.cpp
│   │   ├── counter_store.h
│   │   └── ilock.h           # Antarmuka penguncian bagian kritis (ILock)
│   ├── console_shell/        # Shell perintah Serial (help, show, set, unset, clear, reboot) untuk konfigurasi lewat Serial Monitor
│   │   ├── console_shell.cpp
│   │   └── console_shell.h
│   ├── device_config/        # Manajemen konfigurasi runtime perangkat (MQTT host/user/pass, nama mesin) dengan validasi dan NVS
│   │   ├── device_config.cpp
│   │   ├── device_config.h
│   │   └── iconfig_store.h   # Antarmuka penyimpanan kunci-nilai (IConfigStore)
│   ├── device_identity/      # Identitas perangkat: MAC address dan Client ID MQTT
│   │   ├── device_identity.cpp
│   │   ├── device_identity.h
│   │   └── imac_source.h     # Antarmuka sumber MAC (IMacSource)
│   ├── firmware_info/        # Identitas versi firmware (versi, build, konversi ke JSON)
│   │   ├── firmware_info.cpp
│   │   └── firmware_info.h
│   ├── lcd_view/             # Logika tampilan LCD 20x4 dan manajemen halaman (4 baris: counter, mesin/jaringan, IP, mode)
│   │   ├── ilcd_display.h    # Antarmuka layar karakter (ILcdDisplay)
│   │   ├── lcd_view.cpp
│   │   └── lcd_view.h
│   ├── mqtt_service/         # Layanan MQTT lengkap: koneksi, backoff reconnect, heartbeat, publish, perintah masuk
│   │   ├── imqtt_client.h    # Antarmuka klien MQTT (IMqttClient)
│   │   ├── mqtt_service.cpp
│   │   └── mqtt_service.h
│   ├── net_link/             # Stack jaringan terintegrasi (W5500 + DHCP + monitoring status)
│   │   ├── inet_driver.h     # Antarmuka perangkat keras jaringan (INetDriver) + struct Ip4 + formatIp
│   │   ├── net_monitor.cpp
│   │   └── net_monitor.h
│   └── sensor_filter/        # Filter debounce untuk sensor fisik (hasil + modal)
│       ├── sensor_filter.cpp
│       └── sensor_filter.h
│
├── src/                      # File sumber utama yang dijalankan di board ESP32
│   ├── esp_spin_lock.h       # Implementasi mutex spin lock ESP32 (implementasi ILock)
│   ├── esp_task_watchdog.h   # Pembungkus watchdog tugas ESP32
│   ├── fixed_mac_source.h    # Sumber MAC address dari include/secrets.h (implementasi IMacSource)
│   ├── i2c_lcd_display.h     # Driver LCD I2C 20x4 via LiquidCrystal_I2C (implementasi ILcdDisplay)
│   ├── main.cpp              # Titik masuk firmware: setup semua komponen, membuat dua FreeRTOS task (Core 0 sensor, Core 1 network), loop konsol Serial
│   ├── nvs_config_store.h    # Penyimpanan konfigurasi di NVS/Preferences (implementasi IConfigStore)
│   ├── pubsub_mqtt_client.h  # Pembungkus PubSubClient sebagai IMqttClient (klien MQTT aktual pada perangkat)
│   └── w5500_driver.h        # Driver W5500 Ethernet controller via library Arduino Ethernet (implementasi INetDriver)
│
├── test/                     # Suite test unit (PlatformIO Unity Test Framework, berjalan di environment native/PC)
│   ├── README                # Penjelasan singkat tujuan direktori test/
│   ├── test_command_parser/  # Test parser perintah MQTT
│   │   └── test_command_parser.cpp
│   ├── test_counter_store/   # Test penyimpanan counter thread-safe
│   │   └── test_counter_store.cpp
│   ├── test_lcd_view/        # Test logika tampilan LCD 20x4
│   │   └── test_lcd_view.cpp
│   ├── test_sensor_filter/   # Test filter debounce sensor
│   │   └── test_sensor_filter.cpp
│   └── test_smoke/           # Test smoke (sanity check alat uji)
│       └── test_smoke.cpp
│
├── tools/                    # Utility skript untuk development
│   └── version.py            # Skrip PlatformIO pre-build: menghasilkan include/build_info.h dari git describe / variabel CI
│
└── .vscode/                  # Konfigurasi VS Code (dihasilkan PlatformIO, biasanya tidak di-commit)
    ├── c_cpp_properties.json # Konfigurasi properti IntelliSense C/C++ (include path)
    ├── extensions.json       # Rekomendasi ekstensi VS Code (platformio.platformio-ide)
    └── launch.json           # Konfigurasi debugging GDB (PlatformIO debug)
```

## Direktori yang Diabaikan (tidak ada dalam daftar pohon di atas)

- **`.pio/`**: Direktori internal PlatformIO (build artifacts, `libdeps/` dengan library eksternal yang diunduh). Diabaikan oleh `.gitignore` dan tidak perlu di-commit.

## Pola Arsitektur

- **Antarmuka abstrak (`I...`)**: Setiap modul di `lib/` yang menyentuh hardware didefinisikan terlebih dahulu sebagai antarmuka abstrak (misalnya `IMqttClient`, `INetDriver`, `ILcdDisplay`, `IMacSource`, `IConfigStore`, `ILock`). Implementasi aktual ada di `src/` untuk perangkat, dan implementasi palsu (fake/mock) digunakan dalam test di `test/`.
- **Thread safety**: Akses data yang dibagi antar task (counter) dilindungi oleh `EspSpinLock` melalui antarmuka `ILock`.
- **Dua task FreeRTOS**: `sensorTask` di Core 0 (prioritas 2, stack 4KB) dan `networkTask` di Core 1 (prioritas 1, stack 12KB). Komunikasi antar task melalui `CounterStore` yang dilindungi mutex.
- **Konfigurasi compile-time**: Semua konstanta hardware dan timing di `include/sanding_config.h` dalam `namespace config` dengan `constexpr`.

## File Kunci di Root

| File | Fungsi |
|------|--------|
| `platformio.ini` | Konfigurasi build PlatformIO: environment `sanding_besar` (board nodemcu-32s, framework arduino, monitor 115200 baud) dan `native` (untuk test unit). Library eksternal: Ethernet, LiquidCrystal_I2C, PubSubClient. Skrip pre-build: `tools/version.py`. |
| `partitions.csv` | Tabel partisi flash ESP32. Kosong, sehingga partisi default bawaan Arduino ESP32 digunakan. |
| `CLAUDE.md` | Instruksi kerja untuk Claude Code saat bekerja di repositori ini (arsitektur, perintah build/test, pedoman pengembangan). |
| `.gitignore` | Aturan Git: mengabaikan `.pio`, sebagian `.vscode`, `build_info.h`, dan `secrets.h`. |

## Cara Build dan Upload

| Perintah | Keterangan |
|----------|------------|
| `platformio run` | Build firmware saja. |
| `platformio run --target upload` | Build dan upload ke perangkat ESP32. |
| `platformio test` | Jalankan semua test unit di environment native (PC). |
| `platformio device monitor` | Buka serial monitor (115200 baud). |

## Cara Menjalankan Test

Test dijalankan di environment native (PC) dengan PlatformIO Unity:

```bash
platformio test                    # Jalankan semua test
platformio test -f test_counter_store  # Jalankan satu komponen saja
platformio test --list             # Daftar semua test yang tersedia
```

## Pedoman Pengembangan

- Gunakan C++11 (dan fitur yang kompatibel dengan environment native dan Arduino ESP32).
- Semua header menggunakan `#pragma once`.
- Konstanta konfigurasi di `include/sanding_config.h` dalam `namespace config`, menggunakan `constexpr`.
- Akses data yang dibagi antar task dilindungi mutex `EspSpinLock` melalui antarmuka `ILock`.
- Hindari tipe `String` Arduino; gunakan `char array` dan `snprintf`.
- Dokumentasikan setiap API publik dengan komentar gaya Doxygen.
- Tulis test unit untuk setiap komponen kritis sebelum atau samaan dengan implementasi.
- File sensitif (`secrets.h`, `build_info.h`) tidak boleh di-commit.
