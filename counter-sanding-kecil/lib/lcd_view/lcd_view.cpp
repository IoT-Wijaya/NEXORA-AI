#include "lcd_view.h"
#include <stdio.h>

namespace LcdFormat
{

    void counterLine(char *out, size_t size, const LcdState &s)
    {
        // Karakter terakhir: '*' = sedang menghitung, '-' = berhenti (menunggu START).
        snprintf(out, size, "M:%5lu H:%5lu%c", (unsigned long)s.modal, (unsigned long)s.hasil,
                 s.counting ? '*' : '-');
    }

    void machineLine(char *out, size_t size, const char *machineName)
    {
        snprintf(out, size, "MESIN:%s", machineName);
    }

    void networkLine(char *out, size_t size, bool lanOk, bool mqttOk)
    {
        snprintf(out, size, "LAN:%s MQTT:%s", lanOk ? "OK" : "NO", mqttOk ? "OK" : "NO");
    }

    void pad(char *out, size_t size, const char *text)
    {
        snprintf(out, size, "%-*s", (int)(size - 1), text);
    }

} // namespace LcdFormat

LcdView::LcdView(ILcdDisplay &display, const char *machineName, uint32_t pageIntervalMs)
    : display_(display), machineName_(machineName), pageIntervalMs_(pageIntervalMs),
      machinePage_(false), lastChangeMs_(0) {}

void LcdView::begin() { display_.begin(); }

void LcdView::showMessage(const char *line1, const char *line2)
{
    char line[LCD_LINE_SIZE];
    LcdFormat::pad(line, sizeof(line), line1);
    display_.writeLine(0, line);
    LcdFormat::pad(line, sizeof(line), line2);
    display_.writeLine(1, line);
}

void LcdView::render(const LcdState &s)
{
    char raw[LCD_LINE_SIZE];
    char line[LCD_LINE_SIZE];

    LcdFormat::counterLine(raw, sizeof(raw), s);
    LcdFormat::pad(line, sizeof(line), raw);
    display_.writeLine(0, line);

    if (machinePage_)
    {
        LcdFormat::machineLine(raw, sizeof(raw), machineName_);
    }
    else
    {
        LcdFormat::networkLine(raw, sizeof(raw), s.lanOk, s.mqttOk);
    }
    LcdFormat::pad(line, sizeof(line), raw);
    display_.writeLine(1, line);
}

void LcdView::restartPageTimer(uint32_t nowMs)
{
    machinePage_ = false;
    lastChangeMs_ = nowMs;
}

bool LcdView::advancePageIfDue(uint32_t nowMs)
{
    // Selisih unsigned tetap benar walau millis() melewati batas 32 bit.
    if (nowMs - lastChangeMs_ < pageIntervalMs_)
        return false;
    lastChangeMs_ = nowMs;
    machinePage_ = !machinePage_;
    return true;
}