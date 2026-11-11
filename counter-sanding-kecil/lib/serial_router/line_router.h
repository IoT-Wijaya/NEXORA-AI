#pragma once
#include <stddef.h>

#include <functional>

// Mengarahkan baris Serial: setiap baris lengkap ditawarkan dulu ke penangan
// tambahan (mis. perintah "ota"). Bila tidak ada yang mengambilnya, baris
// diteruskan apa adanya ke tujuan semula (ConsoleShell), karakter demi karakter.
// Dengan begitu ConsoleShell yang sudah teruji tidak perlu diubah.
class LineRouter
{
public:
    using Forward = std::function<void(char c)>;
    // Mengembalikan true bila baris sudah ditangani (tidak diteruskan).
    using Handler = std::function<bool(const char *line)>;

    static constexpr size_t MAX_LINE = 120; // sama dengan CONSOLE_MAX_LINE
    static constexpr size_t MAX_HANDLERS = 4;

    explicit LineRouter(Forward forward) : forward_(forward) {}

    // false bila daftar penangan penuh.
    bool addHandler(Handler handler);

    void feed(char c);

private:
    void forwardBuffered();

    Forward forward_;
    Handler handlers_[MAX_HANDLERS];
    size_t handlerCount_ = 0;
    char line_[MAX_LINE + 1] = {};
    size_t length_ = 0;
    // Baris lebih panjang dari MAX_LINE: tidak lagi ditahan, diteruskan langsung
    // sehingga ConsoleShell sendiri yang melaporkan "baris terlalu panjang".
    bool passthrough_ = false;
};