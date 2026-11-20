#pragma once
#include <stdint.h>
#include <string.h>
#include "secrets.h"
#include "../lib/device_identity/imac_source.h"

// Sumber MAC address dari kunci rahasia compile-time (include/secrets.h).
// Digunakan W5500 yang memerlukan MAC statis yang unik di jaringan.
class FixedMacSource : public IMacSource
{
public:
    bool readMac(uint8_t out[6]) override
    {
        const uint8_t m[6] = DEVICE_MAC;
        for (int i = 0; i < 6; i++)
            out[i] = m[i];
        return true;
    }
};
