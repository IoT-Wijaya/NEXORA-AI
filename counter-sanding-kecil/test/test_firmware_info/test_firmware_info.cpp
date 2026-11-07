#include <string.h>
#include <unity.h>
#include "firmware_info.h"

void setUp() {}
void tearDown() {}

void test_versi_dan_build_disimpan()
{
    FirmwareInfo f("v1.4.0", 128);
    TEST_ASSERT_EQUAL_STRING("v1.4.0", f.version());
    TEST_ASSERT_EQUAL_UINT32(128, f.build());
}

void test_build_nol_bukan_rilis_resmi()
{
    TEST_ASSERT_FALSE(FirmwareInfo("v1.0.0", 0).isOfficial());
    TEST_ASSERT_TRUE(FirmwareInfo("v1.0.0", 1).isOfficial());
}

void test_format_json()
{
    char b[FirmwareInfo::JSON_SIZE];
    FirmwareInfo("v1.4.0", 128).toJson(b, sizeof(b));
    TEST_ASSERT_EQUAL_STRING("{\"version\":\"v1.4.0\",\"build\":128}", b);
}

void test_versi_kosong_atau_null_menjadi_unknown()
{
    TEST_ASSERT_EQUAL_STRING("unknown", FirmwareInfo("", 1).version());
    TEST_ASSERT_EQUAL_STRING("unknown", FirmwareInfo(nullptr, 1).version());
}

void test_karakter_tidak_diizinkan_diganti_garis_bawah()
{
    TEST_ASSERT_EQUAL_STRING("a_b_c", FirmwareInfo("a b\"c", 1).version());
    TEST_ASSERT_EQUAL_STRING("v1_x", FirmwareInfo("v1\nx", 1).version());
}

void test_karakter_yang_diizinkan_dipertahankan()
{
    TEST_ASSERT_EQUAL_STRING("v1.4.0-3-gabc123+x_y", FirmwareInfo("v1.4.0-3-gabc123+x_y", 1).version());
}

void test_versi_dari_git_describe_yang_umum_utuh()
{
    TEST_ASSERT_EQUAL_STRING("baseline-refactor-12-gabc1234-dirty",
                             FirmwareInfo("baseline-refactor-12-gabc1234-dirty", 0).version());
}

void test_versi_terlalu_panjang_dipotong_dan_json_tetap_utuh()
{
    const char panjang[] = "v1.0.0-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    FirmwareInfo f(panjang, 4294967295UL);
    TEST_ASSERT_EQUAL(39, (int)strlen(f.version()));
    char b[FirmwareInfo::JSON_SIZE];
    f.toJson(b, sizeof(b));
    const size_t n = strlen(b);
    TEST_ASSERT_TRUE(n < sizeof(b) - 1);
    TEST_ASSERT_EQUAL('}', b[n - 1]);
}

void test_buffer_json_kecil_tetap_aman()
{
    char b[10];
    FirmwareInfo("v1.4.0", 128).toJson(b, sizeof(b));
    TEST_ASSERT_EQUAL(9, (int)strlen(b));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_versi_dan_build_disimpan);
    RUN_TEST(test_build_nol_bukan_rilis_resmi);
    RUN_TEST(test_format_json);
    RUN_TEST(test_versi_kosong_atau_null_menjadi_unknown);
    RUN_TEST(test_karakter_tidak_diizinkan_diganti_garis_bawah);
    RUN_TEST(test_karakter_yang_diizinkan_dipertahankan);
    RUN_TEST(test_versi_dari_git_describe_yang_umum_utuh);
    RUN_TEST(test_versi_terlalu_panjang_dipotong_dan_json_tetap_utuh);
    RUN_TEST(test_buffer_json_kecil_tetap_aman);
    return UNITY_END();
}