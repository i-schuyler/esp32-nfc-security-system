// src/logging/event_logger.h
// Role: Minimal in-memory JSONL event logger for early milestones (M1+).
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// NOTE: Persistent SD logging + hash chaining arrives in later milestones.
// In M1, this logger provides:
// - A monotonic sequence number (best-effort persisted in NVS)
// - A RAM ring buffer of recent events for `/api/events`
// - A single helper to guarantee "no secrets in logs" (values are never accepted here)

class WssEventLogger {
 public:
  bool begin();

  // Reserves a unique, monotonic sequence number for schema-correct log lines.
  // This is used by lower layers (e.g., storage) for internal header events.
  uint32_t reserve_seq();

  void log_debug(const char* source, const char* event_type, const String& msg, const JsonObjectConst* extra = nullptr);
  void log_info (const char* source, const char* event_type, const String& msg, const JsonObjectConst* extra = nullptr);
  void log_warn (const char* source, const char* event_type, const String& msg, const JsonObjectConst* extra = nullptr);
  void log_error(const char* source, const char* event_type, const String& msg, const JsonObjectConst* extra = nullptr);

  // Adds a config change event. Only key names are allowed.
  void log_config_change(const char* source, const JsonArrayConst& changed_keys);

  // Copies the last N events into `out` as a JSON array of objects.
  void recent_events(JsonDocument& out, size_t limit) const;

 private:
  void log_internal(const char* severity, const char* source, const char* event_type, const String& msg,
                    const JsonObjectConst* extra);

  String iso8601_now(bool& time_valid) const;
  uint32_t next_seq();

  static constexpr size_t kMaxEvents = 60;
  String _events[kMaxEvents];
  size_t _head = 0;
  size_t _count = 0;
};
