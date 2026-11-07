#pragma once
#include <stdint.h>
#include "manifest.h"

// Keadaan perangkat yang dibandingkan dengan manifest.
struct DeviceProfile
{
    const char *deviceType; // "sanding-kecil"
    const char *board;      // "nodemcu-32s"
    uint32_t currentBuild;  // 0 = build lokal
    uint32_t slotSize;      // ukuran slot tujuan OTA dalam byte
};

enum class PolicyDecision
{
    Accept,
    AlreadyInstalled,
    WrongDeviceType,
    WrongBoard,
    InconsistentManifest,
    DowngradeNotAllowed,
    TooOldToUpgrade,
    TooLarge,
};

namespace UpdatePolicy
{

    // Menentukan apakah rilis boleh dipasang. Urutan aturan tetap, galat pertama yang dilaporkan.
    PolicyDecision evaluate(const Manifest &manifest, const DeviceProfile &device);

    inline bool isAccepted(PolicyDecision decision) { return decision == PolicyDecision::Accept; }

    // Kode pendek untuk dilaporkan ke website, misalnya "wrong_device_type".
    const char *code(PolicyDecision decision);

    // Penjelasan untuk log dan layar.
    const char *text(PolicyDecision decision);

} // namespace UpdatePolicy