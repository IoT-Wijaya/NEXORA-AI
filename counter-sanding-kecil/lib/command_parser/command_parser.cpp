#include "command_parser.h"
#include <ctype.h>
#include <string.h>

namespace
{

    struct Entry
    {
        const char *text;
        Command command;
    };

    // Menambah perintah baru cukup menambah satu baris di sini.
    const Entry TABLE[] = {
        {"START", Command::Start},
        {"STOP", Command::Stop},
        {"RESET_S1", Command::ResetHasil},
        {"RESET_S2", Command::ResetModal},
        {"RESET_ALL", Command::ResetAll},
        {"RESET", Command::ResetAll}, // nama lama, tetap diterima
        {"RESTART_DEVICE", Command::RestartDevice},
    };

} // namespace

namespace CommandParser
{

    Command parse(const char *payload, size_t length)
    {
        if (payload == nullptr)
            return Command::Unknown;

        char text[MAX_LENGTH + 1];
        size_t n = (length < MAX_LENGTH) ? length : MAX_LENGTH;
        memcpy(text, payload, n);
        text[n] = '\0';

        // Buang spasi/enter di ujung saja.
        while (n > 0 && isspace((unsigned char)text[n - 1]))
        {
            text[--n] = '\0';
        }

        for (const Entry &e : TABLE)
        {
            if (strcmp(text, e.text) == 0)
                return e.command;
        }
        return Command::Unknown;
    }

    const char *name(Command command)
    {
        switch (command)
        {
        case Command::Start:
            return "START";
        case Command::Stop:
            return "STOP";
        case Command::ResetHasil:
            return "RESET_S1";
        case Command::ResetModal:
            return "RESET_S2";
        case Command::ResetAll:
            return "RESET_ALL";
        case Command::RestartDevice:
            return "RESTART_DEVICE";
        case Command::Unknown:
            break;
        }
        return "UNKNOWN";
    }

} // namespace CommandParser