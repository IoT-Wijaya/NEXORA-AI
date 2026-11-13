# `lib/app/`

Perakitan task aplikasi: susunan `setup()` dan `loop()` (watchdog task ESP32,
pengukur loop lag, jaga waktu NTP, halaman konfigurasi browser port 8080,
OTA berkala, mesin status UI), yang memanggil modul-modul di `lib/` yang lain.

Target akhir (bertahap): `src/main.cpp` disusutkan hingga sekitar 30 baris
yang hanya melakukan inisialisasi dan memanggil task di `lib/app/`.

Status: kosong — `main.cpp` saat ini masih merupakan satu berkas monolitik
(sekitar 1800 baris) yang menampung semua tanggung jawab di atas.
