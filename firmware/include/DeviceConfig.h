#pragma once
#include <Arduino.h>
struct DeviceConfig {
  String ssid, wifiPassword, routerHost="192.168.1.1", routerToken;
  uint16_t routerPort=80;
  uint16_t refreshMs=2000;
  uint8_t brightness=220;
};
void configBegin();
void configLoop();
DeviceConfig configSnapshot();
void configStartPortal();
bool configPortalActive();
String configPortalSSID();
String configAdminPassword();
String configHostname();
String configRouterURL();
bool configSerialProvision(const String& json);
extern bool settingsScreen;
