# Direktori `test/`

Unit test proyek input-display untuk PlatformIO Test Runner (framework Unity,
terdapat di `platformio.ini` di bagian `[env:native]`).
Test berjalan di host (native), bukan di ESP32, sehingga modul yang diuji harus
dapat dikompilasi tanpa dependensi Arduino/ESP32 (mis. logika murni di
`lib/sensor_filter/` dan `lib/counter_store/`).

Struktur yang diharapkan (sesuai `CLAUDE.md` monorepo NEXORA):

| Subdirektori | Tujuan | Status |
|--------------|--------|--------|
| `test_smoke/` | Tes boot dasar (LCD hidup, Ethernet link, MQTT handshake) | kosong, siap diisi |
| `test_sensor_filter/` | Tes debounce rotary encoder + filter pembacaan sensor | kosong, siap diisi |
| `test_counter_store/` | Tes hitungan counter dan bendera "dirty" | kosong, siap diisi |
| `test_command_parser/` | Tes parsing teks perintah menjadi enum | kosong, siap diisi |

Menjalankan test native:

```sh
pio run -e native
```
