#include "ConfigManager.h"

// static function
void ConfigManager::parseKeyValueFile(const char* p, std::function<void(const String&, const String&)> cb){
    if(p == nullptr) return;
    if(!LittleFS.exists(p)) return;
    File f = LittleFS.open(p, "r");
    if(!f) return;
    while(f.available()){
        String line = f.readStringUntil('\n');
        line.trim();
        if(!line.length() || line.startsWith("#")) continue;
        int eq = line.indexOf('=');
        if(eq < 0) continue;
        String k = line.substring(0, eq);
        String v = line.substring(eq + 1);
        cb(k, v);
    }
    f.close();
}

void ConfigManager::load(){
    loaded = false;
    LOG("[Config] Yukleniyor");
    // general settings
    if(LittleFS.exists(String(path) + "general.txt")){
        parseKeyValueFile((String(path) + "general.txt").c_str(), [&](const String& k,const String& v){
            if(k=="company_id") data.companyId = (uint16_t) strtol(v.c_str(), nullptr, 16);
            else if(k=="device_name") strlcpy(data.deviceName, v.c_str(), sizeof(data.deviceName));
            else if(k=="operating_mode") strlcpy(data.operatingMode, v.c_str(), sizeof(data.operatingMode));
        });
        loaded = true;
    } else {
        LOG("[Config] UYARI: /config/general.txt bulunamadi");
    }

    // led settings
    if(LittleFS.exists(String(path) + "led.txt")){
        parseKeyValueFile((String(path) + "led.txt").c_str(), [&](const String& k,const String& v){
            if(k=="led_mode_wifi") strlcpy(data.ledModeWifi, v.c_str(), sizeof(data.ledModeWifi));
            else if(k=="led_speed_wifi") data.ledSpeedWifi = v.toInt();
            else if(k=="led_mode_ble") strlcpy(data.ledModeBle, v.c_str(), sizeof(data.ledModeBle));
            else if(k=="led_speed_ble") data.ledSpeedBle = v.toInt();
            else if(k=="led_mode_dual") strlcpy(data.ledModeDual, v.c_str(), sizeof(data.ledModeDual));
            else if(k=="led_speed_dual") data.ledSpeedDual = v.toInt();
            else if(k=="led_mode_dualsta") strlcpy(data.ledModeDualSta, v.c_str(), sizeof(data.ledModeDualSta));
            else if(k=="led_speed_dualsta") data.ledSpeedDualSta = v.toInt();
            else if(k=="led_mode_wifi_conn") strlcpy(data.ledModeWifiConn, v.c_str(), sizeof(data.ledModeWifiConn));
            else if(k=="led_speed_wifi_conn") data.ledSpeedWifiConn = v.toInt();
            else if(k=="led_mode_wifi_ok") strlcpy(data.ledModeWifiOk, v.c_str(), sizeof(data.ledModeWifiOk));
            else if(k=="led_speed_wifi_ok") data.ledSpeedWifiOk = v.toInt();
            else if(k=="led_mode_wifi_err") strlcpy(data.ledModeWifiErr, v.c_str(), sizeof(data.ledModeWifiErr));
            else if(k=="led_speed_wifi_err") data.ledSpeedWifiErr = v.toInt();
        });
    }

    // WiFi credentials directory: /config/wifi/
    if(LittleFS.exists(String(path) + "wifi/sta.txt")){
        parseKeyValueFile((String(path) + "wifi/sta.txt").c_str(), [&](const String& k,const String& v){ if(k=="sta_ssid") strlcpy(data.staSsid, v.c_str(), sizeof(data.staSsid)); else if(k=="sta_pass") strlcpy(data.staPass, v.c_str(), sizeof(data.staPass)); });
    } else {
        LOG("[Config] UYARI: /config/wifi/sta.txt bulunamadi");
    }

    if(LittleFS.exists(String(path) + "wifi/ap.txt")){
        parseKeyValueFile((String(path) + "wifi/ap.txt").c_str(), [&](const String& k,const String& v){ if(k=="ap_ssid") strlcpy(data.apSsid, v.c_str(), sizeof(data.apSsid)); else if(k=="ap_pass") strlcpy(data.apPass, v.c_str(), sizeof(data.apPass)); });
    } else {
        LOG("[Config] UYARI: /config/wifi/ap.txt bulunamadi");
    }

    bool needSta = (strlen(data.staSsid)==0 || strlen(data.staPass)==0);
    bool needAp  = (strlen(data.apSsid)==0  || strlen(data.apPass)==0);

    if(needSta){
        strlcpy(data.staSsid, WIFI_STA_SSID, sizeof(data.staSsid));
        strlcpy(data.staPass, WIFI_STA_PASS, sizeof(data.staPass));
        File fs = LittleFS.open(String(path) + "wifi/sta.txt", "w");
        if(fs){ char b[128]; sprintf(b, "sta_ssid=%s\n", data.staSsid); fs.print(b); sprintf(b, "sta_pass=%s\n", data.staPass); fs.print(b); fs.close(); }
        // Dosyadan yeniden oku (tek kaynaktan kullanım için)
        parseKeyValueFile((String(path) + "wifi/sta.txt").c_str(), [&](const String& k,const String& v){ if(k=="sta_ssid") strlcpy(data.staSsid, v.c_str(), sizeof(data.staSsid)); else if(k=="sta_pass") strlcpy(data.staPass, v.c_str(), sizeof(data.staPass)); });
    }

    if(needAp){
        strlcpy(data.apSsid, WIFI_AP_SSID, sizeof(data.apSsid));
        strlcpy(data.apPass, WIFI_AP_PASS, sizeof(data.apPass));
        File fa = LittleFS.open(String(path) + "wifi/ap.txt", "w");
        if(fa){ char b[128]; sprintf(b, "ap_ssid=%s\n", data.apSsid); fa.print(b); sprintf(b, "ap_pass=%s\n", data.apPass); fa.print(b); fa.close(); }
        // Dosyadan yeniden oku
        parseKeyValueFile((String(path) + "wifi/ap.txt").c_str(), [&](const String& k,const String& v){ if(k=="ap_ssid") strlcpy(data.apSsid, v.c_str(), sizeof(data.apSsid)); else if(k=="ap_pass") strlcpy(data.apPass, v.c_str(), sizeof(data.apPass)); });
    }

    // time settings
    if(LittleFS.exists(String(path) + "time.txt")){
        parseKeyValueFile((String(path) + "time.txt").c_str(), [&](const String& k,const String& v){ if(k=="epoch_base") data.epochBase = (unsigned long) strtoul(v.c_str(), nullptr, 10); else if(k=="tz_offset") data.timezoneOffset = v.toInt(); else if(k=="time_quality") data.timeQuality = (uint8_t) v.toInt(); });
    } else {
        LOG("[Config] UYARI: /config/time.txt bulunamadi");
    }

    if(data.epochBase){ epochSetMs = millis(); }
    // Ensure apSsid falls back to deviceName if not explicitly set
    if(!strlen(data.apSsid)) strlcpy(data.apSsid, data.deviceName, sizeof(data.apSsid));
    LOGF("[Config] OK mode=%s cid=%04X staSsid=%s apSsid=%s\n", data.operatingMode, data.companyId, data.staSsid, data.apSsid);
}

void ConfigManager::save(){
    LOG("[Config] Kaydediliyor (parcalanmis dosyalar)");
    // ensure base dir exists (LittleFS has flat namespace, so no mkdir)
    // general.txt
    File fg = LittleFS.open(String(path) + "general.txt", "w"); if(fg){ char b[128]; sprintf(b, "company_id=%04X\n", data.companyId); fg.print(b); sprintf(b, "device_name=%s\n", data.deviceName); fg.print(b); sprintf(b, "operating_mode=%s\n", data.operatingMode); fg.print(b); fg.close(); }
    // led.txt
    File fl = LittleFS.open(String(path) + "led.txt", "w"); if(fl){ char b[128]; sprintf(b, "led_mode_wifi=%s\n", data.ledModeWifi); fl.print(b); sprintf(b, "led_speed_wifi=%d\n", data.ledSpeedWifi); fl.print(b); sprintf(b, "led_mode_ble=%s\n", data.ledModeBle); fl.print(b); sprintf(b, "led_speed_ble=%d\n", data.ledSpeedBle); fl.print(b); sprintf(b, "led_mode_dual=%s\n", data.ledModeDual); fl.print(b); sprintf(b, "led_speed_dual=%d\n", data.ledSpeedDual); fl.print(b); sprintf(b, "led_mode_dualsta=%s\n", data.ledModeDualSta); fl.print(b); sprintf(b, "led_speed_dualsta=%d\n", data.ledSpeedDualSta); fl.print(b); sprintf(b, "led_mode_wifi_conn=%s\n", data.ledModeWifiConn); fl.print(b); sprintf(b, "led_speed_wifi_conn=%d\n", data.ledSpeedWifiConn); fl.print(b); sprintf(b, "led_mode_wifi_ok=%s\n", data.ledModeWifiOk); fl.print(b); sprintf(b, "led_speed_wifi_ok=%d\n", data.ledSpeedWifiOk); fl.print(b); sprintf(b, "led_mode_wifi_err=%s\n", data.ledModeWifiErr); fl.print(b); sprintf(b, "led_speed_wifi_err=%d\n", data.ledSpeedWifiErr); fl.print(b); fl.close(); }
    // wifi credential files (sta + ap)
    File fs = LittleFS.open(String(path) + "wifi/sta.txt", "w"); if(fs){ char b[128]; sprintf(b, "sta_ssid=%s\n", data.staSsid); fs.print(b); sprintf(b, "sta_pass=%s\n", data.staPass); fs.print(b); fs.close(); }
    File fa = LittleFS.open(String(path) + "wifi/ap.txt", "w"); if(fa){ char b[128]; sprintf(b, "ap_ssid=%s\n", data.apSsid); fa.print(b); sprintf(b, "ap_pass=%s\n", data.apPass); fa.print(b); fa.close(); }
    // time
    File ft = LittleFS.open(String(path) + "time.txt", "w"); if(ft){ char b[128]; sprintf(b, "epoch_base=%lu\n", data.epochBase); ft.print(b); sprintf(b, "tz_offset=%d\n", data.timezoneOffset); ft.print(b); sprintf(b, "time_quality=%u\n", data.timeQuality); ft.print(b); ft.close(); }
    LOG("[Config] Kaydedildi");
}

bool ConfigManager::setCompany(uint16_t id) { if (id == data.companyId) return false; data.companyId = id; save(); return true; }
bool ConfigManager::setDeviceName(const String& n) { if (n == data.deviceName) return false; strlcpy(data.deviceName, n.c_str(), sizeof(data.deviceName)); save(); return true; }
bool ConfigManager::setOperatingMode(const char* m) { if (strcmp(m, data.operatingMode)==0) return false; strlcpy(data.operatingMode, m, sizeof(data.operatingMode)); save(); return true; }
bool ConfigManager::setStaCredentials(const String& ssid, const String& pass){
    bool changed=false;
    if(ssid != data.staSsid){ strlcpy(data.staSsid, ssid.c_str(), sizeof(data.staSsid)); changed=true; }
    if(pass != data.staPass){ strlcpy(data.staPass, pass.c_str(), sizeof(data.staPass)); changed=true; }
    if(changed) save();
    return changed;
}
bool ConfigManager::setPerModeLed(const char* mode, const char* ledMode, int speed){
    bool changed=false;
    if(strcmp(mode,"wifi")==0){ if(strcmp(ledMode,data.ledModeWifi)!=0){ strlcpy(data.ledModeWifi, ledMode, sizeof(data.ledModeWifi)); changed=true;} if(speed!=data.ledSpeedWifi){ data.ledSpeedWifi=speed; changed=true; }}
    else if(strcmp(mode,"ble")==0){ if(strcmp(ledMode,data.ledModeBle)!=0){ strlcpy(data.ledModeBle, ledMode, sizeof(data.ledModeBle)); changed=true;} if(speed!=data.ledSpeedBle){ data.ledSpeedBle=speed; changed=true; }}
    else if(strcmp(mode,"dual")==0){ if(strcmp(ledMode,data.ledModeDual)!=0){ strlcpy(data.ledModeDual, ledMode, sizeof(data.ledModeDual)); changed=true;} if(speed!=data.ledSpeedDual){ data.ledSpeedDual=speed; changed=true; }}
    else if(strcmp(mode,"dualsta")==0){ if(strcmp(ledMode,data.ledModeDualSta)!=0){ strlcpy(data.ledModeDualSta, ledMode, sizeof(data.ledModeDualSta)); changed=true;} if(speed!=data.ledSpeedDualSta){ data.ledSpeedDualSta=speed; changed=true; }}
    if(changed) save();
    return changed;
}
void ConfigManager::getActiveLed(char* outMode, int& outSpeed){
    if(strcmp(data.operatingMode,"wifi")==0){ strlcpy(outMode,data.ledModeWifi,sizeof(data.ledModeWifi)); outSpeed=data.ledSpeedWifi; }
    else if(strcmp(data.operatingMode,"ble")==0){ strlcpy(outMode,data.ledModeBle,sizeof(data.ledModeBle)); outSpeed=data.ledSpeedBle; }
    else if(strcmp(data.operatingMode,"dual")==0){ strlcpy(outMode,data.ledModeDual,sizeof(data.ledModeDual)); outSpeed=data.ledSpeedDual; }
    else if(strcmp(data.operatingMode,"dualsta")==0){ strlcpy(outMode,data.ledModeDualSta,sizeof(data.ledModeDualSta)); outSpeed=data.ledSpeedDualSta; }
    else { // unknown -> fallback to wifi profile
        strlcpy(outMode,data.ledModeWifi,sizeof(data.ledModeWifi)); outSpeed=data.ledSpeedWifi; }
}
bool ConfigManager::setEpochWithQuality(unsigned long epochUtc, int tzMinutes, uint8_t quality){
    if(epochUtc < 100000000UL) return false; // sanity
    if(quality < data.timeQuality && data.timeQuality!=0) return false; // don't downgrade
    data.epochBase = epochUtc; // store UTC
    data.timezoneOffset = tzMinutes;
    data.timeQuality = quality;
    epochSetMs = millis();
    save();
    return true;
}
void ConfigManager::setEpoch(unsigned long epochSeconds){ setEpochWithQuality(epochSeconds, data.timezoneOffset, data.timeQuality?data.timeQuality:1); }
unsigned long ConfigManager::currentEpochUtc(){ if(!data.epochBase) return 0; unsigned long elapsed=(millis()-epochSetMs)/1000UL; return data.epochBase + elapsed; }
unsigned long ConfigManager::currentEpochLocal(){ unsigned long u=currentEpochUtc(); if(!u) return 0; long loc=(long)u + (long)data.timezoneOffset*60L; return loc<0?0:(unsigned long)loc; }
void ConfigManager::setTimezoneOffset(int off){ if(off == data.timezoneOffset) return; data.timezoneOffset = off; save(); }
