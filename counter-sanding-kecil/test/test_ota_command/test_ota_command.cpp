#include <stdint.h>
#include <unity.h>

#include "ota_command.h"

void setUp() {}
void tearDown() {}

static OtaCommandResult parse(const char *s, uint32_t *out = nullptr)
{
    uint32_t b = 0xDEADBEEF;
    const OtaCommandResult r = OtaCommand::parse(s, b);
    if (out)
        *out = b;
    return r;
}

void test_perintah_sah()
{
    uint32_t b = 0;
    TEST_ASSERT_EQUAL(OtaCommandResult::Ok, parse("ota 2", &b));
    TEST_ASSERT_EQUAL_UINT32(2, b);
    TEST_ASSERT_EQUAL(OtaCommandResult::Ok, parse("ota 4294967295", &b));
    TEST_ASSERT_EQUAL_UINT32(4294967295u, b);
}

void test_huruf_besar_dan_spasi_tidak_masalah()
{
    uint32_t b = 0;
    TEST_ASSERT_EQUAL(OtaCommandResult::Ok, parse("  OTA \t 15  ", &b));
    TEST_ASSERT_EQUAL_UINT32(15, b);
    TEST_ASSERT_EQUAL(OtaCommandResult::Ok, parse("Ota 7", &b));
}

void test_bukan_perintah_ota()
{
    const char *lain[] = {"", "   ", "show", "set mqtt_host x", "otamatic 2", "ota2", "boot 2", "ot 2", "xota 2"};
    for (const char *s : lain)
        TEST_ASSERT_EQUAL_MESSAGE(OtaCommandResult::NotOta, parse(s), s);
    TEST_ASSERT_EQUAL(OtaCommandResult::NotOta, parse(nullptr));
}

void test_argumen_salah()
{
    const char *buruk[] = {"ota", "ota   ", "ota 0", "ota -1", "ota +2", "ota 2a", "ota a2", "ota 2 3",
                           "ota 2x", "ota 4294967296", "ota 99999999999999999999", "ota 1.5", "ota 0x10"};
    for (const char *s : buruk)
        TEST_ASSERT_EQUAL_MESSAGE(OtaCommandResult::BadArgument, parse(s), s);
}

void test_keluaran_tidak_diubah_bila_gagal()
{
    uint32_t b = 0;
    parse("ota abc", &b);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEF, b);
    parse("show", &b);
    TEST_ASSERT_EQUAL_UINT32(0xDEADBEEF, b);
}

static OtaCommandResult bytes(const char *s, size_t n, uint32_t *out = nullptr)
{
    uint32_t b = 0xDEADBEEF;
    const OtaCommandResult r = OtaCommand::parseBytes(reinterpret_cast<const uint8_t *>(s), n, b);
    if (out)
        *out = b;
    return r;
}

void test_bytes_muatan_sah_tanpa_nul()
{
    uint32_t b = 0;
    TEST_ASSERT_EQUAL(OtaCommandResult::Ok, bytes("ota 7", 5, &b));
    TEST_ASSERT_EQUAL_UINT32(7, b);
}

void test_bytes_crlf_di_ujung_diabaikan()
{
    uint32_t b = 0;
    TEST_ASSERT_EQUAL(OtaCommandResult::Ok, bytes("ota 9\n", 6, &b));
    TEST_ASSERT_EQUAL_UINT32(9, b);
    TEST_ASSERT_EQUAL(OtaCommandResult::Ok, bytes("ota 11\r\n", 8, &b));
    TEST_ASSERT_EQUAL_UINT32(11, b);
}

void test_bytes_panjang_dipakai_bukan_nul()
{
    // muatan "ota 5" diikuti sampah di luar panjang yang diberikan tidak ikut terbaca
    uint32_t b = 0;
    TEST_ASSERT_EQUAL(OtaCommandResult::Ok, bytes("ota 5xyz", 5, &b));
    TEST_ASSERT_EQUAL_UINT32(5, b);
}

void test_bytes_kosong_atau_hanya_crlf_bukan_perintah()
{
    TEST_ASSERT_EQUAL(OtaCommandResult::NotOta, bytes("", 0));
    TEST_ASSERT_EQUAL(OtaCommandResult::NotOta, bytes("\r\n", 2));
    uint32_t b = 0;
    TEST_ASSERT_EQUAL(OtaCommandResult::NotOta, OtaCommand::parseBytes(nullptr, 5, b));
}

void test_bytes_nul_di_tengah_ditolak()
{
    TEST_ASSERT_EQUAL(OtaCommandResult::BadArgument, bytes("ota\0 5", 6));
    TEST_ASSERT_EQUAL(OtaCommandResult::BadArgument, bytes("ota 5\0x", 8));
}

void test_bytes_terlalu_panjang_ditolak_tanpa_meluap()
{
    char panjang[200];
    for (int i = 0; i < 200; i++)
        panjang[i] = 'a';
    TEST_ASSERT_EQUAL(OtaCommandResult::BadArgument, bytes(panjang, 200));

    // Batas 32 byte: "ota 1" diikuti spasi sampai panjang yang diuji.
    char p[40];
    for (int i = 0; i < 40; i++)
        p[i] = ' ';
    p[0] = 'o';
    p[1] = 't';
    p[2] = 'a';
    p[3] = ' ';
    p[4] = '1';
    TEST_ASSERT_EQUAL(OtaCommandResult::Ok, bytes(p, 32));
    TEST_ASSERT_EQUAL(OtaCommandResult::BadArgument, bytes(p, 33));
}

void test_bytes_bukan_ota_dan_argumen_salah()
{
    TEST_ASSERT_EQUAL(OtaCommandResult::NotOta, bytes("START", 5));
    TEST_ASSERT_EQUAL(OtaCommandResult::BadArgument, bytes("ota", 3));
    TEST_ASSERT_EQUAL(OtaCommandResult::BadArgument, bytes("ota 0", 5));
    TEST_ASSERT_EQUAL(OtaCommandResult::BadArgument, bytes("ota 4294967296", 14));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_perintah_sah);
    RUN_TEST(test_huruf_besar_dan_spasi_tidak_masalah);
    RUN_TEST(test_bukan_perintah_ota);
    RUN_TEST(test_argumen_salah);
    RUN_TEST(test_keluaran_tidak_diubah_bila_gagal);
    RUN_TEST(test_bytes_muatan_sah_tanpa_nul);
    RUN_TEST(test_bytes_crlf_di_ujung_diabaikan);
    RUN_TEST(test_bytes_panjang_dipakai_bukan_nul);
    RUN_TEST(test_bytes_kosong_atau_hanya_crlf_bukan_perintah);
    RUN_TEST(test_bytes_nul_di_tengah_ditolak);
    RUN_TEST(test_bytes_terlalu_panjang_ditolak_tanpa_meluap);
    RUN_TEST(test_bytes_bukan_ota_dan_argumen_salah);
    return UNITY_END();
}