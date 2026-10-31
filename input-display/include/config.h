#pragma once

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif
#define DEVICE_TYPE "input-display" // harus sama dengan nama folder

static const char *mqttServer = "103.27.206.23";
static const int mqttPort = 1883;
static const char *serverIp = "192.168.1.24:3000"; // Ini bisa diganti menjadi nama domain, misal "sanding.example.com:8000"
static const char *sandingType = "...";