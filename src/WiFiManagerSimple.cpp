#include "WiFiManagerSimple.h"

bool WiFiManagerSimple::attemptNtpSync(DeviceConfig& cfg){
    if(WiFi.status() != WL_CONNECTED) return false;
    // Eğer zaten kalite NTP (2) ise tekrar deneme aralığını uzat
    if(cfg.timeQuality >= 2 && ntpSynced) return true;
    if(millis() - lastNtpAttempt < 15000UL) return false;
    lastNtpAttempt = millis();
    LOG("[NTP] Senkronizasyon baslatiyor (UDP)");

    unsigned long epoch = 0;
    if(!ntpSyncOnce(epoch)){
        LOG("[NTP] Zaman alinamadi");
        return false;
    }

    // Kalite yükselt (veya ilk defa ayarla)
    if(cfg.timeQuality < 2){
        ntpSetMs = millis();
        cfg.epochBase = (unsigned long)epoch;
        cfg.timezoneOffset = 180; // Türkiye UTC+3 = 180 dk
        cfg.timeQuality = 2;
        ntpSynced = true;
        if(onConfigChanged) onConfigChanged();
        LOGF("[NTP] Senkron OK UTC=%lu (yerel=UTC+%ddk)\n", (unsigned long)epoch, cfg.timezoneOffset);
        bootTimeFlag = false; // artik taze
    } else {
        // Kalite zaten 2; sadece epochBase'i güncelleyip drift azaltabiliriz (yenileme)
        cfg.epochBase = (unsigned long)epoch;
        ntpSetMs = millis();
        LOGF("[NTP] Yenileme OK UTC=%lu\n", (unsigned long)epoch);
    }
    return true;
}

void WiFiManagerSimple::begin(DeviceConfig& cfg){
    if(active) return;
    isStation = false;
    // AP SSID comes from wifi ap credentials (ap_ssid). If empty, fallback to deviceName.
    // apSsid is a char array (never null). Check if it's non-empty via first char.
    ssid = String(cfg.apSsid[0] != '\0' ? cfg.apSsid : cfg.deviceName); ssid.replace(" ", "_");
    LOGF("[WiFi] AP baslatiliyor SSID=%s\n", ssid.c_str());
    WiFi.mode(WIFI_AP);
    // Use AP password from runtime config (/config/wifi/ap.txt)
    WiFi.softAP(ssid.c_str(), cfg.apPass);
    mountRoutes(cfg);
    server.begin();
    active=true;
    LOG("[WiFi] AP aktif");
}

void WiFiManagerSimple::beginStation(DeviceConfig& cfg){
    // Start in STA mode using stored credentials; fallback to AP if missing or connection fails
    if(active){
        // Force clean restart of radio for deterministic behavior
        stop();
    }
    if(strlen(cfg.staSsid)==0){ LOG("[WiFi] STA SSID bos -> AP"); begin(cfg); return; }
    isStation = true;
    LOGF("[WiFi] STA moda geciliyor SSID='%s'\n", cfg.staSsid);
    // Fully power-cycle WiFi before switching to STA to avoid leftover AP netif
    WiFi.disconnect(true);
    delay(100);
    #ifdef WIFI_OFF
    WiFi.mode(WIFI_OFF);
    delay(150);
    #endif
    WiFi.mode(WIFI_STA);
    delay(120);
    // Ensure DHCP client is used (reset any static leftover)
    #if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_RP2040)
    WiFi.config(IPAddress(0,0,0,0), IPAddress(0,0,0,0), IPAddress(0,0,0,0), IPAddress(0,0,0,0));
    #endif
    // Pre-scan to validate SSID visibility
    bool found=false; int scanAttempt=0; const int MAX_SCAN=3;
    // Hex dump target SSID for hidden char / case issues
    {
        const char* s = cfg.staSsid; size_t L = strlen(s);
        LOGF("[WiFi] Hedef SSID len=%u -> ", (unsigned)L);
        for(size_t i=0;i<L;i++) Serial.printf("%02X ", (unsigned char)s[i]);
        Serial.println();
    }
    while(scanAttempt < MAX_SCAN && !found){
        int n = WiFi.scanNetworks();
        if(n<0){ LOGF("[WiFi] scanNetworks hata (%d)\n", n); }
        else {
            LOGF("[WiFi] scan=%d bulunanAg=%d\n", scanAttempt+1, n);
            for(int i=0;i<n && i<20;i++){
                String s = WiFi.SSID(i);
                int r = WiFi.RSSI(i);
                int ch = WiFi.channel(i);
                #ifdef WIFI_AUTH_OPEN
                // some cores have encryptionType, some don't
                #endif
                Serial.printf("  [%2d] ch=%2d rssi=%4d '%s'\n", i, ch, r, s.c_str());
                if(s.equals(cfg.staSsid)){
                    found=true;
                    LOGF("[WiFi] Hedef SSID bulundu (scan=%d index=%d ch=%d RSSI=%d)\n", scanAttempt+1, i, ch, r);
                }
            }
        }
        if(!found){ LOGF("[WiFi] SSID bulunamadi scan=%d\n", scanAttempt+1); delay(500); }
        scanAttempt++;
    }
    if(!found) LOG("[WiFi] SSID gorunmuyor -> yine de baglanma denenecek");
    // Connection attempts
    int attempt=0; const int MAX_ATTEMPTS=3; size_t passLen=strlen(cfg.staPass);
    auto ip_ok = [](){
        IPAddress ip = WiFi.localIP();
        IPAddress gw = WiFi.gatewayIP();
        return ip != IPAddress(0,0,0,0) && gw != IPAddress(0,0,0,0) && ip != gw;
    };
    while(attempt < MAX_ATTEMPTS){
        LOGF("[WiFi] Attempt %d begin (passLen=%u)%s\n", attempt+1, (unsigned)passLen, passLen<8 && passLen>0?" [WARN:kisa]":"");
        if(passLen==0) WiFi.begin(cfg.staSsid); else WiFi.begin(cfg.staSsid, cfg.staPass);
        unsigned long start = millis(); int lastStatus=-1;
        while(WiFi.status()!=WL_CONNECTED && millis()-start < 9000){
            int st = WiFi.status();
            if(st != lastStatus){ LOGF("[WiFi] Baglaniyor... status=%s(%d)\n", wifiStatusName(st), st); lastStatus=st; }
            delay(280);
        }
        if(WiFi.status()==WL_CONNECTED){
            // Wait for DHCP lease to be actually assigned
            unsigned long t0 = millis();
            while(!ip_ok() && millis()-t0 < 5000){ delay(100); }
            if(ip_ok()){
                LOGF("[WiFi] STA baglandi IP=%s GW=%s RSSI=%d\n", WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str(), WiFi.RSSI());
                break;
            } else {
                LOG("[WiFi] WL_CONNECTED fakat DHCP IP alinmadi -> yeniden dene");
            }
        }
        LOGF("[WiFi] Attempt %d FAIL status=%s(%d)\n", attempt+1, wifiStatusName(WiFi.status()), WiFi.status());
        attempt++;
        if(attempt < MAX_ATTEMPTS){
            LOG("[WiFi] Tekrar denenecek...");
            WiFi.disconnect(true);
            delay(100);
            #ifdef WIFI_OFF
            WiFi.mode(WIFI_OFF);
            delay(120);
            #endif
            WiFi.mode(WIFI_STA);
            delay(120);
            #if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_RP2040)
            WiFi.config(IPAddress(0,0,0,0), IPAddress(0,0,0,0), IPAddress(0,0,0,0), IPAddress(0,0,0,0));
            #endif
        }
    }
    if(WiFi.status()!=WL_CONNECTED){
        LOG("[WiFi] Tum denemeler basarisiz -> AP fallback");
        isStation=false; begin(cfg); return;
    }
    // NTP zaman senkronizasyonu (dualsta senaryosu icin otomatik)
    attemptNtpSync(cfg);
    // Only now start server/routes if not already
    mountRoutes(cfg);
    server.begin();
    active=true;
}

void WiFiManagerSimple::stop(){
    if(!active) return;
    LOG("[WiFi] Duruyor");
    server.close();
    if(isStation){ WiFi.disconnect(true); }
    else { WiFi.softAPdisconnect(true); }
    // Fully turn off WiFi to clear netifs/state for next mode (if supported)
    #ifdef WIFI_OFF
    WiFi.mode(WIFI_OFF);
    delay(120);
    #endif
    active=false; isStation=false;
}

void WiFiManagerSimple::loop(){ server.handleClient(); }

void WiFiManagerSimple::loopStationMonitor(DeviceConfig& cfg){
    if(!active || !isStation) return;
    static unsigned long lastCheck=0; if(millis()-lastCheck < 5000) return; lastCheck=millis();
    int st = WiFi.status();
    auto ip_ok = [](){
        IPAddress ip = WiFi.localIP();
        IPAddress gw = WiFi.gatewayIP();
        return ip != IPAddress(0,0,0,0) && gw != IPAddress(0,0,0,0) && ip != gw;
    };
    if(st != WL_CONNECTED || !ip_ok()){
        LOGF("[WiFi] Station kayip status=%s(%d) -> hizli reconnect\n", wifiStatusName(st), st);
        WiFi.disconnect();
        delay(100);
        if(strlen(cfg.staPass)==0) WiFi.begin(cfg.staSsid); else WiFi.begin(cfg.staSsid, cfg.staPass);
        unsigned long start=millis();
        while(WiFi.status()!=WL_CONNECTED && millis()-start < 6000){ delay(300); }
        if(WiFi.status()==WL_CONNECTED){
            unsigned long t0=millis(); while(!ip_ok() && millis()-t0<3000){ delay(100); }
            if(ip_ok()) LOGF("[WiFi] Reconnect OK IP=%s RSSI=%d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
            else { LOG("[WiFi] Reconnect WL_CONNECTED fakat IP yok -> tam yeniden baslatma"); beginStation(cfg); return; }
        }
        else { LOGF("[WiFi] Reconnect FAIL status=%s(%d) -> tam yeniden baslatma\n", wifiStatusName(WiFi.status()), WiFi.status()); beginStation(cfg); }
    }
    // If time not yet set (epochBase==0) or not NTP synced, try again
    if(cfg.epochBase==0 || cfg.timeQuality < 2){ attemptNtpSync(cfg); }
}

void WiFiManagerSimple::updateSSID(DeviceConfig& cfg){
    String newSsid = String(cfg.deviceName); newSsid.replace(" ", "_");
    if(newSsid != ssid){
        ssid = newSsid;
        if(active && !isStation){
            LOGF("[WiFi] AP SSID degisiyor -> %s\n", ssid.c_str());
            WiFi.softAPdisconnect(true);
            delay(50);
            WiFi.softAP(ssid.c_str(), cfg.apPass);
        }
    }
}

WiFiManagerSimple::StaLedState WiFiManagerSimple::getStaLedState(){
    if(!isStation) return STA_NONE;
    int st = WiFi.status();
    bool ok = ipOk();
    #ifdef WL_CONNECTED
    if(st == WL_CONNECTED && ok) return STA_OK;
    #else
    if(st == 3 && ok) return STA_OK; // fallback numeric
    #endif
    // Error-like statuses
    #ifdef WL_CONNECT_FAILED
    if(st == WL_CONNECT_FAILED) return STA_ERR;
    #endif
    #ifdef WL_NO_SSID_AVAIL
    if(st == WL_NO_SSID_AVAIL) return STA_ERR;
    #endif
    #ifdef WL_DISCONNECTED
    if(st == WL_DISCONNECTED) return STA_ERR;
    #endif
    return STA_CONNECTING;
}

bool WiFiManagerSimple::ntpOpen(){
    static bool opened=false;
    if(opened) return true;
    if(ntpUdp.begin(ntpLocalPort)) { opened=true; return true; }
    return false;
}

bool WiFiManagerSimple::ntpQueryIP(IPAddress ip, unsigned long& unixEpoch, uint16_t timeoutMs){
    if(!ntpOpen()) return false;
    uint8_t pkt[48]; memset(pkt, 0, sizeof(pkt));
    pkt[0] = 0b11100011; // LI=3(no sync, OK), VN=4, Mode=3(client)
    pkt[1] = 0; pkt[2] = 6; pkt[3] = 0xEC; // poll, precision
    pkt[12]=49; pkt[13]=0x4E; pkt[14]=49; pkt[15]=52; // "1N14" magic
    if(!ntpUdp.beginPacket(ip, 123)) return false;
    ntpUdp.write(pkt, sizeof(pkt));
    if(!ntpUdp.endPacket()) return false;
    unsigned long start = millis();
    while(millis()-start < timeoutMs){
        int p = ntpUdp.parsePacket();
        if(p >= 48){
            uint8_t buf[48];
            ntpUdp.read(buf, 48);
            // Transmit Timestamp @40..47 (seconds.frac) big-endian
            unsigned long secs = ((unsigned long)buf[40] << 24) | ((unsigned long)buf[41] << 16) | ((unsigned long)buf[42] << 8) | (unsigned long)buf[43];
            if(secs > NTP_UNIX_EPOCH_DIFF){
                unixEpoch = secs - NTP_UNIX_EPOCH_DIFF;
                return true;
            }
        }
        delay(10);
    }
    return false;
}

bool WiFiManagerSimple::ntpQueryHost(const char* host, unsigned long& unixEpoch){
    // Önce DNS ile çözmeyi dene, olmazsa direkt endPacket host ile (bazı core'lar destekler)
    IPAddress ip;
    bool haveIp = false;
    // DNS çözümlemesi için küçük bir süre ver
    for(int i=0;i<2 && !haveIp;i++){
        if(WiFi.hostByName(host, ip)) haveIp = true; else delay(30);
    }
    if(haveIp){
        return ntpQueryIP(ip, unixEpoch);
    }
    // Bazı çekirdeklerde beginPacket(host,port) dahili olarak DNS yapar
    if(!ntpOpen()) return false;
    uint8_t pkt[48]; memset(pkt, 0, sizeof(pkt)); pkt[0]=0b11100011; pkt[2]=6; pkt[3]=0xEC; pkt[12]=49; pkt[13]=0x4E; pkt[14]=49; pkt[15]=52;
    if(!ntpUdp.beginPacket(host, 123)) return false;
    ntpUdp.write(pkt, sizeof(pkt)); if(!ntpUdp.endPacket()) return false;
    unsigned long start = millis();
    while(millis()-start < 1500){
        int p = ntpUdp.parsePacket();
        if(p >= 48){
            uint8_t buf[48]; ntpUdp.read(buf, 48);
            unsigned long secs = ((unsigned long)buf[40] << 24) | ((unsigned long)buf[41] << 16) | ((unsigned long)buf[42] << 8) | (unsigned long)buf[43];
            if(secs > NTP_UNIX_EPOCH_DIFF){ unixEpoch = secs - NTP_UNIX_EPOCH_DIFF; return true; }
        }
        delay(10);
    }
    return false;
}

bool WiFiManagerSimple::ntpSyncOnce(unsigned long& epochOut){
    // Denenecek host ve IP'ler (hem TR havuzu hem genel, ayrica sayisal IP'ler)
    const char* hosts[] = {"tr.pool.ntp.org", "pool.ntp.org", "time.google.com", "time.cloudflare.com"};
    IPAddress ips[] = { IPAddress(129,6,15,28),   // time.nist.gov
                        IPAddress(162,159,200,1), // time.cloudflare.com anycast
                        IPAddress(162,159,200,123)};
    const int HN = sizeof(hosts)/sizeof(hosts[0]);
    const int IN = sizeof(ips)/sizeof(ips[0]);
    // Küçük bir bekleme: DHCP/DNS bazen IP alınsa da DNS gecikebilir
    delay(50);
    unsigned long tStart = millis();
    for(int round=0; round<2; ++round){ // iki tur dene
        for(int i=0;i<HN;i++){
            unsigned long e=0; LOGF("[NTP] Host dene: %s\n", hosts[i]);
            if(ntpQueryHost(hosts[i], e) && e > 1700000000UL){ epochOut=e; return true; }
        }
        for(int j=0;j<IN;j++){
            unsigned long e=0; LOGF("[NTP] IP dene: %u.%u.%u.%u\n", ips[j][0],ips[j][1],ips[j][2],ips[j][3]);
            if(ntpQueryIP(ips[j], e) && e > 1700000000UL){ epochOut=e; return true; }
        }
        // turlar arasinda kısa ara
        if(millis()-tStart < 3000) delay(200);
    }
    return false;
}

bool WiFiManagerSimple::ipOk(){
    IPAddress ip = WiFi.localIP();
    IPAddress gw = WiFi.gatewayIP();
    return ip != IPAddress(0,0,0,0) && gw != IPAddress(0,0,0,0) && ip != gw;
}

String WiFiManagerSimple::currentActiveLabel(DeviceConfig& cfg){
    // Time guards similar to enforceSchedule but read-only
    if(cfg.epochBase==0) return String();
    if(cfg.timeQuality == 0) return String();
    if(bootTimeFlag && cfg.timeQuality < 2) return String();
    // Compute current local minutes and ISO DOW
    unsigned long epochNow;
    if(ntpSetMs){ epochNow = cfg.epochBase + (millis() - ntpSetMs)/1000UL; }
    else { epochNow = cfg.epochBase + millis()/1000UL; }
    long localEpoch = (long)epochNow + (long)cfg.timezoneOffset*60L;
    if(localEpoch < 0) return String();
    unsigned long days = (unsigned long)(localEpoch / 86400UL);
    unsigned long secsDay = (unsigned long)(localEpoch % 86400UL);
    uint16_t curMin = secsDay / 60;
    uint8_t isoDow = ((days + 3) % 7) + 1; // Monday=1
    for(int i=0;i<scheduleCount;i++){
        SchedEntry &e = schedule[i];
        if(e.dow != isoDow) continue;
        if(curMin >= e.officialStartMin && curMin <= e.endMin){
            return String(e.label);
        }
    }
    return String();
}

int WiFiManagerSimple::timeToMinutes(const String& t){ int c=t.indexOf(':'); if(c<0) return -1; int h=t.substring(0,c).toInt(); int m=t.substring(c+1).toInt(); if(h<0||h>23||m<0||m>59) return -1; return h*60+m; }

String WiFiManagerSimple::minutesToTime(int v){ 
    int h=v/60; int m=v%60; 
    char b[16]; 
    sprintf(b,"%02d:%02d",h,m); 
    return String(b); 
}

const char* WiFiManagerSimple::dowName(uint8_t d){ 
    switch(d){ 
        case 1: return "Pazartesi"; 
        case 2: return "Salı"; 
        case 3: return "Çarşamba"; 
        case 4: return "Perşembe"; 
        case 5: return "Cuma"; 
        case 6: return "Cumartesi"; 
        case 7: return "Pazar"; 
        default: return "?"; 
    } 
}

void WiFiManagerSimple::addSched(uint8_t dow,const char* off,const char* end,const char* act,const char* label){ if(scheduleCount>=MAX_SCHED) return; SchedEntry &e=schedule[scheduleCount++]; e.dow=dow; e.officialStartMin=timeToMinutes(String(off)); e.endMin=timeToMinutes(String(end)); e.actualStartMin=timeToMinutes(String(act)); strlcpy(e.label,label,sizeof(e.label)); }

void WiFiManagerSimple::createDefaultSchedule(){ scheduleCount=0; addSched(1,"09:30","12:20","09:30","D103"); addSched(1,"13:30","16:20","13:30","Mobil Uygulama"); addSched(2,"09:30","12:20","10:00","Amfi Derslik 2"); addSched(3,"09:30","12:20","10:00","Amfi Derslik 2"); addSched(4,"13:30","16:20","13:30","D103"); addSched(5,"08:30","10:20","09:00","Amfi 3"); addSched(5,"13:30","17:20","13:30","L201"); }

void WiFiManagerSimple::saveSchedule(){ File f=LittleFS.open(scheduleFile,"w"); if(!f) return; for(int i=0;i<scheduleCount;i++){ SchedEntry &e=schedule[i]; f.printf("%d,%s,%s,%s,%s\n", e.dow, minutesToTime(e.officialStartMin).c_str(), minutesToTime(e.endMin).c_str(), minutesToTime(e.actualStartMin).c_str(), e.label); } f.close(); }

void WiFiManagerSimple::loadSchedule(){ scheduleCount=0; if(!LittleFS.exists(scheduleFile)){ LOG("[Sched] UYARI: /config/schedule.txt bulunamadi"); return; } File f=LittleFS.open(scheduleFile,"r"); if(!f){ LOG("[Sched] UYARI: schedule dosyasi acilamadi"); return; } while(f.available() && scheduleCount<MAX_SCHED){ String line=f.readStringUntil('\n'); line.trim(); if(!line.length()||line.startsWith("#")) continue; int p1=line.indexOf(','); int p2=line.indexOf(',',p1+1); int p3=line.indexOf(',',p2+1); int p4=line.indexOf(',',p3+1); if(p1<0||p2<0||p3<0||p4<0) continue; int dow=line.substring(0,p1).toInt(); String off=line.substring(p1+1,p2); String end=line.substring(p2+1,p3); String act=line.substring(p3+1,p4); String label=line.substring(p4+1); label.trim(); int offM=timeToMinutes(off); int endM=timeToMinutes(end); int actM=timeToMinutes(act); if(dow>=1&&dow<=7&&offM>=0&&endM>offM&&actM>=0&&actM<=endM){ SchedEntry &e=schedule[scheduleCount++]; e.dow=dow; e.officialStartMin=offM; e.endMin=endM; e.actualStartMin=actM; strlcpy(e.label,label.c_str(),sizeof(e.label)); } } f.close(); }

uint16_t WiFiManagerSimple::mapLabelToCompany(const String& l){
    for(int i=0;i<companyCount;i++){
        if(l.equalsIgnoreCase(companies[i].name)) return (uint16_t) strtol(companies[i].id.c_str(), nullptr, 16);
    }
    return 0;
}

void WiFiManagerSimple::enforceSchedule(DeviceConfig& cfg, BLEManager& ble){
    if(cfg.epochBase==0) return; // time not set
    // Eğer reboot sonrası eski (stale) zaman (quality <2) ve henüz manuel bile doğrulanmamışsa programı işletme
    if(cfg.timeQuality == 0) return; // tamamen unset
    if(bootTimeFlag && cfg.timeQuality < 2) return; // manuel/NTP yok -> stale snapshot
    if(millis()-lastScheduleCheckMs < 10000) return; // check every 10s
    lastScheduleCheckMs = millis();
    // Epoch hesaplama: epochBase UTC, ntpSetMs ayarlandığı anda millis() snapshot'ı.
    unsigned long epochNow;
    if(ntpSetMs){
        epochNow = cfg.epochBase + (millis() - ntpSetMs)/1000UL; // UTC
    } else {
        epochNow = cfg.epochBase + millis()/1000UL; // fallback (daha kabaca)
    }
    long localEpoch = (long)epochNow + (long)cfg.timezoneOffset*60L; // Türkiye saati
    if(localEpoch < 0) return;
    unsigned long days = (unsigned long)(localEpoch / 86400UL);
    unsigned long secsDay = (unsigned long)(localEpoch % 86400UL);
    uint16_t curMin = secsDay / 60;
    uint8_t isoDow = ((days + 3) % 7) + 1; // Monday=1
    for(int i=0;i<scheduleCount;i++){
        SchedEntry &e = schedule[i];
        if(e.dow != isoDow) continue;
        if(curMin >= e.officialStartMin && curMin <= e.endMin){
            if(strcmp(e.label, lastAppliedLabel)!=0){
                uint16_t id = mapLabelToCompany(String(e.label));
                if(id){
                    bool modeChanged=false;
                    if(strcmp(cfg.operatingMode,"wifi")==0){ strlcpy(cfg.operatingMode,"dual",sizeof(cfg.operatingMode)); modeChanged=true; LOG("[Sched] Mode wifi->dual"); }
                    if(cfg.companyId != id){ cfg.companyId = id; }
                    strlcpy(lastAppliedLabel, e.label, sizeof(lastAppliedLabel));
                    if(onConfigChanged) onConfigChanged();
                    if(modeChanged && !ble.isActive()) ble.begin(cfg.deviceName), ble.updateCompany(cfg.companyId);
                    LOGF("[Sched] Uygulandi label=%s cid=%04X\n", e.label, cfg.companyId);
                }
            }
            return; // active handled
        }
    }
}

void WiFiManagerSimple::loadCompanies(){
    companyCount = 0;
    if(!LittleFS.exists(companyFile)){ LOG("[Company] UYARI: /config/company.txt bulunamadi"); return; }
    File f = LittleFS.open(companyFile, "r"); if(!f){ LOG("[Company] UYARI: company dosyasi acilamadi"); return; }
    while(f.available() && companyCount < MAX_COMPANY){
        String line = f.readStringUntil('\n'); line.trim();
        if(!line.length() || line.startsWith("#")) continue;
        int a=line.indexOf('|'); int b=line.indexOf('|', a+1); if(a<0||b<0) continue;
        String cat=line.substring(0,a); String name=line.substring(a+1,b); String id=line.substring(b+1);
        cat.trim(); name.trim(); id.trim(); id.toUpperCase();
        if(id.length()>=4){ companies[companyCount++] = CompanyEntry{cat,name,id}; }
    }
    f.close();
}

void WiFiManagerSimple::saveCompanies(){
    File f = LittleFS.open(companyFile, "w");
    if(!f) { LOG("[Company] UYARI: company kaydedilemedi"); return; }
    for(int i=0;i<companyCount;i++){
        f.print(companies[i].category);
        f.print('|'); f.print(companies[i].name); f.print('|'); f.print(companies[i].id); f.print('\n');
    }
    f.close();
}

bool WiFiManagerSimple::idExists(const String& hex){
    for(int i=0;i<companyCount;i++) if(companies[i].id.equalsIgnoreCase(hex)) return true;
    return false;
}

void WiFiManagerSimple::buildCompanyOptions(String& out){
    // categories order: L, D, Lab, Amfi, Amfi Derslik
    const char* cats[] = {"L","D","Lab","Amfi","Amfi Derslik"};
    for(auto c: cats){
        out += "<optgroup label='"; out += c; out += "'>";
        for(int i=0;i<companyCount;i++) if(companies[i].category == c){
            out += "<option data-name='"; out += companies[i].name; out += "' data-cat='"; out += companies[i].category; out += "' value='"; out += companies[i].id; out += "'>";
            out += companies[i].name; out += " ("; out += companies[i].id; out += ")</option>";
        }
        out += "</optgroup>";
    }
}

const char* WiFiManagerSimple::sanitizeLed(const String& v){
    if(v == "on" || v == "off" || v == "blink" || v == "pulse") return v.c_str();
    return "blink"; // default
}

int WiFiManagerSimple::clampSpeed(int v){ if(v<50) return 50; if(v>5000) return 5000; return v; }

String WiFiManagerSimple::buildLedModeOptions(const char* current){
    String o;
    const char* modes[4] = {"on","off","blink","pulse"};
    for(int i=0;i<4;i++){
        o += "<option value='"; o += modes[i]; o += "' ";
        if(strcmp(current,modes[i])==0) { o += "selected"; }
        o += ">"; o += modes[i]; o += "</option>";
    }
    return o;
}

void WiFiManagerSimple::handleConfig(DeviceConfig& cfg){
    bool changed=false;
    // Operating mode (wifi | ble | dual | dualsta)
    if(server.hasArg("mode")){
        String v = server.arg("mode"); v.trim();
        if(v == "wifi" || v == "ble" || v == "dual" || v == "dualsta"){
            if(strcmp(cfg.operatingMode, v.c_str())!=0){ strlcpy(cfg.operatingMode, v.c_str(), sizeof(cfg.operatingMode)); changed=true; }
        }
    }
    int beforeCid = cfg.companyId;
    // Station credentials
    if(server.hasArg("sta_ssid")){
        String v=server.arg("sta_ssid"); v.trim(); if(v.length()<sizeof(cfg.staSsid) && v != cfg.staSsid){ strlcpy(cfg.staSsid, v.c_str(), sizeof(cfg.staSsid)); changed=true; }
    }
    if(server.hasArg("sta_pass")){
        String v=server.arg("sta_pass"); v.trim(); if(v.length()<sizeof(cfg.staPass) && v != cfg.staPass){ strlcpy(cfg.staPass, v.c_str(), sizeof(cfg.staPass)); changed=true; }
    }
    if(server.hasArg("company")){
        String v = server.arg("company"); v.trim();
        uint16_t id = (uint16_t) strtol(v.c_str(), nullptr, 16);
        if(id && id != cfg.companyId){ cfg.companyId = id; changed=true; }
    }
    if(server.hasArg("name")){
        String v = server.arg("name"); v.trim();
        if(v.length() && v != cfg.deviceName){ strlcpy(cfg.deviceName, v.c_str(), sizeof(cfg.deviceName)); changed=true; }
    }
    if(server.hasArg("ap_ssid")){
        String v = server.arg("ap_ssid"); v.trim();
        if(v.length() && v != cfg.apSsid && v.length() < (int)sizeof(cfg.apSsid)){
            strlcpy(cfg.apSsid, v.c_str(), sizeof(cfg.apSsid)); changed=true;
        }
    }
    if(server.hasArg("ap_pass")){
        String v = server.arg("ap_pass"); v.trim();
        if(v.length() && v != cfg.apPass && v.length() < (int)sizeof(cfg.apPass)){
            strlcpy(cfg.apPass, v.c_str(), sizeof(cfg.apPass)); changed=true;
        }
    }
    // Per-mode led
    if(server.hasArg("led_wifi")){ String v=server.arg("led_wifi"); const char* sv = sanitizeLed(v); if(strcmp(sv,cfg.ledModeWifi)!=0){ strlcpy(cfg.ledModeWifi,sv,sizeof(cfg.ledModeWifi)); changed=true; }}
    if(server.hasArg("speed_wifi")){ int s=clampSpeed(server.arg("speed_wifi").toInt()); if(s!=cfg.ledSpeedWifi){ cfg.ledSpeedWifi=s; changed=true; }}
    if(server.hasArg("led_ble")){ String v=server.arg("led_ble"); const char* sv = sanitizeLed(v); if(strcmp(sv,cfg.ledModeBle)!=0){ strlcpy(cfg.ledModeBle,sv,sizeof(cfg.ledModeBle)); changed=true; }}
    if(server.hasArg("speed_ble")){ int s=clampSpeed(server.arg("speed_ble").toInt()); if(s!=cfg.ledSpeedBle){ cfg.ledSpeedBle=s; changed=true; }}
    if(server.hasArg("led_dual")){ String v=server.arg("led_dual"); const char* sv = sanitizeLed(v); if(strcmp(sv,cfg.ledModeDual)!=0){ strlcpy(cfg.ledModeDual,sv,sizeof(cfg.ledModeDual)); changed=true; }}
    if(server.hasArg("speed_dual")){ int s=clampSpeed(server.arg("speed_dual").toInt()); if(s!=cfg.ledSpeedDual){ cfg.ledSpeedDual=s; changed=true; }}
    if(server.hasArg("led_dualsta")){ String v=server.arg("led_dualsta"); const char* sv = sanitizeLed(v); if(strcmp(sv,cfg.ledModeDualSta)!=0){ strlcpy(cfg.ledModeDualSta,sv,sizeof(cfg.ledModeDualSta)); changed=true; }}
    if(server.hasArg("speed_dualsta")){ int s=clampSpeed(server.arg("speed_dualsta").toInt()); if(s!=cfg.ledSpeedDualSta){ cfg.ledSpeedDualSta=s; changed=true; }}
    if(server.hasArg("led_wifi_conn")){ String v=server.arg("led_wifi_conn"); const char* sv = sanitizeLed(v); if(strcmp(sv,cfg.ledModeWifiConn)!=0){ strlcpy(cfg.ledModeWifiConn,sv,sizeof(cfg.ledModeWifiConn)); changed=true; }}
    if(server.hasArg("speed_wifi_conn")){ int s=clampSpeed(server.arg("speed_wifi_conn").toInt()); if(s!=cfg.ledSpeedWifiConn){ cfg.ledSpeedWifiConn=s; changed=true; }}
    if(server.hasArg("led_wifi_ok")){ String v=server.arg("led_wifi_ok"); const char* sv = sanitizeLed(v); if(strcmp(sv,cfg.ledModeWifiOk)!=0){ strlcpy(cfg.ledModeWifiOk,sv,sizeof(cfg.ledModeWifiOk)); changed=true; }}
    if(server.hasArg("speed_wifi_ok")){ int s=clampSpeed(server.arg("speed_wifi_ok").toInt()); if(s!=cfg.ledSpeedWifiOk){ cfg.ledSpeedWifiOk=s; changed=true; }}
    if(server.hasArg("led_wifi_err")){ String v=server.arg("led_wifi_err"); const char* sv = sanitizeLed(v); if(strcmp(sv,cfg.ledModeWifiErr)!=0){ strlcpy(cfg.ledModeWifiErr,sv,sizeof(cfg.ledModeWifiErr)); changed=true; }}
    if(server.hasArg("speed_wifi_err")){ int s=clampSpeed(server.arg("speed_wifi_err").toInt()); if(s!=cfg.ledSpeedWifiErr){ cfg.ledSpeedWifiErr=s; changed=true; }}
    if(changed){
        LOG("[Config] Degisiklik algilandi");
        if(beforeCid != cfg.companyId) LOGF("[Config] CompanyID %04X -> %04X\n", beforeCid, cfg.companyId);
        if(onConfigChanged) onConfigChanged();
    }
    server.send(200, "text/plain", changed?"OK":"NO CHANGE");
}

void WiFiManagerSimple::handleTime(DeviceConfig& cfg){
    if(!server.hasArg("epoch")){ server.send(400, "text/plain", "ERR"); return; }
    unsigned long e = (unsigned long) strtoul(server.arg("epoch").c_str(), nullptr, 10);
    int tz = server.hasArg("tz") ? server.arg("tz").toInt() : cfg.timezoneOffset;
    if(e < 100000000UL){ server.send(400, "text/plain", "RANGE"); return; }
    // Manuel ayarı her zaman kabul et; kaliteyi düşürme (>=2 ise 2 olarak koru)
    cfg.epochBase = e;
    cfg.timezoneOffset = tz;
    cfg.timeQuality = (cfg.timeQuality >= 2) ? 2 : 1;
    ntpSetMs = millis();
    // Persist change via callback (ModeController/ConfigManager will save)
    bootTimeFlag = false; // artik stale degil
    if(onConfigChanged) onConfigChanged();
    server.send(200, "text/plain", "OK");
}

void WiFiManagerSimple::handleScheduleData(){
    String out;
    for(int i=0;i<scheduleCount;i++){ SchedEntry &e=schedule[i]; out += String(e.dow)+","+minutesToTime(e.officialStartMin)+","+minutesToTime(e.endMin)+","+minutesToTime(e.actualStartMin)+","+e.label+"\n"; }
    server.send(200, "text/plain", out);
}

void WiFiManagerSimple::handleScheduleSave(){
    if(!server.hasArg("data")){ server.send(400, "text/plain", "NO_DATA"); return; }
    String data=server.arg("data");
    SchedEntry temp[MAX_SCHED]; int tempCount=0; int idx=0;
    while(idx < (int)data.length() && tempCount<MAX_SCHED){ int nl=data.indexOf('\n', idx); if(nl<0) nl=data.length(); String line=data.substring(idx,nl); idx=nl+1; line.trim(); if(!line.length()||line.startsWith("#")) continue; int p1=line.indexOf(','); int p2=line.indexOf(',',p1+1); int p3=line.indexOf(',',p2+1); int p4=line.indexOf(',',p3+1); if(p1<0||p2<0||p3<0||p4<0) continue; int dow=line.substring(0,p1).toInt(); String off=line.substring(p1+1,p2); String end=line.substring(p2+1,p3); String act=line.substring(p3+1,p4); String label=line.substring(p4+1); label.trim(); int offM=timeToMinutes(off); int endM=timeToMinutes(end); int actM=timeToMinutes(act); if(dow>=1&&dow<=7&&offM>=0&&endM>offM&&actM>=0&&actM<=endM && label.length()){ temp[tempCount].dow=dow; temp[tempCount].officialStartMin=offM; temp[tempCount].endMin=endM; temp[tempCount].actualStartMin=actM; strlcpy(temp[tempCount].label,label.c_str(),sizeof(temp[tempCount].label)); tempCount++; } }
    if(tempCount==0){ server.send(400, "text/plain", "EMPTY"); return; }
    scheduleCount=tempCount; for(int i=0;i<tempCount;i++) schedule[i]=temp[i]; saveSchedule();
    server.send(200, "text/plain", "OK");
}

void WiFiManagerSimple::handleScheduleForce(DeviceConfig& cfg){
    if(!server.hasArg("label")){ server.send(400, "text/plain", "NO_LABEL"); return; }
    String lab=server.arg("label"); lab.trim(); if(!lab.length()){ server.send(400, "text/plain", "BAD_LABEL"); return; }
    uint16_t id = mapLabelToCompany(lab);
    if(!id){ server.send(404, "text/plain", "NOT_FOUND"); return; }
    bool changed=false; if(cfg.companyId != id){ cfg.companyId=id; changed=true; }
    if(strcmp(cfg.operatingMode,"wifi")==0){ strlcpy(cfg.operatingMode, "dual", sizeof(cfg.operatingMode)); changed=true; }
    if(changed && onConfigChanged) onConfigChanged();
    LOGF("[Sched] Force label=%s cid=%04X mode=%s\n", lab.c_str(), cfg.companyId, cfg.operatingMode);
    strlcpy(lastAppliedLabel, lab.c_str(), sizeof(lastAppliedLabel));
    // Return JSON so UI can update Company ID field and mode display without reload
    char json[96];
    snprintf(json, sizeof(json), "{\"status\":\"OK\",\"id\":\"%04X\",\"mode\":\"%s\"}", cfg.companyId, cfg.operatingMode);
    server.send(200, "application/json", json);
}

void WiFiManagerSimple::handleCidAdd(){
    if(!(server.hasArg("cat") && server.hasArg("name") && server.hasArg("id"))){ server.send(400, "application/json", "{\"status\":\"BAD_REQ\"}"); return; }
    String cat=server.arg("cat"); String name=server.arg("name"); String id=server.arg("id");
    cat.trim(); name.trim(); id.trim(); id.toUpperCase();
    if(!(cat=="L"||cat=="D"||cat=="Lab"||cat=="Amfi"||cat=="Amfi Derslik")){ server.send(400, "application/json", "{\"status\":\"CAT\"}"); return; }
    if(id.length()<4){ server.send(400, "application/json", "{\"status\":\"HEX\"}"); return; }
    for(int i=0;i< (int)id.length(); i++){ char c = id[i]; if(!((c>='0' && c<='9') || (c>='A' && c<='F'))){ server.send(400, "application/json", "{\"status\":\"HEX\"}"); return; }}
    if(idExists(id)){ server.send(400, "application/json", "{\"status\":\"DUP\"}"); return; }
    if(companyCount >= MAX_COMPANY){ server.send(500, "application/json", "{\"status\":\"FULL\"}"); return; }
    companies[companyCount++] = CompanyEntry{cat, name, id};
    saveCompanies();
    server.send(200, "application/json", "{\"status\":\"OK\"}");
}

void WiFiManagerSimple::handleCidDel(){
    if(!server.hasArg("id")){ server.send(400, "application/json", "{\"status\":\"BAD_REQ\"}"); return; }
    String id=server.arg("id"); id.trim(); id.toUpperCase();
    for(int i=0;i<companyCount;i++){
        if(companies[i].id.equalsIgnoreCase(id)){
            for(int j=i+1;j<companyCount;j++) companies[j-1]=companies[j];
            companyCount--; saveCompanies(); server.send(200, "application/json", "{\"status\":\"OK\"}"); return;
        }
    }
    server.send(404, "application/json", "{\"status\":\"NOT_FOUND\"}");
}

void WiFiManagerSimple::handleCidUpdate(DeviceConfig& /* cfg */){
    if(!(server.hasArg("old_id") && server.hasArg("id") && server.hasArg("name") && server.hasArg("cat"))){ server.send(400, "application/json", "{\"status\":\"BAD_REQ\"}"); return; }
    String oldId = server.arg("old_id"); String id = server.arg("id"); String name = server.arg("name"); String cat = server.arg("cat");
    oldId.trim(); id.trim(); name.trim(); cat.trim(); oldId.toUpperCase(); id.toUpperCase();
    if(!(cat=="L"||cat=="D"||cat=="Lab"||cat=="Amfi"||cat=="Amfi Derslik")){ server.send(400, "application/json", "{\"status\":\"CAT\"}"); return; }
    if(id.length()<4){ server.send(400, "application/json", "{\"status\":\"HEX\"}"); return; }
    for(int i=0;i< (int)id.length(); i++){ char c = id[i]; if(!((c>='0' && c<='9') || (c>='A' && c<='F'))){ server.send(400, "application/json", "{\"status\":\"HEX\"}"); return; }}
    int found=-1; for(int i=0;i<companyCount;i++) if(companies[i].id.equalsIgnoreCase(oldId)) { found=i; break; }
    if(found<0){ server.send(404, "application/json", "{\"status\":\"NOT_FOUND\"}"); return; }
    for(int i=0;i<companyCount;i++) if(i!=found && companies[i].id.equalsIgnoreCase(id)){ server.send(400, "application/json", "{\"status\":\"DUP\"}"); return; }
    companies[found].id = id; companies[found].name = name; companies[found].category = cat; saveCompanies(); server.send(200, "application/json", "{\"status\":\"OK\"}");
}

void WiFiManagerSimple::handleNow(DeviceConfig& cfg){
    if(cfg.epochBase==0){ server.send(200, "application/json", "{\"ok\":false,\"utc_epoch\":0,\"local_epoch\":0,\"tz\":" + String(cfg.timezoneOffset) + ",\"quality\":0,\"stale\":true}"); return; }
    unsigned long nowUtc = cfg.epochBase;
    if(ntpSetMs) nowUtc += (millis() - ntpSetMs)/1000UL; else nowUtc += millis()/1000UL; // fallback progression
    unsigned long local = nowUtc + (unsigned long)cfg.timezoneOffset*60UL;
    bool stale = bootTimeFlag && cfg.timeQuality < 2; // henüz taze NTP ya da manuel kaynak yok
    String out = String('{') + "\"ok\":true,\"utc_epoch\":" + nowUtc + ",\"local_epoch\":" + local + ",\"tz\":" + cfg.timezoneOffset + ",\"quality\":" + cfg.timeQuality + ",\"stale\":" + (stale?"true":"false") + '}';
    server.send(200, "application/json", out);
}

void WiFiManagerSimple::handleConfigGet(DeviceConfig& cfg){
    auto esc = [](const char* s){ String o; for(size_t i=0;s && s[i]; ++i){ char c=s[i]; if(c=='\\' || c=='"'){ o += '\\'; o += c; } else if((unsigned char)c < 0x20){ /* skip control */ } else { o += c; } } return o; };
    char cid[6]; sprintf(cid, "%04X", cfg.companyId);
    String json = "{";
    json += "\"deviceName\":\"" + esc(cfg.deviceName) + "\",";
    json += "\"operatingMode\":\"" + esc(cfg.operatingMode) + "\",";
    json += "\"companyId\":\""; json += cid; json += "\",";
    json += "\"staSsid\":\"" + esc(cfg.staSsid) + "\",";
    json += "\"staPass\":\"" + esc(cfg.staPass) + "\",";
    json += "\"apSsid\":\"" + esc(cfg.apSsid) + "\",";
    json += "\"apPass\":\"" + esc(cfg.apPass) + "\",";
    // LED profiles
    json += "\"led\":{"; 
    json += "\"wifi\":{\"mode\":\"" + esc(cfg.ledModeWifi) + "\",\"speed\":" + String(cfg.ledSpeedWifi) + "},";
    json += "\"ble\":{\"mode\":\"" + esc(cfg.ledModeBle) + "\",\"speed\":" + String(cfg.ledSpeedBle) + "},";
    json += "\"dual\":{\"mode\":\"" + esc(cfg.ledModeDual) + "\",\"speed\":" + String(cfg.ledSpeedDual) + "},";
    json += "\"dualsta\":{\"mode\":\"" + esc(cfg.ledModeDualSta) + "\",\"speed\":" + String(cfg.ledSpeedDualSta) + "}},";
    // WiFi state LEDs
    json += "\"wifiState\":{";
    json += "\"conn\":{\"mode\":\"" + esc(cfg.ledModeWifiConn) + "\",\"speed\":" + String(cfg.ledSpeedWifiConn) + "},";
    json += "\"ok\":{\"mode\":\"" + esc(cfg.ledModeWifiOk) + "\",\"speed\":" + String(cfg.ledSpeedWifiOk) + "},";
    json += "\"err\":{\"mode\":\"" + esc(cfg.ledModeWifiErr) + "\",\"speed\":" + String(cfg.ledSpeedWifiErr) + "}}";
    json += "}";
    server.send(200, "application/json", json);
}

void WiFiManagerSimple::handleCompanyOptions(){
    String out; buildCompanyOptions(out); server.send(200, "text/html", out);
}

void WiFiManagerSimple::handleActiveLabel(DeviceConfig& cfg){
    String lab = currentActiveLabel(cfg);
    String out = String("{\"label\":") + (lab.length()? (String("\"") + lab + "\"") : String("null")) + "}";
    server.send(200, "application/json", out);
}

String WiFiManagerSimple::getContentType(const String& path){
    if(path.endsWith(".html")) return "text/html";
    if(path.endsWith(".htm"))  return "text/html";
    if(path.endsWith(".css"))  return "text/css";
    if(path.endsWith(".js"))   return "application/javascript";
    if(path.endsWith(".json")) return "application/json";
    if(path.endsWith(".png"))  return "image/png";
    if(path.endsWith(".jpg")||path.endsWith(".jpeg")) return "image/jpeg";
    if(path.endsWith(".gif"))  return "image/gif";
    if(path.endsWith(".svg"))  return "image/svg+xml";
    if(path.endsWith(".ico"))  return "image/x-icon";
    if(path.endsWith(".txt"))  return "text/plain";
    if(path.endsWith(".woff")) return "font/woff";
    if(path.endsWith(".woff2"))return "font/woff2";
    if(path.endsWith(".ttf"))  return "font/ttf";
    return "application/octet-stream";
}

bool WiFiManagerSimple::tryServeFromFS(String uri){
    // Normalize
    if(uri.endsWith("/")) uri += "index.html";
    if(uri.indexOf("..")>=0) return false; // simple path traversal guard
    // Try direct path first (files may be at root), otherwise try under /web/ prefix
    String tryPath = uri;
    if(!LittleFS.exists(tryPath)){
        // if requested already has /web prefix, don't double-prefix
        if(!tryPath.startsWith("/web/")) tryPath = String("/web") + uri;
    }
    if(!LittleFS.exists(tryPath)) return false;
    File f = LittleFS.open(tryPath, "r");
    if(!f) return false;
    String ct = getContentType(uri); // determine content type by requested URI
    server.streamFile(f, ct);
    f.close();
    return true;
}

void WiFiManagerSimple::mountRoutes(DeviceConfig& cfg){
    // Root: serve static index only
    server.on("/", HTTP_GET, [this](){
        if(!tryServeFromFS("/index.html")) server.send(404, "text/plain", "index.html not found in LittleFS");
    });
    // Provide JSON/config endpoints consumed by static UI
    server.on("/config.json", HTTP_GET, [this,&cfg](){ handleConfigGet(cfg); });
    server.on("/company_options", HTTP_GET, [this](){ handleCompanyOptions(); });
    server.on("/active_label", HTTP_GET, [this,&cfg](){ handleActiveLabel(cfg); });
    server.on("/config", HTTP_POST, [this,&cfg](){ handleConfig(cfg); });
    server.on("/time", HTTP_POST, [this,&cfg](){ handleTime(cfg); });
    // Company add/delete/update endpoints (persist to single file)
    server.on("/cid_add", HTTP_POST, [this,&cfg](){ handleCidAdd(); });
    server.on("/cid_del", HTTP_POST, [this,&cfg](){ handleCidDel(); });
    server.on("/cid_update", HTTP_POST, [this,&cfg](){ handleCidUpdate(cfg); });
    server.on("/schedule_data", HTTP_GET, [this](){ handleScheduleData(); });
    server.on("/schedule_save", HTTP_POST, [this](){ handleScheduleSave(); });
    server.on("/schedule_force", HTTP_POST, [this,&cfg](){ handleScheduleForce(cfg); });
    server.on("/now", HTTP_GET, [this,&cfg](){ handleNow(cfg); });
    // Static files for all other paths
    server.onNotFound([this](){
        String uri = server.uri();
        if(tryServeFromFS(uri)) return;
        server.send(404, "text/plain", "Not found");
    });
    loadCompanies();
    loadSchedule();
}
