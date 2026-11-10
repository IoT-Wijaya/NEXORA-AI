#pragma once
#include <esp_err.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include "iota_flash.h"

// Adaptor perangkat: IOtaFlash di atas API OTA bawaan ESP-IDF.
// Sengaja TIDAK memakai Update.h: Update.end() langsung mengalihkan slot boot,
// sedangkan kita harus menulis status Pending ke NVS lebih dulu.
class EspOtaFlash : public IOtaFlash
{
public:
    bool begin(uint32_t imageSize) override
    {
        partition_ = esp_ota_get_next_update_partition(nullptr);
        if (partition_ == nullptr)
            return fail("slot OTA berikut tidak ada");
        if (esp_ota_begin(partition_, imageSize, &handle_) != ESP_OK)
            return fail("esp_ota_begin gagal");
        open_ = true;
        return true;
    }

    bool write(const uint8_t *data, size_t len) override
    {
        if (!open_)
            return fail("write tanpa begin");
        if (esp_ota_write(handle_, data, len) != ESP_OK)
            return fail("esp_ota_write gagal");
        return true;
    }

    bool end() override
    {
        if (!open_)
            return fail("end tanpa begin");
        open_ = false; // esp_ota_end melepas handle apa pun hasilnya
        if (esp_ota_end(handle_) != ESP_OK)
            return fail("image tidak valid (esp_ota_end)");
        validated_ = true;
        return true;
    }

    void abort() override
    {
        if (open_)
        {
            esp_ota_abort(handle_);
            open_ = false;
        }
    }

    bool setBootToNew() override
    {
        if (!validated_)
            return fail("setBoot sebelum end() berhasil");
        if (esp_ota_set_boot_partition(partition_) != ESP_OK)
            return fail("esp_ota_set_boot_partition gagal");
        return true;
    }

    const char *lastError() const override { return error_; }

private:
    bool fail(const char *text)
    {
        error_ = text;
        return false;
    }

    const esp_partition_t *partition_ = nullptr;
    esp_ota_handle_t handle_ = 0;
    bool open_ = false;
    bool validated_ = false;
    const char *error_ = "";
};