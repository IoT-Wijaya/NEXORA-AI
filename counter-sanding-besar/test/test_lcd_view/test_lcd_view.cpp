#include <string>
#include <string.h>
#include <unity.h>
#include "lcd_view.h"

static const char *NAMA = "SANDING BESAR";
static const uint32_t HALAMAN = 30000;

struct Tulis
{
    int baris;
    std::string teks;
};

class FakeDisplay : public ILcdDisplay
{
public:
    void begin() override { beginCalls++; }
    void writeLine(uint8_t row, const char *text) override
    {
        if (tulisCount < 64)
        {
            daftar[tulisCount].baris = (int)row;
            daftar[tulisCount].teks = text ? text : "";
            tulisCount++;
        }
    }
    int beginCalls = 0;
    int tulisCount = 0;
    Tulis daftar[64];
};

static LcdState keadaan(uint32_t hasil, uint32_t modal, bool hitung, bool lan, bool mqtt, const char *ip, bool mode)
{
    return LcdState{hasil, modal, hitung, lan, mqtt, ip, mode};
}

static std::string pad20(const char *s)
{
    std::string r = s ? s : "";
    r.resize(20, ' ');
    return r;
}

void setUp() {}
void tearDown() {}

void test_baris_angka_saat_menghitung()
{
    char b[LCD_LINE_SIZE];
    LcdFormat::counterLine(b, sizeof(b), keadaan(345, 12, true, true, false, "192.168.1.10", true));
    TEST_ASSERT_EQUAL_STRING("M:   12 H:  345*", b);
    TEST_ASSERT_EQUAL(16, (int)strlen(b)); // counterLine tidak dipad; pad dipanggil render()
}

void test_baris_angka_saat_berhenti()
{
    char b[LCD_LINE_SIZE];
    LcdFormat::counterLine(b, sizeof(b), keadaan(0, 0, false, true, false, nullptr, false));
    TEST_ASSERT_EQUAL_STRING("M:    0 H:    0-", b);
    TEST_ASSERT_EQUAL(16, (int)strlen(b));
}

void test_baris_angka_lima_digit_pas_duapuluh_kolom()
{
    char b[LCD_LINE_SIZE];
    LcdFormat::counterLine(b, sizeof(b), keadaan(99999, 99999, true, true, false, nullptr, true));
    TEST_ASSERT_EQUAL_STRING("M:99999 H:99999*", b);
    TEST_ASSERT_EQUAL(16, (int)strlen(b));
}

void test_baris_nama_mesin()
{
    char b[LCD_LINE_SIZE];
    LcdFormat::machineLine(b, sizeof(b), NAMA);
    TEST_ASSERT_EQUAL_STRING("MESIN:SANDING BESAR", b);
    TEST_ASSERT_EQUAL(19, (int)strlen(b));
}

void test_baris_status_jaringan_semua_kombinasi()
{
    char b[LCD_LINE_SIZE];
    LcdFormat::networkLine(b, sizeof(b), true, true);
    TEST_ASSERT_EQUAL_STRING("LAN:OK MQTT:OK", b);
    LcdFormat::networkLine(b, sizeof(b), true, false);
    TEST_ASSERT_EQUAL_STRING("LAN:OK MQTT:NO", b);
    LcdFormat::networkLine(b, sizeof(b), false, false);
    TEST_ASSERT_EQUAL_STRING("LAN:NO MQTT:NO", b);
}

void test_baris_ip()
{
    char b[LCD_LINE_SIZE];
    LcdFormat::ipLine(b, sizeof(b), "192.168.1.10");
    TEST_ASSERT_EQUAL_STRING("IP:192.168.1.10", b);
    LcdFormat::ipLine(b, sizeof(b), nullptr);
    TEST_ASSERT_EQUAL_STRING("IP:--.--.--.--", b);
    LcdFormat::ipLine(b, sizeof(b), "");
    TEST_ASSERT_EQUAL_STRING("IP:--.--.--.--", b);
}

void test_baris_mode()
{
    char b[LCD_LINE_SIZE];
    LcdFormat::modeLine(b, sizeof(b), true);
    TEST_ASSERT_EQUAL_STRING("MODE: ON", b);
    LcdFormat::modeLine(b, sizeof(b), false);
    TEST_ASSERT_EQUAL_STRING("MODE: OFF", b);
}

void test_pad_mengisi_spasi_sampai_lebar_kolom()
{
    char b[LCD_LINE_SIZE];
    LcdFormat::pad(b, sizeof(b), "HI");
    TEST_ASSERT_EQUAL(20, (int)strlen(b));
    TEST_ASSERT_EQUAL('H', b[0]);
    TEST_ASSERT_EQUAL('I', b[1]);
    TEST_ASSERT_EQUAL(' ', b[2]);
    TEST_ASSERT_EQUAL(' ', b[19]);
}

void test_pad_teks_pas_dan_terlalu_panjang_dan_kosong()
{
    char b[LCD_LINE_SIZE];
    LcdFormat::pad(b, sizeof(b), "ABCDEFGHIJKLMNOPQRST");
    TEST_ASSERT_EQUAL_STRING("ABCDEFGHIJKLMNOPQRST", b);
    LcdFormat::pad(b, sizeof(b), "ABCDEFGHIJKLMNOPQRSTUV");
    TEST_ASSERT_EQUAL_STRING("ABCDEFGHIJKLMNOPQRST", b);
    LcdFormat::pad(b, sizeof(b), "");
    TEST_ASSERT_EQUAL_STRING(pad20("").c_str(), b);
}

void test_begin_menyalakan_layar_sekali()
{
    FakeDisplay d;
    LcdView v(d, NAMA, HALAMAN);
    v.begin();
    TEST_ASSERT_EQUAL(1, d.beginCalls);
}

void test_showMessage_menulis_dua_baris_selebar_kolom()
{
    FakeDisplay d;
    LcdView v(d, NAMA, HALAMAN);
    v.showMessage("SANDING BESAR", "STARTING...");
    TEST_ASSERT_EQUAL(2, d.tulisCount);
    TEST_ASSERT_EQUAL(0, d.daftar[0].baris);
    TEST_ASSERT_EQUAL_STRING(pad20("SANDING BESAR").c_str(), d.daftar[0].teks.c_str());
    TEST_ASSERT_EQUAL(1, d.daftar[1].baris);
    TEST_ASSERT_EQUAL_STRING(pad20("STARTING...").c_str(), d.daftar[1].teks.c_str());
}

void test_render_menulis_empat_baris_selebar_kolom()
{
    FakeDisplay d;
    LcdView v(d, NAMA, HALAMAN);
    v.restartPageTimer(0);
    v.render(keadaan(345, 12, true, true, false, "192.168.1.10", true));
    TEST_ASSERT_EQUAL(4, d.tulisCount);
    TEST_ASSERT_EQUAL(0, d.daftar[0].baris);
    // Baris 0: counter difit dari 16 ke 20 kolom oleh pad()
    TEST_ASSERT_EQUAL_STRING("M:   12 H:  345*    ", d.daftar[0].teks.c_str());
    TEST_ASSERT_EQUAL(20, (int)strlen(d.daftar[0].teks.c_str()));
    TEST_ASSERT_EQUAL(1, d.daftar[1].baris);
    TEST_ASSERT_EQUAL_STRING(pad20("LAN:OK MQTT:NO").c_str(), d.daftar[1].teks.c_str());
    TEST_ASSERT_EQUAL(20, (int)strlen(d.daftar[1].teks.c_str()));
    TEST_ASSERT_EQUAL(2, d.daftar[2].baris);
    TEST_ASSERT_EQUAL_STRING(pad20("IP:192.168.1.10").c_str(), d.daftar[2].teks.c_str());
    TEST_ASSERT_EQUAL(20, (int)strlen(d.daftar[2].teks.c_str()));
    TEST_ASSERT_EQUAL(3, d.daftar[3].baris);
    TEST_ASSERT_EQUAL_STRING(pad20("MODE: ON").c_str(), d.daftar[3].teks.c_str());
    TEST_ASSERT_EQUAL(20, (int)strlen(d.daftar[3].teks.c_str()));
}

void test_pergantian_halaman_tidak_meninggalkan_sisa_karakter()
{
    FakeDisplay d;
    LcdView v(d, NAMA, HALAMAN);
    v.restartPageTimer(0);
    v.advancePageIfDue(HALAMAN);
    v.render(keadaan(0, 0, false, true, true, nullptr, false));
    TEST_ASSERT_EQUAL_STRING(pad20("MESIN:SANDING BESAR").c_str(), d.daftar[1].teks.c_str());

    v.advancePageIfDue(2 * HALAMAN);
    v.render(keadaan(0, 0, false, true, true, nullptr, false));
    // Baris 1 kembali ke jaringan; kolom sisa dari "MESIN:..." (20 kolom) harus
    // ditimpa spasi, bukan sisa karakter "ESAR". Render kedua menambahkan entri
    // baru di indeks 4..7, jadi baris 1 render kedua ada di indeks 5.
    TEST_ASSERT_EQUAL_STRING(pad20("LAN:OK MQTT:OK").c_str(), d.daftar[5].teks.c_str());
}

void test_halaman_berganti_tiap_interval()
{
    FakeDisplay d;
    LcdView v(d, NAMA, HALAMAN);
    v.restartPageTimer(1000);
    TEST_ASSERT_FALSE(v.advancePageIfDue(1000 + HALAMAN - 1));
    TEST_ASSERT_TRUE(v.advancePageIfDue(1000 + HALAMAN));
    TEST_ASSERT_FALSE(v.advancePageIfDue(1000 + 2 * HALAMAN - 1));
    TEST_ASSERT_TRUE(v.advancePageIfDue(1000 + 2 * HALAMAN));
}

void test_pembanding_waktu_aman_saat_millis_melewati_batas()
{
    FakeDisplay d;
    LcdView v(d, NAMA, HALAMAN);
    const uint32_t t0 = 0xFFFFFFF0u;
    v.restartPageTimer(t0);
    TEST_ASSERT_FALSE(v.advancePageIfDue(t0 + HALAMAN - 1u));
    TEST_ASSERT_TRUE(v.advancePageIfDue(t0 + HALAMAN));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_baris_angka_saat_menghitung);
    RUN_TEST(test_baris_angka_saat_berhenti);
    RUN_TEST(test_baris_angka_lima_digit_pas_duapuluh_kolom);
    RUN_TEST(test_baris_nama_mesin);
    RUN_TEST(test_baris_status_jaringan_semua_kombinasi);
    RUN_TEST(test_baris_ip);
    RUN_TEST(test_baris_mode);
    RUN_TEST(test_pad_mengisi_spasi_sampai_lebar_kolom);
    RUN_TEST(test_pad_teks_pas_dan_terlalu_panjang_dan_kosong);
    RUN_TEST(test_begin_menyalakan_layar_sekali);
    RUN_TEST(test_showMessage_menulis_dua_baris_selebar_kolom);
    RUN_TEST(test_render_menulis_empat_baris_selebar_kolom);
    RUN_TEST(test_pergantian_halaman_tidak_meninggalkan_sisa_karakter);
    RUN_TEST(test_halaman_berganti_tiap_interval);
    RUN_TEST(test_pembanding_waktu_aman_saat_millis_melewati_batas);
    return UNITY_END();
}
