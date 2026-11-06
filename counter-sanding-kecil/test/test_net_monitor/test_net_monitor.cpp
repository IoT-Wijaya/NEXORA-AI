#include <unity.h>
#include "net_monitor.h"

class FakeDriver : public INetDriver
{
public:
    void begin() override { beginCalls++; }
    bool linkUp() override { return link; }
    bool hasIP() override { return ip; }
    bool tryDhcp() override
    {
        dhcpCalls++;
        if (dhcpResult)
            ip = true;
        return dhcpResult;
    }
    void maintain() override { maintainCalls++; }
    Ip4 localIP() override { return Ip4{{192, 168, 1, 5}}; }

    bool link = true;
    bool ip = false;
    bool dhcpResult = false;
    int beginCalls = 0;
    int dhcpCalls = 0;
    int maintainCalls = 0;
};

static const uint32_t INTERVAL = 15000;
static int ev(NetEvent e) { return (int)e; }

void setUp() {}
void tearDown() {}

void test_start_menginisialisasi_driver_lalu_dhcp_sekali()
{
    FakeDriver d;
    d.dhcpResult = true;
    NetMonitor m(d, INTERVAL);
    TEST_ASSERT_TRUE(m.start());
    TEST_ASSERT_TRUE(m.isOnline());
    TEST_ASSERT_EQUAL(1, d.beginCalls);
    TEST_ASSERT_EQUAL(1, d.dhcpCalls);
}

void test_start_dhcp_gagal_berarti_offline()
{
    FakeDriver d;
    NetMonitor m(d, INTERVAL);
    TEST_ASSERT_FALSE(m.start());
    TEST_ASSERT_FALSE(m.isOnline());
}

void test_kabel_putus_offline_tanpa_dhcp_dan_maintain()
{
    FakeDriver d;
    d.link = false;
    NetMonitor m(d, INTERVAL);
    TEST_ASSERT_EQUAL(ev(NetEvent::None), ev(m.update(100000)));
    TEST_ASSERT_FALSE(m.isOnline());
    TEST_ASSERT_EQUAL(0, d.dhcpCalls);
    TEST_ASSERT_EQUAL(0, d.maintainCalls);
}

void test_kabel_ok_dan_ada_ip_online_dan_maintain()
{
    FakeDriver d;
    d.ip = true;
    NetMonitor m(d, INTERVAL);
    TEST_ASSERT_EQUAL(ev(NetEvent::None), ev(m.update(1000)));
    TEST_ASSERT_TRUE(m.isOnline());
    TEST_ASSERT_EQUAL(1, d.maintainCalls);
    TEST_ASSERT_EQUAL(0, d.dhcpCalls);
}

void test_tanpa_ip_belum_mengulang_dhcp_sebelum_interval()
{
    FakeDriver d;
    NetMonitor m(d, INTERVAL);
    TEST_ASSERT_EQUAL(ev(NetEvent::None), ev(m.update(14999)));
    TEST_ASSERT_EQUAL(0, d.dhcpCalls);
    TEST_ASSERT_FALSE(m.isOnline());
    TEST_ASSERT_EQUAL(ev(NetEvent::DhcpRetryFailed), ev(m.update(15000)));
    TEST_ASSERT_EQUAL(1, d.dhcpCalls);
}

void test_ulang_dhcp_berhasil_menjadi_online()
{
    FakeDriver d;
    d.dhcpResult = true;
    NetMonitor m(d, INTERVAL);
    TEST_ASSERT_EQUAL(ev(NetEvent::DhcpRetrySucceeded), ev(m.update(15000)));
    TEST_ASSERT_TRUE(m.isOnline());
}

void test_gagal_berulang_hanya_dicoba_tiap_interval()
{
    FakeDriver d;
    NetMonitor m(d, INTERVAL);
    TEST_ASSERT_EQUAL(ev(NetEvent::DhcpRetryFailed), ev(m.update(15000)));
    TEST_ASSERT_EQUAL(ev(NetEvent::None), ev(m.update(20000)));
    TEST_ASSERT_EQUAL(ev(NetEvent::None), ev(m.update(29999)));
    TEST_ASSERT_EQUAL(1, d.dhcpCalls);
    TEST_ASSERT_EQUAL(ev(NetEvent::DhcpRetryFailed), ev(m.update(30000)));
    TEST_ASSERT_EQUAL(2, d.dhcpCalls);
}

void test_kabel_dicabut_lalu_dipasang_kembali()
{
    FakeDriver d;
    d.ip = true;
    NetMonitor m(d, INTERVAL);
    m.update(1000);
    TEST_ASSERT_TRUE(m.isOnline());
    d.link = false;
    m.update(4000);
    TEST_ASSERT_FALSE(m.isOnline());
    d.link = true;
    m.update(7000);
    TEST_ASSERT_TRUE(m.isOnline());
    TEST_ASSERT_EQUAL(2, d.maintainCalls);
}

void test_pembanding_waktu_aman_saat_millis_melewati_batas()
{
    FakeDriver d;
    NetMonitor m(d, INTERVAL);
    const uint32_t t0 = 0xFFFFFF00u;
    TEST_ASSERT_EQUAL(ev(NetEvent::DhcpRetryFailed), ev(m.update(t0)));
    TEST_ASSERT_EQUAL(ev(NetEvent::None), ev(m.update(t0 + 14999u)));
    TEST_ASSERT_EQUAL(1, d.dhcpCalls);
    TEST_ASSERT_EQUAL(ev(NetEvent::DhcpRetryFailed), ev(m.update(t0 + 15000u)));
    TEST_ASSERT_EQUAL(2, d.dhcpCalls);
}

void test_format_ip()
{
    char buf[20];
    formatIp(Ip4{{192, 168, 1, 5}}, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("192.168.1.5", buf);
    formatIp(Ip4{{0, 0, 0, 0}}, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("0.0.0.0", buf);
    formatIp(Ip4{{255, 255, 255, 255}}, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("255.255.255.255", buf);
}

void test_format_ip_buffer_kecil_terpotong_dengan_aman()
{
    char buf[8];
    formatIp(Ip4{{192, 168, 1, 5}}, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("192.168", buf);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_start_menginisialisasi_driver_lalu_dhcp_sekali);
    RUN_TEST(test_start_dhcp_gagal_berarti_offline);
    RUN_TEST(test_kabel_putus_offline_tanpa_dhcp_dan_maintain);
    RUN_TEST(test_kabel_ok_dan_ada_ip_online_dan_maintain);
    RUN_TEST(test_tanpa_ip_belum_mengulang_dhcp_sebelum_interval);
    RUN_TEST(test_ulang_dhcp_berhasil_menjadi_online);
    RUN_TEST(test_gagal_berulang_hanya_dicoba_tiap_interval);
    RUN_TEST(test_kabel_dicabut_lalu_dipasang_kembali);
    RUN_TEST(test_pembanding_waktu_aman_saat_millis_melewati_batas);
    RUN_TEST(test_format_ip);
    RUN_TEST(test_format_ip_buffer_kecil_terpotong_dengan_aman);
    return UNITY_END();
}