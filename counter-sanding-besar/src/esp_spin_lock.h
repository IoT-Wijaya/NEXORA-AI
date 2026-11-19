#pragma once
#include <Arduino.h>
#include "../lib/counter_store/ilock.h"

// Implementasi mutex spin lock ESP32 (portENTER_CRITICAL / portEXIT_CRITICAL).
// Digunakan CounterStore untuk melindungi counter antar dua inti FreeRTOS.
class EspSpinLock : public ILock
{
public:
    void lock() override { portENTER_CRITICAL(&mux_); }
    void unlock() override { portEXIT_CRITICAL(&mux_); }

private:
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
};
