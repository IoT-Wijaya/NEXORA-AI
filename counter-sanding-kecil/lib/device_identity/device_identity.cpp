#include "device_identity.h"
#include <stdio.h>
#include <string.h>

namespace MacFormat
{

    bool isValid(const uint8_t mac[6])
    {
        bool allZero = true;
        for (int i = 0; i < 6; i++)
        {
            if (mac[i] != 0x00)
                allZero = false;
        }
        const bool multicast = (mac[0] & 0x01) != 0; // bit terendah byte pertama
        return !allZero && !multicast;
    }

    void toHex(const uint8_t mac[6], char *out, size_t size)
    {
        snprintf(out, size, "%02X%02X%02X%02X%02X%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    void toColon(const uint8_t mac[6], char *out, size_t size)
    {
        snprintf(out, size, "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

} // namespace MacFormat

DeviceIdentity::DeviceIdentity(IMacSource &source, const char *clientIdPrefix)
    : source_(source), prefix_(clientIdPrefix), mac_{}, macText_{}, clientId_{}, valid_(false) {}

bool DeviceIdentity::begin()
{
    memset(mac_, 0, sizeof(mac_));
    const bool terbaca = source_.readMac(mac_);
    valid_ = terbaca && MacFormat::isValid(mac_);

    MacFormat::toColon(mac_, macText_, sizeof(macText_));

    if (valid_)
    {
        char hex[MacFormat::HEX_SIZE];
        MacFormat::toHex(mac_, hex, sizeof(hex));
        snprintf(clientId_, sizeof(clientId_), "%s%s", prefix_, hex);
    }
    else
    {
        snprintf(clientId_, sizeof(clientId_), "%sINVALID", prefix_);
    }
    return valid_;
}