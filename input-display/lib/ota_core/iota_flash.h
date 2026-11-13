#pragma once
#include <stddef.h>
#include <stdint.h>

// Penulis slot OTA cadangan. Adaptor perangkat (src/esp_ota_flash.h) membungkus
// esp_ota_*; tes memakai flash palsu di memori.
class IOtaFlash
{
public:
    virtual ~IOtaFlash() {}

    // Memilih slot cadangan dan menghapus area sebesar imageSize.
    virtual bool begin(uint32_t imageSize) = 0;
    virtual bool write(const uint8_t *data, size_t len) = 0;
    // Memvalidasi image yang tertulis. Handle dilepas apa pun hasilnya.
    virtual bool end() = 0;
    // Membuang tulisan yang belum selesai. Aman dipanggil kapan saja.
    virtual void abort() = 0;
    // Mengalihkan boot ke slot baru. Hanya setelah end() berhasil.
    virtual bool setBootToNew() = 0;
    virtual const char *lastError() const = 0;
};