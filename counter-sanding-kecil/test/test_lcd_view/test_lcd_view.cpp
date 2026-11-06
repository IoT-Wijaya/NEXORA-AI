#include <string.h>
#include <string>
#include <vector>
#include <unity.h>
#include "lcd_view.h"

struct Tulis
{
    int baris;
    std::string teks;
};

class FakeDisplay : public ILcdDisplay
{
public:
    void begin() override { beginCalls++; }
    void writeLine(uint8_t row, const char *text) override { tulis.push_back({row, text}); }
    int beginCalls = 0;
    std::vector<Tulis> tulis;
};

static const char *NAMA = "SANDINGKCL";
static const uint32_t HALAMAN = 30000;

static std::string pad16(const std::string &s)
{
    std::string r = s;
    r.resize(16, ' ');
    return r;
}

static LcdState keadaan(uint32_t hasil, uint32_t modal, bool hitung, bool lan, bool mqtt)
{
    return LcdState{hasil, modal, hitung, lan, mqtt};
}

void setUp() {}
void tearDown() {}

void test_baris_angka_saat_menghitung()
{
    char b[LCD_LINE_SIZE];
    LcdFormat::counterLine(b, sizeof(b), keadaan(345, 12, true, true, true));
    TEST_ASSERT_EQUAL_STRING("M:   12 H:  345*", b);
}

void test_baris_angka_saat_berhenti()
{
    char b[LCD_LINE_SIZE];
    LcdFormat::counterLine(b, sizeof(b), keadaan(0, 0, false, true, true));
    TEST_ASSERT_EQUAL_STRING("M:    0 H:    0-", b);
}

void test_baris_angka_lima_digit_pas_enam_belas_kolom()
{
    char b[LCD_LINE_SIZE];
    LcdFormat::counterLine(b, sizeof(b), keadaan(99999, 99999, true, true, true));
    TEST_ASSERT_EQUAL_STRING("M:99999 H:99999*", b);
}

void test_batasan_diketahui_enam_digit_memotong_penanda_status()
{
    // Perilaku kode asli: pada angka >= 100000 baris melebihi 16 kolom,
    // sehingga karakter terakhir ('*' atau '-') terpotong.
    char b[LCD_LINE_SIZE];
    LcdFormat::counterLine(b, sizeof(b), keadaan(5, 100000, true, true, true));
    TEST_ASSERT_EQUAL_STRING("M:100000 H:    5", b);
    TEST_ASSERT_EQUAL(16, (int)strlen(b));
}

void test_baris_nama_mesin()
{
    char b[LCD_LINE_SIZE];
    LcdFormat::machineLine(b, sizeof(b), NAMA);
    TEST_ASSERT_EQUAL_STRING("MESIN:SANDINGKCL", b);
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

void test_pad_mengisi_spasi_sampai_lebar_kolom()
{
    char b[LCD_LINE_SIZE];
    LcdFormat::pad(b, sizeof(b), "HI");
    TEST_ASSERT_EQUAL(16, (int)strlen(b));
    TEST_ASSERT_EQUAL('H', b[0]);
    TEST_ASSERT_EQUAL('I', b[1]);
    TEST_ASSERT_EQUAL(' ', b[2]);
    TEST_ASSERT_EQUAL(' ', b[15]);
}

void test_pad_teks_pas_dan_terlalu_panjang_dan_kosong()
{
    char b[LCD_LINE_SIZE];
    LcdFormat::pad(b, sizeof(b), "ABCDEFGHIJKLMNOP");
    TEST_ASSERT_EQUAL_STRING("ABCDEFGHIJKLMNOP", b);
    LcdFormat::pad(b, sizeof(b), "ABCDEFGHIJKLMNOPQRSTUV");
    TEST_ASSERT_EQUAL_STRING("ABCDEFGHIJKLMNOP", b);
    LcdFormat::pad(b, sizeof(b), "");
    TEST_ASSERT_EQUAL_STRING(pad16("").c_str(), b);
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
    v.showMessage("SANDING KECIL", "STARTING...");
    TEST_ASSERT_EQUAL(2, (int)d.tulis.size());
    TEST_ASSERT_EQUAL(0, d.tulis[0].baris);
    TEST_ASSERT_EQUAL_STRING(pad16("SANDING KECIL").c_str(), d.tulis[0].teks.c_str());
    TEST_ASSERT_EQUAL(1, d.tulis[1].baris);
    TEST_ASSERT_EQUAL_STRING(pad16("STARTING...").c_str(), d.tulis[1].teks.c_str());
}

void test_render_awal_menampilkan_halaman_jaringan()
{
    FakeDisplay d;
    LcdView v(d, NAMA, HALAMAN);
    v.restartPageTimer(0);
    v.render(keadaan(345, 12, true, true, false));
    TEST_ASSERT_EQUAL(2, (int)d.tulis.size());
    TEST_ASSERT_EQUAL(0, d.tulis[0].baris);
    TEST_ASSERT_EQUAL_STRING("M:   12 H:  345*", d.tulis[0].teks.c_str());
    TEST_ASSERT_EQUAL(1, d.tulis[1].baris);
    TEST_ASSERT_EQUAL_STRING(pad16("LAN:OK MQTT:NO").c_str(), d.tulis[1].teks.c_str());
}

void test_render_selalu_menulis_baris_selebar_kolom()
{
    FakeDisplay d;
    LcdView v(d, NAMA, HALAMAN);
    v.restartPageTimer(0);
    v.render(keadaan(1, 2, false, false, false));
    v.advancePageIfDue(HALAMAN);
    v.render(keadaan(1, 2, false, false, false));
    TEST_ASSERT_EQUAL(4, (int)d.tulis.size());
    for (const Tulis &t : d.tulis)
        TEST_ASSERT_EQUAL(16, (int)t.teks.size());
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

void test_halaman_kedua_menampilkan_nama_mesin_lalu_kembali_ke_jaringan()
{
    FakeDisplay d;
    LcdView v(d, NAMA, HALAMAN);
    v.restartPageTimer(0);
    v.advancePageIfDue(HALAMAN);
    v.render(keadaan(0, 0, false, true, true));
    TEST_ASSERT_EQUAL_STRING(pad16("MESIN:SANDINGKCL").c_str(), d.tulis[1].teks.c_str());

    v.advancePageIfDue(2 * HALAMAN);
    v.render(keadaan(0, 0, false, true, true));
    TEST_ASSERT_EQUAL_STRING(pad16("LAN:OK MQTT:OK").c_str(), d.tulis[3].teks.c_str());
}

void test_pergantian_halaman_tidak_meninggalkan_sisa_karakter()
{
    FakeDisplay d;
    LcdView v(d, NAMA, HALAMAN);
    v.restartPageTimer(0);
    v.advancePageIfDue(HALAMAN);
    v.render(keadaan(0, 0, false, true, true)); // "MESIN:SANDINGKCL" (16 karakter)
    v.advancePageIfDue(2 * HALAMAN);
    v.render(keadaan(0, 0, false, true, true)); // "LAN:OK MQTT:OK" (14 karakter)
    // Dua kolom terakhir harus ditimpa spasi, bukan sisa "CL".
    TEST_ASSERT_EQUAL_STRING("LAN:OK MQTT:OK  ", d.tulis[3].teks.c_str());
}

void test_nama_mesin_terlalu_panjang_terpotong_di_kolom_terakhir()
{
    FakeDisplay d;
    LcdView v(d, "ABCDEFGHIJKLMNOPQRST", HALAMAN);
    v.restartPageTimer(0);
    v.advancePageIfDue(HALAMAN);
    v.render(keadaan(0, 0, false, true, true));
    TEST_ASSERT_EQUAL_STRING("MESIN:ABCDEFGHIJ", d.tulis[1].teks.c_str());
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
    RUN_TEST(test_baris_angka_lima_digit_pas_enam_belas_kolom);
    RUN_TEST(test_batasan_diketahui_enam_digit_memotong_penanda_status);
    RUN_TEST(test_baris_nama_mesin);
    RUN_TEST(test_baris_status_jaringan_semua_kombinasi);
    RUN_TEST(test_pad_mengisi_spasi_sampai_lebar_kolom);
    RUN_TEST(test_pad_teks_pas_dan_terlalu_panjang_dan_kosong);
    RUN_TEST(test_begin_menyalakan_layar_sekali);
    RUN_TEST(test_showMessage_menulis_dua_baris_selebar_kolom);
    RUN_TEST(test_render_awal_menampilkan_halaman_jaringan);
    RUN_TEST(test_render_selalu_menulis_baris_selebar_kolom);
    RUN_TEST(test_halaman_berganti_tiap_interval);
    RUN_TEST(test_halaman_kedua_menampilkan_nama_mesin_lalu_kembali_ke_jaringan);
    RUN_TEST(test_pergantian_halaman_tidak_meninggalkan_sisa_karakter);
    RUN_TEST(test_nama_mesin_terlalu_panjang_terpotong_di_kolom_terakhir);
    RUN_TEST(test_pembanding_waktu_aman_saat_millis_melewati_batas);
    return UNITY_END();
}