#pragma once
#include <stddef.h>
#include <stdint.h>

// SHA-256 portabel (FIPS 180-4), dihitung bertahap. Tidak memakai mbedtls agar hasilnya
// sama persis di komputer (test) dan di ESP32.
class Sha256 {
 public:
  static constexpr size_t DIGEST_SIZE = 32;
  static constexpr size_t BLOCK_SIZE = 64;

  Sha256() { reset(); }

  void reset();
  void update(const uint8_t *data, size_t length);

  // Setelah finish(), panggil reset() sebelum dipakai lagi.
  void finish(uint8_t out[DIGEST_SIZE]);

 private:
  void processBlock(const uint8_t *block);

  uint32_t state_[8];
  uint8_t buffer_[BLOCK_SIZE];
  size_t bufferLength_;
  uint64_t totalBytes_;
};