# Direktori `include/`

Header yang dibagi antar file sumber proyek (`src/`). Semua dicakupan oleh
`build_flags = -Iinclude` di `platformio.ini`, sehingga disertakan dengan
`#include "nama.h"` dari mana saja.

| Berkas | Keterangan |
|--------|------------|
| `config.h` | Pin perangkat, konstanta waktu, topik MQTT, versi firmware, identitas mesin (senggak boleh ditimpa lewat `-D` di `platformio.ini`). |
| `diagnostik.h` | Log berlevel, statistik MQTT/HTTP, publish aman, pengukur loop lag, perintah serial + MQTT, dan mode tes hardware (tombol, rotary encoder, LCD I2C, W5500, pemindaian I2C). |
| `trust_anchors.h` | Anchor sertifikat CA (BearSSL format, hasil `pycert_bearssl.py`) untuk verifikasi TLS/HTTPS. Digenerate, **jangan diubah manual**. |
| `ota_github.h` | Implementasi OTA alternatif via GitHub Release (`HTTPClient` + `Update`). **Belum terintegrasi** ke `main.cpp` (main.cpp punya implementasi OTA sendiri lewat Laravel). |
| `secrets.h` | Rahasia lokal (API key, OTA key, server, MAC contoh). **Tidak dikommit** — tercatat di `.gitignore` (`**/include/secrets.h`). |
| `secrets.example.h` | Contoh `secrets.h` dengan nilai placeholder, aman untuk dikommit. |

Contoh pemakaian di `src/main.cpp`:

```cpp
#include "config.h"
#include "diagnostik.h"
```
