#pragma once
#include <EthernetENC.h>

#include "firmware_downloader.h"

// Adaptor perangkat: membungkus EthernetClient (EthernetENC) menjadi IHttpTransport.
//
// PENTING: stack uIP milik EthernetENC tidak aman dipakai dari dua task sekaligus.
// OTA berjalan pada boot bersih (sebelum sensorTask dan networkTask dibuat), jadi
// hanya satu thread yang memakai Ethernet. Jangan pakai objek ini dari task lain
// selagi networkTask dan MQTT berjalan.
class EthernetHttpTransport : public IHttpTransport
{
public:
    explicit EthernetHttpTransport(uint16_t connectTimeoutMs = 5000)
    {
        client_.setConnectionTimeout(connectTimeoutMs);
    }

    // Memblokir paling lama sebesar batas waktu sambung + waktu DNS.
    bool connect(const char *host, uint16_t port) override
    {
        client_.stop();
        return client_.connect(host, port) == 1;
    }

    size_t write(const uint8_t *data, size_t len) override { return client_.write(data, len); }

    // EthernetENC: 0 = belum ada data, -1 = tidak tersambung. Keduanya kita
    // laporkan sebagai 0; putusnya koneksi dibaca lewat connected().
    int read(uint8_t *buf, size_t cap) override
    {
        const int n = client_.read(buf, cap);
        return n > 0 ? n : 0;
    }

    // false hanya setelah lawan menutup DAN seluruh data sudah terbaca.
    bool connected() override { return client_.connected() != 0; }

    void stop() override { client_.stop(); }

private:
    EthernetClient client_;
};