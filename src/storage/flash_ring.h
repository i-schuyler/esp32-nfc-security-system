// src/storage/flash_ring.h
// Role: Small persistent log fallback ring buffer stored in NVS (Preferences).
#pragma once

#include <Arduino.h>

class WssFlashRing {
public:
  bool begin();
  void clear();

  // Append a line to the ring. Lines are truncated to a safe max.
  bool append(const String& line);

  // Export up to `max_items` most-recent entries, newest-last.
  size_t read_recent(String* out, size_t max_items);

  uint32_t count() const { return _count; }

private:
  static const uint32_t kSlots = 40;
  static const uint32_t kMaxLine = 240;

  bool _ok = false;
  uint32_t _head = 0;
  uint32_t _count = 0;

  void load_meta();
  void save_meta();
  String key_for(uint32_t idx) const;
};
