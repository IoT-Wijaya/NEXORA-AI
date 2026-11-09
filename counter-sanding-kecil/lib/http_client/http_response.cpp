#include "http_response.h"

#include <stdio.h>
#include <string.h>

namespace http
{

    namespace
    {

        bool isSafeToken(const char *s)
        {
            if (s == nullptr || *s == '\0')
                return false;
            for (; *s; ++s)
            {
                const unsigned char c = static_cast<unsigned char>(*s);
                if (c <= 0x20 || c == 0x7F)
                    return false; // spasi, CR, LF, kontrol
            }
            return true;
        }

        char lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; }

        bool equalsIgnoreCase(const char *a, size_t aLen, const char *literal)
        {
            if (aLen != strlen(literal))
                return false;
            for (size_t i = 0; i < aLen; ++i)
            {
                if (lower(a[i]) != literal[i])
                    return false;
            }
            return true;
        }

        // Bilangan desimal tanpa tanda, tanpa luapan uint32. Mengembalikan jumlah
        // karakter terbaca (0 = gagal).
        size_t parseUint(const char *s, size_t len, uint32_t &out)
        {
            uint64_t v = 0;
            size_t i = 0;
            for (; i < len && s[i] >= '0' && s[i] <= '9'; ++i)
            {
                v = v * 10 + static_cast<uint64_t>(s[i] - '0');
                if (v > 0xFFFFFFFFull)
                    return 0;
            }
            if (i == 0)
                return 0;
            out = static_cast<uint32_t>(v);
            return i;
        }

    } // namespace

    size_t buildGetRequest(char *out, size_t cap, const char *host, const char *path,
                           uint32_t rangeStart)
    {
        if (out == nullptr || cap == 0 || !isSafeToken(host) || !isSafeToken(path))
            return 0;
        if (path[0] != '/')
            return 0;

        int n;
        if (rangeStart == 0)
        {
            n = snprintf(out, cap,
                         "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: sanding-kecil\r\n"
                         "Accept-Encoding: identity\r\nConnection: close\r\n\r\n",
                         path, host);
        }
        else
        {
            n = snprintf(out, cap,
                         "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: sanding-kecil\r\n"
                         "Accept-Encoding: identity\r\nRange: bytes=%lu-\r\n"
                         "Connection: close\r\n\r\n",
                         path, host, static_cast<unsigned long>(rangeStart));
        }
        if (n < 0 || static_cast<size_t>(n) >= cap)
            return 0;
        return static_cast<size_t>(n);
    }

    const char *parseErrorText(ParseError error)
    {
        switch (error)
        {
        case ParseError::None:
            return "ok";
        case ParseError::BadStatusLine:
            return "baris status rusak";
        case ParseError::HeaderTooLarge:
            return "header terlalu besar";
        case ParseError::LineTooLong:
            return "baris header terlalu panjang";
        case ParseError::BadContentLength:
            return "Content-Length rusak";
        case ParseError::ConflictingLength:
            return "Content-Length bertentangan";
        case ParseError::BadContentRange:
            return "Content-Range rusak";
        case ParseError::TransferEncodingNotSupported:
            return "Transfer-Encoding tidak didukung";
        case ParseError::ContentEncodingNotSupported:
            return "Content-Encoding tidak didukung";
        }
        return "tidak diketahui";
    }

    void HeaderParser::reset()
    {
        state_ = ParseState::NeedMore;
        error_ = ParseError::None;
        info_ = ResponseInfo();
        lineLen_ = 0;
        totalBytes_ = 0;
        gotStatus_ = false;
    }

    void HeaderParser::fail(ParseError e)
    {
        state_ = ParseState::Error;
        error_ = e;
    }

    ParseState HeaderParser::feed(const uint8_t *data, size_t len, size_t &consumed)
    {
        consumed = 0;
        if (data == nullptr)
            return state_;
        while (state_ == ParseState::NeedMore && consumed < len)
        {
            const char c = static_cast<char>(data[consumed++]);
            if (++totalBytes_ > MAX_HEADER_BYTES)
            {
                fail(ParseError::HeaderTooLarge);
                break;
            }
            if (c == '\r')
                continue;
            if (c == '\n')
            {
                line_[lineLen_] = '\0';
                processLine();
                lineLen_ = 0;
                continue;
            }
            if (lineLen_ >= MAX_LINE_BYTES)
            {
                fail(ParseError::LineTooLong);
                break;
            }
            line_[lineLen_++] = c;
        }
        return state_;
    }

    void HeaderParser::processLine()
    {
        if (!gotStatus_)
        {
            parseStatusLine();
            return;
        }
        if (lineLen_ == 0)
        { // baris kosong: header selesai
            state_ = ParseState::Done;
            return;
        }
        parseHeaderLine();
    }

    void HeaderParser::parseStatusLine()
    {
        // "HTTP/1.x SSS alasan"
        if (strncmp(line_, "HTTP/1.", 7) != 0 || line_[7] < '0' || line_[7] > '9' || line_[8] != ' ' ||
            line_[9] < '0' || line_[9] > '9' || line_[10] < '0' || line_[10] > '9' ||
            line_[11] < '0' || line_[11] > '9' || (line_[12] != '\0' && line_[12] != ' '))
        {
            fail(ParseError::BadStatusLine);
            return;
        }
        info_.status = (line_[9] - '0') * 100 + (line_[10] - '0') * 10 + (line_[11] - '0');
        gotStatus_ = true;
    }

    void HeaderParser::parseHeaderLine()
    {
        const char *colon = static_cast<const char *>(memchr(line_, ':', lineLen_));
        if (colon == nullptr)
            return; // baris tanpa titik dua diabaikan

        const size_t nameLen = static_cast<size_t>(colon - line_);
        const char *value = colon + 1;
        size_t valueLen = lineLen_ - nameLen - 1;
        while (valueLen > 0 && (*value == ' ' || *value == '\t'))
        {
            ++value;
            --valueLen;
        }
        while (valueLen > 0 && (value[valueLen - 1] == ' ' || value[valueLen - 1] == '\t'))
            --valueLen;

        if (equalsIgnoreCase(line_, nameLen, "content-length"))
        {
            uint32_t v = 0;
            const size_t n = parseUint(value, valueLen, v);
            if (n == 0 || n != valueLen)
            { // kosong atau ada sisa karakter
                fail(ParseError::BadContentLength);
                return;
            }
            if (info_.hasContentLength && info_.contentLength != v)
            {
                fail(ParseError::ConflictingLength);
                return;
            }
            info_.hasContentLength = true;
            info_.contentLength = v;
        }
        else if (equalsIgnoreCase(line_, nameLen, "content-range"))
        {
            // "bytes S-E/T"
            if (valueLen < 8 || strncmp(value, "bytes ", 6) != 0)
            {
                fail(ParseError::BadContentRange);
                return;
            }
            const char *p = value + 6;
            size_t left = valueLen - 6;
            uint32_t s = 0, e = 0, t = 0;
            size_t n = parseUint(p, left, s);
            if (n == 0 || n >= left || p[n] != '-')
            {
                fail(ParseError::BadContentRange);
                return;
            }
            p += n + 1;
            left -= n + 1;
            n = parseUint(p, left, e);
            if (n == 0 || n >= left || p[n] != '/')
            {
                fail(ParseError::BadContentRange);
                return;
            }
            p += n + 1;
            left -= n + 1;
            n = parseUint(p, left, t);
            if (n == 0 || n != left || e < s || t <= e)
            {
                fail(ParseError::BadContentRange);
                return;
            }
            info_.hasContentRange = true;
            info_.rangeStart = s;
            info_.rangeEnd = e;
            info_.rangeTotal = t;
        }
        else if (equalsIgnoreCase(line_, nameLen, "transfer-encoding"))
        {
            fail(ParseError::TransferEncodingNotSupported);
        }
        else if (equalsIgnoreCase(line_, nameLen, "content-encoding"))
        {
            if (!equalsIgnoreCase(value, valueLen, "identity"))
            {
                fail(ParseError::ContentEncodingNotSupported);
            }
        }
    }

    BodyPlan planBody(const ResponseInfo &info, uint32_t requestedStart)
    {
        if (info.status != 200 && info.status != 206)
            return BodyPlan{BodyCheck::BadStatus, 0};
        if (!info.hasContentLength)
            return BodyPlan{BodyCheck::NoLength, 0};
        if (info.contentLength == 0)
            return BodyPlan{BodyCheck::NoLength, 0};

        if (requestedStart == 0)
        {
            if (info.status == 206)
            {
                if (!info.hasContentRange || info.rangeStart != 0 ||
                    info.rangeEnd - info.rangeStart + 1 != info.contentLength)
                {
                    return BodyPlan{BodyCheck::RangeMismatch, 0};
                }
            }
            return BodyPlan{BodyCheck::Ok, info.contentLength};
        }

        if (info.status == 200)
            return BodyPlan{BodyCheck::RangeIgnored, 0};
        if (!info.hasContentRange || info.rangeStart != requestedStart ||
            info.rangeEnd - info.rangeStart + 1 != info.contentLength)
        {
            return BodyPlan{BodyCheck::RangeMismatch, 0};
        }
        return BodyPlan{BodyCheck::Ok, info.contentLength};
    }

} // namespace http