#include "signature.h"

#include "monocypher-ed25519.h"

namespace {

// Kunci nol adalah titik berorde kecil: bersama tanda tangan nol ia lolos
// verifikasi. Kunci nol di firmware berarti placeholder yang lupa diisi.
bool isAllZero(const uint8_t* data, size_t len) {
    uint8_t acc = 0;
    for (size_t i = 0; i < len; ++i) acc |= data[i];
    return acc == 0;
}

}  // namespace

namespace ota {

bool verifySignature(const uint8_t signature[SIGNATURE_LEN],
                     const uint8_t publicKey[PUBLIC_KEY_LEN],
                     const uint8_t* message, size_t messageLen) {
    if (signature == nullptr || publicKey == nullptr) return false;
    if (message == nullptr && messageLen != 0) return false;
    if (isAllZero(publicKey, PUBLIC_KEY_LEN)) return false;
    // PENTING: crypto_ed25519_check (SHA-512), BUKAN crypto_eddsa_check
    // (BLAKE2b). Hanya yang pertama cocok dengan `openssl pkeyutl -sign`.
    return crypto_ed25519_check(signature, publicKey, message, messageLen) == 0;
}

}  // namespace ota