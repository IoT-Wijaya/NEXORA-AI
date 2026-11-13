#pragma once
#include <stdint.h>

enum class OtaState : uint8_t
{
    Idle = 0,        // normal
    Requested = 1,   // update diminta, menunggu boot bersih untuk mengunduh
    Pending = 2,     // firmware baru terpasang, belum terbukti sehat
    RollingBack = 3, // niat rollback sudah dicatat, belum tentu selesai
};

enum class OtaResult : uint8_t
{
    None = 0,
    Confirmed,
    RolledBackTimeout,
    RolledBackCrashLoop,
    RolledBackNotBooted,
    RollbackFailed,
    DownloadFailed,
    VerifyFailed,
    WriteFailed,
    Abandoned,
};

// Satu-satunya data OTA yang disimpan permanen.
struct OtaRecord
{
    OtaState state;
    uint8_t attempts;       // arti bergantung state: percobaan unduh, boot Pending, atau percobaan rollback
    OtaResult result;       // hasil terakhir yang belum dilaporkan, None jika tidak ada
    uint32_t targetBuild;   // build yang akan atau sedang dipasang
    uint32_t previousBuild; // build sebelum update, tujuan rollback
};

inline OtaRecord freshOtaRecord() { return OtaRecord{OtaState::Idle, 0, OtaResult::None, 0, 0}; }

// Kode pendek untuk dilaporkan ke website.
const char *otaResultCode(OtaResult result);
const char *otaStateCode(OtaState state);