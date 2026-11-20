# Struktur Direktori Proyek

```text
counter-sanding-kecil/
+---.vscode
|       c_cpp_properties.json
|       extensions.json
|       launch.json
|
+---docs
|       DIRECTORY_STRUCTURE.md
|       README.md
|
+---include
|       build_info.h
|       ota_public_key.h
|       README
|       sanding_config.h
|
+---lib
|   |   README
|   |
|   +---app
|   +---command_parser
|   |       command_parser.cpp
|   |       command_parser.h
|   |
|   +---console_shell
|   |       console_shell.cpp
|   |       console_shell.h
|   |
|   +---counter_store
|   |       counter_store.cpp
|   |       counter_store.h
|   |       ilock.h
|   |
|   +---device_config
|   |       device_config.cpp
|   |       device_config.h
|   |       iconfig_store.h
|   |
|   +---device_identity
|   |       device_identity.cpp
|   |       device_identity.h
|   |       imac_source.h
|   |
|   +---firmware_info
|   |       firmware_info.cpp
|   |       firmware_info.h
|   |
|   +---http_client
|   |       buffer_sink.h
|   |       firmware_downloader.cpp
|   |       firmware_downloader.h
|   |       http_response.cpp
|   |       http_response.h
|   |       ota_flash_sink.cpp
|   |       ota_flash_sink.h
|   |
|   +---lcd_view
|   |       ilcd_display.h
|   |       lcd_view.cpp
|   |       lcd_view.h
|   |
|   +---monocypher
|   |       monocypher-ed25519.c
|   |       monocypher-ed25519.h
|   |       monocypher.c
|   |       monocypher.h
|   |
|   +---mqtt_service
|   |       imqtt_client.h
|   |       mqtt_service.cpp
|   |       mqtt_service.h
|   |
|   +---net_link
|   |       inet_driver.h
|   |       net_monitor.cpp
|   |       net_monitor.h
|   |
|   +---ota_core
|   |       blob_ota_store.cpp
|   |       blob_ota_store.h
|   |       firmware_verifier.cpp
|   |       firmware_verifier.h
|   |       iblob_store.h
|   |       iboot_slots.h
|   |       iclock.h
|   |       iota_flash.h
|   |       manifest.cpp
|   |       manifest.h
|   |       ota_record.h
|   |       ota_record_codec.cpp
|   |       ota_record_codec.h
|   |       ota_reporter.cpp
|   |       ota_reporter.h
|   |       ota_state_machine.cpp
|   |       ota_state_machine.h
|   |       rollback.cpp
|   |       rollback.h
|   |       server_proof.h
|   |       sha256.cpp
|   |       sha256.h
|   |       signature.cpp
|   |       signature.h
|   |       signed_manifest.cpp
|   |       signed_manifest.h
|   |       update_policy.cpp
|   |       update_policy.h
|   |
|   +---ota_runner
|   |       ota_command.cpp
|   |       ota_command.h
|   |       ota_runner.cpp
|   |       ota_runner.h
|   |
|   +---sensor_filter
|   |       sensor_filter.cpp
|   |       sensor_filter.h
|   |
|   \---serial_router
|           line_router.cpp
|           line_router.h
|
+---src
|       arduino_clock.h
|       enc_driver.h
|       esp_boot_slots.h
|       esp_mac_source.h
|       esp_ota_flash.h
|       esp_spin_lock.h
|       esp_task_watchdog.h
|       ethernet_http_transport.h
|       i2c_lcd_display.h
|       main.cpp
|       nvs_blob_store.h
|       nvs_config_store.h
|       partition_log.h
|       pubsub_mqtt_client.h
|
+---test
|   |   README
|   |
|   +---test_blob_ota_store
|   |       test_blob_ota_store.cpp
|   |
|   +---test_buffer_sink
|   |       test_buffer_sink.cpp
|   |
|   +---test_command_parser
|   |       test_command_parser.cpp
|   |
|   +---test_config
|   |       test_config.cpp
|   |
|   +---test_console_shell
|   |       test_console_shell.cpp
|   |
|   +---test_counter_store
|   |       test_counter_store.cpp
|   |
|   +---test_device_config
|   |       test_device_config.cpp
|   |
|   +---test_device_identity
|   |       test_device_identity.cpp
|   |
|   +---test_firmware_downloader
|   |       test_firmware_downloader.cpp
|   |
|   +---test_firmware_info
|   |       test_firmware_info.cpp
|   |
|   +---test_firmware_verifier
|   |       test_firmware_verifier.cpp
|   |
|   +---test_http_response
|   |       test_http_response.cpp
|   |
|   +---test_lcd_view
|   |       test_lcd_view.cpp
|   |
|   +---test_line_router
|   |       test_line_router.cpp
|   |
|   +---test_manifest
|   |       test_manifest.cpp
|   |
|   +---test_mqtt_service
|   |       test_mqtt_service.cpp
|   |
|   +---test_net_monitor
|   |       test_net_monitor.cpp
|   |
|   +---test_ota_command
|   |       test_ota_command.cpp
|   |
|   +---test_ota_flash_sink
|   |       test_ota_flash_sink.cpp
|   |
|   +---test_ota_record_codec
|   |       test_ota_record_codec.cpp
|   |
|   +---test_ota_reporter
|   |       test_ota_reporter.cpp
|   |
|   +---test_ota_reset_berulang
|   |       test_ota_reset_berulang.cpp
|   |
|   +---test_ota_runner
|   |       test_ota_runner.cpp
|   |
|   +---test_ota_state_machine
|   |       test_ota_state_machine.cpp
|   |
|   +---test_rollback
|   |       test_rollback.cpp
|   |
|   +---test_sensor_filter
|   |       tes_sensor_filter.cpp
|   |
|   +---test_server_proof
|   |       test_server_proof.cpp
|   |
|   +---test_sha256
|   |       test_sha256.cpp
|   |
|   +---test_signature
|   |       test_signature.cpp
|   |
|   +---test_signed_manifest
|   |       test_signed_manifest.cpp
|   |
|   +---test_smoke
|   |       test_smoke.cpp
|   |
|   \---test_update_policy
|           test_update_policy.cpp
|
+---tools
|       ci_rilis.py
|       make_manifest.py
|       make_public_key.py
|       pasang_di_vps.sh
|       sign_manifest.sh
|       test_ci_rilis.py
|       test_make_manifest.py
|       test_pasang_di_vps.sh
|       version.py
|
        .gitignore
        CLAUDE.md
        manifest.json
        manifest.signed
        platformio.ini
```
