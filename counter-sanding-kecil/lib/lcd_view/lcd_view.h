#pragma once
#include "sanding_config.h"
#include <stddef.h>
#include <stdint.h>
#include "ilcd_display.h"

constexpr size_t LCD_LINE_SIZE = config::LCD_COLUMNS + 1; // kolom + NUL

// Layar dengan baris sebanyak ini atau lebih menampilkan semua informasi sekaligus (tanpa ganti halaman).
constexpr uint8_t LCD_ROWS_SEMUA_SEKALIGUS = 4;

// Semua yang ditampilkan di layar, dikumpulkan dalam satu struct.
struct LcdState
{
    uint32_t hasil;
    uint32_t modal;
    bool counting; // true = sedang menghitung ('*'), false = berhenti ('-')
    bool lanOk;
    bool mqttOk;
};

// Fungsi format murni, dibuka agar bisa diuji satu per satu.
namespace LcdFormat
{
    void counterLine(char *out, size_t size, const LcdState &s);
    void machineLine(char *out, size_t size, const char *machineName);
    void networkLine(char *out, size_t size, bool lanOk, bool mqttOk);
    void countingLine(char *out, size_t size, bool counting); // "HITUNG:AKTIF" / "HITUNG:BERHENTI"
    // Rata kiri, sisa kolom diisi spasi (menghapus sisa karakter lama); teks yang
    // terlalu panjang dipotong. size = jumlah kolom + 1.
    void pad(char *out, size_t size, const char *text);
} // namespace LcdFormat

// Layar 2 baris: baris 1 selalu angka, baris 2 bergantian antara nama mesin dan status jaringan.
// Layar 4 baris: angka, nama mesin, status jaringan, status hitung, semuanya tampil bersamaan.
class LcdView
{
public:
    // rows diisi dari config; parameter ini ada agar kedua tata letak bisa diuji di komputer.
    LcdView(ILcdDisplay &display, const char *machineName, uint32_t pageIntervalMs,
            uint8_t rows = config::LCD_ROWS);

    void begin();                                           // menyalakan layar
    void showMessage(const char *line1, const char *line2); // baris lain dikosongkan
    void render(const LcdState &state);                     // menggambar ulang semua baris

    void restartPageTimer(uint32_t nowMs);
    // true jika halaman baru saja berganti. Pemanggil lalu memanggil render().
    // Selalu false pada layar 4 baris (tidak ada halaman).
    bool advancePageIfDue(uint32_t nowMs);

private:
    void writePadded(uint8_t row, const char *text);

    ILcdDisplay &display_;
    const char *machineName_;
    const uint32_t pageIntervalMs_;
    const uint8_t rows_;
    bool machinePage_;
    uint32_t lastChangeMs_;
};