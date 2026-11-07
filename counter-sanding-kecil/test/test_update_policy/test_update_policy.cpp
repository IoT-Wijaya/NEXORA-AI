#include <stdio.h>
#include <string.h>
#include <unity.h>
#include "update_policy.h"

static const uint32_t SLOT = 1310720; // 0x140000, sama dengan slot di perangkat

static Manifest rilis(uint32_t build)
{
    Manifest m;
    memset(&m, 0, sizeof(m));
    m.schema = 1;
    snprintf(m.deviceType, sizeof(m.deviceType), "sanding-kecil");
    snprintf(m.board, sizeof(m.board), "nodemcu-32s");
    snprintf(m.version, sizeof(m.version), "fw-test");
    m.build = build;
    m.size = 340877;
    m.minFromBuild = 0;
    m.allowDowngrade = false;
    return m;
}

static DeviceProfile perangkat(uint32_t currentBuild)
{
    return DeviceProfile{"sanding-kecil", "nodemcu-32s", currentBuild, SLOT};
}

static int d(PolicyDecision x) { return (int)x; }

void setUp() {}
void tearDown() {}

void test_kenaikan_biasa_diterima()
{
    TEST_ASSERT_EQUAL(d(PolicyDecision::Accept), d(UpdatePolicy::evaluate(rilis(129), perangkat(128))));
    TEST_ASSERT_TRUE(UpdatePolicy::isAccepted(UpdatePolicy::evaluate(rilis(129), perangkat(128))));
}

void test_lompat_banyak_build_diterima_jika_min_from_terpenuhi()
{
    Manifest m = rilis(500);
    m.minFromBuild = 100;
    TEST_ASSERT_EQUAL(d(PolicyDecision::Accept), d(UpdatePolicy::evaluate(m, perangkat(100))));
    TEST_ASSERT_EQUAL(d(PolicyDecision::Accept), d(UpdatePolicy::evaluate(m, perangkat(300))));
}

void test_device_type_berbeda_ditolak()
{
    Manifest m = rilis(129);
    snprintf(m.deviceType, sizeof(m.deviceType), "hmi-sanding");
    TEST_ASSERT_EQUAL(d(PolicyDecision::WrongDeviceType), d(UpdatePolicy::evaluate(m, perangkat(128))));
}

void test_board_berbeda_ditolak()
{
    Manifest m = rilis(129);
    snprintf(m.board, sizeof(m.board), "esp32-s3");
    TEST_ASSERT_EQUAL(d(PolicyDecision::WrongBoard), d(UpdatePolicy::evaluate(m, perangkat(128))));
}

void test_profil_perangkat_tanpa_teks_dianggap_tidak_cocok()
{
    const DeviceProfile tanpaTipe{nullptr, "nodemcu-32s", 128, SLOT};
    const DeviceProfile tanpaBoard{"sanding-kecil", nullptr, 128, SLOT};
    TEST_ASSERT_EQUAL(d(PolicyDecision::WrongDeviceType), d(UpdatePolicy::evaluate(rilis(129), tanpaTipe)));
    TEST_ASSERT_EQUAL(d(PolicyDecision::WrongBoard), d(UpdatePolicy::evaluate(rilis(129), tanpaBoard)));
}

void test_ketidakcocokan_identitas_dilaporkan_lebih_dulu_dari_yang_lain()
{
    Manifest m = rilis(100); // juga downgrade
    m.size = SLOT + 1;       // juga terlalu besar
    snprintf(m.deviceType, sizeof(m.deviceType), "hmi-sanding");
    TEST_ASSERT_EQUAL(d(PolicyDecision::WrongDeviceType), d(UpdatePolicy::evaluate(m, perangkat(128))));
}

void test_build_sama_sudah_terpasang_walau_allow_downgrade()
{
    Manifest m = rilis(128);
    TEST_ASSERT_EQUAL(d(PolicyDecision::AlreadyInstalled), d(UpdatePolicy::evaluate(m, perangkat(128))));
    m.allowDowngrade = true;
    TEST_ASSERT_EQUAL(d(PolicyDecision::AlreadyInstalled), d(UpdatePolicy::evaluate(m, perangkat(128))));
}

void test_downgrade_ditolak_kecuali_diizinkan_manifest()
{
    Manifest m = rilis(100);
    TEST_ASSERT_EQUAL(d(PolicyDecision::DowngradeNotAllowed), d(UpdatePolicy::evaluate(m, perangkat(128))));
    m.allowDowngrade = true;
    TEST_ASSERT_EQUAL(d(PolicyDecision::Accept), d(UpdatePolicy::evaluate(m, perangkat(128))));
}

void test_downgrade_yang_diizinkan_tidak_terkena_min_from()
{
    Manifest m = rilis(150);
    m.minFromBuild = 140;
    m.allowDowngrade = true;
    // Terpasang 200 (di atas 150), tidak ada urusan dengan min_from.
    TEST_ASSERT_EQUAL(d(PolicyDecision::Accept), d(UpdatePolicy::evaluate(m, perangkat(200))));
}

void test_terlalu_lama_ditolak_dan_batasnya_tepat()
{
    Manifest m = rilis(500);
    m.minFromBuild = 100;
    TEST_ASSERT_EQUAL(d(PolicyDecision::TooOldToUpgrade), d(UpdatePolicy::evaluate(m, perangkat(99))));
    TEST_ASSERT_EQUAL(d(PolicyDecision::Accept), d(UpdatePolicy::evaluate(m, perangkat(100))));
}

void test_min_from_harus_lebih_kecil_dari_build_rilis_sendiri()
{
    Manifest m = rilis(100);
    m.minFromBuild = 101;
    TEST_ASSERT_EQUAL(d(PolicyDecision::InconsistentManifest), d(UpdatePolicy::evaluate(m, perangkat(50))));

    m.minFromBuild = 100; // tidak bisa dicapai siapa pun lewat kenaikan versi
    TEST_ASSERT_EQUAL(d(PolicyDecision::InconsistentManifest), d(UpdatePolicy::evaluate(m, perangkat(99))));

    m.minFromBuild = 99; // batas yang paling ketat dan masih masuk akal
    TEST_ASSERT_EQUAL(d(PolicyDecision::Accept), d(UpdatePolicy::evaluate(m, perangkat(99))));
    TEST_ASSERT_EQUAL(d(PolicyDecision::TooOldToUpgrade), d(UpdatePolicy::evaluate(m, perangkat(98))));
}

void test_ukuran_pas_slot_diterima_dan_lebih_satu_byte_ditolak()
{
    Manifest m = rilis(129);
    m.size = SLOT;
    TEST_ASSERT_EQUAL(d(PolicyDecision::Accept), d(UpdatePolicy::evaluate(m, perangkat(128))));
    m.size = SLOT + 1;
    TEST_ASSERT_EQUAL(d(PolicyDecision::TooLarge), d(UpdatePolicy::evaluate(m, perangkat(128))));
}

void test_ukuran_slot_tidak_diketahui_dianggap_tidak_muat()
{
    DeviceProfile p = perangkat(128);
    p.slotSize = 0;
    TEST_ASSERT_EQUAL(d(PolicyDecision::TooLarge), d(UpdatePolicy::evaluate(rilis(129), p)));
}

void test_build_lokal_nol_bisa_naik_ke_rilis_tanpa_batas_bawah()
{
    TEST_ASSERT_EQUAL(d(PolicyDecision::Accept), d(UpdatePolicy::evaluate(rilis(1), perangkat(0))));
}

void test_build_lokal_nol_ditolak_jika_rilis_mensyaratkan_batas_bawah()
{
    Manifest m = rilis(50);
    m.minFromBuild = 1;
    TEST_ASSERT_EQUAL(d(PolicyDecision::TooOldToUpgrade), d(UpdatePolicy::evaluate(m, perangkat(0))));
}

void test_nilai_build_ekstrem()
{
    TEST_ASSERT_EQUAL(d(PolicyDecision::AlreadyInstalled),
                      d(UpdatePolicy::evaluate(rilis(4294967295UL), perangkat(4294967295UL))));
    TEST_ASSERT_EQUAL(d(PolicyDecision::DowngradeNotAllowed),
                      d(UpdatePolicy::evaluate(rilis(1), perangkat(4294967295UL))));
    TEST_ASSERT_EQUAL(d(PolicyDecision::Accept),
                      d(UpdatePolicy::evaluate(rilis(4294967295UL), perangkat(4294967294UL))));
}

void test_semua_keputusan_punya_kode_dan_teks_yang_unik()
{
    const PolicyDecision semua[] = {
        PolicyDecision::Accept, PolicyDecision::AlreadyInstalled,
        PolicyDecision::WrongDeviceType, PolicyDecision::WrongBoard,
        PolicyDecision::InconsistentManifest, PolicyDecision::DowngradeNotAllowed,
        PolicyDecision::TooOldToUpgrade, PolicyDecision::TooLarge};
    const int n = (int)(sizeof(semua) / sizeof(semua[0]));
    for (int i = 0; i < n; i++)
    {
        TEST_ASSERT_TRUE(strlen(UpdatePolicy::code(semua[i])) > 0);
        TEST_ASSERT_TRUE(strlen(UpdatePolicy::text(semua[i])) > 0);
        TEST_ASSERT_TRUE(strcmp(UpdatePolicy::code(semua[i]), "unknown") != 0);
        for (int j = i + 1; j < n; j++)
        {
            TEST_ASSERT_TRUE(strcmp(UpdatePolicy::code(semua[i]), UpdatePolicy::code(semua[j])) != 0);
            TEST_ASSERT_TRUE(strcmp(UpdatePolicy::text(semua[i]), UpdatePolicy::text(semua[j])) != 0);
        }
    }
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_kenaikan_biasa_diterima);
    RUN_TEST(test_lompat_banyak_build_diterima_jika_min_from_terpenuhi);
    RUN_TEST(test_device_type_berbeda_ditolak);
    RUN_TEST(test_board_berbeda_ditolak);
    RUN_TEST(test_profil_perangkat_tanpa_teks_dianggap_tidak_cocok);
    RUN_TEST(test_ketidakcocokan_identitas_dilaporkan_lebih_dulu_dari_yang_lain);
    RUN_TEST(test_build_sama_sudah_terpasang_walau_allow_downgrade);
    RUN_TEST(test_downgrade_ditolak_kecuali_diizinkan_manifest);
    RUN_TEST(test_downgrade_yang_diizinkan_tidak_terkena_min_from);
    RUN_TEST(test_terlalu_lama_ditolak_dan_batasnya_tepat);
    RUN_TEST(test_min_from_harus_lebih_kecil_dari_build_rilis_sendiri);
    RUN_TEST(test_ukuran_pas_slot_diterima_dan_lebih_satu_byte_ditolak);
    RUN_TEST(test_ukuran_slot_tidak_diketahui_dianggap_tidak_muat);
    RUN_TEST(test_build_lokal_nol_bisa_naik_ke_rilis_tanpa_batas_bawah);
    RUN_TEST(test_build_lokal_nol_ditolak_jika_rilis_mensyaratkan_batas_bawah);
    RUN_TEST(test_nilai_build_ekstrem);
    RUN_TEST(test_semua_keputusan_punya_kode_dan_teks_yang_unik);
    return UNITY_END();
}