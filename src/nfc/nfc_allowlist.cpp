// src/nfc/nfc_allowlist.cpp
// Role: NFC allowlist storage interface (stub in M1).

#include "nfc_allowlist.h"

#include "../logging/event_logger.h"

void wss_nfc_allowlist_factory_reset(WssEventLogger& log) {
  // M1 stub: nothing to clear yet, but make behavior observable.
  log.log_info("nfc", "allowlist_factory_reset_stub", "allowlist reset (stub in M1)");
}
