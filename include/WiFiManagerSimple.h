#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <LittleFS.h>
#include <time.h>
#include <functional>
#include <WebServer.h>
#include "ConfigManager.h"
#include "LedController.h"
#include "BLEManager.h"
#include "Log.h"

class WiFiManagerSimple {
public:
    WebServer server{80};
    String ssid;
    // AP credentials are now in DeviceConfig.apSsid / apPass (from /config/wifi/ap.txt)
    bool active=false;
    bool isStation=false; // true if running in STA (dualsta) mode
    bool ntpSynced=false;
    unsigned long lastNtpAttempt=0;
    unsigned long ntpSetMs=0; // epochBase ayarlandigi an (millis)
    bool bootTimeFlag = true; // boot sonrası ilk durumda epoch_base eski olabilir (stale)

    // --- NTP (Turkey Time) -------------------------------------------------
    // SIFIRDAN: UDP tabanli, birden fazla sunucu, DNS varsa isim, yoksa IP ile
    // Turkey is UTC+3 year-round (no DST). We store timezoneOffset=+180 minutes.
    bool attemptNtpSync(DeviceConfig& cfg);
    void begin(DeviceConfig& cfg);
    void beginStation(DeviceConfig& cfg);
    void stop();
    void loop();
    // Station health monitor (call from external loop if needed)
    void loopStationMonitor(DeviceConfig& cfg);
    void updateSSID(DeviceConfig& cfg);
    std::function<void()> onConfigChanged; // callback
    // --- Public STA LED state helper ---------------------------------------
    enum StaLedState { STA_NONE=0, STA_CONNECTING=1, STA_OK=2, STA_ERR=3 };
    StaLedState getStaLedState();
private:
    // --------- NTP (UDP) low-level helpers ---------
    WiFiUDP ntpUdp;
    uint16_t ntpLocalPort = 2390;
    static const unsigned long NTP_UNIX_EPOCH_DIFF = 2208988800UL; // 1900->1970
    bool ntpOpen();
    bool ntpQueryIP(IPAddress ip, unsigned long& unixEpoch, uint16_t timeoutMs=1200);
    bool ntpQueryHost(const char* host, unsigned long& unixEpoch);
    bool ntpSyncOnce(unsigned long& epochOut);
    bool ipOk();
    // --- Helper: compute current active schedule label (no side-effects) ---
    String currentActiveLabel(DeviceConfig& cfg);
    // --- Schedule Management -------------------------------------------------
    struct SchedEntry { uint8_t dow; uint16_t officialStartMin; uint16_t endMin; uint16_t actualStartMin; char label[24]; };
    static const int MAX_SCHED = 40;
    SchedEntry schedule[MAX_SCHED];
    int scheduleCount = 0;
    const char* scheduleFile = "/config/schedule.txt";
    unsigned long lastScheduleCheckMs = 0;
    char lastAppliedLabel[24] = "";
    // Sabit label->id eşlemesi artık kullanılmıyor; her şey data/company/company(.txt) içinden geliyor.
    int timeToMinutes(const String& t);
    String minutesToTime(int v);
    const char* dowName(uint8_t d);
    void addSched(uint8_t dow,const char* off,const char* end,const char* act,const char* label);
    void createDefaultSchedule();
    void saveSchedule();
    void loadSchedule();
    uint16_t mapLabelToCompany(const String& l);
public:
    void enforceSchedule(DeviceConfig& cfg, BLEManager& ble);
private:
    // --- Company ID directory: Tek dosya kullanımı ------------------------
    struct CompanyEntry { String category; String name; String id; };
    static const int MAX_COMPANY = 128;
    CompanyEntry companies[MAX_COMPANY];
    int companyCount = 0;
    const char* companyFile = "/config/company.txt"; // data/config/company.txt
    void loadCompanies();
    void saveCompanies();
    bool idExists(const String& hex);
    // (mountRoutes moved to end of class after handler definitions)
    void buildCompanyOptions(String& out);
    static const char* sanitizeLed(const String& v);
    int clampSpeed(int v);
    String buildLedModeOptions(const char* current);
    void handleConfig(DeviceConfig& cfg);
    void handleTime(DeviceConfig& cfg);
    // Cid ekleme/silme/düzenleme mantığı kaldırıldı (tek dosyadan okunuyor)
    void handleScheduleData();
    void handleScheduleSave();
    void handleScheduleForce(DeviceConfig& cfg);
    // Company add/delete/update handlers (operate on single company file)
    void handleCidAdd();
    void handleCidDel();
    void handleCidUpdate(DeviceConfig& /* cfg */);
    void handleNow(DeviceConfig& cfg);
    void handleConfigGet(DeviceConfig& cfg);
    void handleCompanyOptions();
    void handleActiveLabel(DeviceConfig& cfg);
    // -------- Static file serving from LittleFS ---------
    String getContentType(const String& path);
    bool tryServeFromFS(String uri);
    void mountRoutes(DeviceConfig& cfg);
};
