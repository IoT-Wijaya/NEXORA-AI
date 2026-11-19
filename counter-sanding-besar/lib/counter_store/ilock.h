#pragma once

// Antarmuka penguncian bagian kritis.
// Di ESP32 diisi spin lock (EspSpinLock di src/),
// di komputer (test) diisi penggunaan yang tidak melakukan apa-apa.
class ILock
{
public:
    virtual ~ILock() = default;
    virtual void lock() = 0;
    virtual void unlock() = 0;
};
