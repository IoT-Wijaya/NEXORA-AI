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
        if (!tolakTopic.empty() && tolakTopic == t)
            return false;
        return !tolakSemua;
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
    bool tolakSemua = false;
    std::string tolakTopic; // publish ke topic ini ditolak
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
    s.enabled = true;
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
    TEST_ASSERT_EQUAL(2, (int)f.langganan.size());
    TEST_ASSERT_EQUAL_STRING(config::topicPerintah, f.langganan[0].c_str());
    TEST_ASSERT_EQUAL_STRING(config::topicOta, f.langganan[1].c_str());
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

void test_tidak_aktif_tidak_pernah_menyambung()
{
    FakeMqtt f;
    MqttService m(f);
    MqttSettings s = buatSettings();
    s.enabled = false;
    m.begin(s);
    TEST_ASSERT_EQUAL(ev(MqttEvent::None), ev(m.update(true, 1000)));
    TEST_ASSERT_EQUAL(0, f.connectCalls);
}

void test_publishVersion_retained_ke_topic_versi()
{
    FakeMqtt f;
    MqttService m(f);
    m.begin(buatSettings());
    m.publishVersion("{\"version\":\"v1\",\"build\":1}");
    TEST_ASSERT_EQUAL(1, (int)f.terbit.size());
    TEST_ASSERT_EQUAL_STRING(config::topicVersi, f.terbit[0].topic.c_str());
    TEST_ASSERT_EQUAL_STRING("{\"version\":\"v1\",\"build\":1}", f.terbit[0].payload.c_str());
    TEST_ASSERT_TRUE(f.terbit[0].retain);
}

void test_publishOnConnect_true_bila_semua_publish_diterima()
{
    FakeMqtt f;
    MqttService m(f);
    m.begin(buatSettings());
    TEST_ASSERT_TRUE(m.publishOnConnect(7, 9, false, "192.168.1.5"));
    TEST_ASSERT_TRUE(m.publishOnConnect(7, 9, false, nullptr));
}

void test_publishOnConnect_false_bila_salah_satu_publish_ditolak_dan_sisanya_tetap_dikirim()
{
    const char *target[] = {config::topicJumlah, config::topicJumlah2, config::topicMode, config::topicIP};
    for (const char *topik : target)
    {
        FakeMqtt f;
        f.tolakTopic = topik;
        MqttService m(f);
        m.begin(buatSettings());
        TEST_ASSERT_FALSE_MESSAGE(m.publishOnConnect(7, 9, false, "192.168.1.5"), topik);
        TEST_ASSERT_EQUAL_MESSAGE(4, (int)f.terbit.size(), topik);
    }
}

void test_publishOnConnect_false_bila_semua_ditolak()
{
    FakeMqtt f;
    f.tolakSemua = true;
    MqttService m(f);
    m.begin(buatSettings());
    TEST_ASSERT_FALSE(m.publishOnConnect(1, 2, true, "1.2.3.4"));
}

void test_publishVersion_mengembalikan_hasil_publish()
{
    FakeMqtt f;
    MqttService m(f);
    m.begin(buatSettings());
    TEST_ASSERT_TRUE(m.publishVersion("{}"));
    f.tolakSemua = true;
    TEST_ASSERT_FALSE(m.publishVersion("{}"));
}

struct CatatanOta
{
    int panggilan = 0;
    OtaMessage terakhir = OtaMessage::Malformed;
    uint32_t build = 0;
};

static void pasangOta(MqttService &m, CatatanOta &c)
{
    m.onOtaCommand([&c](OtaMessage pesan, uint32_t build)
                   { c.panggilan++; c.terakhir = pesan; c.build = build; });
}

static const uint32_t HOLDOFF = config::MQTT_OTA_HOLDOFF_MS;

void test_ota_sah_setelah_holdoff_diteruskan_dengan_build()
{
    FakeMqtt f;
    MqttService m(f);
    m.begin(buatSettings());
    CatatanOta c;
    pasangOta(m, c);
    m.update(true, 1000);           // tersambung
    m.update(true, 1000 + HOLDOFF); // waktu berjalan
    f.kirim(config::topicOta, "ota 7");
    TEST_ASSERT_EQUAL(1, c.panggilan);
    TEST_ASSERT_EQUAL((int)OtaMessage::Valid, (int)c.terakhir);
    TEST_ASSERT_EQUAL_UINT32(7, c.build);
}

void test_ota_terlalu_dekat_dengan_sambung_dilaporkan_tooearly()
{
    FakeMqtt f;
    MqttService m(f);
    m.begin(buatSettings());
    CatatanOta c;
    pasangOta(m, c);
    m.update(true, 1000);
    m.update(true, 1000 + HOLDOFF - 1);
    f.kirim(config::topicOta, "ota 7"); // mis. pesan retained yang dikirim broker
    TEST_ASSERT_EQUAL(1, c.panggilan);
    TEST_ASSERT_EQUAL((int)OtaMessage::TooEarly, (int)c.terakhir);
    TEST_ASSERT_EQUAL_UINT32(0, c.build);
}

void test_ota_holdoff_dihitung_ulang_pada_setiap_sambung()
{
    FakeMqtt f;
    MqttService m(f);
    m.begin(buatSettings());
    CatatanOta c;
    pasangOta(m, c);
    m.update(true, 1000);
    m.update(true, 1000 + HOLDOFF + 500);
    f.tersambung = false; // broker putus
    const uint32_t ulang = 1000 + HOLDOFF + 500 + ULANG;
    m.update(true, ulang); // sambung ulang
    f.kirim(config::topicOta, "ota 7");
    TEST_ASSERT_EQUAL((int)OtaMessage::TooEarly, (int)c.terakhir);
}

void test_ota_muatan_salah_dilaporkan_malformed_dan_tidak_membawa_build()
{
    FakeMqtt f;
    MqttService m(f);
    m.begin(buatSettings());
    CatatanOta c;
    pasangOta(m, c);
    m.update(true, 1000);
    m.update(true, 1000 + HOLDOFF);
    const char *buruk[] = {"ota", "ota 0", "ota abc", "START", "", "ota 5 6"};
    for (const char *s : buruk)
    {
        c.panggilan = 0;
        f.kirim(config::topicOta, s);
        TEST_ASSERT_EQUAL_MESSAGE(1, c.panggilan, s);
        TEST_ASSERT_EQUAL_MESSAGE((int)OtaMessage::Malformed, (int)c.terakhir, s);
        TEST_ASSERT_EQUAL_UINT32(0, c.build);
    }
}

void test_ota_tanpa_penangan_tidak_crash()
{
    FakeMqtt f;
    MqttService m(f);
    m.begin(buatSettings());
    m.update(true, 1000);
    f.kirim(config::topicOta, "ota 7");
    TEST_ASSERT_TRUE(true);
}

void test_pesan_ota_tidak_pernah_menjadi_perintah_biasa()
{
    FakeMqtt f;
    MqttService m(f);
    m.begin(buatSettings());
    int perintah = 0;
    m.onCommand([&](Command)
                { perintah++; });
    CatatanOta c;
    pasangOta(m, c);
    m.update(true, 1000);
    m.update(true, 1000 + HOLDOFF);
    f.kirim(config::topicOta, "ota 7");
    TEST_ASSERT_EQUAL(0, perintah);
}

void test_perintah_biasa_tidak_sampai_ke_penangan_ota()
{
    FakeMqtt f;
    MqttService m(f);
    m.begin(buatSettings());
    m.onCommand([](Command) {});
    CatatanOta c;
    pasangOta(m, c);
    m.update(true, 1000);
    m.update(true, 1000 + HOLDOFF);
    f.kirim(config::topicPerintah, "ota 7");
    TEST_ASSERT_EQUAL(0, c.panggilan);
}

void test_publishOtaReport_retained_ke_topic_status_ota_dan_mengembalikan_hasil()
{
    FakeMqtt f;
    MqttService m(f);
    m.begin(buatSettings());
    TEST_ASSERT_TRUE(m.publishOtaReport("{\"result\":\"confirmed\"}"));
    TEST_ASSERT_EQUAL(1, (int)f.terbit.size());
    TEST_ASSERT_EQUAL_STRING(config::topicOtaStatus, f.terbit[0].topic.c_str());
    TEST_ASSERT_EQUAL_STRING("{\"result\":\"confirmed\"}", f.terbit[0].payload.c_str());
    TEST_ASSERT_TRUE(f.terbit[0].retain);
    f.tolakSemua = true;
    TEST_ASSERT_FALSE(m.publishOtaReport("{}"));
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
    RUN_TEST(test_tidak_aktif_tidak_pernah_menyambung);
    RUN_TEST(test_publishVersion_retained_ke_topic_versi);
    RUN_TEST(test_publishOnConnect_true_bila_semua_publish_diterima);
    RUN_TEST(test_publishOnConnect_false_bila_salah_satu_publish_ditolak_dan_sisanya_tetap_dikirim);
    RUN_TEST(test_publishOnConnect_false_bila_semua_ditolak);
    RUN_TEST(test_publishVersion_mengembalikan_hasil_publish);
    RUN_TEST(test_ota_sah_setelah_holdoff_diteruskan_dengan_build);
    RUN_TEST(test_ota_terlalu_dekat_dengan_sambung_dilaporkan_tooearly);
    RUN_TEST(test_ota_holdoff_dihitung_ulang_pada_setiap_sambung);
    RUN_TEST(test_ota_muatan_salah_dilaporkan_malformed_dan_tidak_membawa_build);
    RUN_TEST(test_ota_tanpa_penangan_tidak_crash);
    RUN_TEST(test_pesan_ota_tidak_pernah_menjadi_perintah_biasa);
    RUN_TEST(test_perintah_biasa_tidak_sampai_ke_penangan_ota);
    RUN_TEST(test_publishOtaReport_retained_ke_topic_status_ota_dan_mengembalikan_hasil);
    return UNITY_END();
}