#pragma once
#include <Arduino.h>
#include "ilock.h"

class EspSpinLock : public ILock
{
public:
    void lock() override { portENTER_CRITICAL(&mux_); }
    void unlock() override { portEXIT_CRITICAL(&mux_); }

private:
    portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
};