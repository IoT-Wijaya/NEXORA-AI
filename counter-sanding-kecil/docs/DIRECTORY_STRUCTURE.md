# Struktur Direktori - SANDING KECIL

## Ikhtisar Proyek

SANDING KECIL adalah counter industri berbasis ESP32 yang menghitung dua jenis aktivitas: "hasil" (output) dan "modal" (input). Proyek ini menggunakan PlatformIO dengan Arduino framework, berjalan pada ESP32 NodeMCU-32S board, dan mengimplementasikan sistem pencacakan terdistribusi menggunakan FreeRTOS dengan dua inti:

- **Core 0**: Sensor task yang membaca input fisik dan memperbarui counter
- **Core 1**: Network task yang menangani komunikasi MQTT, tampilan LCD, dan koordinasi OTA

## Pohon Lengkap Direktori dan File

```
counter-sanding-kecil/
├── .gitignore                # Aturan file yang diabaikan Git (.pio, .vscode, *.pem, build_info.h, secrets.h)
├── CLAUDE.md                 # Panduan Claude Code untuk pengembang AI (instruksi kerja repo ini)
├── manifest.json             # Metadata rilis firmware (versi, build, ukuran, sha256) -- dikomitkan
├── ota_publik.pem            # Kunci publik Ed25519 untuk verifikasi tanda tangan OTA (aman di-commit)
├── ota_rahasia.pem           # Kunci pribadi Ed25519 untuk menandatangani manifest OTA (RAHASIA, diabaikan Git)
├── partitions.csv            # Tabel partisi flash ESP32 (kosong: pakai default bawaan Arduino ESP32)
├── platformio.ini            # Konfigurasi PlatformIO: board nodemcu-32s, framework arduino, library dependencies
│
├── docs/                     # Dokumentasi proyek
│   ├── README.md             # Dokumentasi lengkap proyek dalam Bahasa Indonesia
│   └── DIRECTORY_STRUCTURE.md  # Dokumentasi struktur direktori ini
│
├── include/                  # Header konfigurasi global yang dibaca di seluruh proyek
│   ├── README                # Penjelasan singkat tujuan direktori include/
│   ├── build_info.h          # Dihasilkan otomatis oleh tools/version.py saat build (FW_VERSION, FW_BUILD). JANGAN diedit atau di-commit.
│   ├── ota_public_key.h      # Kunci publik penanda tangan rilis (dihasilkan oleh tools/make_public_key.py). Aman di-commit.
│   └── sanding_config.h      # Konfigurasi utama: pin hardware, timing, topik MQTT, pengaturan OTA (namespace config)
│
├── lib/                      # Library aplikasi khusus (bukan library eksternal). Setiap direktori adalah satu modul terpisah.
│   ├── README                # Penjelasan singkat tujuan direktori lib/
│   ├── command_parser/       # Parser perintah MQTT (START, STOP, RESET_S1, RESET_S2, RESET_ALL, RESTART_DEVICE)
│   │   ├── command_parser.cpp
│   │   └── command_parser.h
│   ├── console_shell/        # Shell perintah Serial (help, show, set, unset, clear, reboot) untuk konfigurasi lewat Serial Monitor
│   │   ├── console_shell.cpp
│   │   └── console_shell.h
│   ├── counter_store/        # Penyimpanan counter thread-safe (hasil + modal) dengan mutex EspSpinLock
│   │   ├── counter_store.cpp
│   │   ├── counter_store.h
│   │   └── ilock.h           # Antarmuka penguncian bagian kritis (ILock)
│   ├── device_config/        # Manajemen konfigurasi runtime perangkat (MQTT host/user/pass, nama mesin) dengan validasi dan NVS
│   │   ├── device_config.cpp
│   │   ├── device_config.h
│   │   └── iconfig_store.h   # Antarmuka penyimpanan kunci-nilai (IConfigStore)
│   ├── device_identity/      # Identitas perangkat: MAC address dan Client ID MQTT yang diturunkan dari chip
│   │   ├── device_identity.cpp
│   │   ├── device_identity.h
│   │   └── imac_source.h     # Antarmuka sumber MAC (IMacSource)
│   ├── firmware_info/        # Identitas versi firmware (versi, build, konversi ke JSON)
│   │   ├── firmware_info.cpp
│   │   └── firmware_info.h
│   ├── http_client/          # Klien HTTP untuk unduhan dan verifikasi firmware OTA (header parser, downloader, verifier)
│   │   ├── firmware_downloader.cpp
│   │   ├── firmware_downloader.h
│   │   ├── http_response.cpp
│   │   └── http_response.h
│   ├── lcd_view/             # Logika tampilan LCD dan manajemen halaman (baris counter bergantian nama mesin / status jaringan)
│   │   ├── ilcd_display.h    # Antarmuka layar karakter (ILcdDisplay)
│   │   ├── lcd_view.cpp
│   │   └── lcd_view.h
│   ├── mqtt_service/         # Layanan MQTT lengkap: koneksi, heartbeat, publish, perintah masuk
│   │   ├── imqtt_client.h    # Antarmuka klien MQTT (IMqttClient)
│   │   ├── mqtt_service.cpp
│   │   └── mqtt_service.h
│   ├── net_link/             # Stack jaringan terintegrasi (Ethernet + DHCP + monitoring status)
│   │   ├── inet_driver.h     # Antarmuka perangkat keras jaringan (INetDriver) + struct Ip4 + formatIp
│   │   ├── net_monitor.cpp
│   │   └── net_monitor.h
│   ├── ota_core/             # Logika inti OTA (state machine, penyimpanan catatan, verifikasi tanda tangan, SHA-256)
│   │   ├── iblob_store.h     # Antarmuka penyimpanan byte mentah per kunci (IBlobStore)
│   │   ├── blob_ota_store.cpp
│   │   ├── blob_ota_store.h  # IOtaStore di atas penyimpanan byte (catatan OTA dikodekan CRC, dibaca ulang untuk verifikasi)
│   │   ├── firmware_verifier.cpp
│   │   ├── firmware_verifier.h  # Verifikasi biner streaming terhadap manifest: ukuran + SHA-256
│   │   ├── manifest.cpp
│   │   ├── manifest.h        # Parser manifest JSON rilis firmware (schema, device_type, board, version, build, size, sha256)
│   │   ├── monocypher/       # Library kriptografi Ed25519 portable (monocypher)
│   │   │   ├── monocypher.c
│   │   │   ├── monocypher.h
│   │   │   ├── monocypher-ed25519.c
│   │   │   └── monocypher-ed25519.h
│   │   ├── ota_record.h      # OtaRecord, OtaState, OtaResult, boot action codes
│   │   ├── ota_record_codec.cpp
│   │   ├── ota_record_codec.h  # Format tetap 16 byte catatan OTA dengan CRC-16/CCITT-FALSE
│   │   ├── ota_state_machine.cpp
│   │   ├── ota_state_machine.h # Keputusan OTA: kapan mengunduh, mengonfirmasi, rollback (IOtaStore, OtaConfig, HealthReport)
│   │   ├── sha256.cpp
│   │   ├── sha256.h          # Implementasi SHA-256 portable (FIPS 180-4), hasil identik di komputer dan ESP32
│   │   ├── signature.cpp
│   │   ├── signature.h       # Verifikasi tanda tangan Ed25519 (RFC 8032)
│   │   ├── signed_manifest.cpp
│   │   ├── signed_manifest.h # Parser amplop manifest bertanda tangan ([64 byte tanda tangan][JSON manifest])
│   │   ├── update_policy.cpp
│   │   └── update_policy.h   # Kebijakan terima/tolak rilis (device type, board, ukuran slot, downgrade, min_from_build)
│   └── sensor_filter/        # Filter debounce untuk sensor fisik (hasil + modal)
│       ├── sensor_filter.cpp
│       └── sensor_filter.h
│
├── src/                      # File sumber utama yang dijalankan di board ESP32
│   ├── enc_driver.h          # Driver ENC28J60 Ethernet controller (implementasi INetDriver)
│   ├── esp_mac_source.h      # Sumber MAC address ESP32 dari eFuse (ESP_MAC_ETH)
│   ├── esp_spin_lock.h       # Implementasi mutex spin lock ESP32 (portENTER_CRITICAL / portEXIT_CRITICAL)
│   ├── i2c_lcd_display.h     # Driver LCD I2C 16x2 via LiquidCrystal_I2C (implementasi ILcdDisplay)
│   ├── main.cpp              # Titik masuk firmware: setup semua komponen, membuat dua FreeRTOS task (Core 0 sensor, Core 1 network), loop konsol Serial
│   ├── nvs_blob_store.h      # Penyimpanan blob OTA di NVS (implementasi IBlobStore)
│   ├── nvs_config_store.h    # Penyimpanan konfigurasi di NVS/Preferences (implementasi IConfigStore)
│   ├── partition_log.h       # Pencatat partisi firmware untuk debugging OTA (slot berjalan, slot OTA berikut, data OTA)
│   └── pubsub_mqtt_client.h  # Pembungkus PubSubClient sebagai IMqttClient (klien MQTT aktual pada perangkat)
│
├── test/                     # Suite test unit (PlatformIO Unity Test Framework, berjalan di environment native/PC)
│   ├── README                # Penjelasan singkat tujuan direktori test/
│   ├── test_blob_ota_store/      # Test untuk penyimpanan catatan OTA (blob_ota_store)
│   │   └── test_blob_ota_store.cpp
│   ├── test_command_parser/      # Test parser perintah MQTT
│   │   └── test_command_parser.cpp
│   ├── test_config/              # Test manajemen konfigurasi perangkat (device_config)
│   │   └── test_config.cpp
│   ├── test_counter_store/       # Test penyimpanan counter thread-safe
│   │   └── test_counter_store.cpp
│   ├── test_console_shell/       # Test shell perintah Serial
│   │   └── test_console_shell.cpp
│   ├── test_device_config/       # Test konfigurasi device (validasi host, port, user, pass, nama mesin)
│   │   └── test_device_config.cpp
│   ├── test_device_identity/     # Test identitas perangkat (MAC, Client ID)
│   │   └── test_device_identity.cpp
│   ├── test_firmware_downloader/ # Test unduhan firmware HTTP (firmware_downloader)
│   │   └── test_firmware_downloader.cpp
│   ├── test_firmware_info/       # Test identitas versi firmware
│   │   └── test_firmware_info.cpp
│   ├── test_firmware_verifier/   # Test verifikasi biner firmware (SHA-256, ukuran)
│   │   └── test_firmware_verifier.cpp
│   ├── test_http_response/       # Test parser header HTTP/1.1
│   │   └── test_http_response.cpp
│   ├── test_lcd_view/            # Test logika tampilan LCD
│   │   └── test_lcd_view.cpp
│   ├── test_manifest/            # Test parser manifest JSON
│   │   └── test_manifest.cpp
│   ├── test_mqtt_service/        # Test layanan MQTT (koneksi, publish, perintah masuk)
│   │   └── test_mqtt_service.cpp
│   ├── test_net_monitor/         # Test monitoring jaringan Ethernet/DHCP
│   │   └── test_net_monitor.cpp
│   ├── test_ota_record_codec/    # Test codec catatan OTA (encode/decode 16 byte + CRC)
│   │   └── test_ota_record_codec.cpp
│   ├── test_ota_state_machine/   # Test state machine OTA (boot, pending, watch health, rollback)
│   │   └── test_ota_state_machine.cpp
│   ├── test_sensor_filter/       # Test filter debounce sensor (tes_sensor_filter.cpp)
│   │   └── tes_sensor_filter.cpp
│   ├── test_sha256/              # Test implementasi SHA-256
│   │   └── test_sha256.cpp
│   ├── test_signed_manifest/     # Test parser amplop manifest bertanda tangan
│   │   └── test_signed_manifest.cpp
│   ├── test_signature/           # Test verifikasi tanda tangan Ed25519
│   │   └── test_signature.cpp
│   ├── test_smoke/               # Test smoke (sanity check alat uji)
│   │   └── test_smoke.cpp
│   └── test_update_policy/       # Test kebijakan terima/tolak rilis OTA
│       └── test_update_policy.cpp
│
├── tools/                    # Utility skript untuk development dan rilis
│   ├── __pycache__/          # Bytecode Python (hasil otomatis, tidak perlu di-commit)
│   │   ├── make_manifest.cpython-311.pyc
│   │   └── test_make_manifest.cpython-311.pyc
│   ├── make_manifest.py      # Membuat manifest.json rilis firmware (validasi field sama dengan ManifestParser di firmware)
│   ├── make_public_key.py    # Mengonversi kunci publik Ed25519 PEM menjadi include/ota_public_key.h
│   ├── sign_manifest.sh      # Menandatangani manifest.json dengan kunci pribadi Ed25519 -> manifest.signed ([tanda tangan][JSON])
│   ├── test_make_manifest.py # Test unit untuk make_manifest.py (Python unittest)
│   └── version.py            # Skrip PlatformIO pre-build: menghasilkan include/build_info.h dari git describe / variabel CI
│
└── .vscode/                  # Konfigurasi VS Code (dihasilkan PlatformIO, biasanya tidak di-commit)
    ├── c_cpp_properties.json # Konfigurasi properti IntelliSense C/C++ (include path)
    ├── extensions.json       # Rekomendasi ekstensi VS Code (platformio.platformio-ide)
    └── launch.json           # Konfigurasi debugging GDB (PlatformIO debug)
```

## Direktori yang Diabaikan (tidak ada dalam daftar pohon di atas)

- **`.pio/`**: Direktori internal PlatformIO (build artifacts, `libdeps/` dengan library eksternal yang diunduh seperti PubSubClient, LiquidCrystal_I2C, EthernetENC, ArduinoJson). Diabaikan oleh `.gitignore` dan tidak perlu di-commit.
- **`lib/__pycache__/`** dan file `*.pyc`: Bytecode Python hasil otomatis, tidak perlu di-commit.

## Penjelasan Singkat Tiap Direktori Utama

| Direktori | Fungsi |
|-----------|--------|
| `include/` | Header konfigurasi global yang dibaca seluruh proyek (`namespace config`), termasuk pengaturan OTA dan kunci publik. |
| `lib/` | Library aplikasi khusus, masing-masing dalam direktori sendiri dengan antarmuka abstrak agar bisa diuji di komputer (native test) tanpa hardware ESP32. |
| `src/` | Implementasi aplikasi utama yang berjalan pada ESP32, termasuk `main.cpp` dan driver hardware. |
| `test/` | Suite test unit per komponen menggunakan PlatformIO Unity, berjalan di environment native (PC), bukan pada board. |
| `tools/` | Skrip bantu development dan rilis: versi firmware, kunci publik, pembuatan dan penandatanganan manifest OTA. |
| `docs/` | Dokumentasi proyek (README dan struktur direktori). |
| `.vscode/` | Konfigurasi editor VS Code untuk PlatformIO (IntelliSense dan debugging). |

## File Kunci di Root

| File | Fungsi |
|------|--------|
| `platformio.ini` | Konfigurasi build PlatformIO: environment `sanding_kecil` (board nodemcu-32s, framework arduino, monitor 115200 baud) dan `native` (untuk test unit). Library eksternal: PubSubClient, LiquidCrystal_I2C, EthernetENC, ArduinoJson. Skrip pre-build: `tools/version.py`. |
| `manifest.json` | Metadata rilis firmware saat ini (schema, device_type `sanding-kecil`, board `nodemcu-32s`, versi, build, ukuran, sha256). Digunakan OTA untuk verifikasi rilis. |
| `ota_publik.pem` | Kunci publik Ed25519 untuk memverifikasi tanda tangan manifest OTA. Aman di-commit. |
| `ota_rahasia.pem` | Kunci pribadi Ed25519 untuk menandatangani manifest. RAHASIA — tidak boleh di-commit (sudah diabaikan `.gitignore`). |
| `partitions.csv` | Tabel partisi flash ESP32. Saat ini kosong (menggunakan partisi default bawaan Arduino ESP32 yang mendukung OTA). |
| `CLAUDE.md` | Instruksi kerja untuk Claude Code saat bekerja di repositori ini (arsitektur, perintah build/test, pedoman pengembangan). |
| `.gitignore` | Aturan Git: mengabaikan `.pio`, `.vscode` (kecuali sebagian), `build_info.h`, `secrets.h`, dan semua `*.pem`. |

## Pola Arsitektur

- **Antarmuka abstrak (`I...`)**: Setiap modul di `lib/` yang menyentuh hardware didefinisikan terlebih dahulu sebagai antarmuka abstrak (misalnya `IMqttClient`, `INetDriver`, `ILcdDisplay`, `IMacSource`, `IConfigStore`, `IBlobStore`, `ILock`). Implementasi aktual ada di `src/` untuk perangkat, dan implementasi palsu (fake/mock) digunakan dalam test di `test/`.
- **Thread safety**: Akses data yang dibagi antar task (counter, catatan OTA) dilindungi oleh `EspSpinLock` melalui antarmuka `ILock`.
- **Dua task FreeRTOS**: `sensorTask` di Core 0 (prioritas 2, stack 4KB) dan `networkTask` di Core 1 (prioritas 1, stack 8KB). Komunikasi antar task melalui `CounterStore` dan `OtaStateMachine` yang dilindungi mutex.
- **Konfigurasi compile-time**: Semua konstanta hardware dan timing di `include/sanding_config.h` dalam `namespace config` dengan `constexpr`.
