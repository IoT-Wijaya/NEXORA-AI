#pragma once
#include <stdint.h>
#include "ota_record.h"

// Penyimpan catatan OTA. Di perangkat diisi NVS, di komputer (test) diisi pengganti.
class IOtaStore
{
public:
    virtual ~IOtaStore() = default;
    virtual bool load(OtaRecord &out) = 0; // false jika belum ada atau rusak
    virtual bool save(const OtaRecord &record) = 0;
};

struct OtaConfig
{
    uint8_t maxPendingBoots;
    uint8_t maxRequestAttempts;
    uint8_t maxRollbackAttempts;
    uint32_t healthDeadlineMs;
};

// Bukti bahwa firmware baru sehat. Ketiganya wajib: firmware yang kehilangan
// kemampuan menghubungi server OTA tidak boleh dianggap sehat.
struct HealthReport
{
    bool lanOk;
    bool mqttOk;
    bool serverOk;
    bool allOk() const { return lanOk && mqttOk && serverOk; }
};

enum class BootAction
{
    Normal,      // jalankan seperti biasa
    RunOta,      // boot bersih: unduh dan pasang update
    WatchHealth, // jalankan seperti biasa, tetapi panggil tick() sampai terkonfirmasi
    Rollback,    // pindahkan slot boot ke firmware lama, lalu restart
};

const char *bootActionCode(BootAction action);

enum class HealthAction
{
    NotNeeded, // tidak sedang menunggu konfirmasi
    Waiting,
    Confirmed, // aplikasi memanggil esp_ota_mark_app_valid_cancel_rollback()
    Rollback,  // pindahkan slot boot ke firmware lama, lalu restart
};

struct OtaReport
{
    OtaResult result;
    uint32_t targetBuild;
    uint32_t previousBuild;
};

// Keputusan OTA: kapan mengunduh, kapan mengonfirmasi, kapan rollback.
// Tidak menyentuh flash atau jaringan, hanya memutuskan dan mencatat keputusannya.
// Dibuat baru pada setiap boot; semua yang perlu bertahan ada di IOtaStore.
class OtaStateMachine
{
public:
    OtaStateMachine(IOtaStore &store, const OtaConfig &config);

    // Dipanggil sekali di awal boot. runningBuild = build firmware yang sedang berjalan.
    BootAction onBoot(uint32_t runningBuild, uint32_t nowMs);

    // Idle -> Requested. Mengembalikan false jika bukan Idle atau gagal menyimpan.
    // Jika berhasil, aplikasi me-restart perangkat ke boot bersih.
    bool requestUpdate(uint32_t targetBuild, uint32_t currentBuild);

    // Requested -> Pending, dipanggil setelah biner terpasang. Jika false (gagal menyimpan),
    // aplikasi tetap me-restart: onBoot() mengenali keadaan ini dan memperbaikinya.
    bool otaInstalled();

    // Requested -> Idle dengan alasan kegagalan.
    bool otaFailed(OtaResult reason);

    // Dipanggil berkala saat BootAction::WatchHealth.
    HealthAction tick(uint32_t nowMs, const HealthReport &health);

    OtaState state() const { return record_.state; }
    const OtaRecord &record() const { return record_; }

    bool hasUnreportedResult() const { return record_.result != OtaResult::None; }
    OtaReport report() const { return OtaReport{record_.result, record_.targetBuild, record_.previousBuild}; }
    bool markReported();

private:
    BootAction bootRequested(uint32_t runningBuild);
    BootAction bootPending(uint32_t runningBuild);
    BootAction bootRollingBack(uint32_t runningBuild);
    BootAction beginRollback(OtaResult reason);
    bool finish(OtaResult result);
    bool save();

    IOtaStore &store_;
    OtaConfig config_;
    OtaRecord record_;
    uint32_t windowStartMs_;
};