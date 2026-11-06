#pragma once
#include <stdint.h>

// Sumber MAC. Di perangkat diisi eFuse chip, di komputer (test) diisi pengganti.
class IMacSource
{
public:
    virtual ~IMacSource() = default;
    virtual bool readMac(uint8_t out[6]) = 0;
};