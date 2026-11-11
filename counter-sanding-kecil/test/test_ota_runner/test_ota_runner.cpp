#include <map>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unity.h>
#include <vector>

#include "monocypher-ed25519.h"
#include "ota_runner.h"
#include "sha256.h"

// ---------- jam palsu ----------
class FakeClock : public IClock
{
public:
    uint32_t now = 1000;
    uint32_t nowMs() override { return now; }
    void sleepMs(uint32_t ms) override { now += ms; }
};

// ---------- flash palsu ----------
class FakeFlash : public IOtaFlash
{
public:
    std::string image;
    bool failBegin = false, failBoot = false;
    int begins = 0, aborts = 0, boots = 0;
    bool begin(uint32_t) override
    {
        ++begins;
        if (failBegin)
        {
            err_ = "begin gagal";
            return false;
        }
        image.clear();
        return true;
    }
    bool write(const uint8_t *d, size_t n) override
    {
        image.append((const char *)d, n);
        return true;
    }
    bool end() override { return true; }
    void abort() override { ++aborts; }
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

// ---------- server palsu: jawaban dipilih menurut path pada permintaan ----------
struct Reply
{
    std::string data;
    bool closeAtEnd;
    Reply(const std::string &d = "", bool c = true) : data(d), closeAtEnd(c) {}
};

class FakeServer : public IHttpTransport
{
public:
    std::map<std::string, std::vector<Reply>> routes;
    std::vector<std::string> paths; // urutan path yang diminta
    std::vector<std::string> requests;
    bool connect(const char *, uint16_t) override
    {
        open_ = true;
        cur_ = Reply("", true);
        pos_ = 0;
        return true;
    }
    size_t write(const uint8_t *d, size_t n) override
    {
        std::string req((const char *)d, n);
        requests.push_back(req);
        const size_t a = req.find(' '), b = req.find(' ', a + 1);
        const std::string path = req.substr(a + 1, b - a - 1);
        paths.push_back(path);
        std::vector<Reply> &q = routes[path];
        size_t &used = served_[path];
        if (used < q.size())
            cur_ = q[used++];
        return n;
    }
    int read(uint8_t *buf, size_t cap) override
    {
        if (!open_ || pos_ >= cur_.data.size())
            return 0;
        size_t n = cur_.data.size() - pos_;
        if (n > cap)
            n = cap;
        if (n > 700)
            n = 700;
        memcpy(buf, cur_.data.data() + pos_, n);
        pos_ += n;
        return (int)n;
    }
    bool connected() override { return open_ && (pos_ < cur_.data.size() || !cur_.closeAtEnd); }
    void stop() override { open_ = false; }
    int count(const char *path) const
    {
        int c = 0;
        for (const auto &p : paths)
            if (p == path)
                ++c;
        return c;
    }

private:
    bool open_ = false;
    Reply cur_;
    size_t pos_ = 0;
    std::map<std::string, size_t> served_;
};

// ---------- bahan uji ----------
static uint8_t SECRET[64], PUBLIC[32], OTHER_SECRET[64], OTHER_PUBLIC[32];
static std::string FW;
static const char *HOST = "monitoring.wijayaplywoods.com";
static const char *MPATH = "/fw/manifest.signed";
static const char *FPATH = "/fw/firmware.bin";

static std::string hex(const uint8_t *d, size_t n)
{
    std::string s;
    char t[3];
    for (size_t i = 0; i < n; ++i)
    {
        snprintf(t, 3, "%02x", d[i]);
        s += t;
    }
    return s;
}

static std::string signedManifest(const std::string &fw, const uint8_t *secret, const char *deviceType = "sanding-kecil",
                                  unsigned build = 12, size_t sizeOverride = 0)
{
    uint8_t digest[32];
    Sha256 h;
    h.update((const uint8_t *)fw.data(), fw.size());
    h.finish(digest);
    char json[512];
    snprintf(json, sizeof(json),
             "{\"schema\":1,\"device_type\":\"%s\",\"board\":\"nodemcu-32s\",\"version\":\"fw-v1.0.0\","
             "\"build\":%u,\"size\":%zu,\"sha256\":\"%s\"}",
             deviceType, build, sizeOverride ? sizeOverride : fw.size(), hex(digest, 32).c_str());
    uint8_t sig[64];
    crypto_ed25519_sign(sig, secret, (const uint8_t *)json, strlen(json));
    return std::string((const char *)sig, 64) + json;
}

static std::string http200(const std::string &body)
{
    char h[96];
    snprintf(h, sizeof(h), "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n", body.size());
    return h + body;
}
static std::string http200Promising(const std::string &partialBody, size_t promised)
{
    char h[96];
    snprintf(h, sizeof(h), "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n", promised);
    return h + partialBody;
}
static std::string http206(const std::string &full, size_t from)
{
    char h[192];
    snprintf(h, sizeof(h), "HTTP/1.1 206 Partial Content\r\nContent-Length: %zu\r\nContent-Range: bytes %zu-%zu/%zu\r\n\r\n",
             full.size() - from, from, full.size() - 1, full.size());
    return h + full.substr(from);
}
static std::string http404() { return "HTTP/1.1 404 Not Found\r\nContent-Length: 1\r\n\r\nx"; }

static OtaRunConfig config(bool dryRun)
{
    OtaRunConfig c;
    c.host = HOST;
    c.port = 80;
    c.manifestPath = MPATH;
    c.firmwarePath = FPATH;
    c.publicKey = PUBLIC;
    c.dryRun = dryRun;
    c.profile = DeviceProfile{"sanding-kecil", "nodemcu-32s", 0, 0x140000};
    c.overallTimeoutMs = 60000;
    c.retryDelayMs = 500;
    c.stallTimeoutMs = 3000;
    c.maxFailuresWithoutProgress = 3;
    return c;
}

struct Rig
{
    FakeServer server;
    FakeFlash flash;
    FakeClock clock;
    std::vector<std::string> logs;
    OtaRunner runner;
    Rig() : runner(server, flash, clock, [this](const char *l)
                   { logs.push_back(l); }) {}
    void goodServer()
    {
        server.routes[MPATH].push_back(Reply(http200(signedManifest(FW, SECRET))));
        server.routes[FPATH].push_back(Reply(http200(FW)));
    }
};

void setUp()
{
    FW.clear();
    FW.push_back((char)0xE9);
    for (int i = 1; i < 4000; ++i)
        FW.push_back((char)(i * 7 + 3));
    uint8_t seed[32];
    memset(seed, 0x11, 32);
    crypto_ed25519_key_pair(SECRET, PUBLIC, seed);
    memset(seed, 0x22, 32);
    crypto_ed25519_key_pair(OTHER_SECRET, OTHER_PUBLIC, seed);
}
void tearDown() {}

// ---------- jalur sukses ----------
void test_dry_run_semua_lolos_tetapi_slot_tidak_dialihkan()
{
    Rig r;
    r.goodServer();
    const OtaRunOutcome o = r.runner.run(config(true));
    TEST_ASSERT_EQUAL(OtaRunResult::DryRunOk, o.result);
    TEST_ASSERT_TRUE(o.ok());
    TEST_ASSERT_TRUE(r.flash.image == FW);
    TEST_ASSERT_EQUAL(0, r.flash.boots);
    TEST_ASSERT_TRUE(o.manifestValid);
    TEST_ASSERT_EQUAL_UINT32(12, o.manifest.build);
    TEST_ASSERT_EQUAL(2, (int)r.server.paths.size());
    TEST_ASSERT_EQUAL_STRING(MPATH, r.server.paths[0].c_str());
    TEST_ASSERT_EQUAL_STRING(FPATH, r.server.paths[1].c_str());
}

void test_pemasangan_sungguhan_mengalihkan_slot_sekali()
{
    Rig r;
    r.goodServer();
    const OtaRunOutcome o = r.runner.run(config(false));
    TEST_ASSERT_EQUAL(OtaRunResult::Installed, o.result);
    TEST_ASSERT_EQUAL(1, r.flash.boots);
    TEST_ASSERT_TRUE(r.flash.image == FW);
}

void test_log_memuat_langkah_penting()
{
    Rig r;
    r.goodServer();
    r.runner.run(config(true));
    TEST_ASSERT_TRUE(r.logs.size() >= 5);
}

// ---------- manifest ditolak: flash tidak boleh disentuh ----------
void test_manifest_404()
{
    Rig r;
    r.server.routes[MPATH].push_back(Reply(http404()));
    const OtaRunOutcome o = r.runner.run(config(false));
    TEST_ASSERT_EQUAL(OtaRunResult::ManifestDownloadFailed, o.result);
    TEST_ASSERT_EQUAL(DownloadFailure::BadStatus, o.download);
    TEST_ASSERT_EQUAL(0, r.flash.begins);
}

void test_manifest_ditandatangani_kunci_lain()
{
    Rig r;
    r.server.routes[MPATH].push_back(Reply(http200(signedManifest(FW, OTHER_SECRET))));
    r.server.routes[FPATH].push_back(Reply(http200(FW)));
    const OtaRunOutcome o = r.runner.run(config(false));
    TEST_ASSERT_EQUAL(OtaRunResult::ManifestSignatureInvalid, o.result);
    TEST_ASSERT_FALSE(o.manifestValid);
    TEST_ASSERT_EQUAL(0, r.flash.begins);
    TEST_ASSERT_EQUAL(0, r.server.count(FPATH)); // firmware tidak pernah diminta
}

void test_manifest_diubah_di_tengah_jalan()
{
    Rig r;
    std::string env = signedManifest(FW, SECRET);
    env[64 + 30] ^= 0x01;
    r.server.routes[MPATH].push_back(Reply(http200(env)));
    const OtaRunOutcome o = r.runner.run(config(false));
    TEST_ASSERT_EQUAL(OtaRunResult::ManifestSignatureInvalid, o.result);
    TEST_ASSERT_EQUAL(0, r.flash.begins);
}

void test_manifest_terlalu_pendek()
{
    Rig r;
    r.server.routes[MPATH].push_back(Reply(http200("pendek")));
    const OtaRunOutcome o = r.runner.run(config(false));
    TEST_ASSERT_EQUAL(OtaRunResult::ManifestInvalid, o.result);
    TEST_ASSERT_EQUAL(SignedManifestError::TooShort, o.signedError);
}

// ---------- kebijakan ----------
void test_kebijakan_menolak_jenis_perangkat_lain()
{
    Rig r;
    r.server.routes[MPATH].push_back(Reply(http200(signedManifest(FW, SECRET, "mesin-lain"))));
    const OtaRunOutcome o = r.runner.run(config(false));
    TEST_ASSERT_EQUAL(OtaRunResult::PolicyRejected, o.result);
    TEST_ASSERT_EQUAL(PolicyDecision::WrongDeviceType, o.policy);
    TEST_ASSERT_EQUAL(0, r.flash.begins);
    TEST_ASSERT_EQUAL(0, r.server.count(FPATH));
}

void test_kebijakan_menolak_build_yang_sama()
{
    Rig r;
    r.goodServer();
    OtaRunConfig c = config(false);
    c.profile.currentBuild = 12;
    const OtaRunOutcome o = r.runner.run(c);
    TEST_ASSERT_EQUAL(OtaRunResult::PolicyRejected, o.result);
    TEST_ASSERT_EQUAL(PolicyDecision::AlreadyInstalled, o.policy);
}

void test_kebijakan_menolak_biner_lebih_besar_dari_slot()
{
    Rig r;
    r.goodServer();
    OtaRunConfig c = config(false);
    c.profile.slotSize = 1000;
    const OtaRunOutcome o = r.runner.run(c);
    TEST_ASSERT_EQUAL(OtaRunResult::PolicyRejected, o.result);
    TEST_ASSERT_EQUAL(PolicyDecision::TooLarge, o.policy);
    TEST_ASSERT_EQUAL(0, r.flash.begins);
}

// ---------- firmware ----------
void test_firmware_berbeda_dari_yang_ditandatangani_ditolak_hash()
{
    Rig r;
    std::string lain = FW;
    lain[2500] ^= 0x40;
    r.server.routes[MPATH].push_back(Reply(http200(signedManifest(FW, SECRET))));
    r.server.routes[FPATH].push_back(Reply(http200(lain)));
    const OtaRunOutcome o = r.runner.run(config(false));
    TEST_ASSERT_EQUAL(OtaRunResult::FirmwareVerifyFailed, o.result);
    TEST_ASSERT_EQUAL_STRING("hash_mismatch", o.detail);
    TEST_ASSERT_EQUAL(0, r.flash.boots);
}

void test_firmware_404_membatalkan_tulisan_flash()
{
    Rig r;
    r.server.routes[MPATH].push_back(Reply(http200(signedManifest(FW, SECRET))));
    r.server.routes[FPATH].push_back(Reply(http404()));
    const OtaRunOutcome o = r.runner.run(config(false));
    TEST_ASSERT_EQUAL(OtaRunResult::FirmwareDownloadFailed, o.result);
    TEST_ASSERT_EQUAL(DownloadFailure::BadStatus, o.download);
    TEST_ASSERT_EQUAL(1, r.flash.aborts);
    TEST_ASSERT_EQUAL(0, r.flash.boots);
}

void test_koneksi_putus_di_tengah_firmware_lalu_lanjut()
{
    Rig r;
    r.server.routes[MPATH].push_back(Reply(http200(signedManifest(FW, SECRET))));
    r.server.routes[FPATH].push_back(Reply(http200Promising(FW.substr(0, 1800), FW.size())));
    r.server.routes[FPATH].push_back(Reply(http206(FW, 1800)));
    const OtaRunOutcome o = r.runner.run(config(false));
    TEST_ASSERT_EQUAL(OtaRunResult::Installed, o.result);
    TEST_ASSERT_TRUE(r.flash.image == FW);
    TEST_ASSERT_NOT_NULL(strstr(r.server.requests.back().c_str(), "Range: bytes=1800-"));
}

void test_ukuran_di_manifest_beda_dari_yang_dikirim_server()
{
    Rig r;
    r.server.routes[MPATH].push_back(Reply(http200(signedManifest(FW, SECRET, "sanding-kecil", 12, FW.size() - 10))));
    r.server.routes[FPATH].push_back(Reply(http200(FW)));
    const OtaRunOutcome o = r.runner.run(config(false));
    TEST_ASSERT_EQUAL(OtaRunResult::FirmwareDownloadFailed, o.result);
    TEST_ASSERT_EQUAL(DownloadFailure::SizeMismatch, o.download);
    TEST_ASSERT_EQUAL(0, r.flash.boots);
}

// ---------- kegagalan perangkat ----------
void test_flash_gagal_disiapkan_tanpa_mengunduh_firmware()
{
    Rig r;
    r.goodServer();
    r.flash.failBegin = true;
    const OtaRunOutcome o = r.runner.run(config(false));
    TEST_ASSERT_EQUAL(OtaRunResult::FlashBeginFailed, o.result);
    TEST_ASSERT_EQUAL_STRING("begin gagal", o.detail);
    TEST_ASSERT_EQUAL(0, r.server.count(FPATH));
}

void test_pengalihan_slot_gagal_dilaporkan()
{
    Rig r;
    r.goodServer();
    r.flash.failBoot = true;
    const OtaRunOutcome o = r.runner.run(config(false));
    TEST_ASSERT_EQUAL(OtaRunResult::ActivateFailed, o.result);
    TEST_ASSERT_EQUAL_STRING("boot gagal", o.detail);
}

void test_server_macet_selamanya_berhenti_di_batas_waktu_keseluruhan()
{
    Rig r;
    r.server.routes[MPATH].push_back(Reply(http200(signedManifest(FW, SECRET))));
    r.server.routes[FPATH].push_back(Reply(http200Promising(FW.substr(0, 500), FW.size()), false));
    OtaRunConfig c = config(false);
    c.stallTimeoutMs = 10000000; // pengunduh sendiri tidak akan menyerah
    c.overallTimeoutMs = 5000;
    const uint32_t t0 = r.clock.now;
    const OtaRunOutcome o = r.runner.run(c);
    TEST_ASSERT_EQUAL(OtaRunResult::TimedOut, o.result);
    TEST_ASSERT_EQUAL(1, r.flash.aborts);
    TEST_ASSERT_EQUAL(0, r.flash.boots);
    TEST_ASSERT_TRUE(r.clock.now - t0 < 7000);
}

void test_konfigurasi_kosong()
{
    Rig r;
    OtaRunConfig c = config(false);
    c.publicKey = nullptr;
    TEST_ASSERT_EQUAL(OtaRunResult::BadConfig, r.runner.run(c).result);
    OtaRunConfig d = config(false);
    d.host = nullptr;
    TEST_ASSERT_EQUAL(OtaRunResult::BadConfig, r.runner.run(d).result);
    TEST_ASSERT_EQUAL(0, (int)r.server.paths.size());
}

void test_kunci_publik_nol_menolak_semuanya()
{
    Rig r;
    r.goodServer();
    uint8_t nol[32] = {0};
    OtaRunConfig c = config(false);
    c.publicKey = nol;
    const OtaRunOutcome o = r.runner.run(c);
    TEST_ASSERT_EQUAL(OtaRunResult::ManifestSignatureInvalid, o.result);
    TEST_ASSERT_EQUAL(0, r.flash.begins);
}

void test_build_di_manifest_beda_dari_yang_diminta_ditolak()
{
    Rig r;
    r.goodServer(); // manifest berisi build 12
    OtaRunConfig c = config(false);
    c.expectedBuild = 13;
    const OtaRunOutcome o = r.runner.run(c);
    TEST_ASSERT_EQUAL(OtaRunResult::UnexpectedBuild, o.result);
    TEST_ASSERT_TRUE(o.manifestValid);
    TEST_ASSERT_EQUAL(0, r.flash.begins);
    TEST_ASSERT_EQUAL(0, r.server.count(FPATH));
}

void test_build_yang_diminta_sama_dengan_manifest_diterima()
{
    Rig r;
    r.goodServer();
    OtaRunConfig c = config(true);
    c.expectedBuild = 12;
    TEST_ASSERT_EQUAL(OtaRunResult::DryRunOk, r.runner.run(c).result);
}

void test_pemetaan_hasil_runner_ke_catatan_ota()
{
    TEST_ASSERT_EQUAL(OtaResult::None, otaResultFor(OtaRunResult::Installed));
    TEST_ASSERT_EQUAL(OtaResult::None, otaResultFor(OtaRunResult::DryRunOk));
    TEST_ASSERT_EQUAL(OtaResult::DownloadFailed, otaResultFor(OtaRunResult::ManifestDownloadFailed));
    TEST_ASSERT_EQUAL(OtaResult::DownloadFailed, otaResultFor(OtaRunResult::FirmwareDownloadFailed));
    TEST_ASSERT_EQUAL(OtaResult::DownloadFailed, otaResultFor(OtaRunResult::TimedOut));
    TEST_ASSERT_EQUAL(OtaResult::DownloadFailed, otaResultFor(OtaRunResult::BadConfig));
    TEST_ASSERT_EQUAL(OtaResult::VerifyFailed, otaResultFor(OtaRunResult::ManifestSignatureInvalid));
    TEST_ASSERT_EQUAL(OtaResult::VerifyFailed, otaResultFor(OtaRunResult::ManifestInvalid));
    TEST_ASSERT_EQUAL(OtaResult::VerifyFailed, otaResultFor(OtaRunResult::UnexpectedBuild));
    TEST_ASSERT_EQUAL(OtaResult::VerifyFailed, otaResultFor(OtaRunResult::PolicyRejected));
    TEST_ASSERT_EQUAL(OtaResult::VerifyFailed, otaResultFor(OtaRunResult::FirmwareVerifyFailed));
    TEST_ASSERT_EQUAL(OtaResult::WriteFailed, otaResultFor(OtaRunResult::FlashBeginFailed));
    TEST_ASSERT_EQUAL(OtaResult::WriteFailed, otaResultFor(OtaRunResult::ActivateFailed));
}

void test_runner_bisa_dipakai_ulang()
{
    Rig r;
    r.goodServer();
    TEST_ASSERT_EQUAL(OtaRunResult::DryRunOk, r.runner.run(config(true)).result);
    r.server.routes[MPATH].push_back(Reply(http200(signedManifest(FW, SECRET))));
    r.server.routes[FPATH].push_back(Reply(http200(FW)));
    TEST_ASSERT_EQUAL(OtaRunResult::Installed, r.runner.run(config(false)).result);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_dry_run_semua_lolos_tetapi_slot_tidak_dialihkan);
    RUN_TEST(test_pemasangan_sungguhan_mengalihkan_slot_sekali);
    RUN_TEST(test_log_memuat_langkah_penting);
    RUN_TEST(test_manifest_404);
    RUN_TEST(test_manifest_ditandatangani_kunci_lain);
    RUN_TEST(test_manifest_diubah_di_tengah_jalan);
    RUN_TEST(test_manifest_terlalu_pendek);
    RUN_TEST(test_kebijakan_menolak_jenis_perangkat_lain);
    RUN_TEST(test_kebijakan_menolak_build_yang_sama);
    RUN_TEST(test_kebijakan_menolak_biner_lebih_besar_dari_slot);
    RUN_TEST(test_firmware_berbeda_dari_yang_ditandatangani_ditolak_hash);
    RUN_TEST(test_firmware_404_membatalkan_tulisan_flash);
    RUN_TEST(test_koneksi_putus_di_tengah_firmware_lalu_lanjut);
    RUN_TEST(test_ukuran_di_manifest_beda_dari_yang_dikirim_server);
    RUN_TEST(test_flash_gagal_disiapkan_tanpa_mengunduh_firmware);
    RUN_TEST(test_pengalihan_slot_gagal_dilaporkan);
    RUN_TEST(test_server_macet_selamanya_berhenti_di_batas_waktu_keseluruhan);
    RUN_TEST(test_konfigurasi_kosong);
    RUN_TEST(test_kunci_publik_nol_menolak_semuanya);
    RUN_TEST(test_build_di_manifest_beda_dari_yang_diminta_ditolak);
    RUN_TEST(test_build_yang_diminta_sama_dengan_manifest_diterima);
    RUN_TEST(test_pemetaan_hasil_runner_ke_catatan_ota);
    RUN_TEST(test_runner_bisa_dipakai_ulang);
    return UNITY_END();
}