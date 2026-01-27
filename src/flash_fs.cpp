// src/flash_fs.cpp
#include "flash_fs.h"
#include <LittleFS.h>

bool wss_flash_fs_begin() {
  if (!LittleFS.begin(false)) {
    return false;
  }
  return true;
}

bool wss_flash_fs_has_index() {
  return LittleFS.exists("/index.html");
}
