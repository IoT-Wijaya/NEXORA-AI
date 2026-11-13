# `lib/command_parser/`

Parser teks perintah menjadi enum: perintah yang masuk lewat Serial Monitor
dan topik MQTT `pabrik/hmi_sanding/perintah` (mis. `START`, `STOP`,
`HWTEST_ON`, `HWTEST_OFF`, `STATUS`, `MQTT_RECONNECT`, `DEBUG_0`..`DEBUG_3`,
`reboot`, `pubtest`) diubah menjadi enum tindakan yang dapat dieksekusi aplikasi.

Status: kosong — siap diisi dari `perintahDiagnostik()` / `perintahSerialDiagnostik()`
yang ada saat ini di `src/main.cpp` dan `include/diagnostik.h`.
