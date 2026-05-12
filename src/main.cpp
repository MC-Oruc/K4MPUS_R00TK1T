#include <Arduino.h>

#include <LittleFS.h>
#include "ConfigManager.h"
#include "LEDController.h"
#include "BLEManager.h"
#include "WiFiManagerSimple.h"
#include "ModeController.h"
#include "Log.h"

ConfigManager config;
LedController ledController(LED_BUILTIN);
BLEManager bleManager;
WiFiManagerSimple wifiManager;
ModeController modeController(config, bleManager, wifiManager, ledController);

void setup(){
    Serial.begin(SERIAL_BAUD);
    unsigned long start = millis();
    while (!Serial && (millis() - start < SERIAL_WAIT_MS)) {
        delay(10);
    }
    
    LOG("[BOOT] Basliyor");
    pinMode(LED_BUILTIN, OUTPUT);
    if(!LittleFS.begin()) Serial.println("LittleFS baslatilamadi"); else config.load();
    // Config yoksa LED'i kapalı bırak, çalışma ModeController tarafından da başlamayacak
    if(config.loaded){ char lm[10]; int ls; config.getActiveLed(lm, ls); ledController.apply(lm, ls); }
    modeController.begin();
}

void loop(){
    modeController.loop();
}
