#pragma once
#include <stdint.h>

// Penyaring (debounce) untuk satu sensor digital.
// Logika murni: tidak memanggil digitalRead() atau millis(), sehingga bisa
// diuji di komputer. Level pin dan waktu diberikan oleh pemanggil.
class SensorFilter
{
public:
    SensorFilter(bool activeLevel, uint32_t pressStableMs, uint32_t releaseStableMs);

    // Panggil sekali saat boot dengan level pin saat itu.
    void begin(bool initialLevel, uint32_t nowMs);

    // Panggil berkala. Mengembalikan true SATU KALI setiap ada hitungan baru.
    bool update(bool rawLevel, uint32_t nowMs);

private:
    const bool activeLevel_;
    const uint32_t pressStableMs_;
    const uint32_t releaseStableMs_;
    bool state_;            // status stabil setelah filter
    bool lastRaw_;          // pembacaan mentah terakhir
    uint32_t changeTimeMs_; // kapan pembacaan mentah terakhir berubah
};