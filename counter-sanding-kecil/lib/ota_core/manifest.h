#pragma once
#include <stddef.h>
#include <stdint.h>

namespace ManifestLimits
{
    constexpr size_t MAX_BYTES = 1024;  // manifest lebih besar dari ini ditolak
    constexpr size_t MAX_TYPE_LEN = 23; // device_type dan board
    constexpr size_t MAX_VERSION_LEN = 63;
    constexpr uint32_t MAX_FIRMWARE_BYTES = 16u * 1024u * 1024u; // batas kewarasan, bukan batas slot
    constexpr uint32_t SUPPORTED_SCHEMA = 1;
} // namespace ManifestLimits

// Isi satu rilis firmware. Hanya valid jika diisi oleh ManifestParser::parse().
struct Manifest
{
    uint32_t schema;
    char deviceType[ManifestLimits::MAX_TYPE_LEN + 1];
    char board[ManifestLimits::MAX_TYPE_LEN + 1];
    char version[ManifestLimits::MAX_VERSION_LEN + 1];
    uint32_t build; // naik terus; dipakai untuk anti-downgrade
    uint32_t size;  // ukuran biner dalam byte
    uint8_t sha256[32];
    uint32_t minFromBuild; // build terendah yang boleh langsung naik ke rilis ini
    bool allowDowngrade;
};

enum class ManifestError
{
    None,
    EmptyOrTooLarge,
    InvalidJson,
    UnsupportedSchema,
    MissingField,
    BadField,
};

struct ManifestResult
{
    ManifestError error;
    const char *field; // nama kolom penyebab galat, "" jika tidak ada

    bool ok() const { return error == ManifestError::None; }
};

const char *manifestErrorText(ManifestError error);

namespace ManifestParser
{
    // `json` tidak harus berakhiran NUL; hanya `length` byte yang dibaca.
    // `out` tidak diubah jika hasilnya galat.
    ManifestResult parse(const char *json, size_t length, Manifest &out);
} // namespace ManifestParser