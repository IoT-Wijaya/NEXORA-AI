#include <stdio.h>
#include <string.h>
#include <string>
#include <unity.h>
#include <vector>

#include "firmware_downloader.h"

// ---------- transport palsu: satu "Conn" per sambungan ----------
struct Conn
{
    bool connectOk = true;
    std::string data;       // yang dikirim server (header + isi)
    size_t chunk = 100000;  // maksimum byte per read()
    bool closeAtEnd = true; // false = server diam setelah data habis (macet)
};

class FakeTransport : public IHttpTransport
{
public:
    std::vector<Conn> script;
    std::vector<std::string> requests;
    int connects = 0;
    int stops = 0;
    bool writeFails = false;

    bool connect(const char *, uint16_t) override
    {
        ++connects;
        if (next_ >= script.size())
            return false;
        cur_ = script[next_++];
        if (!cur_.connectOk)
            return false;
        open_ = true;
        pos_ = 0;
        return true;
    }
    size_t write(const uint8_t *d, size_t n) override
    {
        if (writeFails)
            return 0;
        requests.emplace_back(reinterpret_cast<const char *>(d), n);
        return n;
    }
    int read(uint8_t *buf, size_t cap) override
    {
        if (!open_ || pos_ >= cur_.data.size())
            return 0;
        size_t n = cur_.data.size() - pos_;
        if (n > cap)
            n = cap;
        if (n > cur_.chunk)
            n = cur_.chunk;
        memcpy(buf, cur_.data.data() + pos_, n);
        pos_ += n;
        return static_cast<int>(n);
    }
    bool connected() override
    {
        if (!open_)
            return false;
        return pos_ < cur_.data.size() || !cur_.closeAtEnd;
    }
    void stop() override
    {
        ++stops;
        open_ = false;
    }

private:
    size_t next_ = 0;
    Conn cur_;
    size_t pos_ = 0;
    bool open_ = false;
};

class FakeSink : public IByteSink
{
public:
    std::string got;
    size_t rejectAfter = static_cast<size_t>(-1);
    bool write(const uint8_t *d, size_t n) override
    {
        if (got.size() + n > rejectAfter)
            return false;
        got.append(reinterpret_cast<const char *>(d), n);
        return true;
    }
};

static std::string FW; // "firmware" uji

static std::string response(const char *status, const std::string &body,
                            const char *extra = "", long lengthOverride = -1)
{
    char h[256];
    const long len = lengthOverride >= 0 ? lengthOverride : static_cast<long>(body.size());
    snprintf(h, sizeof(h), "HTTP/1.1 %s\r\nContent-Length: %ld\r\n%s\r\n", status, len, extra);
    return std::string(h) + body;
}

static std::string partial(size_t from)
{
    const std::string body = FW.substr(from);
    char extra[96];
    snprintf(extra, sizeof(extra), "Content-Range: bytes %zu-%zu/%zu\r\n", from, FW.size() - 1,
             FW.size());
    return response("206 Partial Content", body, extra);
}

static DownloadConfig cfg()
{
    DownloadConfig c;
    c.host = "fw.example.com";
    c.port = 80;
    c.path = "/fw/1.bin";
    c.expectedSize = static_cast<uint32_t>(FW.size());
    c.maxFailuresWithoutProgress = 3;
    c.retryDelayMs = 1000;
    c.stallTimeoutMs = 5000;
    return c;
}

// Jalankan sampai selesai; waktu maju 100 ms per langkah.
static uint32_t run(FirmwareDownloader &d, uint32_t now = 0, int maxSteps = 5000)
{
    for (int i = 0; i < maxSteps && d.state() == DownloadState::Running; ++i)
    {
        d.step(now);
        now += 100;
    }
    return now;
}

void setUp()
{
    FW.clear();
    for (int i = 0; i < 3000; ++i)
        FW.push_back(static_cast<char>(i * 31 + 7));
}
void tearDown() {}

void test_unduh_penuh_tanpa_range()
{
    FakeTransport t;
    FakeSink s;
    Conn c;
    c.data = response("200 OK", FW);
    t.script.push_back(c);
    FirmwareDownloader d(t, s);
    d.start(cfg(), 0);
    run(d);
    TEST_ASSERT_EQUAL(DownloadState::Done, d.state());
    TEST_ASSERT_TRUE(s.got == FW);
    TEST_ASSERT_EQUAL_UINT32(FW.size(), d.received());
    TEST_ASSERT_EQUAL(1, t.connects);
    TEST_ASSERT_NULL(strstr(t.requests[0].c_str(), "Range:"));
    TEST_ASSERT_NOT_NULL(strstr(t.requests[0].c_str(), "Host: fw.example.com"));
}

void test_potongan_kecil_hasilnya_sama()
{
    FakeTransport t;
    FakeSink s;
    Conn c;
    c.data = response("200 OK", FW);
    c.chunk = 7;
    t.script.push_back(c);
    FirmwareDownloader d(t, s);
    d.start(cfg(), 0);
    run(d);
    TEST_ASSERT_EQUAL(DownloadState::Done, d.state());
    TEST_ASSERT_TRUE(s.got == FW);
}

void test_koneksi_putus_lalu_lanjut_dengan_range()
{
    FakeTransport t;
    FakeSink s;
    const size_t cut = 1234;
    Conn a;
    a.data = response("200 OK", FW.substr(0, cut), "", FW.size()); // janji penuh, kirim sebagian
    Conn b;
    b.data = partial(cut);
    t.script.push_back(a);
    t.script.push_back(b);
    FirmwareDownloader d(t, s);
    d.start(cfg(), 0);
    run(d);
    TEST_ASSERT_EQUAL(DownloadState::Done, d.state());
    TEST_ASSERT_TRUE(s.got == FW);
    TEST_ASSERT_EQUAL(2, t.connects);
    TEST_ASSERT_NOT_NULL(strstr(t.requests[1].c_str(), "Range: bytes=1234-"));
}

void test_jeda_antar_percobaan_dihormati()
{
    FakeTransport t;
    FakeSink s;
    Conn bad;
    bad.connectOk = false;
    t.script.push_back(bad);
    Conn ok;
    ok.data = response("200 OK", FW);
    t.script.push_back(ok);
    FirmwareDownloader d(t, s);
    d.start(cfg(), 0);
    d.step(0); // gagal sambung
    TEST_ASSERT_EQUAL(1, t.connects);
    d.step(500);
    d.step(900); // masih menunggu 1000 ms
    TEST_ASSERT_EQUAL(1, t.connects);
    d.step(1000);
    d.step(1100); // jeda lewat: sambung lagi
    TEST_ASSERT_EQUAL(2, t.connects);
}

void test_gagal_sambung_berulang_menyerah()
{
    FakeTransport t;
    FakeSink s; // script kosong: connect selalu gagal
    FirmwareDownloader d(t, s);
    d.start(cfg(), 0);
    run(d);
    TEST_ASSERT_EQUAL(DownloadState::Failed, d.state());
    TEST_ASSERT_EQUAL(DownloadFailure::TooManyRetries, d.failure());
    TEST_ASSERT_EQUAL(DownloadFailure::ConnectFailed, d.lastTransient());
    TEST_ASSERT_EQUAL(3, t.connects);
}

void test_server_5xx_diulang_lalu_berhasil()
{
    FakeTransport t;
    FakeSink s;
    Conn e;
    e.data = response("503 Service Unavailable", "x");
    Conn ok;
    ok.data = response("200 OK", FW);
    t.script.push_back(e);
    t.script.push_back(ok);
    FirmwareDownloader d(t, s);
    d.start(cfg(), 0);
    run(d);
    TEST_ASSERT_EQUAL(DownloadState::Done, d.state());
    TEST_ASSERT_TRUE(s.got == FW);
}

void test_404_dan_301_tidak_diulang()
{
    const char *status[] = {"404 Not Found", "301 Moved Permanently", "403 Forbidden"};
    for (const char *st : status)
    {
        FakeTransport t;
        FakeSink s;
        Conn c;
        c.data = response(st, "x");
        t.script.push_back(c);
        FirmwareDownloader d(t, s);
        d.start(cfg(), 0);
        run(d);
        TEST_ASSERT_EQUAL_MESSAGE(DownloadState::Failed, d.state(), st);
        TEST_ASSERT_EQUAL_MESSAGE(DownloadFailure::BadStatus, d.failure(), st);
        TEST_ASSERT_EQUAL_MESSAGE(1, t.connects, st);
    }
}

void test_server_mengabaikan_range_dilaporkan()
{
    FakeTransport t;
    FakeSink s;
    Conn a;
    a.data = response("200 OK", FW.substr(0, 500), "", FW.size());
    Conn b;
    b.data = response("200 OK", FW); // seharusnya 206
    t.script.push_back(a);
    t.script.push_back(b);
    FirmwareDownloader d(t, s);
    d.start(cfg(), 0);
    run(d);
    TEST_ASSERT_EQUAL(DownloadState::Failed, d.state());
    TEST_ASSERT_EQUAL(DownloadFailure::RangeIgnored, d.failure());
}

void test_ukuran_beda_dari_manifest_ditolak_sebelum_menulis()
{
    FakeTransport t;
    FakeSink s;
    Conn c;
    c.data = response("200 OK", FW + "tambahan");
    t.script.push_back(c);
    FirmwareDownloader d(t, s);
    d.start(cfg(), 0);
    run(d);
    TEST_ASSERT_EQUAL(DownloadFailure::SizeMismatch, d.failure());
    TEST_ASSERT_EQUAL(0, s.got.size());
}

void test_penulis_menolak_menghentikan_unduhan()
{
    FakeTransport t;
    FakeSink s;
    s.rejectAfter = 1000;
    Conn c;
    c.data = response("200 OK", FW);
    c.chunk = 400;
    t.script.push_back(c);
    FirmwareDownloader d(t, s);
    d.start(cfg(), 0);
    run(d);
    TEST_ASSERT_EQUAL(DownloadState::Failed, d.state());
    TEST_ASSERT_EQUAL(DownloadFailure::SinkRejected, d.failure());
    TEST_ASSERT_TRUE(t.stops > 0);
}

void test_macet_diulang_setelah_batas_waktu()
{
    FakeTransport t;
    FakeSink s;
    Conn a;
    a.data = response("200 OK", FW.substr(0, 800), "", FW.size());
    a.closeAtEnd = false;
    Conn b;
    b.data = partial(800);
    t.script.push_back(a);
    t.script.push_back(b);
    FirmwareDownloader d(t, s);
    d.start(cfg(), 0);
    run(d);
    TEST_ASSERT_EQUAL(DownloadState::Done, d.state());
    TEST_ASSERT_TRUE(s.got == FW);
    TEST_ASSERT_EQUAL(2, t.connects);
}

void test_macet_tanpa_henti_menyerah()
{
    FakeTransport t;
    FakeSink s;
    for (int i = 0; i < 5; ++i)
    {
        Conn a;
        a.data = "";
        a.closeAtEnd = false;
        t.script.push_back(a);
    }
    FirmwareDownloader d(t, s);
    d.start(cfg(), 0);
    run(d);
    TEST_ASSERT_EQUAL(DownloadFailure::TooManyRetries, d.failure());
    TEST_ASSERT_EQUAL(DownloadFailure::Stalled, d.lastTransient());
}

void test_kemajuan_mengembalikan_hitungan_gagal_ke_nol()
{
    // 4 putus berturut-turut (batas 3), tetapi tiap putus ada kemajuan: harus tetap selesai.
    FakeTransport t;
    FakeSink s;
    size_t from = 0;
    for (int i = 0; i < 4; ++i)
    {
        const size_t upto = from + 600;
        Conn c;
        std::string body = FW.substr(from, 600);
        char extra[96] = "";
        if (from > 0)
        {
            snprintf(extra, sizeof(extra), "Content-Range: bytes %zu-%zu/%zu\r\n", from,
                     FW.size() - 1, FW.size());
        }
        c.data = response(from == 0 ? "200 OK" : "206 Partial Content", body, extra,
                          static_cast<long>(FW.size() - from));
        t.script.push_back(c);
        from = upto;
    }
    Conn last;
    last.data = partial(from);
    t.script.push_back(last);
    FirmwareDownloader d(t, s);
    d.start(cfg(), 0);
    run(d);
    TEST_ASSERT_EQUAL(DownloadState::Done, d.state());
    TEST_ASSERT_TRUE(s.got == FW);
}

void test_data_berlebih_dari_content_length_ditolak()
{
    FakeTransport t;
    FakeSink s;
    Conn c;
    c.data = response("200 OK", FW + "SAMPAH", "", FW.size()); // janji = ukuran manifest
    t.script.push_back(c);
    FirmwareDownloader d(t, s);
    d.start(cfg(), 0);
    run(d);
    TEST_ASSERT_EQUAL(DownloadState::Failed, d.state());
    TEST_ASSERT_EQUAL(DownloadFailure::TooMuchData, d.failure());
}

void test_header_rusak_tidak_diulang()
{
    FakeTransport t;
    FakeSink s;
    Conn c;
    c.data = "BUKAN HTTP\r\n\r\n";
    t.script.push_back(c);
    FirmwareDownloader d(t, s);
    d.start(cfg(), 0);
    run(d);
    TEST_ASSERT_EQUAL(DownloadFailure::BadResponse, d.failure());
    TEST_ASSERT_EQUAL(1, t.connects);
}

void test_chunked_ditolak_sebagai_respons_buruk()
{
    FakeTransport t;
    FakeSink s;
    Conn c;
    c.data = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nabcde\r\n0\r\n\r\n";
    t.script.push_back(c);
    FirmwareDownloader d(t, s);
    d.start(cfg(), 0);
    run(d);
    TEST_ASSERT_EQUAL(DownloadFailure::BadResponse, d.failure());
}

void test_konfigurasi_salah()
{
    FakeTransport t;
    FakeSink s;
    FirmwareDownloader d(t, s);
    DownloadConfig c = cfg();
    c.expectedSize = 0;
    d.start(c, 0);
    TEST_ASSERT_EQUAL(DownloadState::Failed, d.state());
    TEST_ASSERT_EQUAL(DownloadFailure::BadConfig, d.failure());
    DownloadConfig h = cfg();
    h.host = nullptr;
    d.start(h, 0);
    TEST_ASSERT_EQUAL(DownloadFailure::BadConfig, d.failure());
    DownloadConfig p = cfg();
    p.path = "tanpa-slash";
    d.start(p, 0);
    run(d);
    TEST_ASSERT_EQUAL(DownloadFailure::BadConfig, d.failure());
    TEST_ASSERT_EQUAL(0, t.connects);
}

void test_gagal_kirim_permintaan_diulang()
{
    FakeTransport t;
    FakeSink s;
    t.writeFails = true;
    Conn c;
    c.data = response("200 OK", FW);
    t.script.push_back(c);
    t.script.push_back(c);
    t.script.push_back(c);
    FirmwareDownloader d(t, s);
    d.start(cfg(), 0);
    run(d);
    TEST_ASSERT_EQUAL(DownloadFailure::TooManyRetries, d.failure());
    TEST_ASSERT_EQUAL(DownloadFailure::RequestFailed, d.lastTransient());
}

void test_abort_menutup_koneksi()
{
    FakeTransport t;
    FakeSink s;
    Conn c;
    c.data = response("200 OK", FW.substr(0, 100), "", FW.size());
    c.closeAtEnd = false;
    t.script.push_back(c);
    FirmwareDownloader d(t, s);
    d.start(cfg(), 0);
    d.step(0);
    d.step(100);
    d.abort();
    TEST_ASSERT_EQUAL(DownloadState::Failed, d.state());
    TEST_ASSERT_EQUAL(DownloadFailure::Aborted, d.failure());
    const int stops = t.stops;
    d.step(200);
    TEST_ASSERT_EQUAL(stops, t.stops); // tidak melakukan apa pun lagi
}

void test_setelah_selesai_step_tidak_berbuat_apa_apa()
{
    FakeTransport t;
    FakeSink s;
    Conn c;
    c.data = response("200 OK", FW);
    t.script.push_back(c);
    FirmwareDownloader d(t, s);
    d.start(cfg(), 0);
    run(d);
    const size_t n = s.got.size();
    d.step(99999);
    TEST_ASSERT_EQUAL(DownloadState::Done, d.step(100000));
    TEST_ASSERT_EQUAL(n, s.got.size());
    TEST_ASSERT_EQUAL(1, t.connects);
}

void test_millis_berputar_tidak_merusak_batas_waktu()
{
    FakeTransport t;
    FakeSink s;
    Conn a;
    a.data = response("200 OK", FW.substr(0, 300), "", FW.size());
    a.closeAtEnd = false;
    Conn b;
    b.data = partial(300);
    t.script.push_back(a);
    t.script.push_back(b);
    FirmwareDownloader d(t, s);
    const uint32_t start = 0xFFFFFF00u; // sebentar lagi berputar ke 0
    d.start(cfg(), start);
    run(d, start);
    TEST_ASSERT_EQUAL(DownloadState::Done, d.state());
    TEST_ASSERT_TRUE(s.got == FW);
}

void test_ukuran_tidak_diketahui_diterima_selama_di_bawah_batas()
{
    FakeTransport t;
    FakeSink s;
    Conn c;
    c.data = response("200 OK", FW);
    t.script.push_back(c);
    FirmwareDownloader d(t, s);
    DownloadConfig c2 = cfg();
    c2.expectedSize = 0;
    c2.maxSize = static_cast<uint32_t>(FW.size()); // tepat di batas: diterima
    d.start(c2, 0);
    run(d);
    TEST_ASSERT_EQUAL(DownloadState::Done, d.state());
    TEST_ASSERT_TRUE(s.got == FW);
}

void test_ukuran_tidak_diketahui_ditolak_bila_melebihi_batas()
{
    FakeTransport t;
    FakeSink s;
    Conn c;
    c.data = response("200 OK", FW);
    t.script.push_back(c);
    FirmwareDownloader d(t, s);
    DownloadConfig c2 = cfg();
    c2.expectedSize = 0;
    c2.maxSize = static_cast<uint32_t>(FW.size() - 1);
    d.start(c2, 0);
    run(d);
    TEST_ASSERT_EQUAL(DownloadFailure::SizeMismatch, d.failure());
    TEST_ASSERT_EQUAL(0, s.got.size());
}

void test_tanpa_ukuran_dan_tanpa_batas_adalah_konfigurasi_salah()
{
    FakeTransport t;
    FakeSink s;
    FirmwareDownloader d(t, s);
    DownloadConfig c2 = cfg();
    c2.expectedSize = 0;
    c2.maxSize = 0;
    d.start(c2, 0);
    TEST_ASSERT_EQUAL(DownloadFailure::BadConfig, d.failure());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_unduh_penuh_tanpa_range);
    RUN_TEST(test_potongan_kecil_hasilnya_sama);
    RUN_TEST(test_koneksi_putus_lalu_lanjut_dengan_range);
    RUN_TEST(test_jeda_antar_percobaan_dihormati);
    RUN_TEST(test_gagal_sambung_berulang_menyerah);
    RUN_TEST(test_server_5xx_diulang_lalu_berhasil);
    RUN_TEST(test_404_dan_301_tidak_diulang);
    RUN_TEST(test_server_mengabaikan_range_dilaporkan);
    RUN_TEST(test_ukuran_beda_dari_manifest_ditolak_sebelum_menulis);
    RUN_TEST(test_penulis_menolak_menghentikan_unduhan);
    RUN_TEST(test_macet_diulang_setelah_batas_waktu);
    RUN_TEST(test_macet_tanpa_henti_menyerah);
    RUN_TEST(test_kemajuan_mengembalikan_hitungan_gagal_ke_nol);
    RUN_TEST(test_data_berlebih_dari_content_length_ditolak);
    RUN_TEST(test_header_rusak_tidak_diulang);
    RUN_TEST(test_chunked_ditolak_sebagai_respons_buruk);
    RUN_TEST(test_konfigurasi_salah);
    RUN_TEST(test_gagal_kirim_permintaan_diulang);
    RUN_TEST(test_abort_menutup_koneksi);
    RUN_TEST(test_setelah_selesai_step_tidak_berbuat_apa_apa);
    RUN_TEST(test_millis_berputar_tidak_merusak_batas_waktu);
    RUN_TEST(test_ukuran_tidak_diketahui_diterima_selama_di_bawah_batas);
    RUN_TEST(test_ukuran_tidak_diketahui_ditolak_bila_melebihi_batas);
    RUN_TEST(test_tanpa_ukuran_dan_tanpa_batas_adalah_konfigurasi_salah);
    return UNITY_END();
}