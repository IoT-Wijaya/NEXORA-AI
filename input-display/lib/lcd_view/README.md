# `lib/lcd_view/`

Tampilan LCD I2C 20x4 untuk mesin status UI 7-state:

1. `STATE_FETCH_API`
2. `STATE_PILIH_DATA`
3. `STATE_PILIH_SISI`
4. `STATE_SANDING_RUNNING`
5. `STATE_KOREKSI_MANUAL`
6. `STATE_VALIDASI_SELISIH`
7. `STATE_KIRIM_DATA`
8. `STATE_KONFIRMASI_LAGI`

Tiap state memiliki fungsi `updateDisplay*` sendiri yang hanya menggambar saat
data berubah (pelacakan `lastModal`, `lastMasuk`, dst. untuk mengurangi
berkayanya LCD).

Status: kosong — siap diisi dari fungsi `updateDisplay*` yang ada saat ini di
`src/main.cpp`.
