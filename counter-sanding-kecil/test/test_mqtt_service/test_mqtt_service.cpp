#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <unity.h>
#include "sanding_config.h"
#include "mqtt_service.h"

struct Terbit
{
    std::string topic;
    std::string payload;
    bool retain;
};

class FakeMqtt : public IMqttClient
{
public:
    bool connected() override { return tersambung; }
    bool connect(const MqttConnectParams &p) override
    {
        connectCalls++;
        clientId = p.clientId;
        userNull = (p.user == nullptr);
        if (p.user)
            user = p.user;
        willNull = (p.willTopic == nullptr);
        if (p.willTopic)
        {
            willTopic = p.willTopic;
            willMessage = p.willMessage;
            willRetain = p.willRetain;
        }
        if (!terima)
            return false;
        tersambung = true;
        return true;
    }
    int state() override { return kodeState; }
    bool subscribe(const char *t) override
    {
        langganan.push_back(t);
        return true;
    }
    bool publish(const char *t, const char *p, bool r) override
    {
        terbit.push_back({t, p, r});
        return true;
    }
    bool loop() override
    {
        loopCalls++;
        return tersambung;
    }
    void disconnect() override
    {
        disconnectCalls++;
        tersambung = false;
    }
    void onMessage(MessageCallback cb) override { callback = cb; }

    void kirim(const char *topic, const char *payload)
    {
        if (callback)
            callback(topic, (const uint8_t *)payload, strlen(payload));
    }

    bool tersambung = false;
    bool terima = true;
    int kodeState = 5;
    int connectCalls = 0, loopCalls = 0, disconnectCalls = 0;
    std::string clientId, user, willTopic, willMessage;
    bool userNull = true, willNull = true, willRetain = false;
    std::vector<std::string> langganan;
    std::vector<Terbit> terbit;
    MessageCallback callback;
};

static MqttSettings buatSettings(const char *user = "", bool lastWill = false)
{
    MqttSettings s{};
    snprintf(s.clientId, sizeof(s.clientId), "UJI-1234");
    s.user = user;
    s.pass = "rahasia";
    s.useLastWill = lastWill;
    return s;
}

static int ev(MqttEvent e) { return (int)e; }
static const uint32_t HB = config::HEARTBEAT_INTERVAL;
static const uint32_t ULANG = config::MQTT_RECONNECT_INTERVAL;

void setUp() {}
void tearDown() {}

void test_jaringan_offline_tidak_menyambung()
{
    FakeMqtt f;
    MqttService m(f);
    m.begin(buatSettings());
    TEST_ASSERT_EQUAL(ev(MqttEvent::None), ev(m.update(false, 1000)));
    TEST_ASSERT_EQUAL(0, f.connectCalls);
}

void test_online_langsung_mencoba_sambung_dan_subscribe_dan_kirim_status()
{
    FakeMqtt f;
    MqttService m(f);
    m.begin(buatSettings());
    TEST_ASSERT_EQUAL(ev(MqttEvent::Connected), ev(m.update(true, 0)));
    TEST_ASSERT_EQUAL(1, f.connectCalls);
    TEST_ASSERT_EQUAL(1, (int)f.langganan.size());
    TEST_ASSERT_EQUAL_STRING(config::topicPerintah, f.langganan[0].c_str());
    TEST_ASSERT_EQUAL(1, (int)f.terbit.size());
    TEST_ASSERT_EQUAL_STRING(config::topicStatus, f.terbit[0].topic.c_str());
    TEST_ASSERT_EQUAL_STRING("ONLINE", f.terbit[0].payload.c_str());
    TEST_ASSERT_TRUE(f.terbit[0].retain);
}

void test_user_kosong_dikirim_null_dan_user_ada_dikirim_apa_adanya()
{
    FakeMqtt a;
    MqttService ma(a);
    ma.begin(buatSettings(""));
    ma.update(true, 0);
    TEST_ASSERT_TRUE(a.userNull);
    TEST_ASSERT_EQUAL_STRING("UJI-1234", a.clientId.c_str());

    FakeMqtt b;
    MqttService mb(b);
    mb.begin(buatSettings("dian"));
    mb.update(true, 0);
    TEST_ASSERT_FALSE(b.userNull);
    TEST_ASSERT_EQUAL_STRING("dian", b.user.c_str());
}

void test_last_will_mati_tanpa_will_dan_hidup_dengan_offline_retained()
{
    FakeMqtt a;
    MqttService ma(a);
    ma.begin(buatSettings("", false));
    ma.update(true, 0);
    TEST_ASSERT_TRUE(a.willNull);

    FakeMqtt b;
    MqttService mb(b);
    mb.begin(buatSettings("", true));
    mb.update(true, 0);
    TEST_ASSERT_FALSE(b.willNull);
    TEST_ASSERT_EQUAL_STRING(config::topicStatus, b.willTopic.c_str());
    TEST_ASSERT_EQUAL_STRING("OFFLINE", b.willMessage.c_str());
    TEST_ASSERT_TRUE(b.willRetain);
}

void test_gagal_sambung_melapor_kode_tanpa_subscribe_dan_publish()
{
    FakeMqtt f;
    f.terima = false;
    f.kodeState = 4;
    MqttService m(f);
    m.begin(buatSettings());
    TEST_ASSERT_EQUAL(ev(MqttEvent::ConnectFailed), ev(m.update(true, 1000)));
    TEST_ASSERT_EQUAL(4, m.lastState());
    TEST_ASSERT_EQUAL(0, (int)f.langganan.size());
    TEST_ASSERT_EQUAL(0, (int)f.terbit.size());
}

void test_gagal_sambung_diulang_hanya_tiap_interval()
{
    FakeMqtt f;
    f.terima = false;
    MqttService m(f);
    m.begin(buatSettings());
    TEST_ASSERT_EQUAL(ev(MqttEvent::ConnectFailed), ev(m.update(true, 1000)));
    TEST_ASSERT_EQUAL(ev(MqttEvent::None), ev(m.update(true, 1000 + ULANG - 1)));
    TEST_ASSERT_EQUAL(1, f.connectCalls);
    TEST_ASSERT_EQUAL(ev(MqttEvent::ConnectFailed), ev(m.update(true, 1000 + ULANG)));
    TEST_ASSERT_EQUAL(2, f.connectCalls);
}

void test_saat_tersambung_loop_dipanggil_dan_heartbeat_tiap_interval()
{
    FakeMqtt f;
    MqttService m(f);
    m.begin(buatSettings());
    m.update(true, 1000);
    f.terbit.clear();

    m.update(true, 1000 + HB - 1);
    TEST_ASSERT_EQUAL(1, f.loopCalls);
    TEST_ASSERT_EQUAL(0, (int)f.terbit.size());

    m.update(true, 1000 + HB);
    TEST_ASSERT_EQUAL(1, (int)f.terbit.size());
    TEST_ASSERT_EQUAL_STRING(config::topicStatus, f.terbit[0].topic.c_str());
    TEST_ASSERT_EQUAL_STRING("ONLINE", f.terbit[0].payload.c_str());

    m.update(true, 1000 + 2 * HB - 1);
    TEST_ASSERT_EQUAL(1, (int)f.terbit.size());
    m.update(true, 1000 + 2 * HB);
    TEST_ASSERT_EQUAL(2, (int)f.terbit.size());
}

void test_jaringan_putus_memutus_koneksi_hanya_sekali()
{
    FakeMqtt f;
    MqttService m(f);
    m.begin(buatSettings());
    m.update(true, 1000);
    m.update(false, 2000);
    TEST_ASSERT_EQUAL(1, f.disconnectCalls);
    m.update(false, 3000);
    TEST_ASSERT_EQUAL(1, f.disconnectCalls);
    TEST_ASSERT_EQUAL(0, f.loopCalls);
}

void test_publishOnConnect_urutan_angka_mode_lalu_ip_semua_retained()
{
    FakeMqtt f;
    MqttService m(f);
    m.begin(buatSettings());
    m.publishOnConnect(7, 9, false, "192.168.1.5");
    TEST_ASSERT_EQUAL(4, (int)f.terbit.size());
    TEST_ASSERT_EQUAL_STRING(config::topicJumlah, f.terbit[0].topic.c_str());
    TEST_ASSERT_EQUAL_STRING("7", f.terbit[0].payload.c_str());
    TEST_ASSERT_EQUAL_STRING(config::topicJumlah2, f.terbit[1].topic.c_str());
    TEST_ASSERT_EQUAL_STRING("9", f.terbit[1].payload.c_str());
    TEST_ASSERT_EQUAL_STRING(config::topicMode, f.terbit[2].topic.c_str());
    TEST_ASSERT_EQUAL_STRING("OFF", f.terbit[2].payload.c_str());
    TEST_ASSERT_EQUAL_STRING(config::topicIP, f.terbit[3].topic.c_str());
    TEST_ASSERT_EQUAL_STRING("192.168.1.5", f.terbit[3].payload.c_str());
    for (const Terbit &t : f.terbit)
        TEST_ASSERT_TRUE(t.retain);
}

void test_publishChange_tanpa_ack_hanya_mengirim_angka()
{
    FakeMqtt f;
    MqttService m(f);
    m.begin(buatSettings());
    m.publishChange(3, 4, true, nullptr);
    TEST_ASSERT_EQUAL(2, (int)f.terbit.size());
    TEST_ASSERT_EQUAL_STRING(config::topicJumlah, f.terbit[0].topic.c_str());
    TEST_ASSERT_EQUAL_STRING(config::topicJumlah2, f.terbit[1].topic.c_str());
}

void test_publishChange_dengan_ack_angka_dulu_lalu_mode_lalu_ack_tidak_retained()
{
    FakeMqtt f;
    MqttService m(f);
    m.begin(buatSettings());
    m.publishChange(0, 0, true, "START_OK");
    TEST_ASSERT_EQUAL(4, (int)f.terbit.size());
    TEST_ASSERT_EQUAL_STRING(config::topicJumlah, f.terbit[0].topic.c_str());
    TEST_ASSERT_EQUAL_STRING(config::topicJumlah2, f.terbit[1].topic.c_str());
    TEST_ASSERT_EQUAL_STRING(config::topicMode, f.terbit[2].topic.c_str());
    TEST_ASSERT_EQUAL_STRING("ON", f.terbit[2].payload.c_str());
    TEST_ASSERT_EQUAL_STRING(config::topicAck, f.terbit[3].topic.c_str());
    TEST_ASSERT_EQUAL_STRING("START_OK", f.terbit[3].payload.c_str());
    TEST_ASSERT_FALSE(f.terbit[3].retain);
}

void test_perintah_masuk_diparse_dan_diteruskan_ke_handler()
{
    FakeMqtt f;
    MqttService m(f);
    m.begin(buatSettings());
    Command terakhir = Command::Unknown;
    int panggilan = 0;
    m.onCommand([&](Command c)
                { terakhir = c; panggilan++; });

    f.kirim(config::topicPerintah, "START\n");
    TEST_ASSERT_EQUAL((int)Command::Start, (int)terakhir);
    f.kirim(config::topicPerintah, "HALO");
    TEST_ASSERT_EQUAL((int)Command::Unknown, (int)terakhir);
    TEST_ASSERT_EQUAL(2, panggilan);
}

void test_perintah_masuk_tanpa_handler_tidak_crash()
{
    FakeMqtt f;
    MqttService m(f);
    m.begin(buatSettings());
    f.kirim(config::topicPerintah, "START");
    TEST_ASSERT_TRUE(true);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_jaringan_offline_tidak_menyambung);
    RUN_TEST(test_online_langsung_mencoba_sambung_dan_subscribe_dan_kirim_status);
    RUN_TEST(test_user_kosong_dikirim_null_dan_user_ada_dikirim_apa_adanya);
    RUN_TEST(test_last_will_mati_tanpa_will_dan_hidup_dengan_offline_retained);
    RUN_TEST(test_gagal_sambung_melapor_kode_tanpa_subscribe_dan_publish);
    RUN_TEST(test_gagal_sambung_diulang_hanya_tiap_interval);
    RUN_TEST(test_saat_tersambung_loop_dipanggil_dan_heartbeat_tiap_interval);
    RUN_TEST(test_jaringan_putus_memutus_koneksi_hanya_sekali);
    RUN_TEST(test_publishOnConnect_urutan_angka_mode_lalu_ip_semua_retained);
    RUN_TEST(test_publishChange_tanpa_ack_hanya_mengirim_angka);
    RUN_TEST(test_publishChange_dengan_ack_angka_dulu_lalu_mode_lalu_ack_tidak_retained);
    RUN_TEST(test_perintah_masuk_diparse_dan_diteruskan_ke_handler);
    RUN_TEST(test_perintah_masuk_tanpa_handler_tidak_crash);
    return UNITY_END();
}