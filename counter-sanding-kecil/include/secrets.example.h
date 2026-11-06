#pragma once
#include <stdint.h>

/*
 * File: secrets.example.h
 * Description: Contoh file header untuk menyimpan informasi sensitif seperti alama server MQTT dan kredensial.
 * Catatan: Jangan pernah mengunggah file ini ke repositori publik. Gunakan file ini sebagai template dan buat salinan dengan nama secrets.h untuk menyimpan informasi sensitif Anda.
 */

const char *mqtt_server = "alamat-broker.contoh"; // Isi sesuai dengan alamat broker MQTT yang digunakan
const uint16_t mqtt_port = 1883;                  // Port default untuk MQTT
const char *mqtt_user = "";                       // Jika broker MQTT memerlukan autentikasi, isi dengan username yang sesuai
const char *mqtt_pass = "";                       // Jika broker MQTT memerlukan autentikasi, isi dengan password yang sesuai