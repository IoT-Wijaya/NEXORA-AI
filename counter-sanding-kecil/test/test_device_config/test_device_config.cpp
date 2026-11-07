#include <stdio.h>
#include <string.h>
#include <map>
#include <string>
#include <unity.h>
#include "device_config.h"

class FakeStore : public IConfigStore
{
public:
    bool getString(const char *key, char *out, size_t size) override
    {
        auto it = data.find(key);
        if (it == data.end())
            return false;
        snprintf(out, size, "%s", it->second.c_str());
        return true;
    }
    bool setString(const char *key, const char *value) override
    {
        if (gagalTulis)
            return false;
        data[key] = value;
        return true;
    }
    bool erase(const char *key) override
    {
        data.erase(key);
        return true;
    }
    bool eraseAll() override
    {
        data.clear();
        return true;
    }

    std::map<std::string, std::string> data;
    bool gagalTulis = false;
};

static FakeStore store;

static bool ditolak(ConfigManager &m, const char *kunci, const char *nilai)
{
    return m.set(kunci, nilai) != nullptr && store.data.find(kunci) == store.data.end();
}

void setUp() { store = FakeStore(); }
void tearDown() {}

void test_load_kosong_memakai_nilai_bawaan()
{
    ConfigManager m(store, "SANDINGKCL", 1883);
    DeviceConfig c;
    TEST_ASSERT_EQUAL(0, m.load(c));
    TEST_ASSERT_EQUAL_STRING("", c.mqttHost);
    TEST_ASSERT_EQUAL_UINT16(1883, c.mqttPort);
    TEST_ASSERT_EQUAL_STRING("", c.mqttUser);
    TEST_ASSERT_EQUAL_STRING("", c.mqttPass);
    TEST_ASSERT_EQUAL_STRING("SANDINGKCL", c.machineName);
    TEST_ASSERT_FALSE(ConfigManager::isComplete(c));
}

void test_set_lalu_load_mengembalikan_semua_nilai()
{
    ConfigManager m(store, "SANDINGKCL", 1883);
    TEST_ASSERT_NULL(m.set(ConfigKeys::MQTT_HOST, "mqtt.contoh.id"));
    TEST_ASSERT_NULL(m.set(ConfigKeys::MQTT_PORT, "2883"));
    TEST_ASSERT_NULL(m.set(ConfigKeys::MQTT_USER, "dian"));
    TEST_ASSERT_NULL(m.set(ConfigKeys::MQTT_PASS, "kata sandi 1!"));
    TEST_ASSERT_NULL(m.set(ConfigKeys::MACHINE_NAME, "SANDING2"));
    DeviceConfig c;
    TEST_ASSERT_EQUAL(0, m.load(c));
    TEST_ASSERT_EQUAL_STRING("mqtt.contoh.id", c.mqttHost);
    TEST_ASSERT_EQUAL_UINT16(2883, c.mqttPort);
    TEST_ASSERT_EQUAL_STRING("dian", c.mqttUser);
    TEST_ASSERT_EQUAL_STRING("kata sandi 1!", c.mqttPass);
    TEST_ASSERT_EQUAL_STRING("SANDING2", c.machineName);
    TEST_ASSERT_TRUE(ConfigManager::isComplete(c));
}

void test_host_valid_diterima()
{
    ConfigManager m(store, "X", 1883);
    TEST_ASSERT_NULL(m.set(ConfigKeys::MQTT_HOST, "192.168.1.10"));
    TEST_ASSERT_NULL(m.set(ConfigKeys::MQTT_HOST, "mqtt.contoh.id"));
    TEST_ASSERT_NULL(m.set(ConfigKeys::MQTT_HOST, std::string(63, 'a').c_str()));
}

void test_host_tidak_valid_ditolak_dan_penyimpanan_tidak_berubah()
{
    ConfigManager m(store, "X", 1883);
    const char *buruk[] = {"", "host name", "a/b", "http://x.id", "a_b", "caf\xC3\xA9"};
    for (const char *h : buruk)
        TEST_ASSERT_TRUE(ditolak(m, ConfigKeys::MQTT_HOST, h));
    TEST_ASSERT_TRUE(ditolak(m, ConfigKeys::MQTT_HOST, std::string(64, 'a').c_str()));
}

void test_port_valid_diterima()
{
    ConfigManager m(store, "X", 1883);
    TEST_ASSERT_NULL(m.set(ConfigKeys::MQTT_PORT, "1"));
    TEST_ASSERT_NULL(m.set(ConfigKeys::MQTT_PORT, "1883"));
    TEST_ASSERT_NULL(m.set(ConfigKeys::MQTT_PORT, "65535"));
}

void test_port_tidak_valid_ditolak()
{
    ConfigManager m(store, "X", 1883);
    const char *buruk[] = {"", "0", "65536", "99999", "abc", "-1", "18 83", "123456", "1e3"};
    for (const char *p : buruk)
        TEST_ASSERT_TRUE(ditolak(m, ConfigKeys::MQTT_PORT, p));
}

void test_user_batas_dan_karakter()
{
    ConfigManager m(store, "X", 1883);
    TEST_ASSERT_NULL(m.set(ConfigKeys::MQTT_USER, "dian"));
    TEST_ASSERT_NULL(m.set(ConfigKeys::MQTT_USER, std::string(31, 'u').c_str()));
    store = FakeStore();
    TEST_ASSERT_TRUE(ditolak(m, ConfigKeys::MQTT_USER, ""));
    TEST_ASSERT_TRUE(ditolak(m, ConfigKeys::MQTT_USER, std::string(32, 'u').c_str()));
    TEST_ASSERT_TRUE(ditolak(m, ConfigKeys::MQTT_USER, "a b"));
    TEST_ASSERT_TRUE(ditolak(m, ConfigKeys::MQTT_USER, "a\tb"));
}

void test_sandi_batas_dan_karakter()
{
    ConfigManager m(store, "X", 1883);
    TEST_ASSERT_NULL(m.set(ConfigKeys::MQTT_PASS, "kata sandi 123!"));
    TEST_ASSERT_NULL(m.set(ConfigKeys::MQTT_PASS, std::string(63, 'p').c_str()));
    store = FakeStore();
    TEST_ASSERT_TRUE(ditolak(m, ConfigKeys::MQTT_PASS, ""));
    TEST_ASSERT_TRUE(ditolak(m, ConfigKeys::MQTT_PASS, std::string(64, 'p').c_str()));
    TEST_ASSERT_TRUE(ditolak(m, ConfigKeys::MQTT_PASS, "ab\tc"));
    TEST_ASSERT_TRUE(ditolak(m, ConfigKeys::MQTT_PASS, "caf\xC3\xA9"));
}

void test_nama_mesin_batas_dan_spasi_ujung()
{
    ConfigManager m(store, "X", 1883);
    TEST_ASSERT_NULL(m.set(ConfigKeys::MACHINE_NAME, "SANDINGKCL"));
    TEST_ASSERT_NULL(m.set(ConfigKeys::MACHINE_NAME, "A"));
    TEST_ASSERT_NULL(m.set(ConfigKeys::MACHINE_NAME, "SANDING 2"));
    store = FakeStore();
    TEST_ASSERT_TRUE(ditolak(m, ConfigKeys::MACHINE_NAME, ""));
    TEST_ASSERT_TRUE(ditolak(m, ConfigKeys::MACHINE_NAME, "ABCDEFGHIJK"));
    TEST_ASSERT_TRUE(ditolak(m, ConfigKeys::MACHINE_NAME, " AB"));
    TEST_ASSERT_TRUE(ditolak(m, ConfigKeys::MACHINE_NAME, "AB "));
    TEST_ASSERT_TRUE(ditolak(m, ConfigKeys::MACHINE_NAME, "caf\xC3\xA9"));
}

void test_kunci_tidak_dikenal_ditolak()
{
    ConfigManager m(store, "X", 1883);
    TEST_ASSERT_NOT_NULL(m.set("foo", "bar"));
    TEST_ASSERT_NOT_NULL(m.unset("foo"));
    TEST_ASSERT_NOT_NULL(m.set(nullptr, "bar"));
    TEST_ASSERT_TRUE(store.data.empty());
}

void test_unset_mengembalikan_ke_bawaan()
{
    ConfigManager m(store, "X", 1883);
    m.set(ConfigKeys::MQTT_PORT, "2000");
    DeviceConfig c;
    m.load(c);
    TEST_ASSERT_EQUAL_UINT16(2000, c.mqttPort);
    TEST_ASSERT_NULL(m.unset(ConfigKeys::MQTT_PORT));
    m.load(c);
    TEST_ASSERT_EQUAL_UINT16(1883, c.mqttPort);
}

void test_set_satu_kunci_tidak_mengubah_kunci_lain()
{
    ConfigManager m(store, "X", 1883);
    m.set(ConfigKeys::MQTT_HOST, "a.id");
    m.set(ConfigKeys::MQTT_USER, "dian");
    m.set(ConfigKeys::MQTT_USER, "budi");
    DeviceConfig c;
    m.load(c);
    TEST_ASSERT_EQUAL_STRING("a.id", c.mqttHost);
    TEST_ASSERT_EQUAL_STRING("budi", c.mqttUser);
}

void test_clearAll_mengosongkan_semuanya()
{
    ConfigManager m(store, "X", 1883);
    m.set(ConfigKeys::MQTT_HOST, "a.id");
    TEST_ASSERT_TRUE(m.clearAll());
    DeviceConfig c;
    m.load(c);
    TEST_ASSERT_FALSE(ConfigManager::isComplete(c));
}

void test_nilai_tersimpan_rusak_dihitung_dan_diganti_bawaan()
{
    ConfigManager m(store, "BAWAAN", 1883);
    store.data["mqtt_host"] = "broker.id";
    store.data["mqtt_port"] = "abc";
    store.data["mach_name"] = "NAMA YANG TERLALU PANJANG";
    DeviceConfig c;
    TEST_ASSERT_EQUAL(2, m.load(c));
    TEST_ASSERT_EQUAL_STRING("broker.id", c.mqttHost);
    TEST_ASSERT_EQUAL_UINT16(1883, c.mqttPort);
    TEST_ASSERT_EQUAL_STRING("BAWAAN", c.machineName);
}

void test_gagal_menulis_dilaporkan_sebagai_galat()
{
    ConfigManager m(store, "X", 1883);
    store.gagalTulis = true;
    TEST_ASSERT_NOT_NULL(m.set(ConfigKeys::MQTT_HOST, "a.id"));
    TEST_ASSERT_TRUE(store.data.empty());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_load_kosong_memakai_nilai_bawaan);
    RUN_TEST(test_set_lalu_load_mengembalikan_semua_nilai);
    RUN_TEST(test_host_valid_diterima);
    RUN_TEST(test_host_tidak_valid_ditolak_dan_penyimpanan_tidak_berubah);
    RUN_TEST(test_port_valid_diterima);
    RUN_TEST(test_port_tidak_valid_ditolak);
    RUN_TEST(test_user_batas_dan_karakter);
    RUN_TEST(test_sandi_batas_dan_karakter);
    RUN_TEST(test_nama_mesin_batas_dan_spasi_ujung);
    RUN_TEST(test_kunci_tidak_dikenal_ditolak);
    RUN_TEST(test_unset_mengembalikan_ke_bawaan);
    RUN_TEST(test_set_satu_kunci_tidak_mengubah_kunci_lain);
    RUN_TEST(test_clearAll_mengosongkan_semuanya);
    RUN_TEST(test_nilai_tersimpan_rusak_dihitung_dan_diganti_bawaan);
    RUN_TEST(test_gagal_menulis_dilaporkan_sebagai_galat);
    return UNITY_END();
}