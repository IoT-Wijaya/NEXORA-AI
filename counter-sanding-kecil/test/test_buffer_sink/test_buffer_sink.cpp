#include <string.h>
#include <unity.h>

#include "buffer_sink.h"

void setUp() {}
void tearDown() {}

void test_menampung_beberapa_potongan_berurutan()
{
    uint8_t buf[16];
    BufferSink s(buf, sizeof(buf));
    TEST_ASSERT_TRUE(s.write(reinterpret_cast<const uint8_t *>("abc"), 3));
    TEST_ASSERT_TRUE(s.write(reinterpret_cast<const uint8_t *>("defg"), 4));
    TEST_ASSERT_EQUAL(7, s.size());
    TEST_ASSERT_EQUAL_MEMORY("abcdefg", s.data(), 7);
}

void test_tepat_penuh_diterima()
{
    uint8_t buf[4];
    BufferSink s(buf, sizeof(buf));
    TEST_ASSERT_TRUE(s.write(reinterpret_cast<const uint8_t *>("abcd"), 4));
    TEST_ASSERT_EQUAL(4, s.size());
}

void test_melebihi_kapasitas_ditolak_dan_tidak_menyimpan_sebagian()
{
    uint8_t buf[4];
    memset(buf, '.', sizeof(buf));
    BufferSink s(buf, sizeof(buf));
    TEST_ASSERT_TRUE(s.write(reinterpret_cast<const uint8_t *>("ab"), 2));
    TEST_ASSERT_FALSE(s.write(reinterpret_cast<const uint8_t *>("cde"), 3));
    TEST_ASSERT_EQUAL(2, s.size());
    TEST_ASSERT_EQUAL_MEMORY("ab..", buf, 4); // 'c' tidak ikut tersalin
}

void test_potongan_kosong_dan_reset()
{
    uint8_t buf[4];
    BufferSink s(buf, sizeof(buf));
    TEST_ASSERT_TRUE(s.write(nullptr, 0));
    TEST_ASSERT_TRUE(s.write(reinterpret_cast<const uint8_t *>("abcd"), 4));
    s.reset();
    TEST_ASSERT_EQUAL(0, s.size());
    TEST_ASSERT_TRUE(s.write(reinterpret_cast<const uint8_t *>("xy"), 2));
    TEST_ASSERT_EQUAL_MEMORY("xy", s.data(), 2);
}

void test_kapasitas_nol_menolak_semua_data()
{
    BufferSink s(nullptr, 0);
    TEST_ASSERT_FALSE(s.write(reinterpret_cast<const uint8_t *>("a"), 1));
    TEST_ASSERT_TRUE(s.write(nullptr, 0));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_menampung_beberapa_potongan_berurutan);
    RUN_TEST(test_tepat_penuh_diterima);
    RUN_TEST(test_melebihi_kapasitas_ditolak_dan_tidak_menyimpan_sebagian);
    RUN_TEST(test_potongan_kosong_dan_reset);
    RUN_TEST(test_kapasitas_nol_menolak_semua_data);
    return UNITY_END();
}