# SANDING BESAR — Dokumentasi Lengkap Proyek 🏭

Selamat datang di dokumentasi **SANDING BESAR**, sebuah mesin pencacah otomatis (industrial counter) berbasis **ESP32 NodeMCU-32S** dengan modul Ethernet **W5500** dan **LCD I2C 20x4** yang menghitung dua jenis aktivitas secara bersamaan: **"hasil" (output)** dan **"modal" (input)**.

Proyek ini dibangun dengan **PlatformIO** (Arduino framework) dan merupakan bagian dari ekosistem **NEXORA** untuk otomatisasi industri. Dokumen ini adalah versi besar dari `counter-sanding-kecil` yang menjadi standar emas; struktur direktori, gaya kode, dan pola arsitekturnya sama, dengan penyesuaian pada modul jaringan (W5500) dan ukuran LCD (20x4).

> **Catatan:** Dokumentasi teknis untuk Claude Code ada di `CLAUDE.md` di direktori root. Struktur lengkap direktori ada di `docs/DIRECTORY_STRUCTURE.md`.

---

## 1. Apa Itu SANDING BESAR?

SANDING BESAR adalah perangkat counter industri yang:

- Menghitung **hasil** (produksi keluar) dan **modal** (bahan masuk) melalui sensor fisik.
- Mengirim data counter dan status ke **HMI** (Human Machine Interface) melalui **MQTT** (topik `pabrik/veneer/*`, kompatibel dengan dashboard lama).
- Menerima perintah dari HMI melalui MQTT: `START`, `STOP`, `RESET_S1`, `RESET_S2`, `RESET_ALL`, `RESTART_DEVICE`.
- Menampilkan counter dan status jaringan pada **LCD I2C 20x4**.
- Memiliki **konsol perintah Serial** untuk mengatur konfigurasi perangkat (MQTT host, port, user, password, nama mesin).
- Menyimpan counter secara permanen di **NVS** (Non-Volatile Storage) ESP32, sehingga hitungan tidak hilang saat restart.
- Memiliki pemulihan jaringan bertingkat: reset chip W5500 setelah 5 menit, restart total setelah 10 menit.
- **Tidak menggunakan Wi-Fi:** jaringan menggunakan kabel Ethernet melalui modul W5500 (SPI).

---

## 2. Arsitektur Sistem

Firmware dibangun di atas **FreeRTOS** dengan dua task yang berjalan pada inti berbeda ESP32:

### Task Sensor (Core 0) — prioritas 2, stack 4KB

Menangani semua hal yang terkait sensor fisik:

- Membaca sensor hasil (GPIO 27) dan sensor modal (GPIO 32) dengan filter debounce.
- Memantau tombol reset lokal (GPIO 0).
- Memperbarui counter hanya saat pencacahan aktif (`hitungAktif = true`).
- Tidak menyentuh MQTT, LCD, atau flash.

### Task Network (Core 1) — prioritas 1, stack 12KB

Satu-satunya pemilik MQTT, LCD, dan flash:

- Mengelola koneksi Ethernet (W5500) dan DHCP.
- Memelihara koneksi MQTT ke broker dengan backoff reconnect, mengirim heartbeat, dan menerima perintah.
- Memperbarui tampilan LCD (4 baris: counter, nama mesin/status jaringan, IP, mode).
- Mengkoordinasikan simpan counter ke flash dan laporan diagnosa.

Komunikasi antar task melalui **CounterStore** yang dilindungi oleh mutex `EspSpinLock`.

### Alur Boot (`src/main.cpp`, fungsi `setup()`)

1. Serial monitor dihidupkan (baud 115200).
2. Konfigurasi dimuat dari NVS (`NvsConfigStore` / `ConfigManager`).
3. MAC address dibaca dari `include/secrets.h` dan Client ID MQTT disusun (`DeviceIdentity`).
4. Pin sensor dan tombol diinisialisasi; filter debounce dimulai.
5. Counter terakhir dimuat dari flash NVS.
6. LCD diinisialisasi dan menampilkan pesan pembuka.
7. W5500 diinisialisasi via SPI; DHCP dimulai (jika gagal, akan dicoba ulang otomatis oleh `NetMonitor`).
8. MQTT dikonfigurasi dengan server, kredensial, dan last will.
9. Watchdog dihidupkan.
10. Dua FreeRTOS task dibuat: `sensorTask` (Core 0) dan `networkTask` (Core 1).
11. `loop()` hanya menangani input konsol Serial.

---

## 3. Alur Kerja Pencacah

```
Sensor fisik → SensorFilter (debounce) → CounterStore (mutex) → MQTT publish → LCD
```

- Sensor difilter terlebih dahulu agar tidak ada double-count akibat getaran kontak (bounce).
- Setiap 10 ms, `sensorTask` membaca kedua sensor. Jika ada tekanan stabil, counter bertambah 1 dan ditandai "dirty".
- `networkTask` mendeteksi counter yang berubah (`takeDirty()`) dan segera mempublish ke MQTT, lalu memperbarui LCD.
- Jika pencacahan belum aktif (belum menerima `START` dari HMI), pembacaan sensor tetap berjalan agar status sinkron, tetapi hasil hitungannya dibuang.
- Counter disimpan ke flash maksimal tiap 10 detik.

---

## 4. Perintah MQTT

Topik dasar: `pabrik/veneer/` (dapat ditimpa via `TOPIC_BASE` di build).

| Topik | Fungsi | Retained |
|-------|--------|----------|
| `pabrik/veneer/perintah` | Menerima perintah dari HMI | — |
| `pabrik/veneer/status` | Status online/offline perangkat ("1"/"0") | Ya |
| `pabrik/veneer/jumlah` | Nilai counter hasil | Ya |
| `pabrik/veneer/jumlah2` | Nilai counter modal | Ya |
| `pabrik/veneer/ip` | Alamat IP perangkat | Ya |
| `pabrik/veneer/mode` | `ON` (menghitung) / `OFF` (berhenti) | Ya |
| `pabrik/veneer/ack` | Balasan perintah (START_OK, STOP_OK, RESET_OK) | Tidak |
| `pabrik/veneer/diag` | Laporan diagnosa lengkap (JSON) | Ya |

Perintah yang diterima di topik `perintah`:

| Perintah | Aksi |
|----------|------|
| `START` | Nol-kan counter, aktifkan pencacahan, kirim `START_OK` |
| `STOP` | Matikan pencacahan, nol-kan counter, kirim `STOP_OK` |
| `RESET_S1` / `RESET_HASIL` | Nol-kan counter hasil |
| `RESET_S2` / `RESET_MODAL` | Nol-kan counter modal |
| `RESET_ALL` / `RESET` | Nol-kan kedua counter |
| `RESTART_DEVICE` | Restart ESP32 |

Catatan: bila `HITUNG_OTOMATIS` = true (bawaan), perintah START/STOP dari HMI diabaikan; perangkat langsung menghitung saat menyala.

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

## 6. Tampilan LCD 20x4

LCD menampilkan empat baris:

| Baris | Isi |
|-------|-----|
| 1 | Counter: `M:<modal> H:<hasil>*` (`*` = sedang menghitung, `-` = berhenti) |
| 2 | Bergantian setiap 30 detik: `MESIN:SANDING BESAR` dan `LAN:OK MQTT:OK` |
| 3 | Alamat IP: `IP:192.168.x.x` atau `IP:--.--.--.--` |
| 4 | Mode: `MODE: ON` / `MODE: OFF` |

---

## 7. Pemulihan Jaringan (Bertingkat)

Jika kabel terpasang tetapi MQTT belum OK:

1. Setelah 5 menit: chip W5500 di-reset dan DHCP diminta ulang.
2. Setelah 10 menit: perangkat restart total (counter sudah tersimpan di flash).

Jika kabel yang putus, tidak akan dilakukan restart karena tidak akan mengubah apa-apa.

---

## 8. Rincian Folder dan File

Berikut penjelasan setiap folder dan file dalam proyek ini.

| File / Folder | Fungsi |
|---------------|--------|
| `platformio.ini` | Konfigurasi PlatformIO. Environment `sanding_besar` (board `nodemcu-32s`, framework `arduino`) dan `native` (test unit). Library eksternal: Ethernet (W5500), LiquidCrystal_I2C, PubSubClient. Skrip pre-build `tools/version.py`. |
| `include/sanding_config.h` | Konfigurasi utama dalam `namespace config`: pin W5500, LCD 20x4, sensor, timing jaringan, topik MQTT, watchdog, dan mode kerja. |
| `include/secrets.h` | MAC address unik W5500 (RAHASIA, tidak di-commit). |
| `include/secrets.example.h` | Contoh `secrets.h`. Aman di-commit. |
| `include/build_info.h` | Dihasilkan otomatis oleh `tools/version.py` saat build (`FW_VERSION`, `FW_BUILD`). Jangan diedit atau di-commit. |
| `lib/` | Library aplikasi khusus, masing-masing dalam direktori sendiri dengan antarmuka abstrak (`I...`) untuk bagian yang menyentuh hardware. |
| `src/` | Driver hardware (W5500, LCD I2C, spin lock, watchdog, MQTT wrapper, NVS, MAC sumber) dan `main.cpp`. |
| `test/` | Suite test unit (PlatformIO Unity, environment native/PC). |
| `tools/` | `version.py` skrip pre-build untuk versi firmware. |
| `docs/` | Dokumentasi proyek. |
| `partitions.csv` | Tabel partisi flash ESP32 (kosong: pakai default bawaan Arduino ESP32). |

### `lib/` — modul-modul

| Folder | Fungsi |
|--------|--------|
| `command_parser/` | Parser payload MQTT menjadi perintah (`Command`). |
| `counter_store/` | Penyimpan counter thread-safe (`hasil` dan `modal`) dengan mutex, plus pemulihan dari flash. |
| `device_config/` | Manajemen konfigurasi runtime perangkat dengan validasi dan NVS. |
| `device_identity/` | Identitas perangkat: MAC dan Client ID MQTT. |
| `firmware_info/` | Identitas versi firmware dan JSON versi. |
| `lcd_view/` | Logika tampilan LCD 20x4 dan manajemen halaman. |
| `mqtt_service/` | Layanan MQTT: koneksi, backoff reconnect, heartbeat, publish, perintah masuk. |
| `net_link/` | Stack jaringan: `INetDriver` dan `NetMonitor` (link + DHCP). |
| `sensor_filter/` | Filter debounce untuk sensor fisik. |
| `console_shell/` | Shell perintah Serial untuk konfigurasi. |

### `src/` — driver hardware

| File | Fungsi |
|------|--------|
| `main.cpp` | Titik masuk firmware: setup semua komponen, dua FreeRTOS task, konsol Serial di `loop()`. |
| `w5500_driver.h` | Driver W5500 (implementasi `INetDriver`). |
| `i2c_lcd_display.h` | Driver LCD I2C 20x4 (implementasi `ILcdDisplay`). |
| `fixed_mac_source.h` | Sumber MAC dari `include/secrets.h` (implementasi `IMacSource`). |
| `esp_spin_lock.h` | Mutex spin lock ESP32 (implementasi `ILock`). |
| `esp_task_watchdog.h` | Pembungkus watchdog tugas ESP32. |
| `pubsub_mqtt_client.h` | Pembungkus PubSubClient sebagai `IMqttClient`. |
| `nvs_config_store.h` | Penyimpanan konfigurasi di NVS/Preferences (implementasi `IConfigStore`). |

---

## 9. Cara Build dan Upload

Persyaratan: PlatformIO terinstal (via `pip install platformio` atau ekstensi VS Code PlatformIO).

| Perintah | Keterangan |
|----------|------------|
| `platformio run` | Build firmware saja. |
| `platformio run --target upload` | Build dan upload ke perangkat ESP32. |
| `platformio test` | Jalankan semua test unit di environment native (PC). |
| `platformio run --target clean` | Bersihkan hasil build. |
| `platformio device monitor` | Buka serial monitor (115200 baud). |

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
4. Tambah test di `test/test_command_parser/`.

### Menambah Library Baru di `lib/`

1. Buat direktori baru di `lib/<nama>/`.
2. Buat antarmuka abstrak (`I...`) untuk bagian yang menyentuh hardware.
3. Buat file `.h` dan `.cpp` implementasi.
4. Tambah test di `test/test_<nama>/`.

---

## 11. Pedoman Pengembangan

- Gunakan C++11 (dan fitur yang kompatibel dengan environment native dan Arduino ESP32).
- Semua header menggunakan `#pragma once`.
- Konstanta konfigurasi di `include/sanding_config.h` dalam `namespace config`, menggunakan `constexpr`.
- Akses data yang dibagi antar task dilindungi mutex `EspSpinLock` melalui antarmuka `ILock`.
- Hindari tipe `String` Arduino untuk mencegah fragmentasi memori; gunakan `char array` dan `snprintf`.
- Dokumentasikan setiap API publik dengan komentar gaya Doxygen.
- Tulis test unit untuk setiap komponen kritis.
- File sensitif (`include/secrets.h`, `include/build_info.h`) tidak boleh di-commit (diatur oleh `.gitignore`).

---

## 12. Perbedaan dengan SANDING KECIL

| Aspek | SANDING KECIL | SANDING BESAR |
|-------|---------------|---------------|
| Modul jaringan | ENC28J60 via SPI (library EthernetENC) | W5500 via SPI (library Arduino Ethernet) |
| Sumber MAC | eFuse chip (`ESP_MAC_ETH`) | `include/secrets.h` (statis per modul) |
| LCD | I2C 16x2 (2 baris) | I2C 20x4 (4 baris: counter, mesin/jaringan, IP, mode) |
| Topik MQTT | `pabrik/sanding/*` | `pabrik/veneer/*` |
| Counter | RAM saja | Tersimpan di flash NVS |
| Diagnosa | — | Laporan JSON ke topik `diag` |
| Pemulihan jaringan | — | Reset W5500 (5 m) / restart (10 m) |
| OTA | Tersedia (Ed25519 + SHA-256) | Belum tersedia |

---

## 13. Referensi dan Sumber Belajar

- [PlatformIO Unit Testing](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html)
- [ESP32 Arduino Core](https://docs.espressif.com/projects/arduino-esp32/en/latest/)
- [FreeRTOS for ESP32](https://www.freertos.org/Port.htm)
- [MQTT Essentials](https://www.hivemq.com/resources/mqtt-essentials/)
- [PubSubClient Library](https://pubsubclient.knolleary.net/)
- [Arduino Ethernet Library (W5500)](https://www.arduino.cc/en/Reference/Ethernet)

---

## Lisensi

- Firmware: Proprietary (hak cipta © 2024 NEXORA)
- Dokumentasi: CC-BY-SA 4.0

---

**Happy coding!** 🚀
