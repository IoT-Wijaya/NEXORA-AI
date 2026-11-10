#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "firmware_downloader.h"

// Menampung byte ke buffer RAM berukuran tetap. Dipakai untuk mengunduh
// manifest kecil. Menolak (write -> false) bila isi akan melebihi kapasitas,
// tanpa menyimpan sebagian pun dari potongan yang tidak muat.
class BufferSink : public IByteSink
{
public:
    BufferSink(uint8_t *buffer, size_t capacity) : buf_(buffer), cap_(capacity) {}

    bool write(const uint8_t *data, size_t len) override
    {
        if (len > cap_ - len_)
            return false;
        if (len > 0)
            memcpy(buf_ + len_, data, len);
        len_ += len;
        return true;
    }

    void reset() { len_ = 0; }
    const uint8_t *data() const { return buf_; }
    size_t size() const { return len_; }

private:
    uint8_t *buf_;
    size_t cap_;
    size_t len_ = 0;
};