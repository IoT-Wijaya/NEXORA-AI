#include <unity.h>
#include "counter_store.h"

// Pengganti kunci: mencatat kedalaman dan jumlah penguncian.
class LockPencatat : public ILock
{
public:
    void lock() override
    {
        kedalaman++;
        jumlahLock++;
        if (kedalaman > maksKedalaman)
            maksKedalaman = kedalaman;
    }
    void unlock() override { kedalaman--; }
    int kedalaman = 0;
    int maksKedalaman = 0;
    int jumlahLock = 0;
};

static LockPencatat kunci;

void setUp() { kunci = LockPencatat(); }
void tearDown() {}

void test_awal_nol_dan_tidak_dirty()
{
    CounterStore c(kunci);
    TEST_ASSERT_EQUAL_UINT32(0, c.snapshot().hasil);
    TEST_ASSERT_EQUAL_UINT32(0, c.snapshot().modal);
    TEST_ASSERT_FALSE(c.takeDirty());
}

void test_increment_hasil_hanya_menambah_hasil()
{
    CounterStore c(kunci);
    c.incrementHasil();
    c.incrementHasil();
    TEST_ASSERT_EQUAL_UINT32(2, c.snapshot().hasil);
    TEST_ASSERT_EQUAL_UINT32(0, c.snapshot().modal);
    TEST_ASSERT_TRUE(c.takeDirty());
}

void test_increment_modal_hanya_menambah_modal()
{
    CounterStore c(kunci);
    c.incrementModal();
    TEST_ASSERT_EQUAL_UINT32(0, c.snapshot().hasil);
    TEST_ASSERT_EQUAL_UINT32(1, c.snapshot().modal);
}

void test_takeDirty_mengambil_lalu_menghapus_bendera()
{
    CounterStore c(kunci);
    c.incrementHasil();
    TEST_ASSERT_TRUE(c.takeDirty());
    TEST_ASSERT_FALSE(c.takeDirty());
}

void test_reset_hasil_saja()
{
    CounterStore c(kunci);
    c.incrementHasil();
    c.incrementModal();
    c.reset(true, false);
    TEST_ASSERT_EQUAL_UINT32(0, c.snapshot().hasil);
    TEST_ASSERT_EQUAL_UINT32(1, c.snapshot().modal);
}

void test_reset_modal_saja()
{
    CounterStore c(kunci);
    c.incrementHasil();
    c.incrementModal();
    c.reset(false, true);
    TEST_ASSERT_EQUAL_UINT32(1, c.snapshot().hasil);
    TEST_ASSERT_EQUAL_UINT32(0, c.snapshot().modal);
}

void test_reset_semua()
{
    CounterStore c(kunci);
    c.incrementHasil();
    c.incrementModal();
    c.reset(true, true);
    TEST_ASSERT_EQUAL_UINT32(0, c.snapshot().hasil);
    TEST_ASSERT_EQUAL_UINT32(0, c.snapshot().modal);
}

void test_reset_selalu_menandai_dirty_walau_sudah_nol()
{
    CounterStore c(kunci);
    TEST_ASSERT_FALSE(c.takeDirty());
    c.reset(true, true); // angka sudah nol, tetapi ack START_OK tetap harus terkirim
    TEST_ASSERT_TRUE(c.takeDirty());
}

void test_increment_setelah_takeDirty_menandai_dirty_lagi()
{
    CounterStore c(kunci);
    c.incrementHasil();
    TEST_ASSERT_TRUE(c.takeDirty());
    c.incrementHasil();
    TEST_ASSERT_TRUE(c.takeDirty());
    TEST_ASSERT_EQUAL_UINT32(2, c.snapshot().hasil);
}

void test_setiap_operasi_memegang_kunci_tanpa_bersarang_dan_seimbang()
{
    CounterStore c(kunci);
    c.incrementHasil();
    c.incrementModal();
    c.reset(true, true);
    c.snapshot();
    c.takeDirty();
    TEST_ASSERT_EQUAL(5, kunci.jumlahLock);
    TEST_ASSERT_EQUAL(1, kunci.maksKedalaman);
    TEST_ASSERT_EQUAL(0, kunci.kedalaman);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_awal_nol_dan_tidak_dirty);
    RUN_TEST(test_increment_hasil_hanya_menambah_hasil);
    RUN_TEST(test_increment_modal_hanya_menambah_modal);
    RUN_TEST(test_takeDirty_mengambil_lalu_menghapus_bendera);
    RUN_TEST(test_reset_hasil_saja);
    RUN_TEST(test_reset_modal_saja);
    RUN_TEST(test_reset_semua);
    RUN_TEST(test_reset_selalu_menandai_dirty_walau_sudah_nol);
    RUN_TEST(test_increment_setelah_takeDirty_menandai_dirty_lagi);
    RUN_TEST(test_setiap_operasi_memegang_kunci_tanpa_bersarang_dan_seimbang);
    return UNITY_END();
}