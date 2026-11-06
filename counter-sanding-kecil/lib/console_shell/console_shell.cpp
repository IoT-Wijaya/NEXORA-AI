#include "console_shell.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace
{
    bool isSpace(char c) { return isspace((unsigned char)c) != 0; }
} // namespace

ConsoleShell::ConsoleShell(ConfigManager &config, Output output, Action reboot)
    : config_(config), output_(output), reboot_(reboot), line_{}, length_(0), overflow_(false) {}

void ConsoleShell::say(const char *text)
{
    if (output_)
        output_(text);
}

void ConsoleShell::feed(char c)
{
    if (c == '\r' || c == '\n')
    {
        if (overflow_)
        {
            say("Galat: baris terlalu panjang (maksimal 120 karakter).");
            overflow_ = false;
            length_ = 0;
            return;
        }
        if (length_ == 0)
            return; // baris kosong, atau pasangan kedua dari "\r\n"
        line_[length_] = '\0';
        length_ = 0;
        execute(line_);
        return;
    }
    if (overflow_)
        return;
    if (length_ >= CONSOLE_MAX_LINE)
    {
        overflow_ = true;
        length_ = 0;
        return;
    }
    line_[length_++] = c;
}

void ConsoleShell::execute(char *line)
{
    char *start = line;
    while (*start != '\0' && isSpace(*start))
        start++;

    size_t n = strlen(start);
    while (n > 0 && isSpace(start[n - 1]))
        start[--n] = '\0';
    if (n == 0)
        return;

    // Pisahkan perintah dan argumennya.
    char *args = start;
    while (*args != '\0' && !isSpace(*args))
        args++;
    if (*args != '\0')
    {
        *args++ = '\0';
        while (*args != '\0' && isSpace(*args))
            args++;
    }
    for (char *p = start; *p != '\0'; ++p)
        *p = (char)tolower((unsigned char)*p);

    if (strcmp(start, "help") == 0)
    {
        cmdHelp();
    }
    else if (strcmp(start, "show") == 0)
    {
        cmdShow();
    }
    else if (strcmp(start, "set") == 0)
    {
        cmdSet(args);
    }
    else if (strcmp(start, "unset") == 0)
    {
        cmdUnset(args);
    }
    else if (strcmp(start, "clear") == 0)
    {
        cmdClear(args);
    }
    else if (strcmp(start, "reboot") == 0)
    {
        say("Memulai ulang...");
        if (reboot_)
            reboot_();
    }
    else
    {
        char text[160];
        snprintf(text, sizeof(text), "Perintah tidak dikenal: %s. Ketik 'help'.", start);
        say(text);
    }
}

void ConsoleShell::cmdHelp()
{
    say("Perintah:");
    say("  show                  tampilkan konfigurasi (sandi disamarkan)");
    say("  set <kunci> <nilai>   simpan satu nilai");
    say("  unset <kunci>         kembalikan satu nilai ke bawaan");
    say("  clear yes             hapus semua konfigurasi");
    say("  reboot                mulai ulang perangkat");
    say("Kunci: mqtt_host, mqtt_port, mqtt_user, mqtt_pass, mach_name");
    say("Perubahan berlaku setelah 'reboot'.");
}

void ConsoleShell::cmdShow()
{
    DeviceConfig c;
    const int invalid = config_.load(c);
    char text[96];

    say("Konfigurasi tersimpan (berlaku setelah 'reboot'):");
    snprintf(text, sizeof(text), "  mqtt_host : %s", c.mqttHost[0] ? c.mqttHost : "(belum diisi)");
    say(text);
    snprintf(text, sizeof(text), "  mqtt_port : %u", (unsigned)c.mqttPort);
    say(text);
    snprintf(text, sizeof(text), "  mqtt_user : %s", c.mqttUser[0] ? c.mqttUser : "(kosong)");
    say(text);
    snprintf(text, sizeof(text), "  mqtt_pass : %s", c.mqttPass[0] ? "****" : "(kosong)");
    say(text);
    snprintf(text, sizeof(text), "  mach_name : %s", c.machineName);
    say(text);

    if (invalid > 0)
    {
        snprintf(text, sizeof(text), "Peringatan: %d nilai tersimpan tidak valid, dipakai nilai bawaan.", invalid);
        say(text);
    }
}

void ConsoleShell::cmdSet(char *args)
{
    if (*args == '\0')
    {
        say("Pemakaian: set <kunci> <nilai>");
        return;
    }
    char *value = args;
    while (*value != '\0' && !isSpace(*value))
        value++;
    if (*value != '\0')
    {
        *value++ = '\0';
        while (*value != '\0' && isSpace(*value))
            value++;
    }
    if (*value == '\0')
    {
        say("Galat: nilai kosong. Untuk menghapus gunakan: unset <kunci>");
        return;
    }
    reportResult(config_.set(args, value), "disimpan", args);
}

void ConsoleShell::cmdUnset(char *args)
{
    if (*args == '\0')
    {
        say("Pemakaian: unset <kunci>");
        return;
    }
    reportResult(config_.unset(args), "dikembalikan ke bawaan", args);
}

void ConsoleShell::cmdClear(const char *args)
{
    if (strcmp(args, "yes") != 0)
    {
        say("Ketik 'clear yes' untuk menghapus semua konfigurasi.");
        return;
    }
    say(config_.clearAll() ? "OK: semua konfigurasi dihapus." : "Galat: gagal menghapus penyimpanan.");
}

// Nilai yang diketik TIDAK PERNAH diulang di keluaran, supaya sandi tidak tercetak.
void ConsoleShell::reportResult(const char *error, const char *successText, const char *key)
{
    char text[160];
    if (error != nullptr)
    {
        snprintf(text, sizeof(text), "Galat: %s", error);
    }
    else
    {
        snprintf(text, sizeof(text), "OK: %s %s. Berlaku setelah 'reboot'.", key, successText);
    }
    say(text);
}