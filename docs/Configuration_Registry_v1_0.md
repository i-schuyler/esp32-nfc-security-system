ARTIFACT — config_registry.md (Configuration Registry v1.0)

# Configuration Registry (V1)

This doc defines all user-adjustable parameters, their meaning, ranges, defaults, and risk notes.

## 1) Rules

- Every config key has:
  - name
  - type
  - units (if applicable)
  - safe default
  - min/max
  - description
  - risk notes
- Config changes are logged (`config_change`), without leaking secrets.
- Config includes a `schema_version` field.

## 2) Proposed Parameters (Initial Set)

### System
- `setup_wizard_completed` (bool, default false)
- `setup_wizard_required` (bool, default true)
- `factory_restore_enabled` (bool, default true)
- `control_web_enabled` (bool, default true)
- `control_nfc_enabled` (bool, default true)
- `device_name` (string) — human-friendly name
- `timezone` (string) — for UI display
- `admin_mode_timeout_s` (int, default 600)
- `admin_web_password_hash` (secret string, default empty) — web-based Admin Config Mode password hash (set during Setup Wizard)


### Wi‑Fi
- `wifi_sta_enabled` (bool)
- `wifi_sta_ssid` (secret string)
- `wifi_sta_password` (secret string)
- `wifi_ap_ssid` (string, default `Workshop Security System - A1B2`)
- `wifi_ap_password` (secret string, default derived if unset/too short: `ChangeMe-<device_suffix>`; temporary provisioning only; must be changed during setup)
- `wifi_sta_connect_timeout_s` (int, default 20)

### NFC / Access
- `allow_user_arm` (bool, default true)
- `allow_user_disarm` (bool, default true)
- `allow_user_silence` (bool, default true)
- `invalid_scan_window_s` (int, default 30)
- `invalid_scan_max` (int, default 5)
- `lockout_duration_s` (int, default 60)

### Alarm Outputs
- `silenced_duration_s` (int, default 180)
- `horn_enabled` (bool, default true)
- `light_enabled` (bool, default true)
- `horn_pattern` (enum, default steady)
- `light_pattern` (enum, default steady)
- `silenced_light_pattern` (enum, default steady)

### Sensors
- `required_primary_sensor` (enum: motion|door, default motion)
- `motion_sensors_max` (int, fixed 2 in V1)
- `door_sensors_max` (int, fixed 2 in V1)
- `enclosure_open_enabled` (bool, default false)
- `motion_enabled` (bool, default true)
- `motion1_enabled` (bool, default true) — per-sensor enable (append-only; derived from `motion_enabled` if missing)
- `motion2_enabled` (bool, default false) — per-sensor enable
- `motion_sensitivity` (range TBD based on LD2410)
- `motion_kind` (enum: gpio|ld2410b_uart, default gpio)
- `motion_ld2410b_rx_gpio` (int, default 16)
- `motion_ld2410b_tx_gpio` (int, default 17)
- `motion_ld2410b_baud` (int, default 256000)
- `door_enabled` (bool, default false)
- `door1_enabled` (bool, default false) — per-sensor enable (append-only; derived from `door_enabled` if missing)
- `door2_enabled` (bool, default false) — per-sensor enable
- `tamper_enabled` (bool, default false)
- `armed_present_mode_enabled` (bool, default false)

M5 adds a minimal, generic GPIO-digital interpretation layer. These keys are only used when a corresponding sensor input pin is configured at build time (pin map is otherwise TBD).

- `motion1_pull` (enum: pullup|pulldown|floating, default floating)
- `motion1_active_level` (enum: high|low, default high)
- `motion2_pull` (enum: pullup|pulldown|floating, default floating)
- `motion2_active_level` (enum: high|low, default high)

- `door1_pull` (enum: pullup|pulldown|floating, default pullup)
- `door1_active_level` (enum: high|low, default high)
- `door2_pull` (enum: pullup|pulldown|floating, default pullup)
- `door2_active_level` (enum: high|low, default high)

- `enclosure1_pull` (enum: pullup|pulldown|floating, default pullup)
- `enclosure1_active_level` (enum: high|low, default high)

### Storage
- `sd_required` (bool, default false) — if true, missing SD triggers FAULT or TRIGGERED per policy
- `log_retention_days` (int, default 365, min 7, max 3650)
- `hash_chain_logs` (bool, default true)

### Power (if implemented)
- `battery_measure_enabled` (bool)
- `battery_low_v` (float)
- `battery_critical_v` (float)
- `battery_wifi_disable_v` (float)

## 3) Parameters Needing Hardware Confirmation

- Any LD2410 sensitivity parameters
- Battery voltage thresholds (if measuring)
- Output pattern timing bounds (to avoid nuisance)


## 4) Setup Wizard + Factory Restore Keys (V1)

### Setup
- `setup_completed` (bool, default false)
- `setup_wizard_version` (int, default 1)
- `setup_last_step` (string, optional)

### Factory restore policy
- `factory_restore_wipes_logs` (bool, default false) — if true, SD logs are deleted; otherwise preserved
- `factory_restore_wipes_allowlist` (bool, default true)
- `factory_restore_requires_hold` (bool, default true)

### NFC optional URL record
- `nfc_url_record_enabled` (bool, default false)
- `nfc_url_record_preserve_if_possible` (bool, default true)
- `nfc_url` (string, default auto: AP=`http://192.168.4.1/`; STA/LAN=admin-supplied)

### Default AP SSID
- `wifi_ap_ssid_base` (string, default "Workshop Security System")
- `wifi_ap_suffix_enabled` (bool, default true)
