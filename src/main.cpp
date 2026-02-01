// src/main.cpp
#include <Arduino.h>
#include <WiFi.h>

#include "version.h"
#include "diagnostics.h"
#include "flash_fs.h"
#include "web_server.h"

#include "logging/event_logger.h"
#include "config/config_store.h"
#include "wifi/wifi_manager.h"

// M2: time + storage tiers
#include "storage/time_manager.h"
#include "storage/storage_manager.h"

// M4: explicit state machine + outputs
#include "state_machine/state_machine.h"
#include "outputs/output_manager.h"

// M5: sensors abstraction (per-sensor enable/disable + trigger routing)
#include "sensors/sensor_manager.h"

// M6: NFC health + scan events (slice 0)
#include "nfc/nfc_manager.h"

static WssEventLogger g_log;
static WssConfigStore g_cfg;
static String g_last_applied_state = "";

void setup() {
  Serial.begin(115200);
  delay(200);

  auto boot = wss_get_boot_info();

  Serial.println();
  Serial.println("[WSS] Boot");
  Serial.print("[WSS] Firmware: "); Serial.print(WSS_FIRMWARE_NAME); Serial.print(" "); Serial.println(WSS_FIRMWARE_VERSION);
  Serial.print("[WSS] Reset reason: "); Serial.println(boot.reset_reason);
  Serial.print("[WSS] Device suffix: "); Serial.println(boot.chip_id_suffix);

  g_log.begin();
  {
    StaticJsonDocument<256> extra;
    extra["reset_reason"] = boot.reset_reason;
    extra["firmware"] = WSS_FIRMWARE_VERSION;
    JsonObjectConst o = extra.as<JsonObjectConst>();
    g_log.log_info("core", "boot", "boot", &o);
  }

  // Flash FS (LittleFS)
#if WSS_FEATURE_WEB
  bool fs_ok = wss_flash_fs_begin();
  Serial.print("[WSS] Flash FS: "); Serial.println(fs_ok ? "OK" : "FAIL");
#endif

  // ConfigStore (M1)
  String cfg_err;
  (void)cfg_err;
  g_cfg.begin(boot.chip_id_suffix, &g_log);

  // Time + storage (M2)
  wss_time_begin(&g_log);
  wss_storage_begin(&g_cfg, &g_log);

  // M4: outputs default OFF until state is known.
  wss_outputs_begin(&g_cfg, &g_log);

  // M4: state machine (loads persisted state; defaults DISARMED if none).
  wss_state_begin(&g_cfg, &g_log);

  // M5: sensors (safe when pins are unset; reports unconfigured status).
  wss_sensors_begin(&g_cfg, &g_log);

  // M6 slice 0: NFC health + scan events only.
  wss_nfc_begin(&g_cfg, &g_log);

  // Apply outputs for initial state after state is known.
  g_last_applied_state = wss_state_status().state;
  wss_outputs_apply_state(g_last_applied_state);

  // Wi‑Fi (M1: STA attempt if configured, else AP)
  wss_wifi_begin(g_cfg, boot.chip_id_suffix, g_log);

#if WSS_FEATURE_WEB
  wss_web_begin(g_cfg, g_log);
  Serial.println("[WSS] Web server: OK");
#endif
}

void loop() {
#if WSS_FEATURE_RTC
  wss_time_loop();
#endif
  wss_state_loop();
  wss_sensors_loop();
  wss_nfc_loop();
  // Apply outputs on state changes (deterministic, no silent transitions).
  {
    String s = wss_state_status().state;
    if (s != g_last_applied_state) {
      g_last_applied_state = s;
      wss_outputs_apply_state(s);
    }
  }
  wss_outputs_loop();
  wss_storage_loop();
#if WSS_FEATURE_WEB
  wss_web_loop();
#endif
  delay(5);
}
