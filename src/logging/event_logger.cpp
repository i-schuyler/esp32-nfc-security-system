// src/logging/event_logger.cpp
// Role: Minimal in-memory JSONL event logger for early milestones (M1+).

#include "event_logger.h"

#include <Preferences.h>
#include <time.h>

#include "../storage/storage_manager.h"

static const char* kPrefsNamespace = "wss";
static const char* kPrefsKeySeq = "event_seq";

bool WssEventLogger::begin() {
  // Ensure sequence key exists.
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    // No persistence, but logger still works.
    return false;
  }
  (void)prefs.getULong(kPrefsKeySeq, 0);
  prefs.end();
  return true;
}

uint32_t WssEventLogger::reserve_seq() {
  return next_seq();
}

uint32_t WssEventLogger::next_seq() {
  Preferences prefs;
  uint32_t seq = 0;
  if (prefs.begin(kPrefsNamespace, false)) {
    seq = prefs.getULong(kPrefsKeySeq, 0);
    seq++;
    prefs.putULong(kPrefsKeySeq, seq);
    prefs.end();
  } else {
    // Best effort in RAM only.
    static uint32_t fallback = 0;
    seq = ++fallback;
  }
  return seq;
}

String WssEventLogger::iso8601_now(bool& time_valid) const {
  time_t now = time(nullptr);
  // If time hasn't been set, ESP32 time will often be near epoch.
  time_valid = (now > 1700000000);
  struct tm tm_utc;
  gmtime_r(&now, &tm_utc);
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
  return String(buf);
}

void WssEventLogger::log_internal(const char* severity, const char* source, const char* event_type,
                                  const String& msg, const JsonObjectConst* extra) {
  StaticJsonDocument<512> d;
  bool time_valid = false;
  d["ts"] = iso8601_now(time_valid);
  d["seq"] = next_seq();
  d["event_type"] = event_type;
  d["severity"] = severity;
  d["source"] = source;
  d["msg"] = msg;
  if (!time_valid) d["time_valid"] = false;
  if (extra) {
    // Copy allowed extra fields (caller-owned). Never pass secrets.
    for (JsonPairConst kv : *extra) {
      d[kv.key()] = kv.value();
    }
  }

  String line;
  serializeJson(d, line);

  _events[_head] = line;
  _head = (_head + 1) % kMaxEvents;
  if (_count < kMaxEvents) _count++;

  // Serial is useful during bring-up; no secrets should reach here.
  Serial.println(line);

  // Persist to active backend (SD preferred, flash ring fallback). Best-effort.
  (void)wss_storage_append_line(line);
}

void WssEventLogger::log_debug(const char* source, const char* event_type, const String& msg, const JsonObjectConst* extra) {
  log_internal("debug", source, event_type, msg, extra);
}
void WssEventLogger::log_info(const char* source, const char* event_type, const String& msg, const JsonObjectConst* extra) {
  log_internal("info", source, event_type, msg, extra);
}
void WssEventLogger::log_warn(const char* source, const char* event_type, const String& msg, const JsonObjectConst* extra) {
  log_internal("warn", source, event_type, msg, extra);
}
void WssEventLogger::log_error(const char* source, const char* event_type, const String& msg, const JsonObjectConst* extra) {
  log_internal("error", source, event_type, msg, extra);
}

void WssEventLogger::log_config_change(const char* source, const JsonArrayConst& changed_keys) {
  StaticJsonDocument<256> extra;
  JsonArray arr = extra.createNestedArray("keys");
  for (JsonVariantConst v : changed_keys) {
    arr.add(v);
  }
  JsonObjectConst o = extra.as<JsonObjectConst>();
  log_internal("info", source, "config_change", "config keys updated", &o);
}

void WssEventLogger::recent_events(JsonDocument& out, size_t limit) const {
  out.clear();
  JsonArray arr = out.to<JsonArray>();
  if (_count == 0) return;

  size_t n = _count;
  if (limit > 0 && limit < n) n = limit;

  // Oldest first.
  size_t start = (_head + kMaxEvents - _count) % kMaxEvents;
  for (size_t i = 0; i < n; i++) {
    size_t idx = (start + i) % kMaxEvents;
    StaticJsonDocument<512> line;
    DeserializationError de = deserializeJson(line, _events[idx]);
    if (!de) arr.add(line);
  }
}
