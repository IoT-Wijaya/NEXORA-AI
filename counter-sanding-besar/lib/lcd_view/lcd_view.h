#pragma once
#include "sanding_config.h"
#include <stddef.h>
#include <stdint.h>
#include "ilcd_display.h"

constexpr size_t LCD_LINE_SIZE = config::LCD_COLUMNS + 1; // kolom + NUL

// Semua yang ditampilkan di layar, dikumpulkan dalam satu struct.
struct LcdState
{
    uint32_t hasil;
    uint32_t modal;
    bool counting; // true = sedang menghitung ('*'), false = menunggu START ('-')
    bool lanOk;
    bool mqttOk;
    const char *ip;      // "a.b.c.d" atau nullptr/"" jika belum diketahui
    bool modeAktif;      // true = ON (menghitung), false = OFF
};

// Fungsi format murni, dibuka agar bisa diuji satu per satu.
namespace LcdFormat
{
    void counterLine(char *out, size_t size, const LcdState &s);
    void machineLine(char *out, size_t size, const char *machineName);
    void networkLine(char *out, size_t size, bool lanOk, bool mqttOk);
    void ipLine(char *out, size_t size, const char *ip);
    void modeLine(char *out, size_t size, bool aktif);
    // Rata kiri, sisa kolom diisi spasi (menghapus sisa karakter lama); teks yang
    // terlalu panjang dipotong. size = jumlah kolom + 1.
    void pad(char *out, size_t size, const char *text);
} // namespace LcdFormat

// Baris 1 selalu angka. Baris 2 bergantian antara nama mesin dan status jaringan.
// Baris 3 menampilkan alamat IP. Baris 4 menampilkan mode (ON/OFF).
class LcdView
{
public:
    LcdView(ILcdDisplay &display, const char *machineName, uint32_t pageIntervalMs);

    void begin(); // menyalakan layar
    void showMessage(const char *line1, const char *line2);
    void render(const LcdState &state); // menggambar ulang semua baris

    void restartPageTimer(uint32_t nowMs);
    // true jika halaman baris 2 baru saja berganti. Pemanggil lalu memanggil render().
    bool advancePageIfDue(uint32_t nowMs);

private:
    ILcdDisplay &display_;
    const char *machineName_;
    const uint32_t pageIntervalMs_;
    bool machinePage_;
    uint32_t lastChangeMs_;
};
