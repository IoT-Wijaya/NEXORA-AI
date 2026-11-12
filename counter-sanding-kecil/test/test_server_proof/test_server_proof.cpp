#include <unity.h>

#include "server_proof.h"

void setUp() {}
void tearDown() {}

void test_awalnya_belum_terbukti()
{
    ServerProof p;
    TEST_ASSERT_FALSE(p.ok());
}

void test_semua_publish_diterima_lalu_selesai_terbukti()
{
    ServerProof p;
    p.begin();
    p.record(true);
    p.record(true);
    p.record(true);
    TEST_ASSERT_FALSE_MESSAGE(p.ok(), "belum finish() tidak boleh terbukti");
    p.finish();
    TEST_ASSERT_TRUE(p.ok());
}

void test_satu_publish_gagal_membatalkan_bukti()
{
    ServerProof p;
    p.begin();
    p.record(true);
    p.record(false);
    p.record(true);
    p.finish();
    TEST_ASSERT_FALSE(p.ok());
}

void test_putaran_tanpa_publish_tidak_terbukti_bila_tidak_selesai()
{
    ServerProof p;
    p.begin();
    TEST_ASSERT_FALSE(p.ok());
}

void test_finish_tanpa_begin_tidak_terbukti()
{
    ServerProof p;
    p.finish();
    TEST_ASSERT_FALSE(p.ok());
    p.record(true);
    TEST_ASSERT_FALSE(p.ok());
}

void test_publish_setelah_finish_diabaikan()
{
    ServerProof p;
    p.begin();
    p.record(true);
    p.finish();
    p.record(false); // mis. heartbeat gagal belakangan
    TEST_ASSERT_TRUE(p.ok());
}

void test_putaran_baru_menghapus_bukti_lama()
{
    ServerProof p;
    p.begin();
    p.record(true);
    p.finish();
    TEST_ASSERT_TRUE(p.ok());
    p.begin(); // MQTT tersambung ulang
    TEST_ASSERT_FALSE(p.ok());
    p.record(false);
    p.finish();
    TEST_ASSERT_FALSE(p.ok());
    p.begin();
    p.record(true);
    p.finish();
    TEST_ASSERT_TRUE(p.ok());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_awalnya_belum_terbukti);
    RUN_TEST(test_semua_publish_diterima_lalu_selesai_terbukti);
    RUN_TEST(test_satu_publish_gagal_membatalkan_bukti);
    RUN_TEST(test_putaran_tanpa_publish_tidak_terbukti_bila_tidak_selesai);
    RUN_TEST(test_finish_tanpa_begin_tidak_terbukti);
    RUN_TEST(test_publish_setelah_finish_diabaikan);
    RUN_TEST(test_putaran_baru_menghapus_bukti_lama);
    return UNITY_END();
}