#pragma once
#include <stddef.h>
#include <stdint.h>

#include <functional>

#include "firmware_downloader.h"
#include "iclock.h"
#include "iota_flash.h"
#include "manifest.h"
#include "signed_manifest.h"
#include "update_policy.h"

struct OtaRunConfig
{
    const char *host = nullptr;
    uint16_t port = 80;
    const char *manifestPath = nullptr; // berkas [64 byte tanda tangan][JSON manifest]
    const char *firmwarePath = nullptr;
    const uint8_t *publicKey = nullptr; // 32 byte, kunci publik penanda tangan rilis
    DeviceProfile profile{};            // identitas dan keadaan perangkat ini
    bool dryRun = false;                // true: berhenti sebelum slot boot dialihkan
    uint32_t overallTimeoutMs = 600000; // batas seluruh proses (manifest + firmware)
    uint8_t maxFailuresWithoutProgress = 5;
    uint32_t retryDelayMs = 2000;
    uint32_t stallTimeoutMs = 15000;
};

enum class OtaRunResult
{
    Installed, // slot boot sudah dialihkan; aplikasi harus mencatat lalu restart
    DryRunOk,  // semua pemeriksaan lolos; slot boot TIDAK diubah
    BadConfig,
    ManifestDownloadFailed,
    ManifestSignatureInvalid,
    ManifestInvalid,
    PolicyRejected,
    FlashBeginFailed,
    FirmwareDownloadFailed,
    FirmwareVerifyFailed,
    ActivateFailed,
    TimedOut,
};

const char *otaRunResultCode(OtaRunResult result);

struct OtaRunOutcome
{
    OtaRunResult result = OtaRunResult::BadConfig;
    PolicyDecision policy = PolicyDecision::Accept;   // berarti untuk PolicyRejected
    DownloadFailure download = DownloadFailure::None; // berarti untuk *DownloadFailed
    SignedManifestError signedError = SignedManifestError::None;
    const char *detail = ""; // teks pendek untuk log
    Manifest manifest{};
    bool manifestValid = false; // manifest sah dan terbaca

    bool ok() const { return result == OtaRunResult::Installed || result == OtaRunResult::DryRunOk; }
};

// Menjalankan seluruh urutan OTA secara berurutan (memblokir sampai selesai atau gagal).
// Dipakai pada boot bersih: tidak ada task lain yang memakai Ethernet.
//
//   1. unduh manifest bertanda tangan, verifikasi tanda tangan, baca isinya
//   2. periksa kebijakan (jenis perangkat, board, ukuran, build)
//   3. unduh firmware ke slot cadangan, verifikasi ukuran + SHA-256 + keabsahan image
//   4. (bukan dry run) alihkan slot boot
// Slot yang sedang berjalan tidak pernah disentuh sebelum langkah 4.
class OtaRunner
{
public:
    using Log = std::function<void(const char *line)>;

    OtaRunner(IHttpTransport &transport, IOtaFlash &flash, IClock &clock, Log log)
        : transport_(transport), flash_(flash), clock_(clock), log_(log) {}

    OtaRunOutcome run(const OtaRunConfig &config);

private:
    // Menjalankan pengunduh sampai selesai, gagal, atau melewati batas waktu keseluruhan.
    DownloadState pump(FirmwareDownloader &downloader, uint32_t startMs, uint32_t timeoutMs,
                       const char *label, bool &timedOut);
    void say(const char *text)
    {
        if (log_)
            log_(text);
    }

    IHttpTransport &transport_;
    IOtaFlash &flash_;
    IClock &clock_;
    Log log_;
};