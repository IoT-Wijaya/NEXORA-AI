#include "ota_reporter.h"

#include <stdio.h>

bool formatOtaReport(const OtaReport &report, uint32_t runningBuild, char *out, size_t size)
{
    if (out == nullptr || size == 0)
        return false;
    const int n = snprintf(out, size,
                           "{\"result\":\"%s\",\"target\":%lu,\"previous\":%lu,\"build\":%lu}",
                           otaResultCode(report.result),
                           (unsigned long)report.targetBuild,
                           (unsigned long)report.previousBuild,
                           (unsigned long)runningBuild);
    if (n < 0 || (size_t)n >= size)
    {
        out[0] = '\0';
        return false;
    }
    return true;
}

OtaReporter::OtaReporter(OtaStateMachine &machine, Publisher publisher, uint32_t retryMs)
    : machine_(machine), publisher_(publisher), retryMs_(retryMs), hasTried_(false), lastTryMs_(0) {}

OtaReportEvent OtaReporter::service(uint32_t nowMs, bool connected, uint32_t runningBuild)
{
    if (!connected || !machine_.hasUnreportedResult())
        return OtaReportEvent::Nothing;
    // Selisih unsigned tetap benar walau millis() melewati batas 32 bit.
    if (hasTried_ && (nowMs - lastTryMs_) < retryMs_)
        return OtaReportEvent::Nothing;
    hasTried_ = true;
    lastTryMs_ = nowMs;

    char json[128];
    if (!formatOtaReport(machine_.report(), runningBuild, json, sizeof(json)))
        return OtaReportEvent::SendFailed; // tidak diharapkan terjadi; dicoba lagi nanti
    if (!publisher_ || !publisher_(json))
        return OtaReportEvent::SendFailed;
    return machine_.markReported() ? OtaReportEvent::Sent : OtaReportEvent::NotMarked;
}