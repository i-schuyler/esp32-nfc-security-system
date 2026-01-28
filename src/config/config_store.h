// src/config/config_store.h
// Role: Persistent configuration store (schema-versioned, migration-aware) for V1.
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

class WssEventLogger;

// ConfigStore contract (M1):
// - Stores an append-only JSON document in NVS (Preferences)
// - Enforces schema_version and provides a migration hook for v1.x
// - Supports redaction for secrets in logs and API responses
// - Corrupt/invalid storage recovery resets to defaults + requires Setup Wizard
class WssConfigStore {
 public:
  bool begin(const String& device_suffix, WssEventLogger* logger);

  // Returns true if configuration is valid and loaded.
  bool ok() const { return _ok; }

  // Setup wizard gating
  bool setup_completed() const;
  String setup_last_step() const;

  // Admin web gate
  bool admin_password_set() const;
  bool verify_admin_password(const String& candidate) const;

  // Read-only view (internal use)
  JsonDocument& doc() { return _doc; }
  const JsonDocument& doc() const { return _doc; }

  // Returns the stable device suffix used in default SSID formatting.
  const String& device_suffix() const { return _device_suffix; }

  // Produces a redacted config view suitable for API.
  // Secrets are replaced with "***".
  void to_redacted_json(JsonDocument& out) const;

  // Applies a patch (partial JSON object) to config.
  // - Only updates known keys.
  // - Never logs secret values.
  // Returns true if something changed.
  bool apply_patch(const JsonObjectConst& patch, String& err, JsonArray changed_keys_out);

  // Wizard helper: set a key regardless of "known keys" list (still type-checked).
  bool wizard_set_variant(const char* key, const JsonVariantConst& value, String& err);
  bool wizard_set(const char* key, const char* value, String& err);
  bool wizard_set(const char* key, const String& value, String& err);
  bool wizard_set(const char* key, bool value, String& err);

  // Persists config to NVS.
  bool save(String& err);

  // Clears config to defaults and persists.
  bool factory_reset(String& err);

  // Ensures derived defaults are present (e.g., AP SSID format).
  void ensure_runtime_defaults();

 private:
  bool load(String& err);
  void set_defaults();
  bool validate_or_recover(String& err);

  // v1.x migration framework.
  bool migrate_if_needed(uint32_t from_version, uint32_t to_version, String& err);

  // Secret handling helpers
  bool is_secret_key(const String& key) const;
  static String sha256_hex(const String& s);

  bool _ok = false;
  String _device_suffix;
  WssEventLogger* _logger = nullptr;

  DynamicJsonDocument _doc{4096};
};
