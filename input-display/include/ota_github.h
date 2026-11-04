#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// Versi firmware, diisi saat build dari tag rilis (lihat catatan di bawah).

static const char *GH_REPO = "IoT-Wijaya/NEXORA-AI";
static const char *TAG_PREFIX = "input-display";
static const char *TOPIK_OTA = "pabrik/hmi_sanding/ota";

extern PubSubClient mqtt;   // SESUAIKAN: objek MQTT Anda
bool bolehUpdateSekarang(); // SESUAIKAN: true bila mesin sedang idle

// ---------- Laporan progres ke server ----------
static void otaLapor(const char *state, int pct = -1, const char *msg = "")
{
    JsonDocument d;
    d["state"] = state;
    if (pct >= 0)
        d["pct"] = pct;
    if (msg[0])
        d["msg"] = msg;

    char buf[200];
    serializeJson(d, buf, sizeof(buf));
    mqtt.publish(TOPIK_OTA, buf, false); // retain = false
    mqtt.loop();
}

// ---------- Bantu: versi ----------
// "input-display-v2026.11.1.1" -> "2026.11.1.1"
static String ekstrakVersi(const String &tag)
{
    for (unsigned i = 0; i < tag.length(); i++)
    {
        if (isDigit(tag[i]))
            return tag.substring(i);
    }
    return "";
}

// true bila a lebih baru dari b. "dev" / kosong dianggap 0 (selalu usang).
static bool lebihBaru(String a, String b)
{
    a.replace("v", "");
    b.replace("v", "");
    for (int i = 0; i < 6; i++)
    {
        int pa = a.toInt(), pb = b.toInt(); // toInt("dev") = 0
        if (pa != pb)
            return pa > pb;
        int da = a.indexOf('.'), db = b.indexOf('.');
        a = (da < 0) ? "" : a.substring(da + 1);
        b = (db < 0) ? "" : b.substring(db + 1);
        if (a == "" && b == "")
            break;
    }
    return false;
}

// ---------- Cari rilis terbaru milik input-display ----------
static bool cariRilis(String &tag, String &urlBin)
{
    WiFiClientSecure client;
    client.setInsecure(); // lihat catatan keamanan di bawah

    HTTPClient http;
    http.useHTTP10(true); // hindari chunked, supaya stream bisa dibaca ArduinoJson
    http.begin(client, String("https://api.github.com/repos/") + GH_REPO + "/releases?per_page=10");
    http.addHeader("Accept", "application/vnd.github+json");
    http.addHeader("User-Agent", "NEXORA-HMI");

    if (http.GET() != 200)
    {
        http.end();
        return false;
    }

    // Filter: simpan hanya field yang dibutuhkan (respons GitHub sangat besar).
    JsonDocument filter;
    filter[0]["tag_name"] = true;
    filter[0]["draft"] = true;
    filter[0]["prerelease"] = true;
    filter[0]["assets"][0]["name"] = true;
    filter[0]["assets"][0]["browser_download_url"] = true;

    JsonDocument doc;
    DeserializationError err =
        deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (err)
        return false;

    // GitHub mengurutkan terbaru dulu: ambil rilis resmi pertama milik produk ini.
    for (JsonObject r : doc.as<JsonArray>())
    {
        if (r["draft"] | false || r["prerelease"] | false)
            continue;

        String t = r["tag_name"] | "";
        if (!t.startsWith(TAG_PREFIX))
            continue;

        for (JsonObject a : r["assets"].as<JsonArray>())
        {
            String nama = a["name"] | "";
            if (nama.endsWith(".bin"))
            {
                tag = t;
                urlBin = String(a["browser_download_url"] | "");
                return true;
            }
        }
    }
    return false;
}

// ---------- Proses utama ----------
static void otaGagal(const char *msg)
{
    Update.abort();
    otaLapor("failed", -1, msg);
}

void otaJalankan()
{
    otaLapor("checking", -1, "Mencari rilis di GitHub");

    if (!bolehUpdateSekarang())
    {
        otaLapor("failed", -1, "Mesin sedang bekerja, coba lagi saat idle");
        return;
    }

    String tag, url;
    if (!cariRilis(tag, url))
    {
        otaLapor("failed", -1, "Rilis input-display atau file .bin tidak ditemukan");
        return;
    }

    if (!lebihBaru(ekstrakVersi(tag), FW_VERSION))
    {
        otaLapor("uptodate", -1, "Firmware sudah terbaru");
        return;
    }

    // ---- Unduh dan tulis ke flash ----
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS); // asset GitHub selalu redirect
    http.begin(client, url);
    http.addHeader("User-Agent", "NEXORA-HMI");

    if (http.GET() != 200)
    {
        http.end();
        otaLapor("failed", -1, "Gagal mengunduh dari GitHub");
        return;
    }

    int total = http.getSize();
    if (total <= 0 || !Update.begin(total))
    {
        http.end();
        otaGagal("Ukuran firmware tidak valid atau flash tidak cukup");
        return;
    }

    WiFiClient *stream = http.getStreamPtr();
    uint8_t buf[1024];
    int done = 0, lastPct = -5;
    uint32_t terakhirData = millis();

    while (done < total)
    {
        size_t tersedia = stream->available();
        if (tersedia)
        {
            int n = stream->readBytes(buf, min(tersedia, sizeof(buf)));
            if (Update.write(buf, n) != (size_t)n)
            {
                http.end();
                otaGagal(Update.errorString());
                return;
            }
            done += n;
            terakhirData = millis();

            int pct = (int)((int64_t)done * 100 / total);
            if (pct >= lastPct + 5 || pct == 100)
            { // lapor tiap 5%, bukan tiap byte
                lastPct = pct;
                otaLapor("downloading", pct, "Mengunduh firmware");
            }
        }
        else
        {
            if (millis() - terakhirData > 15000)
            { // 15 detik tanpa data = putus
                http.end();
                otaGagal("Koneksi unduhan terputus");
                return;
            }
            delay(1);
        }
        mqtt.loop(); // jaga koneksi MQTT tetap hidup
    }
    http.end();

    if (!Update.end(true))
    { // true = validasi ukuran/checksum image
        otaGagal(Update.errorString());
        return;
    }

    otaLapor("installing", 100, "Terpasang, HMI akan restart");
    for (int i = 0; i < 10; i++)
    {
        mqtt.loop();
        delay(50);
    } // pastikan pesan terkirim
    ESP.restart();
}