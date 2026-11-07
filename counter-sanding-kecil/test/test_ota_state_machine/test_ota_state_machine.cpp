#include <string.h>
#include <unity.h>
#include "ota_state_machine.h"

class FakeStore : public IOtaStore
{
public:
    bool load(OtaRecord &out) override
    {
        if (!ada)
            return false;
        out = data;
        return true;
    }
    bool save(const OtaRecord &record) override
    {
        if (gagalSimpan)
            return false;
        data = record;
        ada = true;
        jumlahSimpan++;
        return true;
    }
    OtaRecord data = freshOtaRecord();
    bool ada = false;
    bool gagalSimpan = false;
    int jumlahSimpan = 0;
};

static const uint32_t LAMA = 100;
static const uint32_t BARU = 101;
static const uint32_t BATAS = 180000;

static OtaConfig cfg() { return OtaConfig{3, 3, 3, BATAS}; }

template <typename T>
static int n(T v) { return (int)v; }

// Satu boot: mesin baru di atas penyimpanan yang sama.
static OtaStateMachine mulai(FakeStore &s, uint32_t berjalan, uint32_t now, BootAction &aksi)
{
    OtaStateMachine m(s, cfg());
    aksi = m.onBoot(berjalan, now);
    return m;
}

// Membawa penyimpanan ke keadaan Pending: build LAMA sudah meminta dan memasang BARU.
static void siapkanPending(FakeStore &s)
{
    BootAction a;
    {
        OtaStateMachine m = mulai(s, LAMA, 0, a);
        m.requestUpdate(BARU, LAMA);
    }
    {
        OtaStateMachine m = mulai(s, LAMA, 0, a);
        m.otaInstalled();
    }
}

static const HealthReport SEHAT = {true, true, true};
static const HealthReport TANPA_SERVER = {true, true, false};

void setUp() {}
void tearDown() {}

void test_tanpa_catatan_boot_normal_dan_idle()
{
    FakeStore s;
    BootAction a;
    OtaStateMachine m = mulai(s, LAMA, 0, a);
    TEST_ASSERT_EQUAL(n(BootAction::Normal), n(a));
    TEST_ASSERT_EQUAL(n(OtaState::Idle), n(m.state()));
    TEST_ASSERT_FALSE(m.hasUnreportedResult());
}

void test_catatan_dengan_state_tak_dikenal_dianggap_kosong()
{
    FakeStore s;
    BootAction a;
    s.ada = true;
    s.data = freshOtaRecord();
    s.data.state = (OtaState)9;
    OtaStateMachine m = mulai(s, LAMA, 0, a);
    TEST_ASSERT_EQUAL(n(BootAction::Normal), n(a));
    TEST_ASSERT_EQUAL(n(OtaState::Idle), n(m.state()));
    TEST_ASSERT_EQUAL(n(OtaState::Idle), n(s.data.state));
}

void test_requestUpdate_menyimpan_dan_hanya_boleh_dari_idle()
{
    FakeStore s;
    BootAction a;
    OtaStateMachine m = mulai(s, LAMA, 0, a);
    TEST_ASSERT_TRUE(m.requestUpdate(BARU, LAMA));
    TEST_ASSERT_EQUAL(n(OtaState::Requested), n(s.data.state));
    TEST_ASSERT_EQUAL_UINT32(BARU, s.data.targetBuild);
    TEST_ASSERT_EQUAL_UINT32(LAMA, s.data.previousBuild);
    TEST_ASSERT_FALSE(m.requestUpdate(102, LAMA));
}

void test_requestUpdate_gagal_simpan_tidak_mengubah_apa_pun()
{
    FakeStore s;
    BootAction a;
    OtaStateMachine m = mulai(s, LAMA, 0, a);
    s.gagalSimpan = true;
    TEST_ASSERT_FALSE(m.requestUpdate(BARU, LAMA));
    TEST_ASSERT_EQUAL(n(OtaState::Idle), n(m.state()));
    TEST_ASSERT_EQUAL(n(OtaState::Idle), n(s.data.state));
}

void test_boot_requested_menjalankan_ota_dan_mencatat_percobaan()
{
    FakeStore s;
    BootAction a;
    {
        OtaStateMachine m = mulai(s, LAMA, 0, a);
        m.requestUpdate(BARU, LAMA);
    }
    OtaStateMachine m = mulai(s, LAMA, 0, a);
    TEST_ASSERT_EQUAL(n(BootAction::RunOta), n(a));
    TEST_ASSERT_EQUAL_UINT8(1, s.data.attempts);
}

void test_unduhan_yang_terus_terputus_akhirnya_ditinggalkan()
{
    FakeStore s;
    BootAction a;
    {
        OtaStateMachine m = mulai(s, LAMA, 0, a);
        m.requestUpdate(BARU, LAMA);
    }
    for (int i = 0; i < 3; i++)
    {
        OtaStateMachine m = mulai(s, LAMA, 0, a);
        TEST_ASSERT_EQUAL(n(BootAction::RunOta), n(a));
    }
    OtaStateMachine m = mulai(s, LAMA, 0, a);
    TEST_ASSERT_EQUAL(n(BootAction::Normal), n(a));
    TEST_ASSERT_EQUAL(n(OtaState::Idle), n(s.data.state));
    TEST_ASSERT_EQUAL(n(OtaResult::Abandoned), n(s.data.result));
}

void test_alur_sukses_dari_permintaan_sampai_konfirmasi()
{
    FakeStore s;
    BootAction a;
    {
        OtaStateMachine m = mulai(s, LAMA, 1000, a);
        TEST_ASSERT_TRUE(m.requestUpdate(BARU, LAMA));
    }
    {
        OtaStateMachine m = mulai(s, LAMA, 2000, a);
        TEST_ASSERT_EQUAL(n(BootAction::RunOta), n(a));
        TEST_ASSERT_TRUE(m.otaInstalled());
        TEST_ASSERT_EQUAL(n(OtaState::Pending), n(m.state()));
    }
    OtaStateMachine m = mulai(s, BARU, 3000, a);
    TEST_ASSERT_EQUAL(n(BootAction::WatchHealth), n(a));
    TEST_ASSERT_EQUAL(n(HealthAction::Waiting), n(m.tick(8000, TANPA_SERVER)));
    TEST_ASSERT_EQUAL(n(HealthAction::Confirmed), n(m.tick(12000, SEHAT)));
    TEST_ASSERT_EQUAL(n(OtaState::Idle), n(m.state()));
    TEST_ASSERT_TRUE(m.hasUnreportedResult());
    const OtaReport r = m.report();
    TEST_ASSERT_EQUAL(n(OtaResult::Confirmed), n(r.result));
    TEST_ASSERT_EQUAL_UINT32(BARU, r.targetBuild);
    TEST_ASSERT_EQUAL_UINT32(LAMA, r.previousBuild);
    TEST_ASSERT_TRUE(m.markReported());
    TEST_ASSERT_FALSE(m.hasUnreportedResult());
}

void test_daya_putus_sebelum_otaInstalled_diperbaiki_saat_boot()
{
    FakeStore s;
    BootAction a;
    {
        OtaStateMachine m = mulai(s, LAMA, 0, a);
        m.requestUpdate(BARU, LAMA);
    }
    // Slot sudah pindah, tetapi otaInstalled() belum sempat dipanggil.
    OtaStateMachine m = mulai(s, BARU, 0, a);
    TEST_ASSERT_EQUAL(n(BootAction::WatchHealth), n(a));
    TEST_ASSERT_EQUAL(n(OtaState::Pending), n(s.data.state));
    TEST_ASSERT_EQUAL_UINT8(1, s.data.attempts);
}

void test_batas_waktu_tepat_menjadi_rollback_karena_timeout()
{
    FakeStore s;
    BootAction a;
    siapkanPending(s);
    OtaStateMachine m = mulai(s, BARU, 1000, a);
    TEST_ASSERT_EQUAL(n(HealthAction::Waiting), n(m.tick(1000 + BATAS - 1, TANPA_SERVER)));
    TEST_ASSERT_EQUAL(n(HealthAction::Rollback), n(m.tick(1000 + BATAS, TANPA_SERVER)));
    TEST_ASSERT_EQUAL(n(OtaState::RollingBack), n(s.data.state));
    TEST_ASSERT_EQUAL(n(OtaResult::RolledBackTimeout), n(s.data.result));
}

void test_sehat_tepat_di_batas_waktu_menang()
{
    FakeStore s;
    BootAction a;
    siapkanPending(s);
    OtaStateMachine m = mulai(s, BARU, 1000, a);
    TEST_ASSERT_EQUAL(n(HealthAction::Confirmed), n(m.tick(1000 + BATAS, SEHAT)));
}

void test_hanya_sebagian_sehat_tidak_pernah_mengonfirmasi()
{
    TEST_ASSERT_FALSE((HealthReport{true, true, false}).allOk());
    TEST_ASSERT_FALSE((HealthReport{true, false, true}).allOk());
    TEST_ASSERT_FALSE((HealthReport{false, true, true}).allOk());
    TEST_ASSERT_TRUE((HealthReport{true, true, true}).allOk());
}

void test_crash_berulang_tiga_boot_lalu_rollback_pada_boot_keempat()
{
    FakeStore s;
    BootAction a;
    siapkanPending(s);
    for (int i = 1; i <= 3; i++)
    {
        OtaStateMachine m = mulai(s, BARU, 0, a);
        TEST_ASSERT_EQUAL(n(BootAction::WatchHealth), n(a));
        TEST_ASSERT_EQUAL_UINT8(i, s.data.attempts);
    }
    OtaStateMachine m = mulai(s, BARU, 0, a);
    TEST_ASSERT_EQUAL(n(BootAction::Rollback), n(a));
    TEST_ASSERT_EQUAL(n(OtaState::RollingBack), n(s.data.state));
    TEST_ASSERT_EQUAL(n(OtaResult::RolledBackCrashLoop), n(s.data.result));
}

void test_rollback_selesai_saat_firmware_lama_berjalan_dan_hasil_tetap_dilaporkan()
{
    FakeStore s;
    BootAction a;
    siapkanPending(s);
    for (int i = 0; i < 4; i++)
    {
        OtaStateMachine m = mulai(s, BARU, 0, a);
    }
    OtaStateMachine m = mulai(s, LAMA, 0, a);
    TEST_ASSERT_EQUAL(n(BootAction::Normal), n(a));
    TEST_ASSERT_EQUAL(n(OtaState::Idle), n(m.state()));
    TEST_ASSERT_TRUE(m.hasUnreportedResult());
    TEST_ASSERT_EQUAL(n(OtaResult::RolledBackCrashLoop), n(m.report().result));
    TEST_ASSERT_EQUAL_UINT32(BARU, m.report().targetBuild);
    TEST_ASSERT_EQUAL_UINT32(LAMA, m.report().previousBuild);
}

void test_rollback_yang_terus_gagal_akhirnya_diakui_gagal()
{
    FakeStore s;
    BootAction a;
    siapkanPending(s);
    for (int i = 0; i < 4; i++)
    {
        OtaStateMachine m = mulai(s, BARU, 0, a);
    } // keputusan rollback
    for (int i = 0; i < 3; i++)
    {
        OtaStateMachine m = mulai(s, BARU, 0, a); // slot belum pindah
        TEST_ASSERT_EQUAL(n(BootAction::Rollback), n(a));
    }
    OtaStateMachine m = mulai(s, BARU, 0, a);
    TEST_ASSERT_EQUAL(n(BootAction::Normal), n(a));
    TEST_ASSERT_EQUAL(n(OtaState::Idle), n(s.data.state));
    TEST_ASSERT_EQUAL(n(OtaResult::RollbackFailed), n(s.data.result));
}

void test_daya_putus_setelah_keputusan_rollback_tetap_diulang_pada_boot_berikutnya()
{
    FakeStore s;
    BootAction a;
    siapkanPending(s);
    {
        OtaStateMachine m = mulai(s, BARU, 1000, a);
        m.tick(1000 + BATAS, TANPA_SERVER);
    }
    // Daya putus sebelum slot sempat dipindah: boot berikutnya masih firmware baru.
    OtaStateMachine m = mulai(s, BARU, 0, a);
    TEST_ASSERT_EQUAL(n(BootAction::Rollback), n(a));
}

void test_gambar_baru_tidak_pernah_berjalan_dicatat_dan_tidak_perlu_rollback()
{
    FakeStore s;
    BootAction a;
    siapkanPending(s);
    OtaStateMachine m = mulai(s, LAMA, 0, a); // bootloader kembali ke slot lama
    TEST_ASSERT_EQUAL(n(BootAction::Normal), n(a));
    TEST_ASSERT_EQUAL(n(OtaState::Idle), n(m.state()));
    TEST_ASSERT_EQUAL(n(OtaResult::RolledBackNotBooted), n(m.report().result));
}

void test_otaFailed_menutup_putaran_dan_hanya_valid_saat_requested()
{
    FakeStore s;
    BootAction a;
    {
        OtaStateMachine m = mulai(s, LAMA, 0, a);
        TEST_ASSERT_FALSE(m.otaFailed(OtaResult::DownloadFailed));
        m.requestUpdate(BARU, LAMA);
    }
    OtaStateMachine m = mulai(s, LAMA, 0, a);
    TEST_ASSERT_TRUE(m.otaFailed(OtaResult::DownloadFailed));
    TEST_ASSERT_EQUAL(n(OtaState::Idle), n(m.state()));
    TEST_ASSERT_EQUAL(n(OtaResult::DownloadFailed), n(m.report().result));
    TEST_ASSERT_FALSE(m.otaFailed(OtaResult::DownloadFailed));
}

void test_gagal_menyimpan_konfirmasi_tetap_pending_lalu_dicoba_lagi()
{
    FakeStore s;
    BootAction a;
    siapkanPending(s);
    OtaStateMachine m = mulai(s, BARU, 0, a);
    s.gagalSimpan = true;
    TEST_ASSERT_EQUAL(n(HealthAction::Waiting), n(m.tick(1000, SEHAT)));
    TEST_ASSERT_EQUAL(n(OtaState::Pending), n(m.state()));
    s.gagalSimpan = false;
    TEST_ASSERT_EQUAL(n(HealthAction::Confirmed), n(m.tick(2000, SEHAT)));
}

void test_boot_pending_yang_tidak_bisa_dicatat_dianggap_tidak_layak_dipercaya()
{
    FakeStore s;
    BootAction a;
    siapkanPending(s);
    s.gagalSimpan = true;
    OtaStateMachine m = mulai(s, BARU, 0, a);
    TEST_ASSERT_EQUAL(n(BootAction::Rollback), n(a));
}

void test_tick_saat_idle_tidak_diperlukan()
{
    FakeStore s;
    BootAction a;
    OtaStateMachine m = mulai(s, LAMA, 0, a);
    TEST_ASSERT_EQUAL(n(HealthAction::NotNeeded), n(m.tick(1000, SEHAT)));
}

void test_batas_waktu_aman_saat_millis_melewati_batas_32_bit()
{
    FakeStore s;
    BootAction a;
    siapkanPending(s);
    const uint32_t t0 = 0xFFFFFF00u;
    OtaStateMachine m = mulai(s, BARU, t0, a);
    TEST_ASSERT_EQUAL(n(HealthAction::Waiting), n(m.tick(t0 + (BATAS - 1), TANPA_SERVER)));
    TEST_ASSERT_EQUAL(n(HealthAction::Rollback), n(m.tick(t0 + BATAS, TANPA_SERVER)));
}

void test_permintaan_baru_menimpa_hasil_lama_yang_belum_dilaporkan()
{
    FakeStore s;
    BootAction a;
    siapkanPending(s);
    {
        OtaStateMachine m = mulai(s, BARU, 0, a);
        m.tick(0, SEHAT);
    }
    OtaStateMachine m = mulai(s, BARU, 0, a);
    TEST_ASSERT_TRUE(m.hasUnreportedResult());
    TEST_ASSERT_TRUE(m.requestUpdate(102, BARU));
    TEST_ASSERT_FALSE(m.hasUnreportedResult());
}

void test_markReported_gagal_menyimpan_mempertahankan_hasil()
{
    FakeStore s;
    BootAction a;
    siapkanPending(s);
    {
        OtaStateMachine m = mulai(s, BARU, 0, a);
        m.tick(0, SEHAT);
    }
    OtaStateMachine m = mulai(s, BARU, 0, a);
    s.gagalSimpan = true;
    TEST_ASSERT_FALSE(m.markReported());
    TEST_ASSERT_TRUE(m.hasUnreportedResult());
}

void test_semua_hasil_punya_kode_unik()
{
    const OtaResult semua[] = {
        OtaResult::None, OtaResult::Confirmed, OtaResult::RolledBackTimeout,
        OtaResult::RolledBackCrashLoop, OtaResult::RolledBackNotBooted, OtaResult::RollbackFailed,
        OtaResult::DownloadFailed, OtaResult::VerifyFailed, OtaResult::WriteFailed, OtaResult::Abandoned};
    const int jumlah = (int)(sizeof(semua) / sizeof(semua[0]));
    for (int i = 0; i < jumlah; i++)
    {
        TEST_ASSERT_TRUE(strlen(otaResultCode(semua[i])) > 0);
        TEST_ASSERT_TRUE(strcmp(otaResultCode(semua[i]), "unknown") != 0);
        for (int j = i + 1; j < jumlah; j++)
        {
            TEST_ASSERT_TRUE(strcmp(otaResultCode(semua[i]), otaResultCode(semua[j])) != 0);
        }
    }
}

void test_catatan_hilang_ditulis_bersih_saat_boot()
{
    FakeStore s;
    BootAction a;
    TEST_ASSERT_FALSE(s.ada);
    OtaStateMachine m = mulai(s, LAMA, 0, a);
    TEST_ASSERT_EQUAL(n(BootAction::Normal), n(a));
    TEST_ASSERT_TRUE(s.ada);
    TEST_ASSERT_EQUAL(1, s.jumlahSimpan);
    TEST_ASSERT_EQUAL(n(OtaState::Idle), n(s.data.state));
}

void test_kode_state_dan_aksi_boot_unik()
{
    const OtaState states[] = {OtaState::Idle, OtaState::Requested, OtaState::Pending, OtaState::RollingBack};
    for (int i = 0; i < 4; i++)
    {
        TEST_ASSERT_TRUE(strcmp(otaStateCode(states[i]), "unknown") != 0);
        for (int j = i + 1; j < 4; j++)
        {
            TEST_ASSERT_TRUE(strcmp(otaStateCode(states[i]), otaStateCode(states[j])) != 0);
        }
    }
    const BootAction aksi[] = {BootAction::Normal, BootAction::RunOta, BootAction::WatchHealth, BootAction::Rollback};
    for (int i = 0; i < 4; i++)
    {
        TEST_ASSERT_TRUE(strcmp(bootActionCode(aksi[i]), "unknown") != 0);
        for (int j = i + 1; j < 4; j++)
        {
            TEST_ASSERT_TRUE(strcmp(bootActionCode(aksi[i]), bootActionCode(aksi[j])) != 0);
        }
    }
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_tanpa_catatan_boot_normal_dan_idle);
    RUN_TEST(test_catatan_dengan_state_tak_dikenal_dianggap_kosong);
    RUN_TEST(test_requestUpdate_menyimpan_dan_hanya_boleh_dari_idle);
    RUN_TEST(test_requestUpdate_gagal_simpan_tidak_mengubah_apa_pun);
    RUN_TEST(test_boot_requested_menjalankan_ota_dan_mencatat_percobaan);
    RUN_TEST(test_unduhan_yang_terus_terputus_akhirnya_ditinggalkan);
    RUN_TEST(test_alur_sukses_dari_permintaan_sampai_konfirmasi);
    RUN_TEST(test_daya_putus_sebelum_otaInstalled_diperbaiki_saat_boot);
    RUN_TEST(test_batas_waktu_tepat_menjadi_rollback_karena_timeout);
    RUN_TEST(test_sehat_tepat_di_batas_waktu_menang);
    RUN_TEST(test_hanya_sebagian_sehat_tidak_pernah_mengonfirmasi);
    RUN_TEST(test_crash_berulang_tiga_boot_lalu_rollback_pada_boot_keempat);
    RUN_TEST(test_rollback_selesai_saat_firmware_lama_berjalan_dan_hasil_tetap_dilaporkan);
    RUN_TEST(test_rollback_yang_terus_gagal_akhirnya_diakui_gagal);
    RUN_TEST(test_daya_putus_setelah_keputusan_rollback_tetap_diulang_pada_boot_berikutnya);
    RUN_TEST(test_gambar_baru_tidak_pernah_berjalan_dicatat_dan_tidak_perlu_rollback);
    RUN_TEST(test_otaFailed_menutup_putaran_dan_hanya_valid_saat_requested);
    RUN_TEST(test_gagal_menyimpan_konfirmasi_tetap_pending_lalu_dicoba_lagi);
    RUN_TEST(test_boot_pending_yang_tidak_bisa_dicatat_dianggap_tidak_layak_dipercaya);
    RUN_TEST(test_tick_saat_idle_tidak_diperlukan);
    RUN_TEST(test_batas_waktu_aman_saat_millis_melewati_batas_32_bit);
    RUN_TEST(test_permintaan_baru_menimpa_hasil_lama_yang_belum_dilaporkan);
    RUN_TEST(test_markReported_gagal_menyimpan_mempertahankan_hasil);
    RUN_TEST(test_semua_hasil_punya_kode_unik);
    RUN_TEST(test_catatan_hilang_ditulis_bersih_saat_boot);
    RUN_TEST(test_kode_state_dan_aksi_boot_unik);
    return UNITY_END();
}