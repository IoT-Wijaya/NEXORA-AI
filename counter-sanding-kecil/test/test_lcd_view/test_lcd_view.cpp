#include <unity.h>
#include <string>
#include <vector>
#include "lcd_view.h"

namespace
{
    struct FakeLcd : ILcdDisplay
    {
        std::string baris[4];
        int tulis = 0;
        void begin() override {}
        void writeLine(uint8_t row, const char *t) override
        {
            baris[row] = t;
            ++tulis;
        }
    };
    const LcdState S{12, 345, true, true, false};
}

void setUp() {}
void tearDown() {}

void test_empat_baris_semua_tampil_sekaligus()
{
    FakeLcd lcd;
    LcdView v(lcd, "SANDINGBSR", 1000, 4);
    v.render(S);
    TEST_ASSERT_EQUAL_STRING("M:  345 H:   12*", lcd.baris[0].c_str());
    TEST_ASSERT_EQUAL_STRING("MESIN:SANDINGBSR", lcd.baris[1].c_str());
    TEST_ASSERT_EQUAL_STRING("LAN:OK MQTT:NO  ", lcd.baris[2].c_str());
    TEST_ASSERT_EQUAL_STRING("HITUNG:AKTIF    ", lcd.baris[3].c_str());
}
void test_empat_baris_hitung_berhenti()
{
    FakeLcd lcd;
    LcdView v(lcd, "X", 1000, 4);
    LcdState s = S;
    s.counting = false;
    v.render(s);
    TEST_ASSERT_EQUAL_STRING("HITUNG:BERHENTI ", lcd.baris[3].c_str());
}
void test_empat_baris_tidak_ganti_halaman()
{
    FakeLcd lcd;
    LcdView v(lcd, "X", 1000, 4);
    v.restartPageTimer(0);
    TEST_ASSERT_FALSE(v.advancePageIfDue(5000));
}
void test_empat_baris_showMessage_mengosongkan_baris_lain()
{
    FakeLcd lcd;
    LcdView v(lcd, "X", 1000, 4);
    v.render(S);
    v.showMessage("ROLLBACK", "JANGAN DIMATIKAN");
    TEST_ASSERT_EQUAL_STRING("ROLLBACK        ", lcd.baris[0].c_str());
    TEST_ASSERT_EQUAL_STRING("                ", lcd.baris[2].c_str());
    TEST_ASSERT_EQUAL_STRING("                ", lcd.baris[3].c_str());
}
void test_dua_baris_perilaku_lama_tetap()
{
    FakeLcd lcd;
    LcdView v(lcd, "SANDINGKCL", 1000, 2);
    v.restartPageTimer(0);
    v.render(S);
    TEST_ASSERT_EQUAL_STRING("LAN:OK MQTT:NO  ", lcd.baris[1].c_str());
    TEST_ASSERT_EQUAL_INT(2, lcd.tulis); // hanya 2 baris ditulis
    TEST_ASSERT_FALSE(v.advancePageIfDue(999));
    TEST_ASSERT_TRUE(v.advancePageIfDue(1000));
    v.render(S);
    TEST_ASSERT_EQUAL_STRING("MESIN:SANDINGKCL", lcd.baris[1].c_str());
}
void test_dua_baris_showMessage_hanya_dua_baris()
{
    FakeLcd lcd;
    LcdView v(lcd, "X", 1000, 2);
    v.showMessage("A", "B");
    TEST_ASSERT_EQUAL_INT(2, lcd.tulis);
}
int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_empat_baris_semua_tampil_sekaligus);
    RUN_TEST(test_empat_baris_hitung_berhenti);
    RUN_TEST(test_empat_baris_tidak_ganti_halaman);
    RUN_TEST(test_empat_baris_showMessage_mengosongkan_baris_lain);
    RUN_TEST(test_dua_baris_perilaku_lama_tetap);
    RUN_TEST(test_dua_baris_showMessage_hanya_dua_baris);
    return UNITY_END();
}