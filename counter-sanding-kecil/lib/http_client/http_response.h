#pragma once
#include <stddef.h>
#include <stdint.h>

// Bagian HTTP/1.1 yang murni logika: membuat permintaan GET dan membaca header
// respons. Tidak menyentuh jaringan, jadi bisa diuji di komputer.
namespace http
{

    constexpr size_t MAX_HEADER_BYTES = 1024; // seluruh header, termasuk baris status
    constexpr size_t MAX_LINE_BYTES = 192;    // satu baris header

    // Menulis permintaan GET ke `out`. rangeStart = 0 berarti unduh dari awal
    // (tanpa header Range); > 0 berarti lanjut dari byte itu.
    // Mengembalikan panjang teks (tanpa NUL), atau 0 bila host/path tidak aman
    // (spasi, CR, LF, kontrol) atau tidak muat di `cap`.
    size_t buildGetRequest(char *out, size_t cap, const char *host, const char *path,
                           uint32_t rangeStart);

    enum class ParseState
    {
        NeedMore,
        Done,
        Error
    };

    enum class ParseError
    {
        None,
        BadStatusLine,
        HeaderTooLarge,
        LineTooLong,
        BadContentLength,
        ConflictingLength,
        BadContentRange,
        TransferEncodingNotSupported, // termasuk chunked
        ContentEncodingNotSupported,  // gzip dan sejenisnya
    };

    const char *parseErrorText(ParseError error);

    struct ResponseInfo
    {
        int status = 0;
        bool hasContentLength = false;
        uint32_t contentLength = 0;
        bool hasContentRange = false;
        uint32_t rangeStart = 0;
        uint32_t rangeEnd = 0;
        uint32_t rangeTotal = 0;
    };

    // Pembaca header bertahap: boleh disuapi sepotong-sepotong, bahkan satu byte.
    class HeaderParser
    {
    public:
        HeaderParser() { reset(); }
        void reset();

        // Membaca dari `data` sampai header selesai atau galat. `consumed` diisi
        // jumlah byte yang dipakai; byte sisanya adalah awal isi (body).
        ParseState feed(const uint8_t *data, size_t len, size_t &consumed);

        ParseState state() const { return state_; }
        ParseError error() const { return error_; }
        const ResponseInfo &info() const { return info_; }

    private:
        void processLine();
        void fail(ParseError e);
        void parseStatusLine();
        void parseHeaderLine();

        ParseState state_;
        ParseError error_;
        ResponseInfo info_;
        char line_[MAX_LINE_BYTES + 1];
        size_t lineLen_;
        size_t totalBytes_;
        bool gotStatus_;
    };

    enum class BodyCheck
    {
        Ok,
        BadStatus,     // bukan 200/206
        RangeIgnored,  // diminta lanjut, server membalas 200: harus mulai dari awal
        RangeMismatch, // 206 tetapi titik awal atau panjangnya tidak sesuai
        NoLength,      // tidak ada Content-Length: tidak bisa diverifikasi
    };

    struct BodyPlan
    {
        BodyCheck check;
        uint32_t bodyLength; // jumlah byte isi yang harus datang; berarti jika Ok
        bool ok() const { return check == BodyCheck::Ok; }
    };

    // Memutuskan apakah respons ini boleh dipakai untuk permintaan yang mulai
    // dari `requestedStart`.
    BodyPlan planBody(const ResponseInfo &info, uint32_t requestedStart);

} // namespace http