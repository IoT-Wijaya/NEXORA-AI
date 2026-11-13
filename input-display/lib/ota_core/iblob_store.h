#pragma once
#include <stddef.h>
#include <stdint.h>

// Penyimpanan byte mentah per kunci. Di perangkat diisi NVS, di komputer (test) diisi pengganti.
class IBlobStore
{
public:
    virtual ~IBlobStore() = default;

    // Mengembalikan jumlah byte yang tersimpan (isi hanya disalin jika muat di `capacity`),
    // atau -1 jika kunci tidak ada.
    virtual int read(const char *key, uint8_t *buffer, size_t capacity) = 0;
    virtual bool write(const char *key, const uint8_t *data, size_t length) = 0;
};