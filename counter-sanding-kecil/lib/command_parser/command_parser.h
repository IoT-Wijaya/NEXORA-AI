#pragma once
#include <stddef.h>

enum class Command
{
    Unknown,
    Start,
    Stop,
    ResetHasil,
    ResetModal,
    ResetAll,
    RestartDevice,
};

namespace CommandParser
{

    // Panjang teks perintah yang dibaca; sisanya dipotong.
    constexpr size_t MAX_LENGTH = 23;

    // Mengubah payload MQTT menjadi perintah. Payload TIDAK harus diakhiri NUL;
    // hanya sebanyak `length` byte yang dibaca. Tidak dikenal -> Command::Unknown.
    Command parse(const char *payload, size_t length);

    // Nama perintah untuk log.
    const char *name(Command command);

} // namespace CommandParser