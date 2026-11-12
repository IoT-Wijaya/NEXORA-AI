// Skenario Langkah 21: firmware baru yang macet lalu di-reset watchdog berulang kali.
// Dari sisi state machine itu tampak sebagai boot Pending yang tidak pernah terkonfirmasi.
#include <unity.h>

#include "ota_state_machine.h"

namespace
{
    struct FakeStore : IOtaStore
    {
        OtaRecord data = freshOtaRecord();
        bool ada = false;
        bool load(OtaRecord &out) override
        {
            if (!ada)
                return false;
            out = data;
            return true;
        }
        bool save(const OtaRecord &r) override
        {
            data = r;
            ada = true;
            return true;
        }
    };

    const OtaConfig CFG{3, 3, 3, 180000};
    const uint32_t LAMA = 4, BARU = 5;

    // Firmware LAMA meminta update lalu memasangnya (state Pending tersimpan).
    void pasangBaru(FakeStore &store)
    {
        OtaStateMachine m(store, CFG);
        m.onBoot(LAMA, 0);
        TEST_ASSERT_TRUE(m.requestUpdate(BARU, LAMA));
        TEST_ASSERT_TRUE(m.otaInstalled());
    }
} // namespace

void setUp() {}
void tearDown() {}

void test_reset_berulang_tanpa_konfirmasi_berujung_rollback_pada_boot_keempat()
{
    FakeStore store;
    pasangBaru(store);

    for (int boot = 1; boot <= 3; boot++)
    {
        OtaStateMachine m(store, CFG); // satu objek baru per boot, seperti perangkat sungguhan
        TEST_ASSERT_EQUAL_MESSAGE((int)BootAction::WatchHealth, (int)m.onBoot(BARU, 0), "boot masih dipantau");
        // macet: tick() tidak pernah memberi jawaban, perangkat di-reset watchdog
    }

    OtaStateMachine m(store, CFG);
    TEST_ASSERT_EQUAL((int)BootAction::Rollback, (int)m.onBoot(BARU, 0));
    TEST_ASSERT_EQUAL((int)OtaState::RollingBack, (int)m.state());
    TEST_ASSERT_EQUAL((int)OtaResult::RolledBackCrashLoop, (int)m.record().result);
}

void test_rollback_selesai_saat_firmware_lama_berjalan_kembali()
{
    FakeStore store;
    pasangBaru(store);
    for (int boot = 1; boot <= 4; boot++)
    {
        OtaStateMachine m(store, CFG);
        m.onBoot(BARU, 0);
    }

    OtaStateMachine m(store, CFG);
    TEST_ASSERT_EQUAL((int)BootAction::Normal, (int)m.onBoot(LAMA, 0));
    TEST_ASSERT_EQUAL((int)OtaState::Idle, (int)m.state());
    TEST_ASSERT_EQUAL_MESSAGE((int)OtaResult::RolledBackCrashLoop, (int)m.record().result, "hasil menunggu dilaporkan");
}

void test_konfirmasi_sebelum_batas_menghentikan_hitungan_reset()
{
    FakeStore store;
    pasangBaru(store);

    {
        OtaStateMachine m(store, CFG);
        TEST_ASSERT_EQUAL((int)BootAction::WatchHealth, (int)m.onBoot(BARU, 0));
    } // reset sekali

    OtaStateMachine m(store, CFG);
    TEST_ASSERT_EQUAL((int)BootAction::WatchHealth, (int)m.onBoot(BARU, 0));
    TEST_ASSERT_EQUAL((int)HealthAction::Confirmed, (int)m.tick(1000, HealthReport{true, true, true}));

    OtaStateMachine berikut(store, CFG);
    TEST_ASSERT_EQUAL((int)BootAction::Normal, (int)berikut.onBoot(BARU, 0));
}

void test_rollback_gagal_berulang_berhenti_setelah_batas_percobaan()
{
    FakeStore store;
    pasangBaru(store);
    for (int boot = 1; boot <= 4; boot++)
    {
        OtaStateMachine m(store, CFG);
        m.onBoot(BARU, 0); // boot ke-4 mencatat niat rollback
    }

    // Rollback tak pernah berhasil: firmware BARU terus yang menyala.
    for (int percobaan = 1; percobaan <= 3; percobaan++)
    {
        OtaStateMachine m(store, CFG);
        TEST_ASSERT_EQUAL_MESSAGE((int)BootAction::Rollback, (int)m.onBoot(BARU, 0), "masih mencoba");
    }
    OtaStateMachine m(store, CFG);
    TEST_ASSERT_EQUAL((int)BootAction::Normal, (int)m.onBoot(BARU, 0));
    TEST_ASSERT_EQUAL((int)OtaResult::RollbackFailed, (int)m.record().result);
    TEST_ASSERT_EQUAL((int)OtaState::Idle, (int)m.state());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_reset_berulang_tanpa_konfirmasi_berujung_rollback_pada_boot_keempat);
    RUN_TEST(test_rollback_selesai_saat_firmware_lama_berjalan_kembali);
    RUN_TEST(test_konfirmasi_sebelum_batas_menghentikan_hitungan_reset);
    RUN_TEST(test_rollback_gagal_berulang_berhenti_setelah_batas_percobaan);
    return UNITY_END();
}