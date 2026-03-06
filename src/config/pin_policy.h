// src/config/pin_policy.h
// Role: Board profile + pin policy contract for /api/status and validation.

#pragma once

#include <stddef.h>
#include <ArduinoJson.h>

struct WssPinPolicyRole {
  const char* key;
  int default_gpio;
  const int* allowed_gpios;
  size_t allowed_count;
  const char* note;
};

struct WssPinPolicy {
  const char* board_profile_id;
  const char* board_profile_name;
  const int* reserved_gpios;
  size_t reserved_count;
  const WssPinPolicyRole* roles;
  size_t role_count;
};

const WssPinPolicy& wss_pin_policy();
const WssPinPolicyRole* wss_pin_policy_role(const char* key);
bool wss_pin_policy_gpio_allowed(const char* role_key, int pin);
bool wss_pin_policy_gpio_reserved(int pin);
int wss_pin_policy_role_default_gpio(const char* role_key, int fallback);
const char* wss_pin_policy_default_nfc_interface();
void wss_pin_policy_write_status_json(JsonObject out);
