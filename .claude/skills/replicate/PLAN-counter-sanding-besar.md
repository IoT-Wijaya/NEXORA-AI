# Rencana Replikasi — counter-sanding-besar (direvisi)

Mode: **BUILD** (menyesuaikan struktur dengan blueprint counter-sanding-kecil). Folder proyek counter-sanding-besar **tidak diubah**. File `.pem` tidak dibaca atau ditampilkan.

## Sumber referensi
BLUEPRINT.md (counter-sanding-kecil, gold standard). Konvensi: file snake_case, satu modul per `lib/<module>/` dengan `.cpp`+`.h`, class `PascalCase`, metode `camelCase`, konstanta `UPPER_SNAKE_CASE` di `namespace config`, komentar Bahasa Indonesia, tes Unity di `test/test_<module>/`.

## Pohon saat ini counter-sanding-besar (belum diubah)
- `src/main.cpp` (ada), `include/sanding_besar.h` (ada), `include/secrets.h` + `secrets.example.h` (ada)
- `platformio.ini` (ada), `partitions.csv` (ada)
- `lib/README`, `test/README` (kosong — belum ada implementasi modul/tes)

## NVS di referensi (ditemukan lewat kata kunci Preferences / nvs_ / NVS)
Digunakan untuk penyimpanan konfigurasi dan counter:
- `src/main.cpp` — pakai `NvsConfigStore` / `NvsBlobStore`
- `src/nvs_config_store.h` — implementasi `IConfigStore` (namespace `"sanding"`)
- `src/nvs_blob_store.h` — implementasi `IBlobStore` (namespace `"ota"`)
- `lib/device_config/device_config.h` + `lib/device_config/iconfig_store.h` — `ConfigManager` + `DeviceConfig` disimpan ke NVS
- `lib/ota_core/ota_state_machine.h`, `lib/ota_core/iblob_store.h` — antarmuka/penyimpanan catatan OTA di NVS
- `src/esp_ota_flash.h`, `lib/http_client/ota_flash_sink.h` — flash OTA

Pola target untuk migrasi `secrets.h` ke NVS mengikuti `lib/device_config/` (config store + DeviceConfig) dan `src/nvs_config_store.h`.

## Tempat di target (counter-sanding-besar) yang memakai secrets.h
Semua ada di `src/main.cpp`:
1. Baris 9: `#include "secrets.h"`
2. Baris 39: `byte mac[] = DEVICE_MAC;`
3. Baris 535: `strlen(mqtt_user) > 0`
4. Baris 537: `client.connect(clientID, mqtt_user, mqtt_pass, topicStatus, 0, true, STATUS_OFFLINE);`
5. Baris 1117: `client.setServer(mqtt_server, mqtt_port);`

Variabel yang digunakan: `DEVICE_MAC`, `mqtt_user`, `mqtt_pass`, `mqtt_server`, `mqtt_port`, `apiKey`. Catatan: `mqtt_user`/`mqtt_pass`/`mqtt_server`/`mqtt_port` dipakai di `main.cpp` tetapi belum terdefinisi di `include/secrets.h` (saat ini hanya punya `apiKey`) atau `include/sanding_besar.h` — kemungkinan datang dari `build_flags`.

## Tabel COPY / ADAPT / NEW / DEFER / MIGRATE / SKIP

| Item | Status | Keterangan |
|------|--------|------------|
| src/main.cpp | ADAPT | Sudah ada; disesuaikan pola dua task / inisialisasi referensi. Keputusan OTA (DEFER) diambil dulu sebelum mengerjakan berkas ini. |
| include/sanding_besar.h | ADAPT | `sanding_config.h` → `sanding_besar.h`; `namespace config`, konstanta pin/topic/waktu. Nilai terikat hardware (pin W5500, sensor, LCD) = ADAPT. |
| platformio.ini | ADAPT | Env `counter-sanding-besar`, board, `lib_deps`. |
| partitions.csv | COPY | Sama dengan referensi (sudah ada). |
| lib/command_parser/ | COPY | Sama peran dengan referensi: `command_parser.h` + `command_parser.cpp`. |
| lib/counter_store/ | COPY | Sama peran: `counter_store.h` + `counter_store.cpp`, `ilock.h`. |
| lib/device_identity/ | COPY | Sama peran: `device_identity.h` + `device_identity.cpp`, `imac_source.h`. |
| lib/lcd_view/ | COPY | Sama peran: `lcd_view.h` + `lcd_view.cpp`, `ilcd_display.h`. |
| lib/mqtt_service/ | COPY | Sama peran: `mqtt_service.h` + `mqtt_service.cpp`, `imqtt_client.h`; heartbeat dan `lastState` = COPY. |
| lib/net_link/ | COPY | Sama peran: `net_monitor.h` + `net_monitor.cpp`, `inet_driver.h`; `NetMonitor` = COPY (tidak terkait OTA). |
| lib/sensor_filter/ | COPY | Sama peran: `sensor_filter.h` + `sensor_filter.cpp`. |
| src/esp_task_watchdog.h | COPY | Sama dengan referensi; tidak terkait OTA. |
| lib/app/ | NEW | Koordinasi aplikasi: `app.h` + `app.cpp` (konvensi `lib/<module>/`). |
| test/test_command_parser/ | COPY | Tes Unity `test_command_parser.cpp`. |
| test/test_counter_store/ | COPY | Tes Unity `test_counter_store.cpp`. |
| test/test_sensor_filter/ | COPY | Tes Unity `test_sensor_filter.cpp`. |
| test/test_smoke/ | COPY | Tes Unity `test_smoke.cpp`. |
| docs/ | NEW | `docs/README.md` panduan struktur. |
| lib/ota_core/ | DEFER | Keputusan OTA diambil sebelum mengerjakan `src/main.cpp`. |
| src/partition_log.h | DEFER | Keputusan OTA diambil sebelum mengerjakan `src/main.cpp`. |
| tools/ | DEFER | Alat signing/manifest OTA; keputusan OTA dulu. |
| include/secrets.h | MIGRATE (DEFER) | Diganti mekanisme NVS mengikuti referensi (`lib/device_config/`, `src/nvs_config_store.h`); **tidak dihapus atau diubah dulu**. |
| include/secrets.example.h | MIGRATE (DEFER) | Contoh nilai segikuti migrasi NVS di atas; **tidak dihapus atau diubah dulu**. |
| lib/device_config/ | MIGRATE (DEFER) | Diperlukan untuk migrasi secrets.h ke NVS (ConfigManager + DeviceConfig); diubah status dari SKIP menjadi bagian MIGRATE. |
| src/nvs_config_store.h | MIGRATE (DEFER) | Diperlukan untuk migrasi secrets.h ke NVS (implementasi IConfigStore); ditambahkan ke tabel. |
| lib/console_shell/, firmware_info/, http_client/, ota_runner/, serial_router/, monocypher/ | SKIP | Tidak ada padanan yang dibangun di target saat ini. |
| manifest.json, *.pem | SKIP | Mekanisme signing OTA tidak dibangun di target; `.pem` tidak disentuh. |
| CLAUDE.md di counter-sanding-besar | SKIP | Root CLAUDE.md sudah mencakup proyek. |

Item NEW aktif: 2 (`lib/app/`, `docs/`). Item DEFER: 5 (`lib/ota_core/`, `src/partition_log.h`, `tools/`, migrasi `secrets.h` ke NVS, modul pendukung migrasi). Item NEW aktif di bawah batas 3, tetapi ada item DEFER yang menunggu keputusan OTA dan migrasi NVS. Build hanya dilakukan setelah konfirmasi.
