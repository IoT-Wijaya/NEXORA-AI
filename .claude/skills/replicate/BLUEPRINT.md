# BLUEPRINT — counter-sanding-kecil (reference gold standard)

Cached once per session; reused for replicating `input display`.

## Directory tree (names only, purpose per folder)

```
counter-sanding-kecil/
  src/main.cpp              # entry point: FreeRTOS tasks (sensor core 0, network core 1)
  src/*.h                   # hardware drivers (ESP32-specific: enc, i2c lcd, mac, ota flash, etc.)
  include/*.h               # configuration constants + type definitions (namespace config)
  lib/<module>/             # one library module per folder: .cpp + .h pair
    command_parser, console_shell, counter_store, device_config, device_identity,
    firmware_info, http_client, lcd_view, monocypher, mqtt_service, net_link,
    ota_core, ota_runner, sensor_filter, serial_router
  test/<module>/            # Unity unit test per module: test_<module>/test_<module>.cpp
  tools/                    # manifest signing/version tooling (make_manifest.py, sign_manifest.sh, version.py)
  partitions.csv            # flash partition layout
  platformio.ini            # PlatformIO build config (envs + deps)
  manifest.json             # signed firmware manifest metadata
  ota_publik.pem / ota_rahasia.pem  # OTA signing keys
  CLAUDE.md / docs/         # project guidance + structure doc
```

Skip when reading: `.pio/`, `.git/`, `node_modules`, images, binaries, lockfiles.

## Build / run flow

- `platformio run` — build only; `platformio run --target upload` — build + upload; `platformio test` — native Unity tests
- `setup()` in main.cpp: init config store → load DeviceConfig → identity (MAC/client ID) → LCD → Ethernet/DHCP → MQTT → watchdog → create two pinned tasks
- Core 0 `sensorTask`: filter sensors → increment counters when active → handle reset button (`vTaskDelay 10ms`)
- Core 1 `networkTask`: Ethernet poll → MQTT (connect/publish state/commands/OTA) → publish counter changes on dirty flag → LCD page advance → OTA health monitoring/reporting (`vTaskDelay 20ms`)
- Data moves: sensors → filter → counter store (dirty flag) → networkTask → MQTT publish + LCD render

## Naming conventions

- Files: `snake_case`, one module per `lib/<module>/` folder with `<module>.cpp` + `<module>.h`
- Headers: `#pragma once`, `#include <stdint.h>` first, then project includes
- Classes/types: `PascalCase` (CounterStore, SensorFilter, DeviceConfig, ConfigManager)
- Methods/functions: `camelCase` (incrementHasil, takeDirty, begin, update)
- Constants: `UPPER_SNAKE_CASE` inside `namespace config`
- Struct members / privates: `snake_case` with trailing underscore (hasil_, modal_, lock_)
- Mutex: `ILock &lock_` passed by reference; lock/unlock around every access
- Strings: `char` arrays, not `String` (avoid heap fragmentation)
- Comments: Indonesian, one-line `//` explanations above blocks

## Code style notes

- Include order: system `<...>`, then project `"..."`
- `static_assert` used to document size/safety constraints next to constants
- Error handling: return codes + warning Serial prints (no exceptions); `isComplete()`, `validate()`, `load()` returning invalid-count
- Test pattern (Unity): `setUp()`/`tearDown()`, `test_<deskripsi>()` functions, `RUN_TEST(...)`, `main()` with `UNITY_BEGIN()`/`UNITY_END()`, test fakes subclass the interface (e.g. `LockPencatat : public ILock`)
- Test naming dir: `test_<module>/`, file `test_<module>.cpp`

## How to add a new feature (checklist, files touched in order)

1. Add pin/timing/topic constants to `include/<module>_config.h` (namespace config)
2. Create `lib/<module>/<module>.h` + `<module>.cpp` (class, `#pragma once`, lock-safe if shared)
3. Wire instance + includes in `src/main.cpp`; init in `setup()`, use in tasks
4. Add MQTT command case in `handleCommand()` (if command) + console shell handler (if serial)
5. Add `test_<module>/test_<module>.cpp` (Unity, fake the lock/interface)
6. Verify: `platformio test -f test_<module>`
