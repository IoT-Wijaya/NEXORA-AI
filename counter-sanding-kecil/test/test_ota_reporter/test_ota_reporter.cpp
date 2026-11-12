#include <string.h>
#include <string>
#include <vector>
#include <unity.h>

#include "ota_reporter.h"

namespace
{
    struct FakeStore : IOtaStore
    {
        OtaRecord data = freshOtaRecord();
        bool ada = false;
        bool gagalSimpan = false;
        bool load(OtaRecord &out) override
        {
            if (!ada)
                return false;
            out = data;
            return true;
        }
        bool save(const OtaRecord &r) override
        {
            if (gagalSimpan)
                return false;
            data = r;
            ada = true;
            return true;
        }
    };

    const OtaConfig CFG{3, 3, 3, 180000};
    const uint32_t LAMA = 4, BARU = 5, JEDA = 5000;

    struct Pengirim
    {
        std::vector<std::string> terkirim;
        bool terima = true;
        OtaReporter::Publisher fungsi()
        {
            return [this](const char *json)
            {
                if (!terima)
                    return false;
                terkirim.push_back(json);
                return true;
            };
        }
    };

    // Satu putaran update yang gagal diunduh: state Idle dengan hasil belum dilaporkan.
    void adaHasilGagalUnduh(OtaStateMachine &m)
    {
        m.onBoot(LAMA, 0);
        TEST_ASSERT_TRUE(m.requestUpdate(BARU, LAMA));
        TEST_ASSERT_TRUE(m.otaFailed(OtaResult::DownloadFailed));
        TEST_ASSERT_TRUE(m.hasUnreportedResult());
    }
} // namespace

void setUp() {}
void tearDown() {}

void test_json_berisi_hasil_target_sebelumnya_dan_build()
{
    char buf[128];
    OtaReport r{OtaResult::Confirmed, 5, 4};
    TEST_ASSERT_TRUE(formatOtaReport(r, 5, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("{\"result\":\"confirmed\",\"target\":5,\"previous\":4,\"build\":5}", buf);
}

void test_json_nilai_terbesar_muat_di_buffer_128()
{
    char buf[128];
    OtaReport r{OtaResult::RolledBackCrashLoop, 4294967295u, 4294967295u};
    TEST_ASSERT_TRUE(formatOtaReport(r, 4294967295u, buf, sizeof(buf)));
    TEST_ASSERT_TRUE(strlen(buf) < 127);
}

void test_json_buffer_kecil_ditolak_tanpa_meluap()
{
    char buf[16];
    memset(buf, 'x', sizeof(buf));
    OtaReport r{OtaResult::Confirmed, 5, 4};
    TEST_ASSERT_FALSE(formatOtaReport(r, 5, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL('\0', buf[0]);
    TEST_ASSERT_FALSE(formatOtaReport(r, 5, nullptr, 10));
    TEST_ASSERT_FALSE(formatOtaReport(r, 5, buf, 0));
}

void test_semua_hasil_menghasilkan_json_dengan_kode_masing_masing()
{
    const OtaResult semua[] = {OtaResult::Confirmed, OtaResult::RolledBackTimeout, OtaResult::RolledBackCrashLoop,
                               OtaResult::RolledBackNotBooted, OtaResult::RollbackFailed, OtaResult::DownloadFailed,
                               OtaResult::VerifyFailed, OtaResult::WriteFailed, OtaResult::Abandoned};
    for (OtaResult h : semua)
    {
        char buf[128];
        TEST_ASSERT_TRUE(formatOtaReport(OtaReport{h, 2, 1}, 1, buf, sizeof(buf)));
        std::string kode = std::string("\"result\":\"") + otaResultCode(h) + "\"";
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(buf, kode.c_str()), buf);
    }
}

void test_tanpa_hasil_tidak_mengirim_apa_apa()
{
    FakeStore store;
    OtaStateMachine m(store, CFG);
    m.onBoot(LAMA, 0);
    Pengirim p;
    OtaReporter r(m, p.fungsi(), JEDA);
    TEST_ASSERT_EQUAL((int)OtaReportEvent::Nothing, (int)r.service(1000, true, LAMA));
    TEST_ASSERT_EQUAL(0, (int)p.terkirim.size());
}

void test_belum_tersambung_menunda_dan_setelah_tersambung_langsung_terkirim()
{
    FakeStore store;
    OtaStateMachine m(store, CFG);
    adaHasilGagalUnduh(m);
    Pengirim p;
    OtaReporter r(m, p.fungsi(), JEDA);

    TEST_ASSERT_EQUAL((int)OtaReportEvent::Nothing, (int)r.service(1000, false, LAMA));
    TEST_ASSERT_EQUAL(0, (int)p.terkirim.size());

    TEST_ASSERT_EQUAL((int)OtaReportEvent::Sent, (int)r.service(1020, true, LAMA));
    TEST_ASSERT_EQUAL(1, (int)p.terkirim.size());
    TEST_ASSERT_EQUAL_STRING("{\"result\":\"download_failed\",\"target\":5,\"previous\":4,\"build\":4}",
                             p.terkirim[0].c_str());
    TEST_ASSERT_FALSE(m.hasUnreportedResult());
}

void test_sudah_terlapor_tidak_dikirim_lagi()
{
    FakeStore store;
    OtaStateMachine m(store, CFG);
    adaHasilGagalUnduh(m);
    Pengirim p;
    OtaReporter r(m, p.fungsi(), JEDA);
    r.service(1000, true, LAMA);
    TEST_ASSERT_EQUAL((int)OtaReportEvent::Nothing, (int)r.service(1000 + JEDA + 1, true, LAMA));
    TEST_ASSERT_EQUAL(1, (int)p.terkirim.size());
}

void test_publish_ditolak_dicoba_ulang_hanya_setelah_jeda_lalu_berhasil()
{
    FakeStore store;
    OtaStateMachine m(store, CFG);
    adaHasilGagalUnduh(m);
    Pengirim p;
    p.terima = false;
    OtaReporter r(m, p.fungsi(), JEDA);

    TEST_ASSERT_EQUAL((int)OtaReportEvent::SendFailed, (int)r.service(1000, true, LAMA));
    TEST_ASSERT_TRUE(m.hasUnreportedResult());
    TEST_ASSERT_EQUAL((int)OtaReportEvent::Nothing, (int)r.service(1000 + JEDA - 1, true, LAMA));

    p.terima = true;
    TEST_ASSERT_EQUAL((int)OtaReportEvent::Sent, (int)r.service(1000 + JEDA, true, LAMA));
    TEST_ASSERT_FALSE(m.hasUnreportedResult());
}

void test_gagal_menandai_terlaporkan_dikirim_ulang_setelah_jeda()
{
    FakeStore store;
    OtaStateMachine m(store, CFG);
    adaHasilGagalUnduh(m);
    Pengirim p;
    OtaReporter r(m, p.fungsi(), JEDA);

    store.gagalSimpan = true;
    TEST_ASSERT_EQUAL((int)OtaReportEvent::NotMarked, (int)r.service(1000, true, LAMA));
    TEST_ASSERT_TRUE(m.hasUnreportedResult());

    store.gagalSimpan = false;
    TEST_ASSERT_EQUAL((int)OtaReportEvent::Sent, (int)r.service(1000 + JEDA, true, LAMA));
    TEST_ASSERT_EQUAL_MESSAGE(2, (int)p.terkirim.size(), "minimal sekali: duplikat bisa terjadi");
    TEST_ASSERT_FALSE(m.hasUnreportedResult());
}

void test_jeda_aman_saat_millis_melewati_batas_32_bit()
{
    FakeStore store;
    OtaStateMachine m(store, CFG);
    adaHasilGagalUnduh(m);
    Pengirim p;
    p.terima = false;
    OtaReporter r(m, p.fungsi(), JEDA);
    const uint32_t dekatBatas = 0xFFFFFFFFu - 1000;
    TEST_ASSERT_EQUAL((int)OtaReportEvent::SendFailed, (int)r.service(dekatBatas, true, LAMA));
    TEST_ASSERT_EQUAL((int)OtaReportEvent::Nothing, (int)r.service(dekatBatas + 500, true, LAMA));
    p.terima = true;
    TEST_ASSERT_EQUAL((int)OtaReportEvent::Sent, (int)r.service(dekatBatas + JEDA, true, LAMA)); // melintasi 0
}

void test_ujung_ke_ujung_firmware_baru_terkonfirmasi_lalu_dilaporkan()
{
    FakeStore store;
    {
        OtaStateMachine lama(store, CFG);
        lama.onBoot(LAMA, 0);
        lama.requestUpdate(BARU, LAMA);
        lama.otaInstalled();
    }
    OtaStateMachine m(store, CFG);
    TEST_ASSERT_EQUAL((int)BootAction::WatchHealth, (int)m.onBoot(BARU, 0));
    Pengirim p;
    OtaReporter r(m, p.fungsi(), JEDA);
    TEST_ASSERT_EQUAL((int)OtaReportEvent::Nothing, (int)r.service(500, true, BARU)); // belum ada hasil
    TEST_ASSERT_EQUAL((int)HealthAction::Confirmed, (int)m.tick(1000, HealthReport{true, true, true}));
    TEST_ASSERT_EQUAL((int)OtaReportEvent::Sent, (int)r.service(1020, true, BARU));
    TEST_ASSERT_EQUAL_STRING("{\"result\":\"confirmed\",\"target\":5,\"previous\":4,\"build\":5}",
                             p.terkirim[0].c_str());
}

void test_ujung_ke_ujung_hasil_rollback_dilaporkan_oleh_firmware_lama()
{
    FakeStore store;
    {
        OtaStateMachine lama(store, CFG);
        lama.onBoot(LAMA, 0);
        lama.requestUpdate(BARU, LAMA);
        lama.otaInstalled();
    }
    for (int boot = 1; boot <= 4; boot++) // firmware baru macet berulang
    {
        OtaStateMachine m(store, CFG);
        m.onBoot(BARU, 0);
    }
    OtaStateMachine m(store, CFG); // firmware lama menyala kembali setelah rollback
    TEST_ASSERT_EQUAL((int)BootAction::Normal, (int)m.onBoot(LAMA, 0));
    Pengirim p;
    OtaReporter r(m, p.fungsi(), JEDA);
    TEST_ASSERT_EQUAL((int)OtaReportEvent::Sent, (int)r.service(1000, true, LAMA));
    TEST_ASSERT_EQUAL_STRING("{\"result\":\"rolled_back_crash_loop\",\"target\":5,\"previous\":4,\"build\":4}",
                             p.terkirim[0].c_str());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_json_berisi_hasil_target_sebelumnya_dan_build);
    RUN_TEST(test_json_nilai_terbesar_muat_di_buffer_128);
    RUN_TEST(test_json_buffer_kecil_ditolak_tanpa_meluap);
    RUN_TEST(test_semua_hasil_menghasilkan_json_dengan_kode_masing_masing);
    RUN_TEST(test_tanpa_hasil_tidak_mengirim_apa_apa);
    RUN_TEST(test_belum_tersambung_menunda_dan_setelah_tersambung_langsung_terkirim);
    RUN_TEST(test_sudah_terlapor_tidak_dikirim_lagi);
    RUN_TEST(test_publish_ditolak_dicoba_ulang_hanya_setelah_jeda_lalu_berhasil);
    RUN_TEST(test_gagal_menandai_terlaporkan_dikirim_ulang_setelah_jeda);
    RUN_TEST(test_jeda_aman_saat_millis_melewati_batas_32_bit);
    RUN_TEST(test_ujung_ke_ujung_firmware_baru_terkonfirmasi_lalu_dilaporkan);
    RUN_TEST(test_ujung_ke_ujung_hasil_rollback_dilaporkan_oleh_firmware_lama);
    return UNITY_END();
}