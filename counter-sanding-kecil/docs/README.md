# Halo, Selamat Datang di SANDING KECIL! 📊

## Apa Itu?

Ini adalah panduan super lengkap buat kamu yang mau main-main sama SANDING KECIL - sebuah mesin pencacahan otomatis berbasis ESP32 yang bisa ngerjain dua jenis aktivitas sekaligus: \"hasil\" (output) dan \"modal\" (input). Bayangin ini kayak mesin kasir digital yang bisa ngerjain dua jenis transaksi sekaligus! 🏪

Proyek ini dibangun pake PlatformIO (kayak Arduino IDE modern) dan jalan di ESP32 NodeMCU-32S board. Ini adalah bagian dari ekosistem NEXORA untuk otomatisasi industri! 🔧

## Struktur File: Ini Dia Direktori-Direktori Kita 📁

```
counter-sanding-kecil/
├── .pio/                    # PlatformIO gitu loh
├── .vscode/                 # Konfigurasi VS Code
├── docs/                    # 📚 Dokumentasi ini!
├── include/                 # Header files (konfigurasi global)
├── lib/                     # Library aplikasi khusus
├── src/                     # File sumber utama
├── test/                    # Test unit
├── tools/                   # Utility skript
├── .gitignore              # File abaikan git
├── manifest.json            # Info firmware
├── platformio.ini           # Konfigurasi PlatformIO
└── ... (file kunci lainnya)
```

## Langsung ke Intinya: Direktori-Direktori Penting ✨

### 1. **docs/** - 📚 Panduan Proyek

**Apa:** Semua dokumentasi, README, dan panduan
**Isi:** README.md (ini!), DIRECTORY_STRUCTURE.md
**Kenapa penting:** Biar kamu gak bingung waktu kerja atau kapanpun Claude Code masuk dan bantu!

### 2. **include/** - ⚙️ Konfigurasi Global

**Apa:** Header file yang dibaca di seluruh proyek
**File penting:**
- `sanding_config.h` - **File utama!** Semua pin, timing, dan topik MQTT diatur di sini\n- `device_config.h` - Pengaturan runtime (MQTT host, user, password)\n- `build_info.h` - Info build firmware\n- `ota_public_key.h` - Kunci publik untuk OTA\n
**Fungsi:** Konstanta #define yang dibaca kompiler, jadi tidak perlu ha

### 3. **lib/** - 📦 Library Aplikasi Khusus

**Apa:** Library buatan sendiri, bukan library eksternal\n**Struktur:**\n```\nlib/\n├── command_parser/     # Parser perintah MQTT\n├── console_shell/      # Shell perintah serial\n├── counter_store/      # Penyimpanan counter thread-safe\n├── device_config/      # Manajemen konfigurasi perangkat\n├── device_identity/    # Generator identitas perangkat\n├── firmware_info/      # Info versi firmware\n├── http_client/        # Klien HTTP untuk verifikasi OTA\n├── lcd_view/           # Logika tampilan LCD\n├── mqtt_service/       # Layanan MQTT dengan callback\n├── net_link/           # Stack jaringan (Ethernet, MQTT)\n├── ota_core/           # Logika inti OTA\n├── sensor_filter/      # Filter debounce sensor\n└── ...                # Library lainnya\n```\n
### 4. **src/** - 🚀 File Sumber Utama\n
**Apa:** File .cpp dan .h yang dijalankan di board ESP32\n**File paling penting:**\n- `main.cpp` - **File pertama yang dieksekusi!** setup tugas FreeRTOS, inisialisasi semua komponen\n- `enc_driver.h` - Driver ENC28J60 Ethernet\n- `i2c_lcd_display.h` - Driver LCD I2C 16x2\n- `esp_mac_source.h` - Sumber MAC ESP32\n
**Fungsi:** Implementasi nyata dari semua fitur proyek\n
### 5. **test/** - 🧪 Test Unit\n
**Apa:** Test otomatis untuk setiap komponen\n**Struktur:**\n```\ntest/\n├── test_counter_store/      # Test untuk counter store\n├── test_mqtt_service/       # Test client MQTT\n├── test_sensor_filter/      # Test filtering sensor\n└── ...                     # Test unit lainnya\n```\n
**Fungsi:** Jadi kamu bisa yakin semua komponen jalan dengan benar sebelum di-deploy!\n
### 6. **tools/** - 🛠️ Utility Skript\n
**Apa:** Skript bantu, biasanya untuk development\n**File:**\n- `version.py` - Skrip Python untuk manajemen versi firmware\n
**Fungsi:** Ngebantuin task development sehari-hari\n
## Sekarang, Mari Kita Telusuri File-Filen Penting 📖

### File Utama: `src/main.cpp`\n
Ini dia file yang pertama kali jalan waktu board ESP32 nyala! \n
**Apa yang dilakukan:**\n- **Setup:** Inisialisasi semua komponen (sensor, jaringan, LCD, OTA)\n- **Buat tugas:** Buat dua tugas FreeRTOS (Sensor Task di Core 0, Network Task di Core 1)\n- **Main loop:** Nunggu event dan terus jalan\n\n**Arsitektur Tugas:**\n\n#### Tugas Sensor (Core 0)\n- **Prioritas:** 2 (lebih tinggi dari Network Task)\n- **Fungsi:** Baca sensor, hitung, handle tombol reset\n- **Stack:** 4KB memory\n- **Kapan jalan:** Terus nunggu dan proses data sensor\n\n#### Tugas Network (Core 1)\n- **Prioritas:** 1 (lebih rendah)\n- **Fungsi:** MQTT, LCD, OTA, koordinasi\n- **Stack:** 8KB memory\n- **Kapan jalan:** Proses network, komunikasi, tampilan\n\n### File Konfigurasi: `include/sanding_config.h`\n
**Ini dia file **paling penting** yang ** WAJIB** kamu lihat!** Ini seperti **buku panduan** buat proyek ini! Setiap detail penting -- dari **pin fisik** sampe **waktu operasi** -- semua tercatat di sini.

**Apa yang tercatat di sini:**\n- **Identitas Mesin:** nama mesin (\"SANDINGKCL\"), prefix client ID\n- **Pengaturan LCD:** alamat I2C, jumlah kolom dan baris\n- **Pengaturan ENC28J60:** pin SPI, pin chip select\n- **Pin Sensor:** pin untuk sensor hasil dan modal, plus tombol reset\n- **Waktu Penting:** waktu debounce, interval cek jaringan, retry DHCP\n- **Pengaturan MQTT:** port default, penggunaan last will\n- **Pengaturan OTA:** jumlah boot pending, attemp unduh, rollback attemp\n\n**Cara baca file ini:**\n- Semuanya pakai `constexpr` (konstanta compile-time)\n- Semuanya masuk namespace `config`\n- Siapapun yang butuh konfigurasi, tinggal include file ini!\n
### Library Utama: `lib/counter_store.h`\n
**Apa fungsinya:** Ini adalah **kotak penyimpanan data hitung** yang **super aman**! Bayangkan ini seperti **tempat penyimpanan data hitung** yang bisa diakses **oleh dua tugas** tanpa khawatir data **korup** atau **tidak sinkron**! 🔒

**Bagaimana cara kerjanya:**\n- **Mutex (EspSpinLock):** Kunci thread-safe, biar cuma satu tugas yang bisa **akses data** dalam **satu waktu**\n- **Snapshot:** Fitur **snapshot** yang **capture** data counter saat ini, **seperti ambil foto** keadaan counter\n- **Thread-safe:** Gunakan `incrementHasil()`, `incrementModal()`, `reset()` method\n\n**Kapan harus gunakan:**\n- Kapanpun kamu butuh **simpan atau baca** data hitung\n- **Dua tugas** (Sensor Task dan Network Task) butuh **akses** ke data yang **sama**\n\n### Library Network: `lib/mqtt_service.h`\n
**Apa fungsinya:** Ini adalah **pelayanan MQTT** yang **mengatur semua urusan MQTT**! Bayangkan ini seperti **manager komunikasi** yang **kelola semua urusan MQTT** -- dari **koneksi broker**, **parsing perintah**, **kirim pesan**, **reaksi terhadap perintah**! 🗣️\n
**Fitur utama:**\n- **Koneksi:** Kelola koneksi ke MQTT broker\n- **Parser:** **Parsing perintah** yang **dikirim dari HMI** (Human Machine Interface)\n- **Callback:** **Tangani perintah** seperti `start`, `stop`, `reset_hasil`, `reset_modal`, `restart`\n- **Publikasikan:** **Kirim pembaruan status** ke topik MQTT yang **berbeda-beda**\n\n**Perintah MQTT:**\n- **Topik perintah:** `pabrik/sanding/perintah`\n- **Perintah:** `start`, `stop`, `reset_hasil`, `reset_modal`, `reset_all`, `restart`\n- **Topik status:** `pabrik/sanding/status`, `pabrik/sanding/jumlah`, `pabrik/sanding/ip`\n
## Cara Kerja Sistem Ini: Steps by Steps ⚙️\n
### Langkah 1: Boot dan Inisialisasi (di `src/main.cpp`, fungsi `setup()`)\n
1. **Awal:** Serial monitor nyala di baud 115200\n2. **Load config:** Load konfigurasi dari Non-Volatile Storage (NVS)\n3. **Identitas:** Generate MAC address dan client ID\n4. **Sensor:** Setup pin sensor dan tombol reset\n5. **LCD:** Inisialisasi tampilan LCD\n6. **Jaringan:** Setup Ethernet dan DHCP\n7. **MQTT:** Setting server dan kredensial\n8. **Buat tugas:** Buat Sensor Task (Core 0) dan Network Task (Core 1)\n
### Langkah 2: Tugas Sensor (Core 0)\n
**Setiap detik:** (delay 10ms)\n1. **Baca sensor:** Baca sensor hasil dan modal\n2. **Filter:** Terapkan **debounce filtering** biar gak **double-count**\n3. **Hitung:** Kalau saklar hitung aktif, increment counter\n4. **Reset:** Kalau tombol reset ditekan, reset counter\n
### Langkah 3: Tugas Network (Core 1)\n
**Setiap 20ms:** (delay 20ms)\n1. **Cek jaringan:** Periksa status Ethernet\n2. **Update LCD:** Tampilkan data counter dan status\n3. **MQTT loop:** Process pesan MQTT yang masuk\n4. **Kirim pembaruan:** Kalau counter berubah, kirim ke broker\n5. **Handle perintah:** **Eksekusi perintah** yang **diterima** (start, stop, reset)\n\n### Langkah 4: Konsol Perintah Serial\n
**di `src/main.cpp`, fungsi `loop()`:**\n- **Nunggu input:** Nunggu karakter dari UART\n- **Feed ke console shell:** Kirim ke `ConsoleShell.feed()`\n- **Proses perintah:** Proses perintah serial seperti `help`, `start`, `stop`\n
## Cara Test Proyek Ini 🧪\n
### PlatformIO Test Runner\n\nProyek ini pakai **PlatformIO Unity Test Framework** -- test otomatis yang **berjalan di environment native (PC)**, bukan di board ESP32!\n
**Cara jalan test:**\n```bash
platformio test  # Test semua komponen
platformio test -f test_counter_store  # Test satu komponen aja
```\n
**Direktori test:**\n```
test/\n├── test_counter_store/      # Test untuk counter store\n├── test_mqtt_service/       # Test client MQTT\n├── test_sensor_filter/      # Test filtering sensor\n└── ...                     # Test unit lainnya\n```\n
**Library test yang paling penting:**\n\n#### Test Counter Store (`test/test_counter_store/`)\n**Apa yang test:**\n- **Thread safety:** **Akses data bersamaan** dari **beberapa thread**\n- **Atomic increment:** `incrementHasil()`, `incrementModal()`\n- **Reset:** `reset()` method\n- **Snapshot:** `snapshot()` method\n\n#### Test MQTT Service (`test/test_mqtt_service/`)\n**Apa yang test:**\n- **Parsing:** **Parsing command** dari **MQTT topic**\n- **Callback:** **Callback handler** untuk **perintah yang berbeda-beda**\n- **Publikasikan:** **Kirim pesan** ke **topik MQTT yang berbeda-beda**\n- **Koneksi:** **Koneksi dan disconnect** client MQTT\n\n#### Test Sensor Filter (`test/test_sensor_filter/`)\n**Apa yang test:**\n- **Debouncing:** **Filter waktu stabil** untuk **bouncing sensor**\n- **Edge detection:** **Deteksi edge** (falling, rising)\n- **Multiple filters:** **Dua filter** (hasil dan modal) jalan **bersamaan**\n\n## Cara Bangun dan Jalankan Proyek Ini 🖥️\n
### Persyaratan: Harus Punya PlatformIO!\n\inaiki Berarti Pake Arduino IDE A

###Langkah-langkah:\n
1. **Clone repositori:** `git clone <repo-url>`\n2. **Install PlatformIO:** `pip install platformio` (atau pake VS Code Extension)\n3. **Update dependencies:** `platformio update`\n4. **Build & Upload:** `platformio run --target upload`\n5. **Monitor serial:** `platformio device monitor`\n
###Cara Develop Fitur Baru: Examples\n
#### Menambahkan Sensor Baru\n1. **Tambah pin:** Masukkan pin baru ke `sanding_config.h`\n2. **Inisialisasi:** Tambahkan inisialisasi sensor di `setup()` di `main.cpp`\n3. **Filtering:** Tambahkan logika filtering di `sensorTask()`\n4. **Test:** Tambahkan test di `test_sensor_filter/`\n\n#### Menambahkan Perintah MQTT Baru\n1. **Tambah enum:** Tambahkan enum command baru di `mqtt_service.h`\n2. **Tangani:** Tambahkan case handling di `handleCommand()` di `main.cpp`\n3. **Update console:** Update console shell kalo perlu\n4. **Test:** Tambahkan test di `test_mqtt_service/`\n\n## Tips Pengembangan dan Praktik Terbaik 🛠️\n
### Pengembangan Aman\n- **Mutex:** Gunakan **EspSpinLock** untuk **thread-safe** **akses data**\n- **ISR:** Batasi **interrupt service routine** ke **fungsi singkat**\n- **Konsantan:** Gunakan `const` untuk **konstanta**, `constexpr` untuk **compile-time**\n- **Dokumentasi:** Dokumentasikan **setiap API publik** dengan **komentar Doxygen**\n\n### Testing\n- **Test unit:** Tulis **test unit** untuk **setiap komponen kritis**\n- **PlatformIO:** Gunakan **PlatformIO testing** di **environment native**\n- **Edge cases:** Test **edge cases**, **error conditions**, **recovery**\n\n## Referensi dan Sumber Belajar 📚\n
### Dokumentasi PlatformIO\n- [PlatformIO Unit Testing](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html)\n- [PlatformIO Quick Start](https://docs.platformio.org/page/quick-start.html)\n\n### Referensi ESP32\n- [ESP32 Arduino Core](https://docs.espressif.com/projects/arduino-esp32/en/latest/)\n- [FreeRTOS for ESP32](https://www.freertos.org/Port.htm)\n\n### MQTT Resources\n- [MQTT Essentials](https://www.hivemq.com/resources/mqtt-essentials/)\n- [EMQ Eclipse MQTT Broker](https://www.emqx.io/products/mqtt-broker)\n\n### OTA Resources\n- [ArduinoOTA Library](https://www.arduino.cc/reference/en/libraries/arduinoota/)\n- [Secure OTA with HTTPS](https://www.influxdata.com/blog/secure-over-the-air-updates-with-mosquitto-and-esp32/)\n\n## Kredit dan Lisensi 💳\n\n### Libraries Digunakan\n- **PlatformIO:** Build system untuk IoT development\n- **PubSubClient:** Client MQTT untuk ESP32\n- **LiquidCrystal_I2C:** Driver LCD I2C\n- **EthernetENC:** Driver ENC28J60 Ethernet\n- **ArduinoJson:** Parser/manipulator JSON\n- **Unity:** Framework testing\n\n### Lisensi\n- **Firmware:** Proprietary (hak cipta © 2024 NEXORA)\n- **Libraries:** Masing-masing library memiliki lisensi sendiri\n- **Dokumentasi:** CC-BY-SA 4.0 (boleh dibagi, berbagi serupa)\n\n## Penutup 🎉\n\n**SANDING KECIL** adalah **solusi pencacahan industri** yang **andal** dan **modern**, **menggabungkan teknologi embedded canggih** dengan **antarmuka pengguna yang ramah**. Dengan **arsitektur modular** dan **dokumentasi lengkap**, proyek ini **siap untuk dikembangkan** dan **diintegrasikan** ke dalam **ekosistem industri 4.0**.\n\n**Buat pertanyaan, laporan bug, atau kontribusi,** silakan kunjungi repository dan hubungi tim pengembangan NEXORA! 🚀\n\n---\n**Ingat:** Untuk **belajar** lebih dalam tentang setiap komponen, **cek CLAUDE.md** di **root direktori** -- **itu file panduan** yang **dibuat buat Claude Code**! 🤖\n\n**Happy coding!** 🎉