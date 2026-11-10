#include "firmware_downloader.h"

namespace
{

    constexpr size_t READ_CHUNK = 512;
    constexpr int MAX_READS_PER_STEP = 4; // menjaga loop() tetap responsif

    bool reached(uint32_t now, uint32_t target)
    {
        return static_cast<int32_t>(now - target) >= 0; // aman saat millis() berputar
    }

} // namespace

const char *downloadFailureText(DownloadFailure f)
{
    switch (f)
    {
    case DownloadFailure::None:
        return "ok";
    case DownloadFailure::BadConfig:
        return "konfigurasi unduhan salah";
    case DownloadFailure::ConnectFailed:
        return "gagal tersambung";
    case DownloadFailure::RequestFailed:
        return "gagal mengirim permintaan";
    case DownloadFailure::Dropped:
        return "koneksi putus";
    case DownloadFailure::Stalled:
        return "tidak ada data (macet)";
    case DownloadFailure::ServerError:
        return "server galat (5xx)";
    case DownloadFailure::TooManyRetries:
        return "terlalu banyak percobaan gagal";
    case DownloadFailure::BadResponse:
        return "respons server tidak valid";
    case DownloadFailure::BadStatus:
        return "status HTTP ditolak";
    case DownloadFailure::SizeMismatch:
        return "ukuran tidak cocok dengan manifest";
    case DownloadFailure::RangeIgnored:
        return "server mengabaikan Range";
    case DownloadFailure::TooMuchData:
        return "server mengirim kelebihan data";
    case DownloadFailure::SinkRejected:
        return "penulis menolak data";
    case DownloadFailure::Aborted:
        return "dibatalkan";
    }
    return "tidak diketahui";
}

void FirmwareDownloader::start(const DownloadConfig &config, uint32_t nowMs)
{
    transport_.stop();
    config_ = config;
    state_ = DownloadState::Running;
    failure_ = DownloadFailure::None;
    lastTransient_ = DownloadFailure::None;
    phase_ = Phase::Connect;
    parser_.reset();
    received_ = 0;
    bodyRemaining_ = 0;
    retryAt_ = 0;
    lastActivity_ = nowMs;
    failures_ = 0;
    if (config_.host == nullptr || config_.path == nullptr || (config_.expectedSize == 0 && config_.maxSize == 0) ||
        config_.maxFailuresWithoutProgress == 0)
    {
        fail(DownloadFailure::BadConfig);
    }
}

void FirmwareDownloader::abort()
{
    if (state_ == DownloadState::Running)
        fail(DownloadFailure::Aborted);
}

void FirmwareDownloader::fail(DownloadFailure why)
{
    transport_.stop();
    state_ = DownloadState::Failed;
    failure_ = why;
}

void FirmwareDownloader::transient(DownloadFailure cause, uint32_t now)
{
    transport_.stop();
    lastTransient_ = cause;
    if (++failures_ >= config_.maxFailuresWithoutProgress)
    {
        fail(DownloadFailure::TooManyRetries);
        return;
    }
    phase_ = Phase::Backoff;
    retryAt_ = now + config_.retryDelayMs;
}

DownloadState FirmwareDownloader::step(uint32_t now)
{
    if (state_ != DownloadState::Running)
        return state_;
    switch (phase_)
    {
    case Phase::Backoff:
        if (reached(now, retryAt_))
            phase_ = Phase::Connect;
        break;
    case Phase::Connect:
        connectAndSend(now);
        break;
    case Phase::Header:
        readHeader(now);
        break;
    case Phase::Body:
        readBody(now);
        break;
    }
    return state_;
}

void FirmwareDownloader::connectAndSend(uint32_t now)
{
    const size_t n = http::buildGetRequest(request_, sizeof(request_), config_.host,
                                           config_.path, received_);
    if (n == 0)
    {
        fail(DownloadFailure::BadConfig);
        return;
    }
    if (!transport_.connect(config_.host, config_.port))
    {
        transient(DownloadFailure::ConnectFailed, now);
        return;
    }
    if (transport_.write(reinterpret_cast<const uint8_t *>(request_), n) != n)
    {
        transient(DownloadFailure::RequestFailed, now);
        return;
    }
    parser_.reset();
    lastActivity_ = now;
    phase_ = Phase::Header;
}

void FirmwareDownloader::readHeader(uint32_t now)
{
    uint8_t buf[READ_CHUNK];
    const int n = transport_.read(buf, sizeof(buf));
    if (n > 0)
    {
        lastActivity_ = now;
        size_t used = 0;
        const http::ParseState st = parser_.feed(buf, static_cast<size_t>(n), used);
        if (st == http::ParseState::Error)
        {
            fail(DownloadFailure::BadResponse);
            return;
        }
        if (st == http::ParseState::Done)
        {
            onHeaderDone(now);
            if (state_ == DownloadState::Running && phase_ == Phase::Body &&
                used < static_cast<size_t>(n))
            {
                consumeBody(buf + used, static_cast<size_t>(n) - used);
            }
        }
        return;
    }
    if (!transport_.connected())
    {
        transient(DownloadFailure::Dropped, now);
    }
    else if (reached(now, lastActivity_ + config_.stallTimeoutMs))
    {
        transient(DownloadFailure::Stalled, now);
    }
}

void FirmwareDownloader::onHeaderDone(uint32_t now)
{
    const http::ResponseInfo &info = parser_.info();
    if (info.status >= 500 && info.status <= 599)
    {
        transient(DownloadFailure::ServerError, now);
        return;
    }
    const http::BodyPlan plan = http::planBody(info, received_);
    switch (plan.check)
    {
    case http::BodyCheck::Ok:
        break;
    case http::BodyCheck::BadStatus:
        fail(DownloadFailure::BadStatus);
        return;
    case http::BodyCheck::RangeIgnored:
        fail(DownloadFailure::RangeIgnored);
        return;
    case http::BodyCheck::RangeMismatch:
    case http::BodyCheck::NoLength:
        fail(DownloadFailure::BadResponse);
        return;
    }
    // Total yang akan kita miliki harus persis sama dengan ukuran di manifest,
    // atau (bila ukuran belum diketahui) tidak melebihi batas maxSize.
    const uint64_t total = static_cast<uint64_t>(received_) + plan.bodyLength;
    const bool sizeOk = config_.expectedSize != 0 ? total == config_.expectedSize
                                                  : total <= config_.maxSize;
    if (!sizeOk)
    {
        fail(DownloadFailure::SizeMismatch);
        return;
    }
    bodyRemaining_ = plan.bodyLength;
    phase_ = Phase::Body;
}

void FirmwareDownloader::readBody(uint32_t now)
{
    uint8_t buf[READ_CHUNK];
    for (int i = 0; i < MAX_READS_PER_STEP; ++i)
    {
        const int n = transport_.read(buf, sizeof(buf));
        if (n <= 0)
        {
            if (!transport_.connected())
            {
                transient(DownloadFailure::Dropped, now);
            }
            else if (reached(now, lastActivity_ + config_.stallTimeoutMs))
            {
                transient(DownloadFailure::Stalled, now);
            }
            return;
        }
        lastActivity_ = now;
        consumeBody(buf, static_cast<size_t>(n));
        if (state_ != DownloadState::Running || phase_ != Phase::Body)
            return;
    }
}

void FirmwareDownloader::consumeBody(const uint8_t *data, size_t len)
{
    if (len > bodyRemaining_)
    {
        fail(DownloadFailure::TooMuchData);
        return;
    }
    if (!sink_.write(data, len))
    {
        fail(DownloadFailure::SinkRejected);
        return;
    }
    received_ += static_cast<uint32_t>(len);
    bodyRemaining_ -= static_cast<uint32_t>(len);
    failures_ = 0; // ada kemajuan: hitungan gagal berturut-turut mulai dari nol
    if (bodyRemaining_ == 0)
    {
        transport_.stop();
        state_ = DownloadState::Done;
    }
}