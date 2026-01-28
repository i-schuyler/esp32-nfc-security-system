# Integration Points Map

Purpose: A quick “where to plug in” map for core subsystems to reduce re-entry cost and prevent parallel stacks.

---

## Firmware entrypoints
- Main: `src/main.cpp`
- Web server: `src/web_server.{h,cpp}`
- State machine: `src/state_machine/state_machine.{h,cpp}`
- Config store: `src/config/config_store.{h,cpp}`
- Logging/event chain: `src/logging/event_logger.{h,cpp}` + `src/logging/sha256_hex.{h,cpp}`
- Outputs API: `src/outputs/output_manager.{h,cpp}`
- Storage manager + ring buffer: `src/storage/storage_manager.{h,cpp}` + `src/storage/flash_ring.{h,cpp}`
- Time manager: `src/storage/time_manager.{h,cpp}`
- Wi-Fi manager: `src/wifi/wifi_manager.{h,cpp}`
- Flash filesystem helpers: `src/flash_fs.{h,cpp}`
- Diagnostics: `src/diagnostics.{h,cpp}`

## Embedded UI assets
- LittleFS image source folder: `data/` (served from flash)

## Placeholder module folders (intentional)
- `src/ui/` exists as a placeholder for future UI module work (M7).
