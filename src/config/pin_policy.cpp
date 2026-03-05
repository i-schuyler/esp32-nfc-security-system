// src/config/pin_policy.cpp
// Role: Board profile + pin policy contract for /api/status and validation.

#include "pin_policy.h"

#include <string.h>

#ifndef WSS_BOARD_PROFILE_ID
#define WSS_BOARD_PROFILE_ID "esp32_devkit_v1_wroom32"
#endif

#ifndef WSS_BOARD_PROFILE_NAME
#define WSS_BOARD_PROFILE_NAME "ESP32 DevKit V1 (ESP-WROOM-32)"
#endif

#ifndef WSS_BOARD_PROFILE_S3_N32R16V
#define WSS_BOARD_PROFILE_S3_N32R16V 0
#endif

#define WSS_ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

static const int kDevkitI2cSda[] = {21};
static const int kDevkitI2cScl[] = {22};
static const int kDevkitSpiSck[] = {18};
static const int kDevkitSpiMiso[] = {19};
static const int kDevkitSpiMosi[] = {23};
static const int kDevkitSdCs[] = {13, 16, 17, 25, 26, 27, 32, 33};
static const int kDevkitNfcSpiCs[] = {16, 17, 25, 26, 27, 32, 33};
static const int kDevkitNfcSpiIrq[] = {32, 33, 34, 35, 36, 39};
static const int kDevkitLd2410bRx[] = {4, 5, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
static const int kDevkitLd2410bTx[] = {4, 5, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};
static const int kDevkitInputGpios[] = {13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39};
static const int kDevkitOutputGpios[] = {13, 14, 16, 17, 25, 26, 27, 32, 33};

static const WssPinPolicyRole kDevkitRoles[] = {
  {"i2c.sda", 21, kDevkitI2cSda, WSS_ARRAY_LEN(kDevkitI2cSda), nullptr},
  {"i2c.scl", 22, kDevkitI2cScl, WSS_ARRAY_LEN(kDevkitI2cScl), nullptr},
  {"spi.sck", 18, kDevkitSpiSck, WSS_ARRAY_LEN(kDevkitSpiSck), nullptr},
  {"spi.miso", 19, kDevkitSpiMiso, WSS_ARRAY_LEN(kDevkitSpiMiso), nullptr},
  {"spi.mosi", 23, kDevkitSpiMosi, WSS_ARRAY_LEN(kDevkitSpiMosi), nullptr},
  {"sd.cs", 13, kDevkitSdCs, WSS_ARRAY_LEN(kDevkitSdCs), nullptr},
  {"nfc.spi_cs", 27, kDevkitNfcSpiCs, WSS_ARRAY_LEN(kDevkitNfcSpiCs), nullptr},
  {"nfc.spi_irq", 32, kDevkitNfcSpiIrq, WSS_ARRAY_LEN(kDevkitNfcSpiIrq), nullptr},
  {"nfc.uart_rx", 16, kDevkitLd2410bRx, WSS_ARRAY_LEN(kDevkitLd2410bRx), nullptr},
  {"nfc.uart_tx", 17, kDevkitLd2410bTx, WSS_ARRAY_LEN(kDevkitLd2410bTx), nullptr},
  {"ld2410b.rx", 16, kDevkitLd2410bRx, WSS_ARRAY_LEN(kDevkitLd2410bRx), nullptr},
  {"ld2410b.tx", 17, kDevkitLd2410bTx, WSS_ARRAY_LEN(kDevkitLd2410bTx), nullptr},
  {"inputs.gpio", -1, kDevkitInputGpios, WSS_ARRAY_LEN(kDevkitInputGpios), nullptr},
  {"outputs.gpio", -1, kDevkitOutputGpios, WSS_ARRAY_LEN(kDevkitOutputGpios), nullptr},
};

static const int kS3ReservedGpios[] = {19, 20, 33, 34, 35, 36, 37};
static const int kS3I2cSda[] = {8};
static const int kS3I2cScl[] = {9};
static const int kS3SpiSck[] = {12};
static const int kS3SpiMiso[] = {11};
static const int kS3SpiMosi[] = {10};
static const int kS3SdCs[] = {13};
static const int kS3NfcUartRx[] = {44};
static const int kS3NfcUartTx[] = {43};
static const int kS3GeneralGpios[] = {8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21};

static const WssPinPolicyRole kS3Roles[] = {
  {"i2c.sda", 8, kS3I2cSda, WSS_ARRAY_LEN(kS3I2cSda), nullptr},
  {"i2c.scl", 9, kS3I2cScl, WSS_ARRAY_LEN(kS3I2cScl), nullptr},
  {"spi.sck", 12, kS3SpiSck, WSS_ARRAY_LEN(kS3SpiSck), nullptr},
  {"spi.miso", 11, kS3SpiMiso, WSS_ARRAY_LEN(kS3SpiMiso), nullptr},
  {"spi.mosi", 10, kS3SpiMosi, WSS_ARRAY_LEN(kS3SpiMosi), nullptr},
  {"sd.cs", 13, kS3SdCs, WSS_ARRAY_LEN(kS3SdCs), nullptr},
  {"nfc.spi_cs", 14, kS3GeneralGpios, WSS_ARRAY_LEN(kS3GeneralGpios), nullptr},
  {"nfc.spi_irq", -1, kS3GeneralGpios, WSS_ARRAY_LEN(kS3GeneralGpios), nullptr},
  {"nfc.uart_rx", 44, kS3NfcUartRx, WSS_ARRAY_LEN(kS3NfcUartRx), nullptr},
  {"nfc.uart_tx", 43, kS3NfcUartTx, WSS_ARRAY_LEN(kS3NfcUartTx), nullptr},
  {"ld2410b.rx", 16, kS3GeneralGpios, WSS_ARRAY_LEN(kS3GeneralGpios), nullptr},
  {"ld2410b.tx", 17, kS3GeneralGpios, WSS_ARRAY_LEN(kS3GeneralGpios), nullptr},
  {"inputs.gpio", -1, kS3GeneralGpios, WSS_ARRAY_LEN(kS3GeneralGpios), nullptr},
  {"outputs.gpio", -1, kS3GeneralGpios, WSS_ARRAY_LEN(kS3GeneralGpios), nullptr},
};

const WssPinPolicy& wss_pin_policy() {
#if WSS_BOARD_PROFILE_S3_N32R16V
  static const WssPinPolicy policy = {
    WSS_BOARD_PROFILE_ID,
    WSS_BOARD_PROFILE_NAME,
    kS3ReservedGpios,
    WSS_ARRAY_LEN(kS3ReservedGpios),
    kS3Roles,
    WSS_ARRAY_LEN(kS3Roles),
  };
  return policy;
#else
  static const WssPinPolicy policy = {
    WSS_BOARD_PROFILE_ID,
    WSS_BOARD_PROFILE_NAME,
    nullptr,
    0,
    kDevkitRoles,
    WSS_ARRAY_LEN(kDevkitRoles),
  };
  return policy;
#endif
}

const WssPinPolicyRole* wss_pin_policy_role(const char* key) {
  if (!key) return nullptr;
  const WssPinPolicy& policy = wss_pin_policy();
  for (size_t i = 0; i < policy.role_count; i++) {
    if (strcmp(policy.roles[i].key, key) == 0) return &policy.roles[i];
  }
  return nullptr;
}

bool wss_pin_policy_gpio_allowed(const char* role_key, int pin) {
  if (pin < 0) return true;
  if (wss_pin_policy_gpio_reserved(pin)) return false;
  const WssPinPolicyRole* role = wss_pin_policy_role(role_key);
  if (!role || !role->allowed_gpios || role->allowed_count == 0) return true;
  for (size_t i = 0; i < role->allowed_count; i++) {
    if (role->allowed_gpios[i] == pin) return true;
  }
  return false;
}

bool wss_pin_policy_gpio_reserved(int pin) {
  if (pin < 0) return false;
  const WssPinPolicy& policy = wss_pin_policy();
  if (!policy.reserved_gpios || policy.reserved_count == 0) return false;
  for (size_t i = 0; i < policy.reserved_count; i++) {
    if (policy.reserved_gpios[i] == pin) return true;
  }
  return false;
}

int wss_pin_policy_role_default_gpio(const char* role_key, int fallback) {
  const WssPinPolicyRole* role = wss_pin_policy_role(role_key);
  if (!role) return fallback;
  return role->default_gpio;
}

void wss_pin_policy_write_status_json(JsonObject out) {
  const WssPinPolicy& policy = wss_pin_policy();
  JsonArray reserved = out.createNestedArray("reserved_gpios");
  for (size_t i = 0; i < policy.reserved_count; i++) {
    reserved.add(policy.reserved_gpios[i]);
  }
  JsonObject roles = out.createNestedObject("roles");
  for (size_t i = 0; i < policy.role_count; i++) {
    const WssPinPolicyRole& role = policy.roles[i];
    JsonObject r = roles.createNestedObject(role.key);
    r["default_gpio"] = role.default_gpio;
    JsonArray allowed = r.createNestedArray("allowed_gpios");
    for (size_t j = 0; j < role.allowed_count; j++) {
      allowed.add(role.allowed_gpios[j]);
    }
    if (role.note && role.note[0]) r["note"] = role.note;
  }
}
