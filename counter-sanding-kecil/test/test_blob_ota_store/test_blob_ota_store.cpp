#include <string.h>
#include <map>
#include <string>
#include <vector>
#include <unity.h>
#include "blob_ota_store.h"
#include "ota_record_codec.h"

class FakeBlob : public IBlobStore
{
public:
    int read(const char *key, uint8_t *buffer, size_t capacity) override
    {
        auto it = data.find(key);
        if (it == data.end())
            return -1;
        const std::vector<uint8_t> &v = it->second;
        if (v.size() <= capacity)
            memcpy(buffer, v.data(), v.size());
        return (int)v.size();
    }
    bool write(const char *key, const uint8_t *bytes, size_t length) override
    {
        if (gagalTulis)
            return false;
        std::vector<uint8_t> v(bytes, bytes + length);
        if (rusakSaatTulis && v.size() > 6)
            v[6] ^= 0x01;
        data[key] = v;
        return true;
    }
    std::map<std::string, std::vector<uint8_t>> data;
    bool gagalTulis = false;
    bool rusakSaatTulis = false;
};

template <typename T>
static int n(T v) { return (int)v; }

static const char *KUNCI = "state";

static OtaRecord contoh()
{
    return OtaRecord{OtaState::Pending, 1, OtaResult::None, 101, 100};
}

void setUp() {}
void tearDown() {}

void test_load_tanpa_catatan_gagal()
{
    FakeBlob b;
    BlobOtaStore s(b, KUNCI);
    OtaRecord r = freshOtaRecord();
    TEST_ASSERT_FALSE(s.load(r));
}

void test_simpan_lalu_baca_kembali_sama()
{
    FakeBlob b;
    BlobOtaStore s(b, KUNCI);
    TEST_ASSERT_TRUE(s.save(contoh()));
    OtaRecord r = freshOtaRecord();
    TEST_ASSERT_TRUE(s.load(r));
    TEST_ASSERT_EQUAL(n(OtaState::Pending), n(r.state));
    TEST_ASSERT_EQUAL_UINT32(101, r.targetBuild);
    TEST_ASSERT_EQUAL_UINT32(100, r.previousBuild);
}

void test_simpan_menimpa_catatan_sebelumnya()
{
    FakeBlob b;
    BlobOtaStore s(b, KUNCI);
    s.save(contoh());
    OtaRecord baru = contoh();
    baru.state = OtaState::Idle;
    baru.result = OtaResult::Confirmed;
    TEST_ASSERT_TRUE(s.save(baru));
    OtaRecord r = freshOtaRecord();
    TEST_ASSERT_TRUE(s.load(r));
    TEST_ASSERT_EQUAL(n(OtaState::Idle), n(r.state));
    TEST_ASSERT_EQUAL(n(OtaResult::Confirmed), n(r.result));
}

void test_catatan_yang_rusak_di_penyimpanan_ditolak()
{
    FakeBlob b;
    BlobOtaStore s(b, KUNCI);
    s.save(contoh());
    b.data[KUNCI][8] ^= 0x10;
    OtaRecord r = freshOtaRecord();
    TEST_ASSERT_FALSE(s.load(r));
}

void test_catatan_berukuran_salah_ditolak()
{
    FakeBlob b;
    BlobOtaStore s(b, KUNCI);
    s.save(contoh());
    b.data[KUNCI].push_back(0);
    OtaRecord r = freshOtaRecord();
    TEST_ASSERT_FALSE(s.load(r));
    b.data[KUNCI].resize(10);
    TEST_ASSERT_FALSE(s.load(r));
}

void test_gagal_menulis_dilaporkan()
{
    FakeBlob b;
    BlobOtaStore s(b, KUNCI);
    b.gagalTulis = true;
    TEST_ASSERT_FALSE(s.save(contoh()));
}

void test_tulisan_yang_rusak_terdeteksi_lewat_baca_ulang()
{
    FakeBlob b;
    BlobOtaStore s(b, KUNCI);
    b.rusakSaatTulis = true;
    TEST_ASSERT_FALSE(s.save(contoh()));
}

// Gabungan dengan mesin status: keadaan bertahan melewati "reboot" (mesin baru, RAM hilang).
void test_mesin_status_bertahan_melewati_reboot_lewat_penyimpan_blob()
{
    FakeBlob b;
    BlobOtaStore s(b, KUNCI);
    const OtaConfig cfg{3, 3, 3, 180000};
    {
        OtaStateMachine m(s, cfg);
        m.onBoot(100, 0);
        TEST_ASSERT_TRUE(m.requestUpdate(101, 100));
    }
    {
        OtaStateMachine m(s, cfg);
        TEST_ASSERT_EQUAL(n(BootAction::RunOta), n(m.onBoot(100, 0)));
        TEST_ASSERT_TRUE(m.otaInstalled());
    }
    OtaStateMachine m(s, cfg);
    TEST_ASSERT_EQUAL(n(BootAction::WatchHealth), n(m.onBoot(101, 0)));
    TEST_ASSERT_EQUAL(n(OtaState::Pending), n(m.state()));
}

void test_catatan_rusak_saat_boot_diganti_catatan_bersih_yang_valid()
{
    FakeBlob b;
    BlobOtaStore s(b, KUNCI);
    s.save(contoh());
    b.data[KUNCI][2] ^= 0xFF; // rusak
    const OtaConfig cfg{3, 3, 3, 180000};
    OtaStateMachine m(s, cfg);
    TEST_ASSERT_EQUAL(n(BootAction::Normal), n(m.onBoot(100, 0)));
    OtaRecord r = contoh();
    TEST_ASSERT_TRUE(s.load(r));
    TEST_ASSERT_EQUAL(n(OtaState::Idle), n(r.state));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_load_tanpa_catatan_gagal);
    RUN_TEST(test_simpan_lalu_baca_kembali_sama);
    RUN_TEST(test_simpan_menimpa_catatan_sebelumnya);
    RUN_TEST(test_catatan_yang_rusak_di_penyimpanan_ditolak);
    RUN_TEST(test_catatan_berukuran_salah_ditolak);
    RUN_TEST(test_gagal_menulis_dilaporkan);
    RUN_TEST(test_tulisan_yang_rusak_terdeteksi_lewat_baca_ulang);
    RUN_TEST(test_mesin_status_bertahan_melewati_reboot_lewat_penyimpan_blob);
    RUN_TEST(test_catatan_rusak_saat_boot_diganti_catatan_bersih_yang_valid);
    return UNITY_END();
}