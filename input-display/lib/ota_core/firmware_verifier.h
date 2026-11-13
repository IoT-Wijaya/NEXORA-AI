#pragma once
#include <stddef.h>
#include <stdint.h>
#include "manifest.h"
#include "sha256.h"

enum class VerifyResult
{
    Ok,
    SizeMismatch, // jumlah byte yang diterima tidak sama dengan ukuran di manifest
    TooMuchData,  // aliran melebihi ukuran di manifest (ditolak sejak terjadi)
    HashMismatch, // ukuran benar, isi salah
};

const char *verifyResultCode(VerifyResult result);

// Memeriksa biner yang mengalir terhadap manifest: ukuran dan SHA-256.
// Tidak menyimpan biner; hanya menghitung.
class FirmwareVerifier
{
public:
    explicit FirmwareVerifier(const Manifest &manifest);

    // Mulai dari awal. Dipakai pada unduhan ulang total, bukan pada kelanjutan (resume).
    void begin();

    // false jika data melebihi ukuran manifest atau pemeriksaan sudah selesai.
    // Setelah false, pemanggil harus membatalkan unduhan.
    bool update(const uint8_t *data, size_t length);

    uint32_t received() const { return received_; }

    // Hasil akhir. Aman dipanggil berulang: hasil pertama yang dikembalikan.
    VerifyResult finish();

private:
    uint32_t expectedSize_;
    uint8_t expectedHash_[Sha256::DIGEST_SIZE];
    Sha256 hash_;
    uint32_t received_;
    bool overflow_;
    bool finished_;
    VerifyResult result_;
};