#pragma once
#include <stddef.h>
#include <stdint.h>

#include "manifest.h"
#include "signature.h"

// Amplop manifest bertanda tangan, satu berkas:
//   [ 64 byte tanda tangan Ed25519 ][ JSON manifest ]
// Tanda tangan dihitung atas byte JSON persis seperti yang dikirim.
// Satu berkas dipilih agar manifest dan tanda tangannya tidak bisa tertukar
// atau berbeda versi akibat dua unduhan terpisah.
enum class SignedManifestError
{
    None,
    TooShort,     // lebih pendek dari tanda tangan + 1 byte
    TooLarge,     // lebih besar dari tanda tangan + batas manifest
    BadSignature, // tanda tangan tidak sah untuk kunci ini
    BadManifest,  // tanda tangan sah, tetapi isi manifest ditolak parser
};

struct SignedManifestResult
{
    SignedManifestError error;
    ManifestResult manifest; // berarti hanya jika error == BadManifest

    bool ok() const { return error == SignedManifestError::None; }
};

const char *signedManifestErrorText(SignedManifestError error);

namespace SignedManifestParser
{
    // Urutan: cek ukuran -> verifikasi tanda tangan -> baru parse JSON.
    // JSON dari sumber yang tidak terpercaya tidak pernah disentuh parser.
    // `out` tidak diubah jika hasilnya galat.
    SignedManifestResult parse(const uint8_t *envelope, size_t length,
                               const uint8_t publicKey[ota::PUBLIC_KEY_LEN],
                               Manifest &out);
} // namespace SignedManifestParser