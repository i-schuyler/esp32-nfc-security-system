// src/storage/flash_ring.cpp
// Role: Small persistent log fallback ring buffer stored in NVS (Preferences).

#include "flash_ring.h"

#include <Preferences.h>

static const char* kPrefsNamespace = "wss_log";
static const char* kKeyHead = "head";
static const char* kKeyCount = "count";
static Preferences g_prefs;

String WssFlashRing::key_for(uint32_t idx) const {
  return String("e") + String(idx);
}

bool WssFlashRing::begin() {
  if (!g_prefs.begin(kPrefsNamespace, false)) {
    _ok = false;
    return false;
  }
  _ok = true;
  load_meta();
  return true;
}

void WssFlashRing::load_meta() {
  if (!_ok) return;
  _head = g_prefs.getUInt(kKeyHead, 0);
  _count = g_prefs.getUInt(kKeyCount, 0);
  if (_head >= kSlots) _head = 0;
  if (_count > kSlots) _count = kSlots;
}

void WssFlashRing::save_meta() {
  if (!_ok) return;
  g_prefs.putUInt(kKeyHead, _head);
  g_prefs.putUInt(kKeyCount, _count);
}

void WssFlashRing::clear() {
  if (!_ok) return;
  for (uint32_t i = 0; i < kSlots; ++i) {
    g_prefs.remove(key_for(i).c_str());
  }
  _head = 0;
  _count = 0;
  save_meta();
}

bool WssFlashRing::append(const String& line) {
  if (!_ok) return false;
  String trimmed = line;
  if (trimmed.length() > kMaxLine) {
    trimmed.remove(kMaxLine);
  }

  const uint32_t slot = _head % kSlots;
  bool ok = g_prefs.putString(key_for(slot).c_str(), trimmed) > 0;
  _head = (slot + 1) % kSlots;
  if (_count < kSlots) _count++;
  save_meta();
  return ok;
}

size_t WssFlashRing::read_recent(String* out, size_t max_items) {
  if (!_ok || !out || max_items == 0) return 0;
  uint32_t n = _count;
  if (n > max_items) n = max_items;

  // Oldest entry index = head - count (mod slots)
  int32_t start = (int32_t)_head - (int32_t)n;
  while (start < 0) start += (int32_t)kSlots;

  for (uint32_t i = 0; i < n; ++i) {
    uint32_t idx = (start + (int32_t)i) % kSlots;
    out[i] = g_prefs.getString(key_for(idx).c_str(), "");
  }
  return n;
}
