#pragma once
#include <stdint.h>

// Sumber MAC address. Di perangkat diisi dari stiker/kunci rahasia,
// di komputer (test) diisi penggunaan palsu.
class IMacSource
{
public:
    virtual ~IMacSource() = default;
    virtual bool readMac(uint8_t out[6]) = 0;
};
