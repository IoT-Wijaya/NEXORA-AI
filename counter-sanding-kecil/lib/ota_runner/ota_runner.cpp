#include "ota_runner.h"

#include <stdio.h>

#include "buffer_sink.h"
#include "ota_flash_sink.h"

namespace
{

    constexpr uint32_t PROGRESS_LOG_INTERVAL_MS = 2000;

} // namespace

const char *otaRunResultCode(OtaRunResult r)
{
    switch (r)
    {
    case OtaRunResult::Installed:
        return "installed";
    case OtaRunResult::DryRunOk:
        return "dry_run_ok";
    case OtaRunResult::BadConfig:
        return "bad_config";
    case OtaRunResult::ManifestDownloadFailed:
        return "manifest_download_failed";
    case OtaRunResult::ManifestSignatureInvalid:
        return "manifest_signature_invalid";
    case OtaRunResult::ManifestInvalid:
        return "manifest_invalid";
    case OtaRunResult::PolicyRejected:
        return "policy_rejected";
    case OtaRunResult::FlashBeginFailed:
        return "flash_begin_failed";
    case OtaRunResult::FirmwareDownloadFailed:
        return "firmware_download_failed";
    case OtaRunResult::FirmwareVerifyFailed:
        return "firmware_verify_failed";
    case OtaRunResult::ActivateFailed:
        return "activate_failed";
    case OtaRunResult::TimedOut:
        return "timed_out";
    }
    return "unknown";
}

DownloadState OtaRunner::pump(FirmwareDownloader &d, uint32_t startMs, uint32_t timeoutMs,
                              const char *label, bool &timedOut)
{
    uint32_t lastLog = clock_.nowMs();
    while (d.state() == DownloadState::Running)
    {
        const uint32_t now = clock_.nowMs();
        if (now - startMs >= timeoutMs)
        {
            d.abort();
            timedOut = true;
            break;
        }
        d.step(now);
        if (now - lastLog >= PROGRESS_LOG_INTERVAL_MS)
        {
            lastLog = now;
            char line[64];
            snprintf(line, sizeof(line), "%s: %lu byte diterima", label,
                     static_cast<unsigned long>(d.received()));
            say(line);
        }
        clock_.sleepMs(1); // beri waktu ke sistem; mencegah loop menyita CPU
    }
    return d.state();
}

OtaRunOutcome OtaRunner::run(const OtaRunConfig &cfg)
{
    OtaRunOutcome out;
    if (cfg.host == nullptr || cfg.manifestPath == nullptr || cfg.firmwarePath == nullptr ||
        cfg.publicKey == nullptr)
    {
        out.result = OtaRunResult::BadConfig;
        out.detail = "host, path, atau kunci publik kosong";
        return out;
    }

    const uint32_t start = clock_.nowMs();
    bool timedOut = false;
    char line[160];

    DownloadConfig dc;
    dc.host = cfg.host;
    dc.port = cfg.port;
    dc.maxFailuresWithoutProgress = cfg.maxFailuresWithoutProgress;
    dc.retryDelayMs = cfg.retryDelayMs;
    dc.stallTimeoutMs = cfg.stallTimeoutMs;

    // ---- 1. manifest bertanda tangan ----
    say("OTA: mengunduh manifest...");
    uint8_t envelope[ota::SIGNATURE_LEN + ManifestLimits::MAX_BYTES];
    BufferSink envelopeSink(envelope, sizeof(envelope));
    FirmwareDownloader manifestDl(transport_, envelopeSink);
    dc.path = cfg.manifestPath;
    dc.expectedSize = 0;
    dc.maxSize = sizeof(envelope);
    manifestDl.start(dc, clock_.nowMs());
    pump(manifestDl, start, cfg.overallTimeoutMs, "manifest", timedOut);
    if (timedOut)
    {
        out.result = OtaRunResult::TimedOut;
        out.detail = "melewati batas waktu saat mengunduh manifest";
        return out;
    }
    if (manifestDl.state() != DownloadState::Done)
    {
        out.result = OtaRunResult::ManifestDownloadFailed;
        out.download = manifestDl.failure();
        out.detail = downloadFailureText(manifestDl.failure());
        return out;
    }

    Manifest m;
    const SignedManifestResult sm =
        SignedManifestParser::parse(envelope, envelopeSink.size(), cfg.publicKey, m);
    if (!sm.ok())
    {
        out.signedError = sm.error;
        out.result = sm.error == SignedManifestError::BadSignature
                         ? OtaRunResult::ManifestSignatureInvalid
                         : OtaRunResult::ManifestInvalid;
        out.detail = signedManifestErrorText(sm.error);
        return out;
    }
    out.manifest = m;
    out.manifestValid = true;
    snprintf(line, sizeof(line), "OTA: manifest sah, versi %s, build %lu, %lu byte", m.version,
             static_cast<unsigned long>(m.build), static_cast<unsigned long>(m.size));
    say(line);

    // ---- 2. kebijakan ----
    const PolicyDecision pd = UpdatePolicy::evaluate(m, cfg.profile);
    out.policy = pd;
    if (!UpdatePolicy::isAccepted(pd))
    {
        out.result = OtaRunResult::PolicyRejected;
        out.detail = UpdatePolicy::text(pd);
        return out;
    }
    say("OTA: kebijakan menerima rilis ini");

    // ---- 3. firmware ke slot cadangan ----
    OtaFlashSink sink(flash_, m);
    say("OTA: menyiapkan slot cadangan (menghapus flash)...");
    if (!sink.begin())
    {
        out.result = OtaRunResult::FlashBeginFailed;
        out.detail = sink.error();
        return out;
    }

    FirmwareDownloader fwDl(transport_, sink);
    dc.path = cfg.firmwarePath;
    dc.expectedSize = m.size;
    dc.maxSize = 0;
    say("OTA: mengunduh firmware...");
    fwDl.start(dc, clock_.nowMs());
    pump(fwDl, start, cfg.overallTimeoutMs, "firmware", timedOut);
    if (timedOut)
    {
        sink.abort();
        out.result = OtaRunResult::TimedOut;
        out.detail = "melewati batas waktu saat mengunduh firmware";
        return out;
    }
    if (fwDl.state() != DownloadState::Done)
    {
        sink.abort();
        out.result = OtaRunResult::FirmwareDownloadFailed;
        out.download = fwDl.failure();
        out.detail = fwDl.failure() == DownloadFailure::SinkRejected ? sink.error()
                                                                     : downloadFailureText(fwDl.failure());
        return out;
    }

    say("OTA: memverifikasi SHA-256 dan image...");
    if (!sink.finish())
    {
        out.result = OtaRunResult::FirmwareVerifyFailed;
        out.detail = sink.error();
        return out;
    }

    // ---- 4. alihkan slot boot ----
    if (cfg.dryRun)
    {
        say("OTA: DRY RUN selesai, semua pemeriksaan lolos. Slot boot TIDAK diubah.");
        out.result = OtaRunResult::DryRunOk;
        out.detail = "dry run";
        return out;
    }
    if (!sink.activate())
    {
        out.result = OtaRunResult::ActivateFailed;
        out.detail = sink.error();
        return out;
    }
    say("OTA: slot boot dialihkan ke firmware baru.");
    out.result = OtaRunResult::Installed;
    out.detail = "terpasang";
    return out;
}