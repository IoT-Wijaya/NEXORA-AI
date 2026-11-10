#pragma once
#include "firmware_downloader.h"
#include "firmware_verifier.h"
#include "iota_flash.h"
#include "manifest.h"

// Tujuan byte firmware untuk FirmwareDownloader: menulis ke slot cadangan lewat
// IOtaFlash sambil memeriksa ukuran + SHA-256 terhadap manifest.
//
// Alur: begin() -> downloader memanggil write() berulang -> finish() -> activate()
//   finish()   memeriksa ukuran, SHA-256, dan keabsahan image. Slot boot TIDAK berubah.
//   activate() mengalihkan boot ke slot baru. Panggil HANYA setelah catatan OTA
//              berstatus Pending tertulis di NVS, lalu reboot.
// Bila unduhan gagal di tengah jalan, panggil abort(); slot yang berjalan tidak tersentuh.
class OtaFlashSink : public IByteSink
{
public:
    OtaFlashSink(IOtaFlash &flash, const Manifest &manifest)
        : flash_(flash), manifest_(manifest), verifier_(manifest) {}
    ~OtaFlashSink() { abort(); }

    OtaFlashSink(const OtaFlashSink &) = delete;
    OtaFlashSink &operator=(const OtaFlashSink &) = delete;

    bool begin();
    bool write(const uint8_t *data, size_t len) override;
    bool finish();
    bool activate();
    void abort();

    bool ready() const { return ready_; }
    const char *error() const { return error_; }
    uint32_t received() const { return verifier_.received(); }

private:
    bool fail(const char *text)
    {
        error_ = text;
        return false;
    }

    IOtaFlash &flash_;
    Manifest manifest_;
    FirmwareVerifier verifier_;
    bool open_ = false;
    bool ready_ = false;
    const char *error_ = "";
};