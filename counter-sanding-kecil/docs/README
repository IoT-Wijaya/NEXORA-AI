## Struktur Proyek

```text
sanding-kecil/
├─ platformio.ini                    (1)
├─ partitions.csv                    (10)
├─ .gitignore                        (1)
├─ include/
│  ├─ config.h                       (2)  pin, waktu, topic
│  ├─ secrets.h                           rahasia, tidak masuk git
│  └─ secrets.example.h              (1)
├─ src/
│  └─ main.cpp                       (1)  dikecilkan bertahap sampai ±30 baris
├─ lib/
│  ├─ sensor_filter/                 (3)  debounce, logika murni
│  ├─ counter_store/                 (4)  counter + bendera dirty
│  ├─ command_parser/                (5)  teks "START" jadi enum
│  ├─ net_link/                      (6)  ENC28J60 di balik antarmuka
│  ├─ mqtt_service/                  (7)  publish, subscribe, heartbeat, LWT
│  ├─ lcd_view/                      (8)  tampilan LCD
│  ├─ device_identity/               (9)  MAC dari eFuse, konfigurasi NVS
│  └─ app/                           (10) perakitan task
├─ test/
│  ├─ test_smoke/                    (1)
│  ├─ test_sensor_filter/            (3)
│  ├─ test_counter_store/            (4)
│  └─ test_command_parser/           (5)
└─ docs/                                  terakhir
```
