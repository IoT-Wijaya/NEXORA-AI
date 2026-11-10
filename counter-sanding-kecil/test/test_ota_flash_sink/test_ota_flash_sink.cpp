#include <stdio.h>
#include <string.h>
#include <string>
#include <unity.h>
#include <vector>

#include "firmware_downloader.h"
#include "firmware_verifier.h"
#include "manifest.h"
#include "ota_flash_sink.h"
#include "sha256.h"

// ---------- flash palsu ----------
class FakeFlash : public IOtaFlash
{
public:
    std::string image;
    bool failBegin = false, failWriteAfter = false, failEnd = false, failBoot = false;
    size_t failWriteAtBytes = static_cast<size_t>(-1);
    int begins = 0, ends = 0, aborts = 0, boots = 0;
    uint32_t beginSize = 0;
    bool open = false;

    bool begin(uint32_t size) override
    {
        ++begins;
        beginSize = size;
        if (failBegin)
        {
            err_ = "begin gagal";
            return false;
        }
        image.clear();
        open = true;
        return true;
    }
    bool write(const uint8_t *d, size_t n) override
    {
        if (image.size() + n > failWriteAtBytes)
        {
            err_ = "tulis gagal";
            return false;
        }
        image.append(reinterpret_cast<const char *>(d), n);
        return true;
    }
    bool end() override
    {
        ++ends;
        open = false;
        if (failEnd)
        {
            err_ = "image tidak valid";
            return false;
        }
        return true;
    }
    void abort() override
    {
        ++aborts;
        open = false;
    }
    bool setBootToNew() override
    {
        if (failBoot)
        {
            err_ = "boot gagal";
            return false;
        }
        ++boots;
        return true;
    }
    const char *lastError() const override { return err_; }

private:
    const char *err_ = "";
};

static std::string FW;

static Manifest manifestFor(const std::string &data)
{
    Manifest m;
    memset(&m, 0, sizeof(m));
    m.size = static_cast<uint32_t>(data.size());
    Sha256 h;
    h.update(reinterpret_cast<const uint8_t *>(data.data()), data.size());
    h.finish(m.sha256);
    return m;
}

static const uint8_t *b(const std::string &s) { return reinterpret_cast<const uint8_t *>(s.data()); }

void setUp()
{
    FW.clear();
    FW.push_back(static_cast<char>(0xE9)); // magic image ESP32
    for (int i = 1; i < 3000; ++i)
        FW.push_back(static_cast<char>(i * 13 + 5));
}
void tearDown() {}

void test_alur_normal_slot_baru_hanya_dialihkan_saat_activate()
{
    FakeFlash f;
    const Manifest m = manifestFor(FW);
    OtaFlashSink s(f, m);
    TEST_ASSERT_TRUE(s.begin());
    TEST_ASSERT_EQUAL_UINT32(FW.size(), f.beginSize);
    TEST_ASSERT_TRUE(s.write(b(FW), 1000));
    TEST_ASSERT_TRUE(s.write(b(FW) + 1000, FW.size() - 1000));
    TEST_ASSERT_TRUE(s.finish());
    TEST_ASSERT_TRUE(s.ready());
    TEST_ASSERT_TRUE(f.image == FW);
    TEST_ASSERT_EQUAL(0, f.boots); // belum dialihkan
    TEST_ASSERT_TRUE(s.activate());
    TEST_ASSERT_EQUAL(1, f.boots);
    TEST_ASSERT_EQUAL(0, f.aborts);
}

void test_activate_sebelum_finish_ditolak()
{
    FakeFlash f;
    OtaFlashSink s(f, manifestFor(FW));
    s.begin();
    s.write(b(FW), FW.size());
    TEST_ASSERT_FALSE(s.activate());
    TEST_ASSERT_EQUAL(0, f.boots);
}

void test_hash_salah_ditolak_dan_flash_dibatalkan()
{
    FakeFlash f;
    const Manifest m = manifestFor(FW);
    std::string rusak = FW;
    rusak[1500] ^= 0x01;
    OtaFlashSink s(f, m);
    s.begin();
    s.write(b(rusak), rusak.size());
    TEST_ASSERT_FALSE(s.finish());
    TEST_ASSERT_FALSE(s.ready());
    TEST_ASSERT_EQUAL_STRING("hash_mismatch", s.error());
    TEST_ASSERT_EQUAL(1, f.aborts);
    TEST_ASSERT_EQUAL(0, f.ends); // image tidak pernah "disahkan"
    TEST_ASSERT_FALSE(s.activate());
    TEST_ASSERT_EQUAL(0, f.boots);
}

void test_data_kurang_ditolak()
{
    FakeFlash f;
    OtaFlashSink s(f, manifestFor(FW));
    s.begin();
    s.write(b(FW), FW.size() - 1);
    TEST_ASSERT_FALSE(s.finish());
    TEST_ASSERT_EQUAL_STRING("size_mismatch", s.error());
    TEST_ASSERT_EQUAL(1, f.aborts);
}

void test_data_berlebih_ditolak_saat_terjadi()
{
    FakeFlash f;
    OtaFlashSink s(f, manifestFor(FW));
    s.begin();
    TEST_ASSERT_TRUE(s.write(b(FW), FW.size()));
    TEST_ASSERT_FALSE(s.write(b(FW), 1));
    TEST_ASSERT_EQUAL_STRING("data melebihi ukuran manifest", s.error());
    TEST_ASSERT_EQUAL_UINT32(FW.size(), static_cast<uint32_t>(f.image.size())); // byte lebih tidak ditulis
}

void test_galat_tulis_flash_dilaporkan()
{
    FakeFlash f;
    f.failWriteAtBytes = 1000;
    OtaFlashSink s(f, manifestFor(FW));
    s.begin();
    TEST_ASSERT_TRUE(s.write(b(FW), 900));
    TEST_ASSERT_FALSE(s.write(b(FW) + 900, 200));
    TEST_ASSERT_EQUAL_STRING("tulis gagal", s.error());
}

void test_begin_gagal_tidak_membuka_apa_pun()
{
    FakeFlash f;
    f.failBegin = true;
    OtaFlashSink s(f, manifestFor(FW));
    TEST_ASSERT_FALSE(s.begin());
    TEST_ASSERT_EQUAL_STRING("begin gagal", s.error());
    TEST_ASSERT_FALSE(s.write(b(FW), 10));
    s.abort();
    TEST_ASSERT_EQUAL(0, f.aborts);
}

void test_image_tidak_valid_menurut_flash_ditolak()
{
    FakeFlash f;
    f.failEnd = true;
    OtaFlashSink s(f, manifestFor(FW));
    s.begin();
    s.write(b(FW), FW.size());
    TEST_ASSERT_FALSE(s.finish());
    TEST_ASSERT_FALSE(s.ready());
    TEST_ASSERT_FALSE(s.activate());
    TEST_ASSERT_EQUAL(0, f.boots);
    TEST_ASSERT_EQUAL(0, f.aborts); // end() sudah melepas handle: tidak boleh abort lagi
}

void test_galat_pengalihan_boot_dilaporkan()
{
    FakeFlash f;
    f.failBoot = true;
    OtaFlashSink s(f, manifestFor(FW));
    s.begin();
    s.write(b(FW), FW.size());
    s.finish();
    TEST_ASSERT_FALSE(s.activate());
    TEST_ASSERT_EQUAL_STRING("boot gagal", s.error());
}

void test_write_sebelum_begin_ditolak()
{
    FakeFlash f;
    OtaFlashSink s(f, manifestFor(FW));
    TEST_ASSERT_FALSE(s.write(b(FW), 10));
    TEST_ASSERT_FALSE(s.finish());
}

void test_destruktor_membatalkan_tulisan_yang_menggantung()
{
    FakeFlash f;
    {
        OtaFlashSink s(f, manifestFor(FW));
        s.begin();
        s.write(b(FW), 100);
    }
    TEST_ASSERT_EQUAL(1, f.aborts);
    FakeFlash g;
    {
        OtaFlashSink s(g, manifestFor(FW));
        s.begin();
        s.write(b(FW), FW.size());
        s.finish();
    }
    TEST_ASSERT_EQUAL(0, g.aborts); // sudah selesai: tidak dibatalkan
}

void test_begin_dua_kali_ditolak()
{
    FakeFlash f;
    OtaFlashSink s(f, manifestFor(FW));
    TEST_ASSERT_TRUE(s.begin());
    TEST_ASSERT_FALSE(s.begin());
    TEST_ASSERT_EQUAL(1, f.begins);
}

// ---------- rantai penuh: downloader + sink + flash, dengan koneksi putus ----------
struct OneConn
{
    std::string data;
    bool closeAtEnd;
    OneConn(const std::string &d = "", bool close = true) : data(d), closeAtEnd(close) {}
};
class ScriptTransport : public IHttpTransport
{
public:
    std::vector<OneConn> script;
    std::vector<std::string> requests;
    bool connect(const char *, uint16_t) override
    {
        if (next_ >= script.size())
            return false;
        cur_ = script[next_++];
        pos_ = 0;
        open_ = true;
        return true;
    }
    size_t write(const uint8_t *d, size_t n) override
    {
        requests.emplace_back((const char *)d, n);
        return n;
    }
    int read(uint8_t *buf, size_t cap) override
    {
        if (!open_ || pos_ >= cur_.data.size())
            return 0;
        size_t n = cur_.data.size() - pos_;
        if (n > cap)
            n = cap;
        if (n > 333)
            n = 333;
        memcpy(buf, cur_.data.data() + pos_, n);
        pos_ += n;
        return (int)n;
    }
    bool connected() override { return open_ && (pos_ < cur_.data.size() || !cur_.closeAtEnd); }
    void stop() override { open_ = false; }

private:
    size_t next_ = 0, pos_ = 0;
    OneConn cur_;
    bool open_ = false;
};

static std::string http200(const std::string &body, size_t promised)
{
    char h[128];
    snprintf(h, sizeof(h), "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n", promised);
    return h + body;
}
static std::string http206(const std::string &full, size_t from)
{
    char h[192];
    snprintf(h, sizeof(h), "HTTP/1.1 206 Partial Content\r\nContent-Length: %zu\r\nContent-Range: bytes %zu-%zu/%zu\r\n\r\n",
             full.size() - from, from, full.size() - 1, full.size());
    return h + full.substr(from);
}

static DownloadState drive(FirmwareDownloader &d)
{
    uint32_t now = 0;
    for (int i = 0; i < 20000 && d.state() == DownloadState::Running; ++i, now += 100)
        d.step(now);
    return d.state();
}

void test_rantai_penuh_putus_di_tengah_lalu_lanjut_hash_tetap_cocok()
{
    ScriptTransport t;
    t.script.push_back({http200(FW.substr(0, 1700), FW.size())});
    t.script.push_back({http206(FW, 1700)});
    FakeFlash f;
    const Manifest m = manifestFor(FW);
    OtaFlashSink sink(f, m);
    TEST_ASSERT_TRUE(sink.begin());
    FirmwareDownloader d(t, sink);
    DownloadConfig c;
    c.host = "monitoring.wijayaplywoods.com";
    c.path = "/fw/firmware.bin";
    c.expectedSize = m.size;
    c.retryDelayMs = 500;
    c.stallTimeoutMs = 3000;
    d.start(c, 0);
    TEST_ASSERT_EQUAL(DownloadState::Done, drive(d));
    TEST_ASSERT_TRUE(sink.finish());
    TEST_ASSERT_TRUE(f.image == FW);
    TEST_ASSERT_NOT_NULL(strstr(t.requests[1].c_str(), "Range: bytes=1700-"));
    TEST_ASSERT_EQUAL(0, f.boots);
}

void test_rantai_penuh_server_mengirim_firmware_lain_ditolak_oleh_hash()
{
    std::string lain = FW;
    lain[2000] ^= 0x40; // ukuran sama, isi beda
    ScriptTransport t;
    t.script.push_back({http200(lain, lain.size())});
    FakeFlash f;
    const Manifest m = manifestFor(FW);
    OtaFlashSink sink(f, m);
    sink.begin();
    FirmwareDownloader d(t, sink);
    DownloadConfig c;
    c.host = "h";
    c.path = "/f";
    c.expectedSize = m.size;
    d.start(c, 0);
    TEST_ASSERT_EQUAL(DownloadState::Done, drive(d)); // unduhan sendiri sukses...
    TEST_ASSERT_FALSE(sink.finish());                 // ...tetapi hash menolaknya
    TEST_ASSERT_EQUAL_STRING("hash_mismatch", sink.error());
    TEST_ASSERT_EQUAL(0, f.boots);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_alur_normal_slot_baru_hanya_dialihkan_saat_activate);
    RUN_TEST(test_activate_sebelum_finish_ditolak);
    RUN_TEST(test_hash_salah_ditolak_dan_flash_dibatalkan);
    RUN_TEST(test_data_kurang_ditolak);
    RUN_TEST(test_data_berlebih_ditolak_saat_terjadi);
    RUN_TEST(test_galat_tulis_flash_dilaporkan);
    RUN_TEST(test_begin_gagal_tidak_membuka_apa_pun);
    RUN_TEST(test_image_tidak_valid_menurut_flash_ditolak);
    RUN_TEST(test_galat_pengalihan_boot_dilaporkan);
    RUN_TEST(test_write_sebelum_begin_ditolak);
    RUN_TEST(test_destruktor_membatalkan_tulisan_yang_menggantung);
    RUN_TEST(test_begin_dua_kali_ditolak);
    RUN_TEST(test_rantai_penuh_putus_di_tengah_lalu_lanjut_hash_tetap_cocok);
    RUN_TEST(test_rantai_penuh_server_mengirim_firmware_lain_ditolak_oleh_hash);
    return UNITY_END();
}