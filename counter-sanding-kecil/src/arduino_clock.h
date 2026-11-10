#pragma once
#include <Arduino.h>

#include "iclock.h"

// Adaptor perangkat: IClock di atas millis() dan delay().
class ArduinoClock : public IClock
{
public:
    uint32_t nowMs() override { return millis(); }
    void sleepMs(uint32_t ms) override { delay(ms); }
};