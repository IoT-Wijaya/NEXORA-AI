#include "net_monitor.h"
#include <stdio.h>

void formatIp(const Ip4 &ip, char *out, size_t size)
{
    snprintf(out, size, "%u.%u.%u.%u", (unsigned)ip.b[0], (unsigned)ip.b[1],
             (unsigned)ip.b[2], (unsigned)ip.b[3]);
}

NetMonitor::NetMonitor(INetDriver &driver, uint32_t dhcpRetryIntervalMs)
    : driver_(driver), retryMs_(dhcpRetryIntervalMs), online_(false), lastDhcpTryMs_(0) {}

bool NetMonitor::start()
{
    driver_.begin();
    online_ = driver_.tryDhcp();
    return online_;
}

NetEvent NetMonitor::update(uint32_t nowMs)
{
    if (!driver_.linkUp())
    {
        online_ = false;
        return NetEvent::None;
    }

    if (!driver_.hasIP())
    {
        // Kabel terpasang tetapi belum dapat IP: ulangi DHCP secara berkala.
        online_ = false;
        if (nowMs - lastDhcpTryMs_ < retryMs_)
            return NetEvent::None;
        lastDhcpTryMs_ = nowMs;
        if (driver_.tryDhcp())
        {
            online_ = true;
            return NetEvent::DhcpRetrySucceeded;
        }
        return NetEvent::DhcpRetryFailed;
    }

    driver_.maintain();
    online_ = true;
    return NetEvent::None;
}