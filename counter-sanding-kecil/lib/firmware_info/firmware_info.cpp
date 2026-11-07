#include "firmware_info.h"
#include <stdio.h>

namespace
{
    bool diizinkan(char c)
    {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
               c == '.' || c == '-' || c == '_' || c == '+';
    }
} // namespace

FirmwareInfo::FirmwareInfo(const char *version, uint32_t build) : version_{}, build_(build)
{
    if (version == nullptr || version[0] == '\0')
    {
        snprintf(version_, sizeof(version_), "unknown");
        return;
    }
    size_t i = 0;
    for (; version[i] != '\0' && i < MAX_VERSION_LEN; i++)
    {
        version_[i] = diizinkan(version[i]) ? version[i] : '_';
    }
    version_[i] = '\0';
}

void FirmwareInfo::toJson(char *out, size_t size) const
{
    snprintf(out, size, "{\"version\":\"%s\",\"build\":%lu}", version_, (unsigned long)build_);
}