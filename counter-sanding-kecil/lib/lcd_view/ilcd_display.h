#pragma once
#include <stdint.h>

// Antarmuka layar karakter. Di perangkat diisi LCD I2C, di komputer (test) diisi pengganti.
class ILcdDisplay
{
public:
    virtual ~ILcdDisplay() = default;
    virtual void begin() = 0;                                  // inisialisasi, lampu latar, hapus layar
    virtual void writeLine(uint8_t row, const char *text) = 0; // text sudah selebar kolom
};