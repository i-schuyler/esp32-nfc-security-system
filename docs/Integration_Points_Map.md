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
