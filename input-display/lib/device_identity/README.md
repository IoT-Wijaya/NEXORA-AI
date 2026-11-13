# `lib/device_identity/`

Identitas perangkat: MAC dari eFuse ESP32 (`ESP.getEfuseMac()` dengan bit
"locally administered" di byte pertama) dan konfigurasi disimpan di NVS
(Preferences namespace `"cfg"`: API key Laravel, OTA device key, alamat server).

Status: kosong — siap diisi dari `siapkanMac()`, `macString()`, `muatKunci()`,
dan rekan-rekannya yang ada saat ini di `src/main.cpp`.
