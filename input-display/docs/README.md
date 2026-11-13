# Dokumentasi `input-display` (HMI Sanding)

Berkas-berkas dokumentasi untuk proyek HMI penghitung sanding (LCD 20x4 +
rotary encoder, Ethernet W5500, MQTT + HTTP/HTTPS ke Laravel, OTA).

## Daftar isian yang disarankan

| Berkas | Isi |
|--------|-----|
| `hardware.md` | Rangkaian pin W5500 (SPI), LCD I2C, rotary encoder, tombol OK/BACK, dan pemasangan pull-up eksternal (GPIO34/35 input-only, tidak punya pull-up internal). |
| `wiring.md` | Skema sambungan antar mesin sanding besar/kecil dan HMI. |
| `build.md` | Cara build dan upload (`pio run -e input-display`, `pio run -e input-display -t upload`), serial monitor 115200 baud. |
| `deploy.md` | Proses rilis firmware (tag GitHub / Laravel OTA server), penomoran versi `FW_VERSION`, dan `partitions.csv`. |
| `ota.md` | Alur OTA: pengecekan versi, unduhan, verifikasi SHA-256, dan rollback. |
| `mqtt.md` | Daftar lengkap topik MQTT HMI dan sensor (format payload JSON). |
| `ui.md` | Mesin status UI 7-state (`FETCH_API`, `PILIH_DATA`, `PILIH_SISI`, `SANDING_RUNNING`, `KOREKSI_MANUAL`, `VALIDASI_SELISIH`, `KIRIM_DATA`, `KONFIRMASI_LAGI`). |
| `konfigurasi.md` | Halaman konfigurasi browser di port 8080, perintah Serial Monitor (`setkey`, `setotakey`, `status`, `hwtest`, dst.), dan variabel yang disimpan di NVS. |

## Contoh perintah development

```sh
# Build firmware
pio run -e input-display

# Upload ke ESP32 NodeMCU-32S
pio run -e input-display -t upload

# Serial monitor 115200 baud
pio device monitor -e input-display -b 115200

# Unit test native (host)
pio run -e native
```

Semua komentar dan pengidentifikasi dalam kode menggunakan Bahasa Indonesia
(lihat `CLAUDE.md` di root monorepo).
