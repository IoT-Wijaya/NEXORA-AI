#pragma once
#include <stddef.h>
#include <stdint.h>

// Identitas versi firmware. Teks versi dibersihkan agar aman dimasukkan ke JSON.
class FirmwareInfo
{
public:
    static constexpr size_t MAX_VERSION_LEN = 63;
    static constexpr size_t JSON_SIZE = 100; // cukup untuk versi terpanjang dan build 10 digit

    FirmwareInfo(const char *version, uint32_t build);

    const char *version() const { return version_; }
    uint32_t build() const { return build_; }

    // Build 0 = build lokal, bukan rilis resmi dari CI.
    bool isOfficial() const { return build_ > 0; }

    // {"version":"v1.0.0","build":128}
    void toJson(char *out, size_t size) const;

private:
    char version_[MAX_VERSION_LEN + 1];
    uint32_t build_;
};