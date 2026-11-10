# SANDING KECIL — Dokumentasi Lengkap Proyek 📊

Selamat datang di dokumentasi **SANDING KECIL**, sebuah mesin pencacah otomatis (industrial counter) berbasis **ESP32 NodeMCU-32S** yang dihitung dua jenis aktivitas secara bersamaan: **"hasil" (output)** dan **"modal" (input)**. Bayangkan seperti mesin kasir digital yang bisa mencatat dua jenis transaksi sekaligus.

Proyek ini dibangun dengan **PlatformIO** (Arduino framework) dan merupakan bagian dari ekosistem **NEXORA** untuk otomatisasi industri. Dokumentasi ini ditulis dalam Bahasa Indonesia.

> **Catatan:** Dokumentasi teknis untuk Claude Code ada di `CLAUDE.md` di direktori root. Struktur lengkap direktori ada di `docs/DIRECTORY_STRUCTURE.md`.

---

## 1. Apa Itu SANDING KECIL?

SANDING KECIL adalah perangkat counter industri yang:

- Menghitung **hasil** (produksi keluar) dan **modal** (bahan masuk) melalui sensor fisik.
- Mengirim data counter dan status ke **HMI** (Human Machine Interface) melalui **MQTT**.
- Menerima perintah dari HMI melalui MQTT: `START`, `STOP`, `RESET_HASIL`, `RESET_MODAL`, `RESET_ALL`, `RESTART`.
- Menampilkan counter dan status jaringan pada **LCD I2C 16x2**.
- Memiliki **konsol perintah Serial** untuk mengatur konfigurasi perangkat (MQTT host, port, user, password, nama mesin).
- Mendukung **Over-The-Air (OTA) update** firmware yang aman dengan tanda tangan Ed25519 dan verifikasi SHA-256.
- Menyimpan konfigurasi secara permanen di **NVS** (Non-Volatile Storage) ESP32.

## 2. Arsitektur Sistem

Firmware dibangun di atas **FreeRTOS** dengan dua task yang berjalan pada inti berbeda ESP32:

### Task Sensor (Core 0) — prioritas 2, stack 4KB
Menangani semua hal yang terkait sensor fisik:

- Membaca sensor hasil (GPIO 27) dan sensor modal (GPIO 32) dengan filter debounce.
- Memantau tombol reset lokal (GPIO 0).
- Memperbarui counter hanya saat pencacahan aktif (`hitungAktif = true`).
- Tidak menyentuh MQTT atau LCD.

### Task Network (Core 1) — prioritas 1, stack 8KB
Satu-satunya pemilik MQTT dan LCD:

- Mengelola koneksi Ethernet (ENC28J60) dan DHCP.
- Memelihara koneksi MQTT ke broker, mengirim heartbeat, dan menerima perintah.
- Memperbarui tampilan LCD (bergantian menampilkan counter dan status jaringan).
- Mengkoordinasikan operasi OTA.

Komunikasi antar task melalui **CounterStore** dan **OtaStateMachine** yang dilindungi oleh mutex `EspSpinLock`.

### Alur Boot (`src/main.cpp`, fungsi `setup()`)

1. Serial monitor dihidupkan (baud 115200).
2. Konfigurasi dimuat dari NVS (`NvsConfigStore` / `ConfigManager`).
3. MAC address dibaca dari eFuse chip dan Client ID MQTT disusun (`DeviceIdentity`).
4. Pin sensor dan tombol diinisialisasi; filter debounce dimulai.
5. LCD diinisialisasi dan menampilkan pesan pembuka.
6. SPI dan ENC28J60 diinisialisasi; DHCP dimulai (jika gagal, akan dicoba ulang otomatis oleh `NetMonitor`).
7. MQTT dikonfigurasi dengan server, kredensial, dan last will.
8. Catatan OTA dimuat dan `OtaStateMachine` menentukan aksi boot (normal, run OTA, watch health, atau rollback).
9. Dua FreeRTOS task dibuat: `sensorTask` (Core 0) dan `networkTask` (Core 1).
10. `loop()` hanya menangani input konsol Serial.

---

## 3. Alur Kerja Pencacahan

```
Sensor fisik → SensorFilter (debounce) → CounterStore (mutex) → MQTT publish → LCD
```

- Sensor difilter terlebih dahulu agar tidak ada double-count akibat getaran kontak (bounce).
- Setiap detik (delay 10 ms), `sensorTask` membaca kedua sensor. Jika ada tekanan stabil, counter bertambah 1 dan ditandai "dirty".
- `networkTask` mendeteksi counter yang berubah (`takeDirty()`) dan segera mempublish ke MQTT, lalu memperbarui LCD.
- Jika pencacahan belum aktif (belum menerima `START` dari HMI), pembacaan sensor tetap berjalan agar status sinkron, tetapi hasil hitungannya dibuang.

---

## 4. Perintah MQTT

Topik dasar: `pabrik/sanding/` (dapat ditimpa via `TOPIC_BASE` di build).

| Topik | Fungsi | Retained |
|-------|--------|----------|
| `pabrik/sanding/perintah` | Menerima perintah dari HMI | — |
| `pabrik/sanding/status` | Status online/offline perangkat | Ya |
| `pabrik/sanding/jumlah` | Nilai counter hasil | Ya |
| `pabrik/sanding/jumlah2` | Nilai counter modal | Ya |
| `pabrik/sanding/mode` | `ON` (menghitung) / `OFF` (berhenti) | Ya |
| `pabrik/sanding/ip` | Alamat IP perangkat | Ya |
| `pabrik/sanding/ack` | Balasan perintah (START_OK, STOP_OK, RESET_OK) | Tidak |
| `pabrik/sanding/versi` | Versi firmware dalam JSON | Ya |

Perintah yang diterima di topik `perintah`:

| Perintah | Aksi |
|----------|------|
| `START` | Nol-kan counter, aktifkan pencacahan, kirim `START_OK` |
| `STOP` | Matikan pencacahan, nol-kan counter, kirim `STOP_OK` |
| `RESET_S1` / `RESET_HASIL` | Nol-kan counter hasil |
| `RESET_S2` / `RESET_MODAL` | Nol-kan counter modal |
| `RESET_ALL` / `RESET` | Nol-kan kedua counter |
| `RESTART_DEVICE` | Restart ESP32 |

---

## 5. Konsol Perintah Serial

Diakses melalui Serial Monitor (115200 baud) di fungsi `loop()`. Perintah yang tersedia:

| Perintah | Keterangan |
|----------|------------|
| `help` | Tampilkan daftar perintah |
| `show` | Tampilkan konfigurasi tersimpan (sandi disamarkan sebagai `****`) |
| `set <kunci> <nilai>` | Simpan satu nilai konfigurasi |
| `unset <kunci>` | Kembalikan satu nilai ke bawaan |
| `clear yes` | Hapus semua konfigurasi |
| `reboot` | Restart perangkat |

Kunci konfigurasi: `mqtt_host`, `mqtt_port`, `mqtt_user`, `mqtt_pass`, `mach_name`. Perubahan berlaku setelah `reboot`.

---

## 6. OTA (Over-The-Air) Update

Sistem OTA dirancang aman dan andal:

- **Tanda tangan Ed25519**: Setiap rilis firmware dikemas dalam amplop `[64 byte tanda tangan][JSON manifest]` dan divalidasi dengan kunci publik `include/ota_public_key.h`. JSON dari sumber tidak terpercaya disentuh parser hanya setelah tanda tangan diverifikasi.
- **Verifikasi SHA-256**: Ukuran dan hash biner divalidasi selama unduhan streaming (`FirmwareVerifier`).
- **Kebijakan update (`UpdatePolicy`)**: Memeriksa jenis perangkat, board, ukuran slot, versi saat ini, dan aturan downgrade sebelum menerima rilis.
- **State machine OTA (`OtaStateMachine`)**: Mengelola siklus hidup update — request, pending (menunggu konfirmasi kesehatan), watch health, dan rollback otomatis jika firmware baru tidak sehat (timeout, crash loop, atau gagal boot).
- **Catatan OTA disimpan di NVS** dengan format 16 byte terenkode CRC (`OtaRecordCodec`) dan dibaca ulang setelah setiap tulisan untuk memastikan integritas.

### Alat pengembang OTA di `tools/`

| Skrip | Fungsi |
|-------|--------|
| `make_manifest.py` | Membuat `manifest.json` untuk rilis firmware (validasi field sama dengan parser di firmware). |
| `sign_manifest.sh` | Menandatangani manifest dengan kunci pribadi Ed25519 → `manifest.signed`. |
| `make_public_key.py` | Mengonversi kunci publik PEM menjadi `include/ota_public_key.h`. |
| `version.py` | Skrip pre-build PlatformIO yang menghasilkan `include/build_info.h` (versi dan build dari `git describe` / variabel CI). |

---

## 7. Rincian Folder dan File

Berikut penjelasan lengkap setiap folder dan file dalam proyek ini, mengacu pada struktur di `docs/DIRECTORY_STRUCTURE.md`.

### Root (direktori utama proyek)

| File / Folder | Fungsi |
|---------------|--------|
| `platformio.ini` | Konfigurasi PlatformIO. Menentukan environment `sanding_kecil` (board `nodemcu-32s`, framework `arduino`, monitor 115200 baud) dan environment `native` untuk test unit. Mencantumkan library eksternal: PubSubClient, LiquidCrystal_I2C, EthernetENC, ArduinoJson. Skrip pre-build `tools/version.py` dijalankan sebelum kompilasi. |
| `manifest.json` | Metadata rilis firmware saat ini: schema, device_type `sanding-kecil`, board `nodemcu-32s`, versi, build, ukuran, dan sha256 biner. Digunakan dalam proses OTA. |
| `ota_publik.pem` | Kunci publik Ed25519 untuk memverifikasi tanda tangan manifest OTA. Aman untuk di-commit. |
| `ota_rahasia.pem` | Kunci pribadi Ed25519 untuk menandatangani manifest OTA. **RAHASIA** — tidak boleh di-commit (sudah diabaikan oleh `.gitignore`). |
| `partitions.csv` | Tabel partisi flash ESP32. Saat ini kosong, sehingga partisi default bawaan Arduino ESP32 (yang mendukung OTA) digunakan. |
| `CLAUDE.md` | Instruksi kerja untuk Claude Code saat bekerja di repositori ini: arsitektur, perintah build/test, dan pedoman pengembangan. |
| `.gitignore` | Aturan Git: mengabaikan `.pio`, sebagian `.vscode`, `build_info.h`, `secrets.h`, dan semua file `*.pem`. |

### `docs/` — Dokumentasi proyek

- `README.md` — Dokumentasi lengkap proyek ini (file yang sedang Anda baca).
- `DIRECTORY_STRUCTURE.md` — Pohon lengkap direktori dan penjelasan setiap file.

### `include/` — Header konfigurasi global

Header yang dibaca di seluruh proyek. Semua konstanta menggunakan `constexpr` dalam `namespace config`.

| File | Fungsi |
|------|--------|
| `sanding_config.h` | **Konfigurasi utama.** Identitas mesin (`namaMesin = "SANDINGKCL"`), pengaturan LCD (alamat I2C `0x27`, 16x2), pin ENC28J60 (SPI dan chip select), pin sensor hasil/modal dan tombol reset, waktu debounce dan interval jaringan, topik MQTT (`status`, `jumlah`, `jumlah2`, `ip`, `perintah`, `ack`, `versi`, `mode`), dan pengaturan OTA. |
| `build_info.h` | **Dihasilkan otomatis** oleh `tools/version.py` saat build. Berisi `FW_VERSION` dan `FW_BUILD`. Jangan diedit atau di-commit. |
| `ota_public_key.h` | Kunci publik penanda tangan rilis Ed25519. Dihasilkan oleh `tools/make_public_key.py`. Aman di-commit. |
| `README` | Penjelasan singkat tujuan direktori `include/`. |

### `lib/` — Library aplikasi khusus

Berisi modul-modul yang dibuat sendiri untuk proyek ini (bukan library eksternal yang diunduh). Setiap modul dalam direktori terpisah. Modul yang menyentuh hardware menggunakan antarmuka abstrak (`I...`) sehingga dapat diuji di komputer (native test) dengan implementasi palsu.

| Folder | File | Fungsi |
|--------|------|--------|
| `command_parser/` | `command_parser.h`, `command_parser.cpp` | Parser payload MQTT menjadi perintah (`Command`: `Unknown`, `Start`, `Stop`, `ResetHasil`, `ResetModal`, `ResetAll`, `RestartDevice`). Menerima teks seperti `START`, `STOP`, `RESET_S1`, `RESET_S2`, `RESET_ALL`, `RESTART_DEVICE`. |
| `console_shell/` | `console_shell.h`, `console_shell.cpp` | Shell perintah Serial untuk konfigurasi lewat Serial Monitor (`help`, `show`, `set`, `unset`, `clear`, `reboot`). Tidak membaca Serial langsung — karakter disuapkan melalui `feed()`, sehingga logika dapat diuji di komputer. |
| `counter_store/` | `counter_store.h`, `counter_store.cpp`, `ilock.h` | Penyimpanan counter thread-safe (`hasil` dan `modal`) dengan metode `incrementHasil()`, `incrementModal()`, `reset()`, `snapshot()`, dan `takeDirty()`. Dilindungi oleh `ILock` (EspSpinLock pada perangkat). |
| `device_config/` | `device_config.h`, `device_config.cpp`, `iconfig_store.h` | Manajemen konfigurasi runtime perangkat (MQTT host, port, user, password, nama mesin) dengan validasi input dan penyimpanan di NVS. `ConfigManager` memuat nilai saat boot dan hanya membaca setelahnya. |
| `device_identity/` | `device_identity.h`, `device_identity.cpp`, `imac_source.h` | Identitas perangkat: MAC address dan Client ID MQTT yang diturunkan dari chip. MAC valid jika bukan semua nol dan bukan multicast. Client ID = prefix + 12 digit heksadesimal MAC. |
| `firmware_info/` | `firmware_info.h`, `firmware_info.cpp` | Identitas versi firmware (teks versi yang dibersihkan agar aman dalam JSON, dan nomor build). Dapat menghasilkan JSON `{"version":"...","build":N}`. |
| `http_client/` | `http_response.h/.cpp`, `firmware_downloader.h/.cpp` | Klien HTTP untuk unduhan dan verifikasi firmware OTA. `HeaderParser` membaca header HTTP/1.1 bertahap (bisa satu per satu byte). `FirmwareDownloader` mengunduh firmware ke `IByteSink` dengan opsiRange, retry, dan deteksi macet. |
| `lcd_view/` | `lcd_view.h`, `lcd_view.cpp`, `ilcd_display.h` | Logika tampilan LCD dan manajemen halaman. Baris 1 selalu menampilkan counter (`M:... H:... *` atau `-`). Baris 2 bergantian antara nama mesin dan status jaringan (`LAN:OK/NO MQTT:OK/NO`) setiap 30 detik. |
| `mqtt_service/` | `mqtt_service.h`, `mqtt_service.cpp`, `imqtt_client.h` | Layanan MQTT lengkap: koneksi, reconnect, heartbeat (`ONLINE` setiap 5 detik), publish counter, versi, dan IP, serta menangani perintah masuk dari HMI melalui callback. |
| `net_link/` | `net_monitor.h/.cpp`, `inet_driver.h` | Stack jaringan terintegrasi. `INetDriver` adalah antarmuka perangkat keras jaringan (diimplementasikan `EncDriver` di `src/`). `NetMonitor` memantau status Ethernet: online = kabel terpasang **dan** sudah punya IP, dengan pencobaan ulang DHCP berkala. |
| `ota_core/` | (lihat detail di bawah) | Logika inti OTA: state machine, penyimpanan catatan, verifikasi tanda tangan, SHA-256, dan kebijakan update. |
| `sensor_filter/` | `sensor_filter.h`, `sensor_filter.cpp` | Filter debounce untuk satu sensor digital. Mengembalikan `true` satu kali setiap ada hitungan baru setelah tekanan stabil (100 ms) dan lepas stabil (400 ms). Logika murni (tidak memanggil `digitalRead` atau `millis` langsung), sehingga dapat diuji di komputer. |

#### Detail `lib/ota_core/`

| File | Fungsi |
|------|--------|
| `ota_state_machine.h/.cpp` | Keputusan OTA: kapan mengunduh, mengonfirmasi kesehatan, dan rollback. Keluaran berupa `BootAction` (`Normal`, `RunOta`, `WatchHealth`, `Rollback`). Catatan disimpan melalui `IOtaStore`. |
| `ota_record.h` | Definisi `OtaRecord`, `OtaState` (`Idle`, `Requested`, `Pending`, `RollingBack`), `OtaResult`, dan kode aksi boot. |
| `ota_record_codec.h/.cpp` | Format tetap 16 byte catatan OTA dengan CRC-16/CCITT-FALSE untuk integritas penyimpanan di NVS. |
| `blob_ota_store.h/.cpp` | Implementasi `IOtaStore` di atas `IBlobStore` (NVS). Catatan dikodekan dengan CRC dan dibaca ulang setelah tulisan untuk verifikasi. |
| `iblob_store.h` | Antarmuka penyimpanan byte mentah per kunci (`IBlobStore`). Diimplementasikan `NvsBlobStore` di `src/`. |
| `manifest.h/.cpp` | Parser manifest JSON rilis firmware (`schema`, `device_type`, `board`, `version`, `build`, `size`, `sha256`, `min_from_build`, `allow_downgrade`). |
| `update_policy.h/.cpp` | Kebijakan terima/tolak rilis: memeriksa jenis perangkat, board, ukuran slot, versi saat ini, dan aturan downgrade. Hasilkan `PolicyDecision` (`Accept`, `AlreadyInstalled`, `WrongDeviceType`, `WrongBoard`, `InconsistentManifest`, `DowngradeNotAllowed`, `TooOldToUpgrade`, `TooLarge`). |
| `sha256.h/.cpp` | Implementasi SHA-256 portable (FIPS 180-4), dihitung bertahap. Hasil identik di komputer dan ESP32 (tidak bergantung mbedtls). |
| `firmware_verifier.h/.cpp` | Memeriksa biner streaming terhadap manifest: ukuran dan SHA-256. Hasil: `Ok`, `SizeMismatch`, `TooMuchData`, atau `HashMismatch`. |
| `signature.h/.cpp` | Verifikasi tanda tangan Ed25519 (RFC 8032) melalui Monocypher. |
| `signed_manifest.h/.cpp` | Parser amplop manifest bertanda tangan: `[64 byte tanda tangan Ed25519][JSON manifest]`. Verifikasi tanda tangan dilakukan **sebelum** JSON disentuh parser. |
| `monocypher/` | Library kriptografi Ed25519 portable: `monocypher.c/.h` dan `monocypher-ed25519.c/.h`. |

### `src/` — File sumber utama (berjalan di ESP32)

| File | Fungsi |
|------|--------|
| `main.cpp` | **Titik masuk firmware.** Inisialisasi semua komponen, membuat dua FreeRTOS task (`sensorTask` di Core 0, `networkTask` di Core 1), menangani konsol Serial di `loop()`. Berisi `handleCommand()` untuk mengeksekusi perintah MQTT dan `publishKeadaanAwal()` untuk publish status saat MQTT terhubung. |
| `enc_driver.h` | Driver ENC28J60 Ethernet controller. Implementasi `INetDriver`: inisialisasi SPI, cek link, DHCP, dan alamat IP. |
| `i2c_lcd_display.h` | Driver LCD I2C 16x2 melalui `LiquidCrystal_I2C`. Implementasi `ILcdDisplay`: inisialisasi layar, backlight, dan menulis teks ke baris. |
| `esp_mac_source.h` | Sumber MAC address ESP32 dari eFuse chip (`ESP_MAC_ETH`), unik per chip dan dicadangkan untuk Ethernet. Implementasi `IMacSource`. |
| `esp_spin_lock.h` | Implementasi mutex spin lock ESP32 (`portENTER_CRITICAL` / `portEXIT_CRITICAL`). Implementasi `ILock` untuk thread safety antar inti. |
| `nvs_config_store.h` | Penyimpanan konfigurasi di NVS/Preferences ESP32. Implementasi `IConfigStore` (namespace `"sanding"`). |
| `nvs_blob_store.h` | Penyimpanan blob OTA di NVS. Implementasi `IBlobStore` (namespace `"ota"`). |
| `partition_log.h` | Pencatat partisi firmware untuk debugging OTA: mencetak slot berjalan, slot OTA berikut, dan data OTA. |
| `pubsub_mqtt_client.h` | Pembungkus `PubSubClient` sebagai `IMqttClient`. Klien MQTT aktual yang berjalan pada perangkat. |

### `test/` — Suite test unit

Semua test menggunakan **PlatformIO Unity Test Framework** dan berjalan di environment **native (PC)**, bukan pada board ESP32. Setiap komponen memiliki direktori test sendiri dengan satu file `.cpp`.

| Folder | File | Fungsi |
|--------|------|--------|
| `test_smoke/` | `test_smoke.cpp` | Test smoke / sanity check alat uji. |
| `test_counter_store/` | `test_counter_store.cpp` | Test counter store: awal nol, increment, reset, dirty flag, thread safety (kedalaman kunci tidak bersarang). |
| `test_sensor_filter/` | `tes_sensor_filter.cpp` | Test filter debounce sensor: stabilisasi 100/400 ms, gangguan singkat diabaikan, tekanan lama hanya dihitung sekali, overflow 32-bit aman. |
| `test_mqtt_service/` | `test_mqtt_service.cpp` | Test layanan MQTT dengan klien palsu: koneksi, subscribe, publish, heartbeat, last will, perintah masuk, publish change dengan/ tanpa ack. |
| `test_device_config/` | `test_device_config.cpp` | Test konfigurasi device: validasi host, port, user, password, nama mesin; load/simpan/hapus; nilai rusak diganti bawaan. |
| `test_command_parser/` | `test_command_parser.cpp` | Test parser perintah MQTT. |
| `test_net_monitor/` | `test_net_monitor.cpp` | Test monitoring jaringan Ethernet/DHCP. |
| `test_lcd_view/` | `test_lcd_view.cpp` | Test logika tampilan LCD. |
| `test_device_identity/` | `test_device_identity.cpp` | Test identitas perangkat (MAC, Client ID). |
| `test_console_shell/` | `test_console_shell.cpp` | Test shell perintah Serial. |
| `test_firmware_info/` | `test_firmware_info.cpp` | Test identitas versi firmware. |
| `test_config/` | `test_config.cpp` | Test manajemen konfigurasi. |
| `test_manifest/` | `test_manifest.cpp` | Test parser manifest JSON. |
| `test_update_policy/` | `test_update_policy.cpp` | Test kebijakan terima/tolak rilis OTA. |
| `test_ota_state_machine/` | `test_ota_state_machine.cpp` | Test state machine OTA (boot, pending, watch health, rollback). |
| `test_ota_record_codec/` | `test_ota_record_codec.cpp` | Test codec catatan OTA (encode/decode 16 byte + CRC). |
| `test_blob_ota_store/` | `test_blob_ota_store.cpp` | Test penyimpanan catatan OTA di blob. |
| `test_sha256/` | `test_sha256.cpp` | Test implementasi SHA-256. |
| `test_firmware_verifier/` | `test_firmware_verifier.cpp` | Test verifikasi biner firmware (SHA-256, ukuran). |
| `test_signature/` | `test_signature.cpp` | Test verifikasi tanda tangan Ed25519. |
| `test_signed_manifest/` | `test_signed_manifest.cpp` | Test parser amplop manifest bertanda tangan. |
| `test_http_response/` | `test_http_response.cpp` | Test parser header HTTP/1.1. |
| `test_firmware_downloader/` | `test_firmware_downloader.cpp` | Test unduhan firmware HTTP. |
| `README` | — | Penjelasan singkat tujuan direktori `test/`. |

### `tools/` — Utility skript untuk development dan rilis

| File | Fungsi |
|------|--------|
| `version.py` | Skrip pre-build PlatformIO. Menghasilkan `include/build_info.h` dari `git describe --tags --always --dirty` atau variabel CI (`FW_VERSION`, `FW_BUILD`). Hanya menulis jika isi berubah untuk menghindari kompilasi ulang sia-sia. |
| `make_public_key.py` | Mengonversi kunci publik Ed25519 PEM menjadi `include/ota_public_key.h` (array byte C++). Hanya butuh Python, tidak butuh OpenSSL. |
| `make_manifest.py` | Membuat `manifest.json` untuk rilis firmware. Validasi field sama dengan `ManifestParser` di firmware, sehingga kesalahan tertangkap di CI bukan di lapangan. |
| `sign_manifest.sh` | Menandatangani `manifest.json` dengan kunci pribadi Ed25519 melalui OpenSSL → `manifest.signed` (`[64 byte tanda tangan][JSON]`). Di CI, kunci rahasia ditulis dari GitHub Secrets ke file sementara lalu dihapus. |
| `test_make_manifest.py` | Test unit Python (unittest) untuk `make_manifest.py`. |
| `__pycache__/` | Bytecode Python hasil otomatis. Tidak perlu di-commit. |

### `.vscode/` — Konfigurasi VS Code

Dihasilkan oleh PlatformIO, biasanya tidak perlu di-commit:

| File | Fungsi |
|------|--------|
| `c_cpp_properties.json` | Konfigurasi properti IntelliSense C/C++ (include path untuk semua library dan framework ESP32). |
| `extensions.json` | Rekomendasi ekstensi VS Code (`platformio.platformio-ide`). |
| `launch.json` | Konfigurasi debugging GDB untuk PlatformIO (tiga konfigurasi debug). |

### `.pio/` — Direktori internal PlatformIO (diabaikan Git)

Berisi build artifacts dan library eksternal yang diunduh (PubSubClient, LiquidCrystal_I2C, EthernetENC, ArduinoJson, Unity). Diabaikan oleh `.gitignore` dan tidak perlu di-commit.

---

## 8. Cara Build dan Upload

Persyaratan: PlatformIO terinstal (via `pip install platformio` atau ekstensi VS Code PlatformIO).

| Perintah | Keterangan |
|----------|------------|
| `platformio run` | Build firmware saja. |
| `platformio run --target upload` | Build dan upload ke perangkat ESP32. |
| `platformio upload` | Hanya upload (asumsi sudah build). |
| `platformio run --target clean` | Bersihkan hasil build. |
| `platformio device monitor` | Buka serial monitor (115200 baud). |

Environment `native` digunakan untuk menjalankan test unit di PC.

---

## 9. Cara Menjalankan Test

Test dijalankan di environment native (PC) dengan PlatformIO Unity:

```bash
platformio test                    # Jalankan semua test
platformio test -f test_counter_store  # Jalankan satu komponen saja
platformio test --list             # Daftar semua test yang tersedia
```

Untuk test individual, masuk ke direktori test masing-masing.

---

## 10. Panduan Menambah Fitur

### Menambah Sensor Baru

1. Tambah definisi pin di `include/sanding_config.h`.
2. Tambah inisialisasi pin di `setup()` dalam `src/main.cpp`.
3. Tambah logika filtering di `sensorTask()` dalam `src/main.cpp`.
4. Tambah test di `test/test_sensor_filter/`.

### Menambah Perintah MQTT Baru

1. Tambah enum `Command` baru di `lib/command_parser/command_parser.h`.
2. Tambah entri tabel parsing di `lib/command_parser/command_parser.cpp`.
3. Tambah case handling di `handleCommand()` dalam `src/main.cpp`.
4. Perbarui konsol shell jika diperlukan.
5. Tambah test di `test/test_mqtt_service/` atau direktori test yang relevan.

### Menambah Library Baru di `lib/`

1. Buat direktori baru di `lib/<nama>/`.
2. Buat antarmuka abstrak (`I...`) untuk bagian yang menyentuh hardware.
3. Buat file `.h` dan `.cpp` implementasi.
4. Include direktori library sudah otomatis terbaca oleh PlatformIO Library Dependency Finder.
5. Tambah test di `test/test_<nama>/`.

---

## 11. Pedoman Pengembangan

- Gunakan C++11 (dan fitur yang kompatibel dengan environment native dan Arduino ESP32).
- Semua header menggunakan `#pragma once`.
- Konstanta konfigurasi di `include/sanding_config.h` dalam `namespace config`, menggunakan `constexpr`.
- Akses data yang dibagi antar task dilindungi mutex `EspSpinLock` melalui antarmuka `ILock`.
- Hindari tipe `String` Arduino untuk mencegah fragmentasi memori; gunakan `char array` dan `snprintf`.
- Dokumentasikan setiap API publik dengan komentar gaya Doxygen.
- Tulis test unit untuk setiap komponen kritis sebelum atau samaan dengan implementasi.
- File sensitif (`ota_rahasia.pem`, `build_info.h`, `secrets.h`) tidak boleh di-commit (diatur oleh `.gitignore`).

---

## 12. Referensi dan Sumber Belajar

- [PlatformIO Unit Testing](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html)
- [PlatformIO Quick Start](https://docs.platformio.org/page/quick-start.html)
- [ESP32 Arduino Core](https://docs.espressif.com/projects/arduino-esp32/en/latest/)
- [FreeRTOS for ESP32](https://www.freertos.org/Port.htm)
- [MQTT Essentials](https://www.hivemq.com/resources/mqtt-essentials/)
- [PubSubClient Library](https://pubsubclient.knolleary.net/)
- [Monocypher (Ed25519)](https://monocypher.org/)

---

## 13. Lisensi dan Kredit

### Library yang Digunakan

- **PlatformIO** — Build system untuk IoT development
- **PubSubClient** — Client MQTT untuk ESP32 (knolleary)
- **LiquidCrystal_I2C** — Driver LCD I2C (marcoschwartz)
- **EthernetENC** — Driver ENC28J60 Ethernet (jandrassy)
- **ArduinoJson** — Parser/manipulator JSON (bblanchon)
- **Unity** — Framework unit testing untuk C
- **Monocypher** — Library kriptografi Ed25519 portable

Setiap library memiliki lisensi sendiri.

### Lisensi Proyek

- **Firmware:** Proprietary (hak cipta © 2024 NEXORA)
- **Dokumentasi:** CC-BY-SA 4.0 (boleh dibagi dengan syarat berbagi serupa)

---

## Penutup

**SANDING KECIL** adalah solusi pencacahan industri yang andal dan modern, menggabungkan teknologi embedded canggih dengan arsitektur modular, dua task FreeRTOS, konfigurasi NVS, konsol Serial, dan OTA update yang aman dengan tanda tangan Ed25519 serta verifikasi SHA-256. Dokumentasi lengkap ini memudahkan pengembangan, testing, dan pemeliharaan kode.

Untuk pertanyaan, laporan bug, atau kontribusi, silakan kunjungi repository dan hubungi tim pengembangan NEXORA!

**Happy coding!** 🚀

---

*Dokumentasi ini adalah bagian dari `docs/` di repositori `counter-sanding-kecil`. Untuk instruksi kerja Claude Code, lihat `CLAUDE.md` di direktori root. Untuk struktur direktori lengkap, lihat `docs/DIRECTORY_STRUCTURE.md`.*
