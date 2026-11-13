# `lib/mqtt_service/`

Koneksi MQTT: publish/subscribe dengan backoff eksponensial (+ jitter), pesan
retained (status, IP, diagnosis), Last Will and Testament (`topicHmiStatus =
"0"` saat connect), dan publish aman dengan deteksi putus setengah mati
(`mqttPublishAman` + sambung ulang paksa setelah 3 kegagalan beruntun).

Status: kosong — siap diisi dari logika MQTT yang ada saat ini di
`src/main.cpp` dan `include/diagnostik.h`.
