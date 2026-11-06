#include <unity.h>
#include "sanding_config.h"
#include "sensor_filter.h"

static const bool AKTIF = config::SENSOR_ACTIVE; // false = LOW
static const bool LEPAS = !AKTIF;

static SensorFilter buatFilter()
{
    SensorFilter f(AKTIF, config::PRESS_STABLE_TIME, config::RELEASE_STABLE_TIME);
    f.begin(LEPAS, 0);
    return f;
}

// Meniru sensorTask: membaca tiap 10 ms. Mengembalikan jumlah hitungan.
static int jalankan(SensorFilter &f, bool level, uint32_t mulai, uint32_t durasi)
{
    int hitungan = 0;
    for (uint32_t d = 0; d < durasi; d += 10)
    {
        if (f.update(level, mulai + d))
            hitungan++;
    }
    return hitungan;
}

void setUp() {}
void tearDown() {}

void test_belum_dihitung_sebelum_stabil_100ms()
{
    SensorFilter f = buatFilter();
    TEST_ASSERT_EQUAL(0, jalankan(f, AKTIF, 1000, 100)); // 1000..1090
    TEST_ASSERT_EQUAL(1, jalankan(f, AKTIF, 1100, 10));  // tepat 100 ms
}

void test_ditahan_lama_hanya_dihitung_sekali()
{
    SensorFilter f = buatFilter();
    TEST_ASSERT_EQUAL(1, jalankan(f, AKTIF, 1000, 2000));
}

void test_gangguan_singkat_tidak_dihitung()
{
    SensorFilter f = buatFilter();
    TEST_ASSERT_EQUAL(0, jalankan(f, AKTIF, 1000, 50));
    TEST_ASSERT_EQUAL(0, jalankan(f, LEPAS, 1050, 1000));
}

void test_getaran_cepat_tidak_dihitung_lalu_tekan_stabil_dihitung_sekali()
{
    SensorFilter f = buatFilter();
    int total = 0;
    for (int i = 0; i < 20; i++)
    {
        total += jalankan(f, (i % 2 == 0) ? AKTIF : LEPAS, 1000 + i * 30, 30);
    }
    TEST_ASSERT_EQUAL(0, total);
    TEST_ASSERT_EQUAL(1, jalankan(f, AKTIF, 1600, 200));
}

void test_tekan_ulang_sebelum_400ms_setelah_lepas_tidak_dihitung()
{
    SensorFilter f = buatFilter();
    TEST_ASSERT_EQUAL(1, jalankan(f, AKTIF, 1000, 200));
    TEST_ASSERT_EQUAL(0, jalankan(f, LEPAS, 1200, 300)); // dilepas hanya 300 ms
    TEST_ASSERT_EQUAL(0, jalankan(f, AKTIF, 1500, 1000));
}

void test_tekan_ulang_setelah_lepas_400ms_dihitung_lagi()
{
    SensorFilter f = buatFilter();
    int total = 0;
    total += jalankan(f, AKTIF, 1000, 200);
    total += jalankan(f, LEPAS, 1200, 500);
    total += jalankan(f, AKTIF, 1700, 200);
    TEST_ASSERT_EQUAL(2, total);
}

void test_waktu_melewati_batas_32_bit_tetap_benar()
{
    SensorFilter f = buatFilter();
    const uint32_t dekatBatas = 0xFFFFFFFFu - 50u;
    TEST_ASSERT_EQUAL(1, jalankan(f, AKTIF, dekatBatas, 300));
}

void test_sensor_sudah_aktif_saat_boot_tidak_langsung_dihitung()
{
    SensorFilter f(AKTIF, config::PRESS_STABLE_TIME, config::RELEASE_STABLE_TIME);
    f.begin(AKTIF, 0);
    int total = 0;
    total += jalankan(f, AKTIF, 0, 1000);
    total += jalankan(f, LEPAS, 1000, 500);
    TEST_ASSERT_EQUAL(0, total);
    TEST_ASSERT_EQUAL(1, jalankan(f, AKTIF, 1500, 200));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_belum_dihitung_sebelum_stabil_100ms);
    RUN_TEST(test_ditahan_lama_hanya_dihitung_sekali);
    RUN_TEST(test_gangguan_singkat_tidak_dihitung);
    RUN_TEST(test_getaran_cepat_tidak_dihitung_lalu_tekan_stabil_dihitung_sekali);
    RUN_TEST(test_tekan_ulang_sebelum_400ms_setelah_lepas_tidak_dihitung);
    RUN_TEST(test_tekan_ulang_setelah_lepas_400ms_dihitung_lagi);
    RUN_TEST(test_waktu_melewati_batas_32_bit_tetap_benar);
    RUN_TEST(test_sensor_sudah_aktif_saat_boot_tidak_langsung_dihitung);
    return UNITY_END();
}