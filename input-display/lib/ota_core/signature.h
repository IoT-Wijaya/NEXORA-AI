#pragma once
#include <stddef.h>
#include <stdint.h>

// Verifikasi tanda tangan Ed25519 (standar RFC 8032, hash SHA-512).
// Pembungkus ini ada agar sisa kode tidak perlu tahu bahwa Monocypher
// mengembalikan 0 untuk "valid" dan -1 untuk "tidak valid".
namespace ota {

constexpr size_t SIGNATURE_LEN  = 64;
constexpr size_t PUBLIC_KEY_LEN = 32;

// true hanya jika tanda tangan sah untuk persis isi pesan ini.
bool verifySignature(const uint8_t signature[SIGNATURE_LEN],
                     const uint8_t publicKey[PUBLIC_KEY_LEN],
                     const uint8_t* message, size_t messageLen);

}  // namespace ota