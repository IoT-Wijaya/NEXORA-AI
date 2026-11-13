# `lib/net_link/`

Enkapsulasi modul Ethernet W5500 di balik antarmuka yang stabil: inisialisasi
SPI + `Ethernet.begin`, pemantauan status link (`LinkOFF`), pergantian alamat
DHCP (`Ethernet.maintain`), dan reset modul W5500 saat gangguan berkelanjutan.

Catatan hardware: W5500 pada input-display pakai SPI SCK=18, MISO=23, MOSI=19,
CS=16 (lihat `include/config.h`).

Status: kosong — siap diisi dari logika `cekEthernet`, `resetEthernet`,
`pantauJaringan`, dan rekan-rekannya di `src/main.cpp`.
