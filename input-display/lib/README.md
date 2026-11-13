# Direktori `lib/`

Modul-modul **internal** proyek input-display (bukan perpustakaan pihak ketiga;
yang pihak ketiga dielola PlatformIO lewat `lib_deps` di `platformio.ini`).
Setiap subdirektori adalah modul mandiri yang akan dikompilasi PlatformIO
sebagai library statis dan disatukan ke dalam executable.

Struktur yang diharapkan (sesuai `CLAUDE.md` monorepo NEXORA):

| Subdirektori | Peran | Status |
|--------------|-------|--------|
| `sensor_filter/` | Debounce + filter logika murni (rotary encoder, pembacaan sensor) | kosong, siap diisi |
| `counter_store/` | Counter + bendera "dirty" (penyimpanan hitungan sanding) | kosong, siap diisi |
| `command_parser/` | Teks perintah jadi enum (perintah MQTT/serial: `START`, `STOP`, `HWTEST_ON`, dst.) | kosong, siap diisi |
| `net_link/` | Enkapsulasi W5500 di balik antarmuka (inisiasi Ethernet, status link, DHCP, reset modul) | kosong, siap diisi |
| `mqtt_service/` | Publish, subscribe, heartbeat, LWT (koneksi MQTT, pesan retained, backoff) | kosong, siap diisi |
| `lcd_view/` | Tampilan LCD 20x4 (7-state UI, tiap state memiliki fungsi `updateDisplay*` sendiri) | kosong, siap diisi |
| `device_identity/` | MAC dari eFuse + konfigurasi NVS (`siapkanMac`, Preferences untuk API key/server/OTA key) | kosong, siap diisi |
| `app/` | Perakitan task (susunan `setup()`/`loop()`, watchdog, FreeRTOS dua core) | kosong, siap diisi |

Perencana migrasi (bertahap, tidak harus sekali kalian):

1. Pindahkan logika yang berkaitan dari `src/main.cpp` ke modul di `lib/`.
2. `main.cpp` disusutkan bertahap menjadi perakitan task (±30 baris) yang
   memanggil modul-modul di `lib/app/` dan rekan-rekannya.
