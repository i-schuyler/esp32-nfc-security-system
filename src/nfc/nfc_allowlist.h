// src/nfc/nfc_allowlist.h
// Role: NFC allowlist storage interface (stub in M1).
#pragma once

class WssEventLogger;

// M1 stub: allowlist persistence and provisioning arrive in later milestones.
// This hook exists so Factory Restore can explicitly clear allowlist state.
void wss_nfc_allowlist_factory_reset(WssEventLogger& log);
