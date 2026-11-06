#pragma once
#include <stddef.h>
#include <stdint.h>

// Alamat IPv4 sederhana, tanpa bergantung pada tipe IPAddress milik Arduino.
struct Ip4
{
    uint8_t b[4];
};

// Menulis "a.b.c.d" ke buffer. Aman terhadap buffer kecil (terpotong, tetap diakhiri NUL).
void formatIp(const Ip4 &ip, char *out, size_t size);

// Antarmuka perangkat keras jaringan. Di perangkat diisi ENC28J60,
// di komputer (test) diisi pengganti.
class INetDriver
{
public:
    virtual ~INetDriver() = default;
    virtual void begin() = 0;    // inisialisasi SPI dan chip
    virtual bool linkUp() = 0;   // kabel terpasang
    virtual bool hasIP() = 0;    // sudah punya alamat IP
    virtual bool tryDhcp() = 0;  // coba DHCP, true jika berhasil (boleh memblok)
    virtual void maintain() = 0; // perpanjangan lease DHCP
    virtual Ip4 localIP() = 0;
};