# Integration Points Map

Purpose:
- List the authoritative entrypoints so changes don’t create parallel stacks.

---

## Firmware entrypoints
- Main: `src/main.cpp`
- Web server: `src/web_server.*`
- State machine: `src/state_machine/state_machine.*`
- Config store: `src/config/config_store.*`
- Logging/event schema: `src/logging/event_logger.*` + `docs/Event_Log_Schema_v1_0.md`
- Storage manager: `src/storage/storage_manager.*`
- Outputs manager: `src/outputs/output_manager.*`

---

## 2026-01-28 — Integration updates (wizard/config)

- `WssConfigStore::doc()` now supports both const and non-const access (const-correctness across modules).
- Wizard write paths support typed inputs:
  - `wizard_set(const char* key, const char* value, String& err)`
  - `wizard_set(const char* key, const String& value, String& err)`
  - `wizard_set(const char* key, bool value, String& err)`
- JSON-variant wizard writes use `wizard_set_variant(...)` to avoid overload ambiguity.

