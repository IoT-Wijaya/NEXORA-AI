#pragma once
#include <functional>
#include <stddef.h>
#include <stdint.h>

#include "ota_state_machine.h"

// Mengubah laporan OTA menjadi JSON, misalnya:
//   {"result":"confirmed","target":5,"previous":4,"build":5}
// "build" = build firmware yang sedang berjalan saat melapor. Mengembalikan false bila
// buffer terlalu kecil; buffer tidak pernah ditulis melewati `size`.
bool formatOtaReport(const OtaReport &report, uint32_t runningBuild, char *out, size_t size);

enum class OtaReportEvent
{
    Nothing,    // tidak ada yang perlu dikirim, atau belum waktunya mencoba lagi
    Sent,       // terkirim dan sudah ditandai terlaporkan
    SendFailed, // publish ditolak; dicoba lagi setelah jeda
    NotMarked,  // terkirim, tetapi gagal menandai terlaporkan: akan terkirim ulang
};

// Mengirim hasil OTA yang belum dilaporkan, sampai berhasil. Dipanggil berkala dari
// networkTask (pemilik OtaStateMachine dan MQTT).
//
// Jaminannya "minimal sekali": MQTT QoS 0 tidak memberi tanda terima, dan bila penandaan
// terlaporkan gagal tersimpan, hasil yang sama terkirim lagi. Penerima harus tahan duplikat.
class OtaReporter
{
public:
    using Publisher = std::function<bool(const char *json)>;

    OtaReporter(OtaStateMachine &machine, Publisher publisher, uint32_t retryMs);

    OtaReportEvent service(uint32_t nowMs, bool connected, uint32_t runningBuild);

private:
    OtaStateMachine &machine_;
    Publisher publisher_;
    uint32_t retryMs_;
    bool hasTried_;
    uint32_t lastTryMs_;
};