#include <stdio.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>
#include <unity.h>
#include "console_shell.h"

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
};

// Satu set peralatan uji: penyimpanan palsu, penampung keluaran, penghitung reboot.
struct Uji
{
    FakeStore store;
    ConfigManager mgr;
    std::vector<std::string> keluar;
    int reboot = 0;
    ConsoleShell sh;

    Uji()
        : mgr(store, "SANDINGKCL", 1883),
          sh(mgr, [this](const char *l)
             { keluar.push_back(l); }, [this]()
             { reboot++; }) {}

    void ketik(const char *teks)
    {
        for (const char *p = teks; *p; ++p)
            sh.feed(*p);
    }
    void baris(const char *teks)
    {
        ketik(teks);
        sh.feed('\n');
    }
    bool memuat(const char *potongan) const
    {
        for (const std::string &l : keluar)
            if (l.find(potongan) != std::string::npos)
                return true;
        return false;
    }
};

void setUp() {}
void tearDown() {}

void test_help_mencantumkan_semua_perintah()
{
    Uji u;
    u.baris("help");
    TEST_ASSERT_TRUE(u.memuat("show"));
    TEST_ASSERT_TRUE(u.memuat("set <kunci>"));
    TEST_ASSERT_TRUE(u.memuat("unset"));
    TEST_ASSERT_TRUE(u.memuat("clear yes"));
    TEST_ASSERT_TRUE(u.memuat("reboot"));
}

void test_set_menyimpan_nilai_dan_menjawab_ok()
{
    Uji u;
    u.baris("set mqtt_host broker.contoh.id");
    TEST_ASSERT_EQUAL_STRING("broker.contoh.id", u.store.data["mqtt_host"].c_str());
    TEST_ASSERT_TRUE(u.memuat("OK"));
}

void test_set_nilai_dengan_spasi_diambil_utuh()
{
    Uji u;
    u.baris("set mqtt_pass abc def");
    TEST_ASSERT_EQUAL_STRING("abc def", u.store.data["mqtt_pass"].c_str());
}

void test_spasi_berlebih_dan_huruf_besar_pada_perintah_ditoleransi()
{
    Uji u;
    u.baris("  SET   mqtt_host    broker.id   ");
    TEST_ASSERT_EQUAL_STRING("broker.id", u.store.data["mqtt_host"].c_str());
}

void test_show_menyamarkan_sandi()
{
    Uji u;
    u.baris("set mqtt_pass rahasia123");
    u.keluar.clear();
    u.baris("show");
    TEST_ASSERT_TRUE(u.memuat("****"));
    TEST_ASSERT_FALSE(u.memuat("rahasia123"));
}

void test_show_tanpa_isi_menandai_host_belum_diisi()
{
    Uji u;
    u.baris("show");
    TEST_ASSERT_TRUE(u.memuat("(belum diisi)"));
    TEST_ASSERT_TRUE(u.memuat("1883"));
    TEST_ASSERT_TRUE(u.memuat("SANDINGKCL"));
}

void test_sandi_tidak_pernah_muncul_di_keluaran_apa_pun()
{
    Uji u;
    u.baris("set mqtt_pass rahasia123");
    u.baris("show");
    u.baris("set mqtt_pass");
    u.baris("unset mqtt_pass");
    u.baris("help");
    TEST_ASSERT_FALSE(u.memuat("rahasia123"));
}

void test_nilai_tidak_valid_ditolak_dengan_galat()
{
    Uji u;
    u.baris("set mqtt_port 99999");
    TEST_ASSERT_TRUE(u.memuat("Galat"));
    TEST_ASSERT_TRUE(u.store.data.empty());
}

void test_set_tanpa_nilai_dan_tanpa_argumen()
{
    Uji u;
    u.baris("set mqtt_host");
    TEST_ASSERT_TRUE(u.memuat("unset"));
    u.keluar.clear();
    u.baris("set");
    TEST_ASSERT_TRUE(u.memuat("Pemakaian"));
    TEST_ASSERT_TRUE(u.store.data.empty());
}

void test_set_kunci_tidak_dikenal()
{
    Uji u;
    u.baris("set foo bar");
    TEST_ASSERT_TRUE(u.memuat("Galat"));
    TEST_ASSERT_TRUE(u.store.data.empty());
}

void test_unset_menghapus_nilai()
{
    Uji u;
    u.baris("set mqtt_port 2000");
    u.baris("unset mqtt_port");
    TEST_ASSERT_TRUE(u.store.data.find("mqtt_port") == u.store.data.end());
}

void test_clear_tanpa_yes_hanya_meminta_konfirmasi()
{
    Uji u;
    u.baris("set mqtt_host a.id");
    u.baris("clear");
    TEST_ASSERT_TRUE(u.memuat("clear yes"));
    TEST_ASSERT_FALSE(u.store.data.empty());
}

void test_clear_yes_menghapus_semua()
{
    Uji u;
    u.baris("set mqtt_host a.id");
    u.baris("clear yes");
    TEST_ASSERT_TRUE(u.store.data.empty());
}

void test_reboot_memanggil_tindakan_sekali()
{
    Uji u;
    u.baris("reboot");
    TEST_ASSERT_EQUAL(1, u.reboot);
}

void test_perintah_tidak_dikenal()
{
    Uji u;
    u.baris("halo");
    TEST_ASSERT_TRUE(u.memuat("tidak dikenal"));
}

void test_akhir_baris_crlf_tidak_menjalankan_perintah_dua_kali()
{
    Uji u;
    u.ketik("reboot\r\n");
    TEST_ASSERT_EQUAL(1, u.reboot);
}

void test_baris_kosong_diabaikan()
{
    Uji u;
    u.ketik("\n\n\r\n");
    TEST_ASSERT_TRUE(u.keluar.empty());
}

void test_baris_terlalu_panjang_dilaporkan_lalu_konsol_pulih()
{
    Uji u;
    u.ketik(std::string(200, 'a').c_str());
    u.sh.feed('\n');
    TEST_ASSERT_TRUE(u.memuat("terlalu panjang"));
    u.baris("reboot");
    TEST_ASSERT_EQUAL(1, u.reboot);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_help_mencantumkan_semua_perintah);
    RUN_TEST(test_set_menyimpan_nilai_dan_menjawab_ok);
    RUN_TEST(test_set_nilai_dengan_spasi_diambil_utuh);
    RUN_TEST(test_spasi_berlebih_dan_huruf_besar_pada_perintah_ditoleransi);
    RUN_TEST(test_show_menyamarkan_sandi);
    RUN_TEST(test_show_tanpa_isi_menandai_host_belum_diisi);
    RUN_TEST(test_sandi_tidak_pernah_muncul_di_keluaran_apa_pun);
    RUN_TEST(test_nilai_tidak_valid_ditolak_dengan_galat);
    RUN_TEST(test_set_tanpa_nilai_dan_tanpa_argumen);
    RUN_TEST(test_set_kunci_tidak_dikenal);
    RUN_TEST(test_unset_menghapus_nilai);
    RUN_TEST(test_clear_tanpa_yes_hanya_meminta_konfirmasi);
    RUN_TEST(test_clear_yes_menghapus_semua);
    RUN_TEST(test_reboot_memanggil_tindakan_sekali);
    RUN_TEST(test_perintah_tidak_dikenal);
    RUN_TEST(test_akhir_baris_crlf_tidak_menjalankan_perintah_dua_kali);
    RUN_TEST(test_baris_kosong_diabaikan);
    RUN_TEST(test_baris_terlalu_panjang_dilaporkan_lalu_konsol_pulih);
    return UNITY_END();
}