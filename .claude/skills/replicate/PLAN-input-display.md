# Rencana Replikasi — input display

Disepakati dari hasil selidiki counter-sanding-kecil. Folder proyek belum diubah.

## Asumsi yang dipakai
- input display memakai OTA signed-manifest dari counter-sanding-kecil (bukan OTA GitHub).
- Jaringan: Ethernet ENC28J60 (plain MQTT 1883, tidak TLS).
- trust_anchors.h, NTP, logging heap, selftest hardware = NEW & DEFER (menunggu pilihan pustaka TLS dan jenis jaringan).

## Tabel COPY / ADAPT / NEW / SKIP

| Item | Status | Keterangan |
|------|--------|------------|
| src/main.cpp | ADAPT | Titik masuk; label/domain diubah ke input display. |
| include/config.h | ADAPT | sanding_config.h → config.h; namespace config, konstanta pin/topic/waktu input display. |
| platformio.ini | ADAPT | Env input_display, board, lib_deps. |
| partitions.csv | COPY | Sama dengan referensi. |
| docs/README.md | COPY | Panduan struktur. |
| lib/command_parser/ | ADAPT | Tambah command_parser.h + command_parser.cpp. |
| lib/counter_store/ | ADAPT | Tambah counter_store.h + counter_store.cpp, ilock.h. |
| lib/device_identity/ | ADAPT | Tambah device_identity.h + device_identity.cpp, imac_source.h. |
| lib/lcd_view/ | ADAPT | Tambah lcd_view.h + lcd_view.cpp, ilcd_display.h. |
| lib/mqtt_service/ | ADAPT | Tambah mqtt_service.h + mqtt_service.cpp, imqtt_client.h; heartbeat dan lastState = COPY. |
| lib/net_link/ | ADAPT | Tambah net_monitor.h + net_monitor.cpp, inet_driver.h; NetMonitor = COPY. |
| lib/sensor_filter/ | ADAPT | Tambah sensor_filter.h + sensor_filter.cpp. |
| lib/ota_core/ | COPY | Sama dengan referensi (ota_state_machine dll.). |
| src/partition_log.h | COPY | Sama dengan referensi. |
| src/esp_task_watchdog.h | COPY | Sama dengan referensi. |
| lib/app/ | NEW | Koordinasi aplikasi: app.h + app.cpp (konvensi lib/<module>/). |
| test/test_command_parser/ | ADAPT | Tambah test_command_parser.cpp (Unity). |
| test/test_counter_store/ | ADAPT | Tambah test_counter_store.cpp (Unity). |
| test/test_sensor_filter/ | ADAPT | Tambah test_sensor_filter.cpp (Unity). |
| test/test_smoke/ | ADAPT | Tambah test_smoke.cpp (Unity). |
| trust_anchors.h | NEW, DEFER | Menunggu pilihan pustaka TLS. |
| Sinkronisasi waktu (NTP) | NEW, DEFER | Menunggu jenis jaringan/TLS. |
| Logging heap | NEW, DEFER | Menunggu kebutuhan diagnostik. |
| Selftest hardware | NEW, DEFER | Menunggu kebutuhan diagnostik. |
| ota_github.h | SKIP | Tidak dipakai. |
| secrets.h | SKIP | Tidak dipakai. |
| lib/console_shell/, device_config/, firmware_info/, http_client/, ota_runner/, serial_router/, monocypher/ | SKIP | Diganti/dihapus dari desain input display. |
| tools/, manifest.json, *.pem | SKIP | Mekanisme OTA berbeda (signed-manifest referensi, tidak perlu tools signing di target). |
| CLAUDE.md di input-display | SKIP | Root CLAUDE.md sudah mencakup proyek. |

Item NEW aktif: 1 (lib/app). Item NEW/DEFER: 4. Dibawah batas 3 item NEW aktif, lanjut ke Build (Step 3) bila disetujui.
