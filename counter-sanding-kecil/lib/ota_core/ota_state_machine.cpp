#include "ota_state_machine.h"

const char *otaResultCode(OtaResult result)
{
    switch (result)
    {
    case OtaResult::None:
        return "none";
    case OtaResult::Confirmed:
        return "confirmed";
    case OtaResult::RolledBackTimeout:
        return "rolled_back_timeout";
    case OtaResult::RolledBackCrashLoop:
        return "rolled_back_crash_loop";
    case OtaResult::RolledBackNotBooted:
        return "rolled_back_not_booted";
    case OtaResult::RollbackFailed:
        return "rollback_failed";
    case OtaResult::DownloadFailed:
        return "download_failed";
    case OtaResult::VerifyFailed:
        return "verify_failed";
    case OtaResult::WriteFailed:
        return "write_failed";
    case OtaResult::Abandoned:
        return "abandoned";
    }
    return "unknown";
}

const char *otaStateCode(OtaState state)
{
    switch (state)
    {
    case OtaState::Idle:
        return "idle";
    case OtaState::Requested:
        return "requested";
    case OtaState::Pending:
        return "pending";
    case OtaState::RollingBack:
        return "rolling_back";
    }
    return "unknown";
}

const char *bootActionCode(BootAction action)
{
    switch (action)
    {
    case BootAction::Normal:
        return "normal";
    case BootAction::RunOta:
        return "run_ota";
    case BootAction::WatchHealth:
        return "watch_health";
    case BootAction::Rollback:
        return "rollback";
    }
    return "unknown";
}

OtaStateMachine::OtaStateMachine(IOtaStore &store, const OtaConfig &config)
    : store_(store), config_(config), record_(freshOtaRecord()), windowStartMs_(0) {}

bool OtaStateMachine::save() { return store_.save(record_); }

// Menutup satu putaran OTA: kembali ke Idle dengan hasil yang menunggu dilaporkan.
bool OtaStateMachine::finish(OtaResult result)
{
    record_.state = OtaState::Idle;
    record_.attempts = 0;
    record_.result = result;
    return save();
}

// Niat rollback dicatat SEBELUM dilaksanakan, supaya daya putus di antaranya tidak
// meninggalkan firmware baru berjalan tanpa pengawasan.
BootAction OtaStateMachine::beginRollback(OtaResult reason)
{
    record_.state = OtaState::RollingBack;
    record_.attempts = 0;
    record_.result = reason;
    save(); // kegagalan menyimpan tidak boleh menahan rollback
    return BootAction::Rollback;
}

BootAction OtaStateMachine::onBoot(uint32_t runningBuild, uint32_t nowMs)
{
    windowStartMs_ = nowMs;
    if (!store_.load(record_))
    {
        // Catatan belum ada atau rusak: mulai bersih dan tulis ulang, supaya boot berikutnya mengenalinya.
        record_ = freshOtaRecord();
        save();
        return BootAction::Normal;
    }

    switch (record_.state)
    {
    case OtaState::Idle:
        return BootAction::Normal;
    case OtaState::Requested:
        return bootRequested(runningBuild);
    case OtaState::Pending:
        return bootPending(runningBuild);
    case OtaState::RollingBack:
        return bootRollingBack(runningBuild);
    }
    // Nilai state tidak dikenal: mulai bersih.
    record_ = freshOtaRecord();
    save();
    return BootAction::Normal;
}

BootAction OtaStateMachine::bootRequested(uint32_t runningBuild)
{
    // Update sebenarnya sudah selesai, tetapi catatan Pending belum sempat ditulis
    // (daya putus di antara Update.end() dan otaInstalled()).
    if (runningBuild == record_.targetBuild)
    {
        record_.state = OtaState::Pending;
        record_.attempts = 1;
        if (!save())
            return beginRollback(OtaResult::RolledBackCrashLoop);
        return BootAction::WatchHealth;
    }

    record_.attempts++;
    if (record_.attempts > config_.maxRequestAttempts)
    {
        finish(OtaResult::Abandoned);
        return BootAction::Normal;
    }
    // Percobaan yang tidak bisa dicatat dilewati, supaya unduhan tidak berulang tanpa batas.
    if (!save())
        return BootAction::Normal;
    return BootAction::RunOta;
}

BootAction OtaStateMachine::bootPending(uint32_t runningBuild)
{
    // Gambar baru tidak pernah berjalan: bootloader sudah kembali ke slot lama.
    if (runningBuild != record_.targetBuild)
    {
        finish(OtaResult::RolledBackNotBooted);
        return BootAction::Normal;
    }

    record_.attempts++;
    if (record_.attempts > config_.maxPendingBoots)
    {
        return beginRollback(OtaResult::RolledBackCrashLoop);
    }
    // Boot yang tidak bisa dicatat tidak bisa dihitung, jadi firmware baru tidak boleh dipercaya.
    if (!save())
        return beginRollback(OtaResult::RolledBackCrashLoop);
    return BootAction::WatchHealth;
}

BootAction OtaStateMachine::bootRollingBack(uint32_t runningBuild)
{
    // Rollback sudah berhasil: yang berjalan sekarang firmware lama.
    if (runningBuild == record_.previousBuild)
    {
        record_.state = OtaState::Idle;
        record_.attempts = 0;
        save();
        return BootAction::Normal;
    }

    record_.attempts++;
    if (record_.attempts > config_.maxRollbackAttempts)
    {
        finish(OtaResult::RollbackFailed);
        return BootAction::Normal;
    }
    save();
    return BootAction::Rollback;
}

bool OtaStateMachine::requestUpdate(uint32_t targetBuild, uint32_t currentBuild)
{
    if (record_.state != OtaState::Idle)
        return false;
    const OtaRecord before = record_;
    record_ = OtaRecord{OtaState::Requested, 0, OtaResult::None, targetBuild, currentBuild};
    if (save())
        return true;
    record_ = before;
    return false;
}

bool OtaStateMachine::otaInstalled()
{
    if (record_.state != OtaState::Requested)
        return false;
    const OtaRecord before = record_;
    record_.state = OtaState::Pending;
    record_.attempts = 0;
    if (save())
        return true;
    record_ = before;
    return false;
}

bool OtaStateMachine::otaFailed(OtaResult reason)
{
    if (record_.state != OtaState::Requested)
        return false;
    return finish(reason);
}

HealthAction OtaStateMachine::tick(uint32_t nowMs, const HealthReport &health)
{
    if (record_.state != OtaState::Pending)
        return HealthAction::NotNeeded;

    // Sehat menang atas batas waktu, termasuk pada tick yang sama.
    if (health.allOk())
    {
        const OtaRecord before = record_;
        record_.state = OtaState::Idle;
        record_.attempts = 0;
        record_.result = OtaResult::Confirmed;
        if (save())
            return HealthAction::Confirmed;
        record_ = before; // gagal menyimpan: tetap Pending, coba lagi pada tick berikutnya
        return HealthAction::Waiting;
    }

    // Selisih unsigned tetap benar walau millis() melewati batas 32 bit.
    if (nowMs - windowStartMs_ >= config_.healthDeadlineMs)
    {
        beginRollback(OtaResult::RolledBackTimeout);
        return HealthAction::Rollback;
    }
    return HealthAction::Waiting;
}

bool OtaStateMachine::markReported()
{
    if (record_.result == OtaResult::None)
        return true;
    const OtaRecord before = record_;
    record_.result = OtaResult::None;
    if (save())
        return true;
    record_ = before;
    return false;
}