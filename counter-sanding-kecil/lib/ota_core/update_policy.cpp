#include "update_policy.h"
#include <string.h>

namespace UpdatePolicy
{

    PolicyDecision evaluate(const Manifest &m, const DeviceProfile &d)
    {
        if (d.deviceType == nullptr || strcmp(m.deviceType, d.deviceType) != 0)
        {
            return PolicyDecision::WrongDeviceType;
        }
        if (d.board == nullptr || strcmp(m.board, d.board) != 0)
        {
            return PolicyDecision::WrongBoard;
        }
        if (m.minFromBuild >= m.build)
        {
            return PolicyDecision::InconsistentManifest;
        }
        if (d.slotSize == 0 || m.size > d.slotSize)
        {
            return PolicyDecision::TooLarge;
        }
        if (m.build == d.currentBuild)
        {
            return PolicyDecision::AlreadyInstalled;
        }
        if (m.build < d.currentBuild)
        {
            return m.allowDowngrade ? PolicyDecision::Accept : PolicyDecision::DowngradeNotAllowed;
        }
        if (d.currentBuild < m.minFromBuild)
        {
            return PolicyDecision::TooOldToUpgrade;
        }
        return PolicyDecision::Accept;
    }

    const char *code(PolicyDecision decision)
    {
        switch (decision)
        {
        case PolicyDecision::Accept:
            return "accept";
        case PolicyDecision::AlreadyInstalled:
            return "already_installed";
        case PolicyDecision::WrongDeviceType:
            return "wrong_device_type";
        case PolicyDecision::WrongBoard:
            return "wrong_board";
        case PolicyDecision::InconsistentManifest:
            return "inconsistent_manifest";
        case PolicyDecision::DowngradeNotAllowed:
            return "downgrade_not_allowed";
        case PolicyDecision::TooOldToUpgrade:
            return "too_old_to_upgrade";
        case PolicyDecision::TooLarge:
            return "too_large";
        }
        return "unknown";
    }

    const char *text(PolicyDecision decision)
    {
        switch (decision)
        {
        case PolicyDecision::Accept:
            return "rilis diterima";
        case PolicyDecision::AlreadyInstalled:
            return "build ini sudah terpasang";
        case PolicyDecision::WrongDeviceType:
            return "rilis untuk jenis perangkat lain";
        case PolicyDecision::WrongBoard:
            return "rilis untuk board lain";
        case PolicyDecision::InconsistentManifest:
            return "manifest tidak konsisten";
        case PolicyDecision::DowngradeNotAllowed:
            return "downgrade tidak diizinkan";
        case PolicyDecision::TooOldToUpgrade:
            return "versi terpasang terlalu lama, naik bertahap";
        case PolicyDecision::TooLarge:
            return "biner lebih besar dari slot";
        }
        return "tidak diketahui";
    }

} // namespace UpdatePolicy