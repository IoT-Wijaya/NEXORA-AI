#include "sensor_filter.h"

SensorFilter::SensorFilter(bool activeLevel, uint32_t pressStableMs, uint32_t releaseStableMs)
    : activeLevel_(activeLevel),
      pressStableMs_(pressStableMs),
      releaseStableMs_(releaseStableMs),
      state_(!activeLevel),
      lastRaw_(!activeLevel),
      changeTimeMs_(0) {}

void SensorFilter::begin(bool initialLevel, uint32_t nowMs)
{
    state_ = initialLevel;
    lastRaw_ = initialLevel;
    changeTimeMs_ = nowMs;
}

bool SensorFilter::update(bool rawLevel, uint32_t nowMs)
{
    // Pembacaan mentah berubah: mulai hitung ulang waktu stabil.
    if (rawLevel != lastRaw_)
    {
        lastRaw_ = rawLevel;
        changeTimeMs_ = nowMs;
    }

    // Selisih unsigned tetap benar walau millis() melewati batas 32 bit.
    const uint32_t stableFor = nowMs - changeTimeMs_;
    const bool rawActive = (rawLevel == activeLevel_);
    const bool stateActive = (state_ == activeLevel_);

    if (rawActive && !stateActive)
    {
        // Aktif dan stabil cukup lama: hitung 1.
        if (stableFor >= pressStableMs_)
        {
            state_ = activeLevel_;
            return true;
        }
    }
    else if (!rawActive && stateActive)
    {
        // Lepas dan stabil cukup lama: siap menghitung lagi.
        if (stableFor >= releaseStableMs_)
        {
            state_ = rawLevel;
        }
    }
    return false;
}