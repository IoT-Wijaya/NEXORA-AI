#include "lcd_view.h"
#include <stdio.h>

namespace LcdFormat
{

    void counterLine(char *out, size_t size, const LcdState &s)
    {
        // Karakter terakhir: '*' = sedang menghitung, '-' = berhenti.
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

    void countingLine(char *out, size_t size, bool counting)
    {
        snprintf(out, size, "HITUNG:%s", counting ? "AKTIF" : "BERHENTI");
    }

    void pad(char *out, size_t size, const char *text)
    {
        snprintf(out, size, "%-*s", (int)(size - 1), text);
    }

} // namespace LcdFormat

LcdView::LcdView(ILcdDisplay &display, const char *machineName, uint32_t pageIntervalMs, uint8_t rows)
    : display_(display), machineName_(machineName), pageIntervalMs_(pageIntervalMs), rows_(rows),
      machinePage_(false), lastChangeMs_(0) {}

void LcdView::begin() { display_.begin(); }

void LcdView::writePadded(uint8_t row, const char *text)
{
    char line[LCD_LINE_SIZE];
    LcdFormat::pad(line, sizeof(line), text);
    display_.writeLine(row, line);
}

void LcdView::showMessage(const char *line1, const char *line2)
{
    writePadded(0, line1);
    writePadded(1, line2);
    for (uint8_t row = 2; row < rows_; ++row)
        writePadded(row, ""); // hapus sisa tampilan lama
}

void LcdView::render(const LcdState &s)
{
    char raw[LCD_LINE_SIZE];

    LcdFormat::counterLine(raw, sizeof(raw), s);
    writePadded(0, raw);

    if (rows_ >= LCD_ROWS_SEMUA_SEKALIGUS)
    {
        LcdFormat::machineLine(raw, sizeof(raw), machineName_);
        writePadded(1, raw);
        LcdFormat::networkLine(raw, sizeof(raw), s.lanOk, s.mqttOk);
        writePadded(2, raw);
        LcdFormat::countingLine(raw, sizeof(raw), s.counting);
        writePadded(3, raw);
        return;
    }

    if (machinePage_)
    {
        LcdFormat::machineLine(raw, sizeof(raw), machineName_);
    }
    else
    {
        LcdFormat::networkLine(raw, sizeof(raw), s.lanOk, s.mqttOk);
    }
    writePadded(1, raw);
}

void LcdView::restartPageTimer(uint32_t nowMs)
{
    machinePage_ = false;
    lastChangeMs_ = nowMs;
}

bool LcdView::advancePageIfDue(uint32_t nowMs)
{
    if (rows_ >= LCD_ROWS_SEMUA_SEKALIGUS)
        return false; // semua baris sudah tampil, tidak ada halaman
    // Selisih unsigned tetap benar walau millis() melewati batas 32 bit.
    if (nowMs - lastChangeMs_ < pageIntervalMs_)
        return false;
    lastChangeMs_ = nowMs;
    machinePage_ = !machinePage_;
    return true;
}