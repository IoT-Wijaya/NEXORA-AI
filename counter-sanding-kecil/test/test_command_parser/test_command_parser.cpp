#include <string.h>
#include <unity.h>
#include "command_parser.h"

static int p(const char *teks)
{
    return (int)CommandParser::parse(teks, strlen(teks));
}
static int c(Command k) { return (int)k; }

void setUp() {}
void tearDown() {}

void test_semua_perintah_dikenali()
{
    TEST_ASSERT_EQUAL(c(Command::Start), p("START"));
    TEST_ASSERT_EQUAL(c(Command::Stop), p("STOP"));
    TEST_ASSERT_EQUAL(c(Command::ResetHasil), p("RESET_S1"));
    TEST_ASSERT_EQUAL(c(Command::ResetModal), p("RESET_S2"));
    TEST_ASSERT_EQUAL(c(Command::ResetAll), p("RESET_ALL"));
    TEST_ASSERT_EQUAL(c(Command::RestartDevice), p("RESTART_DEVICE"));
}

void test_reset_polos_adalah_nama_lama_reset_all()
{
    TEST_ASSERT_EQUAL(c(Command::ResetAll), p("RESET"));
}

void test_spasi_dan_enter_di_ujung_dibuang()
{
    TEST_ASSERT_EQUAL(c(Command::Start), p("START\n"));
    TEST_ASSERT_EQUAL(c(Command::Start), p("START\r\n"));
    TEST_ASSERT_EQUAL(c(Command::Stop), p("STOP   "));
}

void test_spasi_di_depan_ditolak()
{
    TEST_ASSERT_EQUAL(c(Command::Unknown), p(" START"));
}

void test_huruf_kecil_ditolak()
{
    TEST_ASSERT_EQUAL(c(Command::Unknown), p("start"));
    TEST_ASSERT_EQUAL(c(Command::Unknown), p("Start"));
}

void test_tidak_dikenal_dan_kosong_dan_null()
{
    TEST_ASSERT_EQUAL(c(Command::Unknown), p("HALO"));
    TEST_ASSERT_EQUAL(c(Command::Unknown), p(""));
    TEST_ASSERT_EQUAL(c(Command::Unknown), (int)CommandParser::parse(nullptr, 5));
}

void test_hanya_membaca_sebanyak_length_tanpa_butuh_NUL()
{
    const char buf[] = {'S', 'T', 'A', 'R', 'T', 'X', 'X'};
    TEST_ASSERT_EQUAL(c(Command::Start), (int)CommandParser::parse(buf, 5));
    TEST_ASSERT_EQUAL(c(Command::Unknown), (int)CommandParser::parse(buf, 7));
}

void test_payload_sangat_panjang_tidak_merusak_memori()
{
    char panjang[201];
    memset(panjang, 'A', 200);
    panjang[200] = '\0';
    TEST_ASSERT_EQUAL(c(Command::Unknown), p(panjang));
}

void test_spasi_berlebih_melewati_batas_potong_tetap_dikenali()
{
    // Perilaku kode asli: payload dipotong 23 byte, lalu spasi di ujung dibuang.
    char buf[36];
    memset(buf, ' ', sizeof(buf) - 1);
    memcpy(buf, "START", 5);
    buf[sizeof(buf) - 1] = '\0';
    TEST_ASSERT_EQUAL(c(Command::Start), p(buf));
}

void test_nama_perintah_untuk_log()
{
    TEST_ASSERT_EQUAL_STRING("START", CommandParser::name(Command::Start));
    TEST_ASSERT_EQUAL_STRING("RESET_ALL", CommandParser::name(Command::ResetAll));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", CommandParser::name(Command::Unknown));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_semua_perintah_dikenali);
    RUN_TEST(test_reset_polos_adalah_nama_lama_reset_all);
    RUN_TEST(test_spasi_dan_enter_di_ujung_dibuang);
    RUN_TEST(test_spasi_di_depan_ditolak);
    RUN_TEST(test_huruf_kecil_ditolak);
    RUN_TEST(test_tidak_dikenal_dan_kosong_dan_null);
    RUN_TEST(test_hanya_membaca_sebanyak_length_tanpa_butuh_NUL);
    RUN_TEST(test_payload_sangat_panjang_tidak_merusak_memori);
    RUN_TEST(test_spasi_berlebih_melewati_batas_potong_tetap_dikenali);
    RUN_TEST(test_nama_perintah_untuk_log);
    return UNITY_END();
}