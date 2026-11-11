#include "ota_command.h"

#include <stddef.h>

namespace
{

    bool isSpace(char c) { return c == ' ' || c == '\t'; }
    char lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; }

} // namespace

namespace OtaCommand
{

    OtaCommandResult parse(const char *line, uint32_t &build)
    {
        if (line == nullptr)
            return OtaCommandResult::NotOta;
        const char *p = line;
        while (isSpace(*p))
            ++p;

        // token pertama harus tepat "ota"
        const char *word = p;
        while (*p != '\0' && !isSpace(*p))
            ++p;
        if (p - word != 3 || lower(word[0]) != 'o' || lower(word[1]) != 't' || lower(word[2]) != 'a')
        {
            return OtaCommandResult::NotOta;
        }

        while (isSpace(*p))
            ++p;
        if (*p == '\0')
            return OtaCommandResult::BadArgument;

        uint64_t value = 0;
        size_t digits = 0;
        while (*p >= '0' && *p <= '9')
        {
            value = value * 10 + static_cast<uint64_t>(*p - '0');
            if (value > 0xFFFFFFFFull)
                return OtaCommandResult::BadArgument;
            ++digits;
            ++p;
        }
        if (digits == 0 || value == 0)
            return OtaCommandResult::BadArgument;

        while (isSpace(*p))
            ++p;
        if (*p != '\0')
            return OtaCommandResult::BadArgument; // ada sisa: "ota 2 x", "ota 2a"

        build = static_cast<uint32_t>(value);
        return OtaCommandResult::Ok;
    }

} // namespace OtaCommand