#include "ModeController.h"

ModeController::ModeController(ConfigManager& c, BLEManager& b, WiFiManagerSimple& w, LedController& l): cfg(c), ble(b), wifi(w), led(l){}

void ModeController::begin(){
    attachCallbacks();
    if(!cfg.loaded){
        LOG("[Mode] UYARI: Config yuklenemedi, calisma durduruldu");
        return;
    }
    startCurrentMode();
}

void ModeController::loop(){
    if(!cfg.loaded){ delay(50); return; }
    handleButton();
    if(strcmp(cfg.data.operatingMode, "wifi")==0){
        wifi.loop();
    } else if(strcmp(cfg.data.operatingMode, "ble")==0){
        ble.loop();
    } else if(strcmp(cfg.data.operatingMode, "dual")==0){ // dual (AP + BLE)
        wifi.loop();
        ble.loop();
    } else if(strcmp(cfg.data.operatingMode, "dualsta")==0){ // dualsta (STA + BLE)
        wifi.loop();
        ble.loop();
        wifi.loopStationMonitor(cfg.data);
    }
    // LED state overrides for STA conditions
    if(strcmp(cfg.data.operatingMode, "dualsta")==0 || (strcmp(cfg.data.operatingMode, "wifi")==0 && wifi.isStation)){
        auto st = wifi.getStaLedState();
        if(st == WiFiManagerSimple::STA_OK){ led.apply(cfg.data.ledModeWifiOk, cfg.data.ledSpeedWifiOk); }
        else if(st == WiFiManagerSimple::STA_ERR){ led.apply(cfg.data.ledModeWifiErr, cfg.data.ledSpeedWifiErr); }
        else { led.apply(cfg.data.ledModeWifiConn, cfg.data.ledSpeedWifiConn); }
    }
    // Schedule enforcement (uses cfg time + timezone). Provide approximate epoch.
    wifi.enforceSchedule(cfg.data, ble);
    led.loop();
    delay(5);
}

void ModeController::attachCallbacks(){
    wifi.onConfigChanged = [this](){
        // Operating mode or LED/company/name may have changed via web UI.
        restartForMode();
        applyCurrentLed();
        ble.updateCompany(cfg.data.companyId);
        wifi.updateSSID(cfg.data);
        cfg.save();
    };
}

void ModeController::startCurrentMode(){
    applyCurrentLed();
    if(strcmp(cfg.data.operatingMode, "wifi")==0){
        LOG("[Mode] Baslangic WIFI");
        wifi.begin(cfg.data);
    } else if(strcmp(cfg.data.operatingMode, "ble")==0){
        LOG("[Mode] Baslangic BLE");
        ble.begin(cfg.data.deviceName); ble.updateCompany(cfg.data.companyId);
    } else if(strcmp(cfg.data.operatingMode, "dual")==0){ // AP + BLE
        LOG("[Mode] Baslangic DUAL(AP+BLE)");
        wifi.begin(cfg.data);
        ble.begin(cfg.data.deviceName); ble.updateCompany(cfg.data.companyId);
    } else if(strcmp(cfg.data.operatingMode, "dualsta")==0){ // STA + BLE
        LOG("[Mode] Baslangic DUAL-STA(STA+BLE)");
        wifi.beginStation(cfg.data); // new station mode
        ble.begin(cfg.data.deviceName); ble.updateCompany(cfg.data.companyId);
    }
}

void ModeController::toggleMode(){
    if(strcmp(cfg.data.operatingMode, "wifi")==0){
        // wifi -> ble
        LOG("[Mode] wifi -> ble");
        wifi.stop();
        strlcpy(cfg.data.operatingMode, "ble", sizeof(cfg.data.operatingMode));
        ble.begin(cfg.data.deviceName); ble.updateCompany(cfg.data.companyId);
    } else if(strcmp(cfg.data.operatingMode, "ble")==0){
        // ble -> dual (AP + BLE)
        LOG("[Mode] ble -> dual");
        wifi.begin(cfg.data); // ble already active
        strlcpy(cfg.data.operatingMode, "dual", sizeof(cfg.data.operatingMode));
    } else if(strcmp(cfg.data.operatingMode, "dual")==0){
        // dual -> dualsta (switch AP to STA)
        LOG("[Mode] dual -> dualsta");
        wifi.stop();
        wifi.beginStation(cfg.data);
        strlcpy(cfg.data.operatingMode, "dualsta", sizeof(cfg.data.operatingMode));
    } else if(strcmp(cfg.data.operatingMode, "dualsta")==0){
        // dualsta -> wifi (stop BLE, restart AP)
        LOG("[Mode] dualsta -> wifi");
        ble.stop();
        wifi.stop();
        strlcpy(cfg.data.operatingMode, "wifi", sizeof(cfg.data.operatingMode));
        wifi.begin(cfg.data);
    }
    cfg.save();
    applyCurrentLed();
    blinkConfirm();
}

void ModeController::blinkConfirm(){ for(int i=0;i<4;i++){ digitalWrite(LED_BUILTIN,HIGH); delay(80); digitalWrite(LED_BUILTIN,LOW); delay(80);} }

void ModeController::handleButton(){
    if(BOOTSEL){
        if(!buttonPressed && millis()-lastButton>debounce){ buttonPressed=true; pending=true; lastButton=millis(); }
    } else if(buttonPressed && millis()-lastButton>debounce){ buttonPressed=false; lastButton=millis(); if(pending){ toggleMode(); pending=false; }}
}

void ModeController::applyCurrentLed(){
    char m[10]; int s; cfg.getActiveLed(m,s); led.apply(m,s);
}

void ModeController::restartForMode(){
    // Ensure services match cfg.data.operatingMode
    if(strcmp(cfg.data.operatingMode, "wifi")==0){
        // Want only WiFi
        LOG("[Mode] restart -> wifi");
        ble.stop();
        if(!wifi.active) wifi.begin(cfg.data);
    } else if(strcmp(cfg.data.operatingMode, "ble")==0){
        LOG("[Mode] restart -> ble");
        wifi.stop();
        if(!ble.isActive()) ble.begin(cfg.data.deviceName), ble.updateCompany(cfg.data.companyId);
    } else if(strcmp(cfg.data.operatingMode, "dual")==0){ // AP + BLE
        LOG("[Mode] restart -> dual(AP+BLE)");
        if(!wifi.active) wifi.begin(cfg.data);
        if(!ble.isActive()) ble.begin(cfg.data.deviceName), ble.updateCompany(cfg.data.companyId);
    } else if(strcmp(cfg.data.operatingMode, "dualsta")==0){ // STA + BLE
        LOG("[Mode] restart -> dualsta");
        // For simplicity, restart wifi in station mode if not already.
        if(!wifi.active || !wifi.isStation){ wifi.stop(); wifi.beginStation(cfg.data); }
        if(!ble.isActive()) ble.begin(cfg.data.deviceName), ble.updateCompany(cfg.data.companyId);
    }
}
