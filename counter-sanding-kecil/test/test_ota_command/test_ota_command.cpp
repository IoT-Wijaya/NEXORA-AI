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

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_perintah_sah);
    RUN_TEST(test_huruf_besar_dan_spasi_tidak_masalah);
    RUN_TEST(test_bukan_perintah_ota);
    RUN_TEST(test_argumen_salah);
    RUN_TEST(test_keluaran_tidak_diubah_bila_gagal);
    return UNITY_END();
}