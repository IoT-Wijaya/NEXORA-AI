# `lib/sensor_filter/`

Debounce dan filter logika murni: pembacaan rotary encoder (tombol CLK/DT,
debounce kontak), dan filter pembacaan sensor sanding.

Tujuan: logika ini bebas dependensi Arduino/ESP32 sehingga dapat diuji di host
melalui `test/test_sensor_filter/`.

Status: kosong — siap diisi dari logika yang ada saat ini di `src/main.cpp`
(`ambilLangkahEncoder`, `ENC_DEBOUNCE_US`, dan rekan-rekannya).
