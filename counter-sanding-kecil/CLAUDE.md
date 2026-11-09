# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an ESP32-based industrial counter device called "SANDING KECIL" (Sand Grinder Small) built using PlatformIO with Arduino framework. The device counts two types of events: "hasil" (output) and "modal" (input) using physical sensors connected to an ESP32 NodeMCU-32S board.

The architecture is divided into two main cores:
- **Core 0**: Sensor task that reads physical inputs and updates counters
- **Core 1**: Network task that handles MQTT communication, LCD display, and coordination

## Build and Development Commands

### Building and Upload
- **Build**: `platformio run --target upload` (builds and uploads to device)
- **Build only**: `platformio run` (builds firmware)
- **Upload only**: `platformio upload`
- **Clean build**: `platformio run --target clean`

### Testing
- **Run all tests**: `platformio test` (executes unit tests in native environment)
- **Run specific test**: `platformio test -f test_<component_name>`
- **Run single test**: For individual unit tests, navigate to the specific test directory
- **Test list**: `platformio test --list` (shows available tests)

### PlatformIO Management
- **Initialize**: `platformio init --board nodemcu-32s --framework arduino`
- **Update**: `platformio update` (updates dependencies)
- **Check status**: `platformio status`

### VS Code Integration
- **Open with PlatformIO**: Use the "PlatformIO: Initialize Project" command
- **Debug**: Configure launch.json in .vscode for GDB debugging
- **Code analysis**: Use VS Code's IntelliSense with the provided C/C++ settings

## Code Architecture

### Core Structure
- **src/main.cpp**: Entry point with FreeRTOS tasks setup
- **include/:** Header files with configuration constants and type definitions
- **lib/:** PlatformIO library dependencies (PubSubClient, LiquidCrystal_I2C, etc.)
- **test/:** Unit test directories for each component

### Key Components

#### 1. Configuration Management
- **sanding_config.h**: Main configuration constants (MQTT topics, pin assignments, timing intervals)
- **device_config.h**: Runtime device configuration (MQTT host, user, password)
- **build_info.h**: Firmware build information

#### 2. Hardware Drivers
- **enc_driver.h**: ENC28J60 Ethernet controller driver
- **i2c_lcd_display.h**: I2C-based LCD display driver
- **esp_mac_source.h**: ESP32 MAC address source

#### 3. State Management
- **counter_store.h**: Thread-safe counter storage with mutex protection
- **nvs_config_store.h**: Non-volatile storage for persistent configuration
- **nvs_blob_store.h**: Blob storage for OTA state

#### 4. Communication
- **pubsub_mqtt_client.h**: MQTT client wrapper
- **net_monitor.h**: Network monitoring (Ethernet/DHCP)
- **mqtt_service.h**: MQTT service with command handling

#### 5. Services
- **console_shell.h**: Serial console command interface
- **sensor_filter.h**: Hardware debounce filtering for sensors
- **lcd_view.h**: LCD display state management

### Task Architecture

#### Sensor Task (Core 0)
- Runs at priority 2
- Reads physical sensors with debounce filtering
- Updates counters when enabled
- Handles local reset button
- Stack: 4KB

#### Network Task (Core 1)  
- Runs at priority 1
- Manages MQTT connection and communication
- Controls LCD display updates
- Coordinates OTA operations
- Stack: 8KB

### OTA System
- **ota_state_machine.h**: State management for firmware updates
- **blob_ota_store.h**: Binary storage for firmware images
- **firmware_info.h**: Version and build information management

## Development Guidelines

### Code Style
- Use C++11 features
- Include guards in header files
- `#pragma once` instead of include guards
- Namespace `config` for configuration constants
- Thread safety using `EspSpinLock` mutexes

### Testing
- Use PlatformIO's Unity test framework
- Each component has dedicated unit tests
- Tests run in native environment (not on device)
- Test naming: `test_<component_name>/` directory

### Configuration
- Constants defined in `sanding_config.h` as `constexpr`
- Runtime config in `DeviceConfig` structure
- NVS persistence for device settings
- Build-time configuration via compiler flags

### File Organization
- **Main**: `src/main.cpp`
- **Headers**: `include/*.h`
- **Drivers**: `src/*.h` (hardware-specific)
- **Tests**: `test/<component>/test.cpp`

## Common Development Tasks

### Adding New Sensors
1. Add pin definition to `sanding_config.h`
2. Add initialization in `setup()` in main.cpp
3. Add filtering logic in `sensorTask()`
4. Add tests in `test_sensor_filter/`

### Adding New MQTT Commands
1. Add command enum in `mqtt_service.h`
2. Add case handling in `handleCommand()` in main.cpp
3. Update console shell if needed
4. Add tests in `test_mqtt_service/`

### Debugging
1. Use PlatformIO's monitor: `platformio device monitor`
2. Set breakpoints in VS Code GDB
3. Check serial output at 115200 baud
4. Use `platformio run --target clean` for rebuilds

## Important Files to Review

1. **src/main.cpp**: Complete task architecture and main flow
2. **include/sanding_config.h**: All hardware and timing constants
3. **platformio.ini**: PlatformIO configuration
4. **test/README**: PlatformIO testing overview
5. **tools/version.py**: Version management script

## Project Notes

- Firmware version: fw-v1.0.0
- Device type: sanding-kecil
- Build target: nodemcu-32s
- No existing CLAUDE.md file - this is the initial version