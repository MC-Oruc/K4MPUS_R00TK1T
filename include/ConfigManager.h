#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <functional> // added to use std::function
#include "Log.h"

struct DeviceConfig {
    uint16_t companyId = 0x0047;
    char deviceName[32] = "TRAKYA UNI KAMPUS4.0";
    // Per-mode LED configs
    char ledModeWifi[10] = "blink";
    int ledSpeedWifi = 500;
    char ledModeBle[10] = "blink";
    int ledSpeedBle = 500;
    char ledModeDual[10] = "blink";
    int ledSpeedDual = 500;
    // Dual-STA (STA+BLE) base profile
    char ledModeDualSta[10] = "blink";
    int ledSpeedDualSta = 500;
    // WiFi state-specific overrides (apply for STA modes, optionally for WiFi if desired)
    char ledModeWifiConn[10] = "pulse";
    int ledSpeedWifiConn = 250;
    char ledModeWifiOk[10] = "on";
    int ledSpeedWifiOk = 0;
    char ledModeWifiErr[10] = "blink";
    int ledSpeedWifiErr = 200;
    // dualsta = BLE + WiFi (station) instead of AP
    char operatingMode[10] = "wifi"; // wifi | ble | dual | dualsta
    // Station credentials for dualsta (and possible future sta-only mode)
    char staSsid[32] = "";
    char staPass[64] = "";
    // AP credentials (kept under /config/wifi/)
    char apSsid[32] = "";
    char apPass[64] = "kampus40";
    unsigned long epochBase = 0; // seconds since Unix epoch when set (0 = not set)
    int timezoneOffset = 0; // minutes (JavaScript getTimezoneOffset value)
    uint8_t timeQuality = 0; // 0=unset,1=client,2=NTP
};

class ConfigManager {
public:
    DeviceConfig data;
    const char* path = "/config/"; // base config directory
    bool loaded = false;

    // helpers to read simple key=value files
    static void parseKeyValueFile(const char* p, std::function<void(const String&, const String&)> cb);

    void load();
    void save();

    // helpers
    bool setCompany(uint16_t id);
    bool setDeviceName(const String& n);
    bool setOperatingMode(const char* m);
    bool setStaCredentials(const String& ssid, const String& pass);
    bool setPerModeLed(const char* mode, const char* ledMode, int speed);
    void getActiveLed(char* outMode, int& outSpeed);

    // Time keeping
    unsigned long epochSetMs = 0; // millis() snapshot when epochBase (UTC) last updated
    bool setEpochWithQuality(unsigned long epochUtc, int tzMinutes, uint8_t quality);
    void setEpoch(unsigned long epochSeconds);
    unsigned long currentEpochUtc();
    unsigned long currentEpochLocal();
    void setTimezoneOffset(int off);
};
