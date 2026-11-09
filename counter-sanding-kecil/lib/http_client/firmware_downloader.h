#pragma once
#include <stddef.h>
#include <stdint.h>

#include "http_response.h"

// Satu koneksi TCP. Adaptor perangkat membungkus EthernetClient; tes memakai
// transport palsu. Semua pemanggilan TIDAK boleh menunggu lama, kecuali
// connect() yang boleh menunggu sampai batas waktunya sendiri.
class IHttpTransport
{
public:
    virtual ~IHttpTransport() {}
    virtual bool connect(const char *host, uint16_t port) = 0;
    virtual size_t write(const uint8_t *data, size_t len) = 0;
    // Mengembalikan jumlah byte yang dibaca; 0 bila saat ini tidak ada data.
    virtual int read(uint8_t *buf, size_t cap) = 0;
    // false bila lawan sudah menutup DAN semua data sudah terbaca.
    virtual bool connected() = 0;
    virtual void stop() = 0;
};

// Tujuan byte isi firmware (kelak: penulis flash + pemverifikasi SHA-256).
// false berarti "tolak": unduhan dihentikan.
class IByteSink
{
public:
    virtual ~IByteSink() {}
    virtual bool write(const uint8_t *data, size_t len) = 0;
};

struct DownloadConfig
{
    const char *host = nullptr; // pointer harus hidup selama unduhan
    uint16_t port = 80;
    const char *path = nullptr;
    uint32_t expectedSize = 0; // dari manifest bertanda tangan
    uint8_t maxFailuresWithoutProgress = 5;
    uint32_t retryDelayMs = 2000;
    uint32_t stallTimeoutMs = 15000;
};

enum class DownloadState
{
    Idle,
    Running,
    Done,
    Failed
};

enum class DownloadFailure
{
    None,
    BadConfig,
    ConnectFailed, // hanya muncul sebagai penyebab terakhir
    RequestFailed,
    Dropped,        // koneksi putus sebelum isi selesai
    Stalled,        // tidak ada data selama stallTimeoutMs
    ServerError,    // 5xx
    TooManyRetries, // gagal berturut-turut tanpa kemajuan; lihat lastTransient()
    BadResponse,    // header rusak atau tidak bisa diverifikasi
    BadStatus,      // 3xx/4xx dan sejenisnya: tidak diulang
    SizeMismatch,   // ukuran dari server tidak cocok dengan manifest
    RangeIgnored,   // minta lanjut, server mengirim dari awal
    TooMuchData,    // server mengirim lebih dari Content-Length
    SinkRejected,
    Aborted,
};

const char *downloadFailureText(DownloadFailure failure);

// Mengunduh firmware lewat HTTP, mengalirkannya ke IByteSink. Tidak memakai
// buffer besar dan tidak pernah menahan loop(): panggil step() berulang.
// Koneksi putus atau macet diulang dengan Range dari byte yang sudah diterima.
class FirmwareDownloader
{
public:
    FirmwareDownloader(IHttpTransport &transport, IByteSink &sink)
        : transport_(transport), sink_(sink) {}

    void start(const DownloadConfig &config, uint32_t nowMs);
    DownloadState step(uint32_t nowMs);
    void abort();

    DownloadState state() const { return state_; }
    DownloadFailure failure() const { return failure_; }
    DownloadFailure lastTransient() const { return lastTransient_; }
    uint32_t received() const { return received_; }

private:
    enum class Phase
    {
        Connect,
        Backoff,
        Header,
        Body
    };

    void connectAndSend(uint32_t now);
    void readHeader(uint32_t now);
    void readBody(uint32_t now);
    void onHeaderDone(uint32_t now);
    void consumeBody(const uint8_t *data, size_t len);
    void transient(DownloadFailure cause, uint32_t now);
    void fail(DownloadFailure why);

    IHttpTransport &transport_;
    IByteSink &sink_;
    DownloadConfig config_;
    DownloadState state_ = DownloadState::Idle;
    DownloadFailure failure_ = DownloadFailure::None;
    DownloadFailure lastTransient_ = DownloadFailure::None;
    Phase phase_ = Phase::Connect;
    http::HeaderParser parser_;
    char request_[256];
    uint32_t received_ = 0;
    uint32_t bodyRemaining_ = 0;
    uint32_t retryAt_ = 0;
    uint32_t lastActivity_ = 0;
    uint8_t failures_ = 0;
};