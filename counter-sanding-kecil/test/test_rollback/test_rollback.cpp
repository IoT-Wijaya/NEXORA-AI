#include <string.h>
#include <unity.h>

#include "rollback.h"

namespace
{
    struct FakeSlots : IBootSlots
    {
        bool ada = true;
        bool valid = true;
        bool bisaAlih = true;
        int cekAda = 0, cekValid = 0, alih = 0;

        bool hasPreviousSlot() override
        {
            cekAda++;
            return ada;
        }
        bool previousSlotValid() override
        {
            cekValid++;
            return valid;
        }
        bool setBootToPrevious() override
        {
            alih++;
            return bisaAlih;
        }
        const char *lastError() const override { return ""; }
    };
} // namespace

void setUp() {}
void tearDown() {}

void test_semua_lolos_boot_dialihkan()
{
    FakeSlots s;
    TEST_ASSERT_EQUAL((int)RollbackResult::Switched, (int)rollbackToPrevious(s));
    TEST_ASSERT_EQUAL(1, s.alih);
}

void test_tanpa_slot_lama_tidak_ada_yang_diubah()
{
    FakeSlots s;
    s.ada = false;
    TEST_ASSERT_EQUAL((int)RollbackResult::NoPreviousSlot, (int)rollbackToPrevious(s));
    TEST_ASSERT_EQUAL(0, s.cekValid);
    TEST_ASSERT_EQUAL(0, s.alih);
}

void test_slot_lama_rusak_tidak_pernah_dialihkan()
{
    FakeSlots s;
    s.valid = false;
    TEST_ASSERT_EQUAL((int)RollbackResult::PreviousSlotInvalid, (int)rollbackToPrevious(s));
    TEST_ASSERT_EQUAL(0, s.alih);
}

void test_pengalihan_ditolak_dilaporkan_gagal()
{
    FakeSlots s;
    s.bisaAlih = false;
    TEST_ASSERT_EQUAL((int)RollbackResult::SwitchFailed, (int)rollbackToPrevious(s));
    TEST_ASSERT_EQUAL(1, s.alih);
}

void test_tiap_pemeriksaan_hanya_sekali()
{
    FakeSlots s;
    rollbackToPrevious(s);
    TEST_ASSERT_EQUAL(1, s.cekAda);
    TEST_ASSERT_EQUAL(1, s.cekValid);
    TEST_ASSERT_EQUAL(1, s.alih);
}

void test_kode_hasil_unik_dan_tidak_kosong()
{
    const RollbackResult semua[] = {RollbackResult::Switched, RollbackResult::NoPreviousSlot,
                                    RollbackResult::PreviousSlotInvalid, RollbackResult::SwitchFailed};
    for (int i = 0; i < 4; i++)
    {
        TEST_ASSERT_TRUE(strlen(rollbackResultCode(semua[i])) > 0);
        TEST_ASSERT_TRUE(strcmp(rollbackResultCode(semua[i]), "unknown") != 0);
        for (int j = i + 1; j < 4; j++)
            TEST_ASSERT_TRUE(strcmp(rollbackResultCode(semua[i]), rollbackResultCode(semua[j])) != 0);
    }
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_semua_lolos_boot_dialihkan);
    RUN_TEST(test_tanpa_slot_lama_tidak_ada_yang_diubah);
    RUN_TEST(test_slot_lama_rusak_tidak_pernah_dialihkan);
    RUN_TEST(test_pengalihan_ditolak_dilaporkan_gagal);
    RUN_TEST(test_tiap_pemeriksaan_hanya_sekali);
    RUN_TEST(test_kode_hasil_unik_dan_tidak_kosong);
    return UNITY_END();
}