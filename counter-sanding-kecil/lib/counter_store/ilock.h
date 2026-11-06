#pragma once

// Antarmuka penguncian bagian kritis. Di ESP32 diisi spinlock,
// di komputer (test) diisi pengganti yang tidak melakukan apa-apa.
class ILock
{
public:
    virtual ~ILock() = default;
    virtual void lock() = 0;
    virtual void unlock() = 0;
};