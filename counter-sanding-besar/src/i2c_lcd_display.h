#pragma once
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "sanding_config.h"
#include "../lib/lcd_view/ilcd_display.h"

// Driver LCD I2C 20x4 melalui LiquidCrystal_I2C (implementasi ILcdDisplay).
// Baris: 0..3, kolom: 0..19.
class I2cLcdDisplay : public ILcdDisplay
{
public:
    I2cLcdDisplay() : lcd_(config::LCD_ADDRESS, config::LCD_COLUMNS, config::LCD_ROWS) {}

    void begin() override
    {
        Wire.begin();
        lcd_.init();
        lcd_.backlight();
        lcd_.clear();
    }

    void writeLine(uint8_t row, const char *text) override
    {
        lcd_.setCursor(0, row);
        lcd_.print(text);
    }

private:
    LiquidCrystal_I2C lcd_;
};
