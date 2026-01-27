// src/web_server.h
#pragma once
#include <Arduino.h>

class WssConfigStore;
class WssEventLogger;

void wss_web_begin(WssConfigStore& cfg, WssEventLogger& log);
void wss_web_loop();
