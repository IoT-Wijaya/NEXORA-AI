#pragma once
#include <stdint.h>
#include "inet_driver.h"

enum class NetEvent
{
    None,
    DhcpRetryFailed,
    DhcpRetrySucceeded,
};

// Logika status jaringan. Menggantikan lanOK, cekEthernet, dan lastDhcpTry
// yang tadinya tersebar di main.cpp. online = kabel terpasang DAN punya IP.
class NetMonitor
{
public:
    NetMonitor(INetDriver &driver, uint32_t dhcpRetryIntervalMs);

    // Memulai driver dan mencoba DHCP sekali (dipakai di setup). Mengembalikan status online.
    bool start();

    // Dipanggil berkala. Mengembalikan peristiwa yang perlu dicatat pemanggil.
    NetEvent update(uint32_t nowMs);

    bool isOnline() const { return online_; }
    Ip4 localIp() const { return driver_.localIP(); }

private:
    INetDriver &driver_;
    const uint32_t retryMs_;
    bool online_;
    uint32_t lastDhcpTryMs_;
};