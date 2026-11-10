#pragma once
#include <stdint.h>

// Jam dan tidur. Di perangkat: millis() dan delay(); di tes: jam palsu yang
// maju sendiri saat sleepMs() dipanggil, sehingga tes tidak menunggu waktu nyata.
class IClock
{
public:
    virtual ~IClock() {}
    virtual uint32_t nowMs() = 0;
    virtual void sleepMs(uint32_t ms) = 0;
};