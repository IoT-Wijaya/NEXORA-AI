#pragma once
#include <stddef.h>
#include <functional>
#include "device_config.h"

constexpr size_t CONSOLE_MAX_LINE = 120;

// Konsol perintah untuk mengisi konfigurasi lewat Serial Monitor.
// Tidak membaca Serial sendiri: pemanggil menyuapkan karakter lewat feed(),
// sehingga seluruh logikanya bisa diuji di komputer.
class ConsoleShell
{
public:
    using Output = std::function<void(const char *line)>;
    using Action = std::function<void()>;

    ConsoleShell(ConfigManager &config, Output output, Action reboot);

    void feed(char c);

private:
    void execute(char *line);
    void cmdHelp();
    void cmdShow();
    void cmdSet(char *args);
    void cmdUnset(char *args);
    void cmdClear(const char *args);
    void say(const char *text);
    void reportResult(const char *error, const char *successText, const char *key);

    ConfigManager &config_;
    Output output_;
    Action reboot_;
    char line_[CONSOLE_MAX_LINE + 1];
    size_t length_;
    bool overflow_;
};