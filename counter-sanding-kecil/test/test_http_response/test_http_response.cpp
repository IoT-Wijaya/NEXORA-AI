#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "http_response.h"

using namespace http;

void setUp() {}
void tearDown() {}

static ParseState feedAll(HeaderParser &p, const char *s, size_t *consumedOut = nullptr)
{
    size_t used = 0;
    const ParseState st = p.feed(reinterpret_cast<const uint8_t *>(s), strlen(s), used);
    if (consumedOut)
        *consumedOut = used;
    return st;
}

// ---------- buildGetRequest ----------

void test_request_tanpa_range()
{
    char buf[256];
    const size_t n = buildGetRequest(buf, sizeof(buf), "fw.example.com", "/fw/1.bin", 0);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_EQUAL(strlen(buf), n);
    TEST_ASSERT_NOT_NULL(strstr(buf, "GET /fw/1.bin HTTP/1.1\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Host: fw.example.com\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Connection: close\r\n"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Accept-Encoding: identity\r\n"));
    TEST_ASSERT_NULL(strstr(buf, "Range:"));
    TEST_ASSERT_EQUAL_STRING("\r\n\r\n", buf + n - 4);
}

void test_request_dengan_range()
{
    char buf[256];
    TEST_ASSERT_GREATER_THAN(0, buildGetRequest(buf, sizeof(buf), "h", "/a", 123456));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Range: bytes=123456-\r\n"));
}

void test_request_menolak_penyuntikan_header()
{
    char buf[256];
    TEST_ASSERT_EQUAL(0, buildGetRequest(buf, sizeof(buf), "h\r\nX: 1", "/a", 0));
    TEST_ASSERT_EQUAL(0, buildGetRequest(buf, sizeof(buf), "h", "/a\r\nX: 1", 0));
    TEST_ASSERT_EQUAL(0, buildGetRequest(buf, sizeof(buf), "h", "/a b", 0));
    TEST_ASSERT_EQUAL(0, buildGetRequest(buf, sizeof(buf), "h h", "/a", 0));
}

void test_request_menolak_masukan_tidak_valid()
{
    char buf[256];
    TEST_ASSERT_EQUAL(0, buildGetRequest(buf, sizeof(buf), "", "/a", 0));
    TEST_ASSERT_EQUAL(0, buildGetRequest(buf, sizeof(buf), nullptr, "/a", 0));
    TEST_ASSERT_EQUAL(0, buildGetRequest(buf, sizeof(buf), "h", "tanpa-slash", 0));
    TEST_ASSERT_EQUAL(0, buildGetRequest(nullptr, 10, "h", "/a", 0));
    TEST_ASSERT_EQUAL(0, buildGetRequest(buf, 0, "h", "/a", 0));
}

void test_request_tidak_muat_ditolak_bukan_terpotong()
{
    char buf[40];
    TEST_ASSERT_EQUAL(0, buildGetRequest(buf, sizeof(buf), "fw.example.com", "/fw/1.bin", 0));
}

// ---------- HeaderParser ----------

void test_respons_200_normal()
{
    HeaderParser p;
    size_t used;
    const char *r = "HTTP/1.1 200 OK\r\nContent-Length: 340877\r\nServer: nginx\r\n\r\nISI";
    TEST_ASSERT_EQUAL(ParseState::Done, feedAll(p, r, &used));
    TEST_ASSERT_EQUAL(200, p.info().status);
    TEST_ASSERT_TRUE(p.info().hasContentLength);
    TEST_ASSERT_EQUAL_UINT32(340877, p.info().contentLength);
    TEST_ASSERT_EQUAL(strlen(r) - 3, used); // "ISI" belum dipakai: itu body
}

void test_hasil_sama_jika_disuap_satu_byte_satu_byte()
{
    HeaderParser p;
    const char *r = "HTTP/1.1 206 Partial Content\r\nContent-Length: 100\r\nContent-Range: bytes 900-999/1000\r\n\r\n";
    ParseState st = ParseState::NeedMore;
    for (size_t i = 0; i < strlen(r); ++i)
    {
        size_t used;
        st = p.feed(reinterpret_cast<const uint8_t *>(r) + i, 1, used);
        TEST_ASSERT_EQUAL(1, used);
        if (i + 1 < strlen(r))
            TEST_ASSERT_EQUAL(ParseState::NeedMore, st);
    }
    TEST_ASSERT_EQUAL(ParseState::Done, st);
    TEST_ASSERT_EQUAL(206, p.info().status);
    TEST_ASSERT_EQUAL_UINT32(900, p.info().rangeStart);
    TEST_ASSERT_EQUAL_UINT32(999, p.info().rangeEnd);
    TEST_ASSERT_EQUAL_UINT32(1000, p.info().rangeTotal);
}

void test_nama_header_tidak_peka_huruf_besar()
{
    HeaderParser p;
    TEST_ASSERT_EQUAL(ParseState::Done, feedAll(p, "HTTP/1.1 200 OK\r\ncOnTeNt-LeNgTh:   42  \r\n\r\n"));
    TEST_ASSERT_EQUAL_UINT32(42, p.info().contentLength);
}

void test_lf_saja_tanpa_cr_diterima()
{
    HeaderParser p;
    TEST_ASSERT_EQUAL(ParseState::Done, feedAll(p, "HTTP/1.1 200 OK\nContent-Length: 5\n\n"));
    TEST_ASSERT_EQUAL_UINT32(5, p.info().contentLength);
}

void test_status_404_dibaca_bukan_galat_parser()
{
    HeaderParser p;
    TEST_ASSERT_EQUAL(ParseState::Done, feedAll(p, "HTTP/1.1 404 Not Found\r\n\r\n"));
    TEST_ASSERT_EQUAL(404, p.info().status);
}

void test_baris_status_rusak()
{
    const char *buruk[] = {"HTTP/2 200 OK\r\n\r\n", "200 OK\r\n\r\n", "HTTP/1.1 20 OK\r\n\r\n",
                           "HTTP/1.1 2000 OK\r\n\r\n", "HTTP/1.1 abc OK\r\n\r\n", "\r\n"};
    for (const char *b : buruk)
    {
        HeaderParser p;
        TEST_ASSERT_EQUAL_MESSAGE(ParseState::Error, feedAll(p, b), b);
        TEST_ASSERT_EQUAL(ParseError::BadStatusLine, p.error());
    }
}

void test_content_length_rusak()
{
    const char *buruk[] = {"-5", "abc", "", "12abc", "99999999999", "1.5", "+5"};
    for (const char *v : buruk)
    {
        HeaderParser p;
        char r[128];
        snprintf(r, sizeof(r), "HTTP/1.1 200 OK\r\nContent-Length: %s\r\n\r\n", v);
        TEST_ASSERT_EQUAL_MESSAGE(ParseState::Error, feedAll(p, r), v);
        TEST_ASSERT_EQUAL(ParseError::BadContentLength, p.error());
    }
}

void test_content_length_batas_uint32()
{
    HeaderParser p;
    TEST_ASSERT_EQUAL(ParseState::Done, feedAll(p, "HTTP/1.1 200 OK\r\nContent-Length: 4294967295\r\n\r\n"));
    TEST_ASSERT_EQUAL_UINT32(4294967295u, p.info().contentLength);
    HeaderParser q;
    TEST_ASSERT_EQUAL(ParseState::Error, feedAll(q, "HTTP/1.1 200 OK\r\nContent-Length: 4294967296\r\n\r\n"));
}

void test_content_length_ganda_beda_ditolak_sama_diterima()
{
    HeaderParser a;
    TEST_ASSERT_EQUAL(ParseState::Error, feedAll(a, "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Length: 6\r\n\r\n"));
    TEST_ASSERT_EQUAL(ParseError::ConflictingLength, a.error());
    HeaderParser b;
    TEST_ASSERT_EQUAL(ParseState::Done, feedAll(b, "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Length: 5\r\n\r\n"));
}

void test_chunked_dan_content_encoding_ditolak()
{
    HeaderParser a;
    TEST_ASSERT_EQUAL(ParseState::Error, feedAll(a, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"));
    TEST_ASSERT_EQUAL(ParseError::TransferEncodingNotSupported, a.error());
    HeaderParser b;
    TEST_ASSERT_EQUAL(ParseState::Error, feedAll(b, "HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\n\r\n"));
    TEST_ASSERT_EQUAL(ParseError::ContentEncodingNotSupported, b.error());
    HeaderParser c;
    TEST_ASSERT_EQUAL(ParseState::Done, feedAll(c, "HTTP/1.1 200 OK\r\nContent-Encoding: identity\r\nContent-Length: 3\r\n\r\n"));
}

void test_content_range_rusak()
{
    const char *buruk[] = {"bytes 5-3/10", "bytes 0-9/9", "bytes 0-9/*", "items 0-9/10",
                           "bytes 0-9", "bytes -9/10", "bytes 0-/10", "bytes 0-9/10x"};
    for (const char *v : buruk)
    {
        HeaderParser p;
        char r[160];
        snprintf(r, sizeof(r), "HTTP/1.1 206 Partial Content\r\nContent-Range: %s\r\n\r\n", v);
        TEST_ASSERT_EQUAL_MESSAGE(ParseState::Error, feedAll(p, r), v);
        TEST_ASSERT_EQUAL(ParseError::BadContentRange, p.error());
    }
}

void test_header_terlalu_besar_ditolak()
{
    HeaderParser p;
    feedAll(p, "HTTP/1.1 200 OK\r\n");
    ParseState st = ParseState::NeedMore;
    for (int i = 0; i < 100 && st == ParseState::NeedMore; ++i)
    {
        st = feedAll(p, "X-Isi: 0123456789012345678901234567890123456789\r\n");
    }
    TEST_ASSERT_EQUAL(ParseState::Error, st);
    TEST_ASSERT_EQUAL(ParseError::HeaderTooLarge, p.error());
}

void test_satu_baris_terlalu_panjang_ditolak()
{
    HeaderParser p;
    char r[400] = "HTTP/1.1 200 OK\r\nX: ";
    memset(r + strlen(r), 'a', 250);
    TEST_ASSERT_EQUAL(ParseState::Error, feedAll(p, r));
    TEST_ASSERT_EQUAL(ParseError::LineTooLong, p.error());
}

void test_setelah_selesai_atau_galat_tidak_memakai_byte_lagi()
{
    HeaderParser p;
    feedAll(p, "HTTP/1.1 200 OK\r\n\r\n");
    size_t used = 99;
    p.feed(reinterpret_cast<const uint8_t *>("BODY"), 4, used);
    TEST_ASSERT_EQUAL(0, used);
    TEST_ASSERT_EQUAL(ParseState::Done, p.state());
}

void test_reset_memakai_ulang_parser()
{
    HeaderParser p;
    feedAll(p, "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n");
    p.reset();
    TEST_ASSERT_EQUAL(ParseState::NeedMore, p.state());
    TEST_ASSERT_FALSE(p.info().hasContentLength);
    TEST_ASSERT_EQUAL(ParseState::Done, feedAll(p, "HTTP/1.1 404 X\r\n\r\n"));
    TEST_ASSERT_EQUAL(404, p.info().status);
}

// ---------- planBody ----------

static ResponseInfo info(int status, bool hasLen, uint32_t len)
{
    ResponseInfo i;
    i.status = status;
    i.hasContentLength = hasLen;
    i.contentLength = len;
    return i;
}
static ResponseInfo withRange(ResponseInfo i, uint32_t s, uint32_t e, uint32_t t)
{
    i.hasContentRange = true;
    i.rangeStart = s;
    i.rangeEnd = e;
    i.rangeTotal = t;
    return i;
}

void test_plan_unduh_penuh()
{
    const auto p = planBody(info(200, true, 1000), 0);
    TEST_ASSERT_TRUE(p.ok());
    TEST_ASSERT_EQUAL_UINT32(1000, p.bodyLength);
}

void test_plan_status_selain_200_206_ditolak()
{
    const int kode[] = {301, 403, 404, 416, 500, 503};
    for (int s : kode)
    {
        TEST_ASSERT_EQUAL(BodyCheck::BadStatus, planBody(info(s, true, 10), 0).check);
    }
}

void test_plan_tanpa_atau_nol_content_length_ditolak()
{
    TEST_ASSERT_EQUAL(BodyCheck::NoLength, planBody(info(200, false, 0), 0).check);
    TEST_ASSERT_EQUAL(BodyCheck::NoLength, planBody(info(200, true, 0), 0).check);
}

void test_plan_lanjut_unduh_206_cocok()
{
    const auto p = planBody(withRange(info(206, true, 100), 900, 999, 1000), 900);
    TEST_ASSERT_TRUE(p.ok());
    TEST_ASSERT_EQUAL_UINT32(100, p.bodyLength);
}

void test_plan_lanjut_unduh_tapi_server_membalas_200()
{
    TEST_ASSERT_EQUAL(BodyCheck::RangeIgnored, planBody(info(200, true, 1000), 900).check);
}

void test_plan_lanjut_unduh_titik_awal_salah()
{
    TEST_ASSERT_EQUAL(BodyCheck::RangeMismatch,
                      planBody(withRange(info(206, true, 100), 800, 899, 1000), 900).check);
}

void test_plan_lanjut_unduh_panjang_tidak_cocok_dengan_range()
{
    TEST_ASSERT_EQUAL(BodyCheck::RangeMismatch,
                      planBody(withRange(info(206, true, 50), 900, 999, 1000), 900).check);
}

void test_plan_lanjut_unduh_206_tanpa_content_range()
{
    TEST_ASSERT_EQUAL(BodyCheck::RangeMismatch, planBody(info(206, true, 100), 900).check);
}

void test_plan_awal_dengan_206_harus_konsisten()
{
    TEST_ASSERT_TRUE(planBody(withRange(info(206, true, 1000), 0, 999, 1000), 0).ok());
    TEST_ASSERT_EQUAL(BodyCheck::RangeMismatch, planBody(info(206, true, 1000), 0).check);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_request_tanpa_range);
    RUN_TEST(test_request_dengan_range);
    RUN_TEST(test_request_menolak_penyuntikan_header);
    RUN_TEST(test_request_menolak_masukan_tidak_valid);
    RUN_TEST(test_request_tidak_muat_ditolak_bukan_terpotong);
    RUN_TEST(test_respons_200_normal);
    RUN_TEST(test_hasil_sama_jika_disuap_satu_byte_satu_byte);
    RUN_TEST(test_nama_header_tidak_peka_huruf_besar);
    RUN_TEST(test_lf_saja_tanpa_cr_diterima);
    RUN_TEST(test_status_404_dibaca_bukan_galat_parser);
    RUN_TEST(test_baris_status_rusak);
    RUN_TEST(test_content_length_rusak);
    RUN_TEST(test_content_length_batas_uint32);
    RUN_TEST(test_content_length_ganda_beda_ditolak_sama_diterima);
    RUN_TEST(test_chunked_dan_content_encoding_ditolak);
    RUN_TEST(test_content_range_rusak);
    RUN_TEST(test_header_terlalu_besar_ditolak);
    RUN_TEST(test_satu_baris_terlalu_panjang_ditolak);
    RUN_TEST(test_setelah_selesai_atau_galat_tidak_memakai_byte_lagi);
    RUN_TEST(test_reset_memakai_ulang_parser);
    RUN_TEST(test_plan_unduh_penuh);
    RUN_TEST(test_plan_status_selain_200_206_ditolak);
    RUN_TEST(test_plan_tanpa_atau_nol_content_length_ditolak);
    RUN_TEST(test_plan_lanjut_unduh_206_cocok);
    RUN_TEST(test_plan_lanjut_unduh_tapi_server_membalas_200);
    RUN_TEST(test_plan_lanjut_unduh_titik_awal_salah);
    RUN_TEST(test_plan_lanjut_unduh_panjang_tidak_cocok_dengan_range);
    RUN_TEST(test_plan_lanjut_unduh_206_tanpa_content_range);
    RUN_TEST(test_plan_awal_dengan_206_harus_konsisten);
    return UNITY_END();
}