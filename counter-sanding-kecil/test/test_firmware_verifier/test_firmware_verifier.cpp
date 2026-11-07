#include <string.h>
#include <vector>
#include <unity.h>
#include "firmware_verifier.h"

template <typename T>
static int n(T v) { return (int)v; }

static std::vector<uint8_t> isi(size_t jumlah)
{
    std::vector<uint8_t> d(jumlah);
    for (size_t i = 0; i < jumlah; i++)
        d[i] = (uint8_t)(i * 31 + 7);
    return d;
}

static Manifest manifestUntuk(const std::vector<uint8_t> &data)
{
    Manifest m;
    memset(&m, 0, sizeof(m));
    m.size = (uint32_t)data.size();
    Sha256 h;
    h.update(data.data(), data.size());
    h.finish(m.sha256);
    return m;
}

// Mengalirkan data dalam potongan; false jika ada update() yang ditolak.
static bool alirkan(FirmwareVerifier &v, const std::vector<uint8_t> &data, size_t potongan)
{
    for (size_t i = 0; i < data.size(); i += potongan)
    {
        const size_t n2 = (data.size() - i < potongan) ? data.size() - i : potongan;
        if (!v.update(data.data() + i, n2))
            return false;
    }
    return true;
}

void setUp() {}
void tearDown() {}

void test_aliran_utuh_lolos_untuk_berbagai_ukuran_potongan()
{
    const std::vector<uint8_t> data = isi(5000);
    const size_t ukuran[] = {1, 64, 1000, 4096, 5000};
    for (size_t u : ukuran)
    {
        FirmwareVerifier v(manifestUntuk(data));
        TEST_ASSERT_TRUE(alirkan(v, data, u));
        TEST_ASSERT_EQUAL(n(VerifyResult::Ok), n(v.finish()));
        TEST_ASSERT_EQUAL_UINT32(5000, v.received());
    }
}

void test_satu_byte_berubah_terdeteksi_sebagai_hash_salah()
{
    const std::vector<uint8_t> data = isi(5000);
    std::vector<uint8_t> rusak = data;
    rusak[2500] ^= 0x01;
    FirmwareVerifier v(manifestUntuk(data));
    TEST_ASSERT_TRUE(alirkan(v, rusak, 1024));
    TEST_ASSERT_EQUAL(n(VerifyResult::HashMismatch), n(v.finish()));
}

void test_aliran_terpotong_terdeteksi_sebagai_ukuran_salah()
{
    const std::vector<uint8_t> data = isi(5000);
    std::vector<uint8_t> pendek(data.begin(), data.end() - 1);
    FirmwareVerifier v(manifestUntuk(data));
    TEST_ASSERT_TRUE(alirkan(v, pendek, 1024));
    TEST_ASSERT_EQUAL(n(VerifyResult::SizeMismatch), n(v.finish()));
}

void test_aliran_berlebih_ditolak_saat_terjadi_dan_tidak_dihitung()
{
    const std::vector<uint8_t> data = isi(5000);
    FirmwareVerifier v(manifestUntuk(data));
    TEST_ASSERT_TRUE(alirkan(v, data, 1024));
    const uint8_t ekstra = 0xAA;
    TEST_ASSERT_FALSE(v.update(&ekstra, 1));
    TEST_ASSERT_EQUAL_UINT32(5000, v.received());
    TEST_ASSERT_EQUAL(n(VerifyResult::TooMuchData), n(v.finish()));
}

void test_potongan_yang_melewati_batas_ditolak_seluruhnya()
{
    const std::vector<uint8_t> data = isi(100);
    FirmwareVerifier v(manifestUntuk(data));
    const std::vector<uint8_t> besar = isi(101);
    TEST_ASSERT_FALSE(v.update(besar.data(), besar.size()));
    TEST_ASSERT_EQUAL_UINT32(0, v.received());
}

void test_setelah_ditolak_update_berikutnya_tetap_ditolak()
{
    const std::vector<uint8_t> data = isi(100);
    FirmwareVerifier v(manifestUntuk(data));
    const std::vector<uint8_t> besar = isi(101);
    v.update(besar.data(), besar.size());
    TEST_ASSERT_FALSE(v.update(data.data(), 10));
}

void test_begin_mengulang_dari_awal()
{
    const std::vector<uint8_t> data = isi(3000);
    FirmwareVerifier v(manifestUntuk(data));
    TEST_ASSERT_TRUE(v.update(data.data(), 1500));
    v.begin();
    TEST_ASSERT_EQUAL_UINT32(0, v.received());
    TEST_ASSERT_TRUE(alirkan(v, data, 512));
    TEST_ASSERT_EQUAL(n(VerifyResult::Ok), n(v.finish()));
}

void test_begin_memulihkan_dari_keadaan_berlebih()
{
    const std::vector<uint8_t> data = isi(100);
    FirmwareVerifier v(manifestUntuk(data));
    const std::vector<uint8_t> besar = isi(101);
    v.update(besar.data(), besar.size());
    v.begin();
    TEST_ASSERT_TRUE(alirkan(v, data, 100));
    TEST_ASSERT_EQUAL(n(VerifyResult::Ok), n(v.finish()));
}

void test_kelanjutan_unduhan_memakai_verifier_yang_sama_tanpa_begin()
{
    // Resume setelah koneksi putus: potongan pertama dan sisanya berasal dari aliran yang sama.
    const std::vector<uint8_t> data = isi(8000);
    FirmwareVerifier v(manifestUntuk(data));
    TEST_ASSERT_TRUE(v.update(data.data(), 3333));
    TEST_ASSERT_TRUE(v.update(data.data() + 3333, 8000 - 3333));
    TEST_ASSERT_EQUAL(n(VerifyResult::Ok), n(v.finish()));
}

void test_finish_berulang_mengembalikan_hasil_yang_sama_dan_menutup_update()
{
    const std::vector<uint8_t> data = isi(200);
    FirmwareVerifier v(manifestUntuk(data));
    alirkan(v, data, 64);
    TEST_ASSERT_EQUAL(n(VerifyResult::Ok), n(v.finish()));
    TEST_ASSERT_EQUAL(n(VerifyResult::Ok), n(v.finish()));
    const uint8_t b = 1;
    TEST_ASSERT_FALSE(v.update(&b, 1));
}

void test_update_kosong_dan_penunjuk_null()
{
    const std::vector<uint8_t> data = isi(10);
    FirmwareVerifier v(manifestUntuk(data));
    TEST_ASSERT_TRUE(v.update(nullptr, 0));
    TEST_ASSERT_FALSE(v.update(nullptr, 5));
}

void test_biner_satu_byte()
{
    const std::vector<uint8_t> data = isi(1);
    FirmwareVerifier v(manifestUntuk(data));
    TEST_ASSERT_TRUE(alirkan(v, data, 1));
    TEST_ASSERT_EQUAL(n(VerifyResult::Ok), n(v.finish()));
}

void test_hash_di_manifest_yang_berbeda_ditolak()
{
    const std::vector<uint8_t> data = isi(500);
    Manifest m = manifestUntuk(data);
    m.sha256[31] ^= 0x01;
    FirmwareVerifier v(m);
    TEST_ASSERT_TRUE(alirkan(v, data, 100));
    TEST_ASSERT_EQUAL(n(VerifyResult::HashMismatch), n(v.finish()));
}

void test_semua_hasil_punya_kode_unik()
{
    const VerifyResult semua[] = {VerifyResult::Ok, VerifyResult::SizeMismatch, VerifyResult::TooMuchData,
                                  VerifyResult::HashMismatch};
    for (int i = 0; i < 4; i++)
    {
        TEST_ASSERT_TRUE(strcmp(verifyResultCode(semua[i]), "unknown") != 0);
        for (int j = i + 1; j < 4; j++)
        {
            TEST_ASSERT_TRUE(strcmp(verifyResultCode(semua[i]), verifyResultCode(semua[j])) != 0);
        }
    }
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_aliran_utuh_lolos_untuk_berbagai_ukuran_potongan);
    RUN_TEST(test_satu_byte_berubah_terdeteksi_sebagai_hash_salah);
    RUN_TEST(test_aliran_terpotong_terdeteksi_sebagai_ukuran_salah);
    RUN_TEST(test_aliran_berlebih_ditolak_saat_terjadi_dan_tidak_dihitung);
    RUN_TEST(test_potongan_yang_melewati_batas_ditolak_seluruhnya);
    RUN_TEST(test_setelah_ditolak_update_berikutnya_tetap_ditolak);
    RUN_TEST(test_begin_mengulang_dari_awal);
    RUN_TEST(test_begin_memulihkan_dari_keadaan_berlebih);
    RUN_TEST(test_kelanjutan_unduhan_memakai_verifier_yang_sama_tanpa_begin);
    RUN_TEST(test_finish_berulang_mengembalikan_hasil_yang_sama_dan_menutup_update);
    RUN_TEST(test_update_kosong_dan_penunjuk_null);
    RUN_TEST(test_biner_satu_byte);
    RUN_TEST(test_hash_di_manifest_yang_berbeda_ditolak);
    RUN_TEST(test_semua_hasil_punya_kode_unik);
    return UNITY_END();
}