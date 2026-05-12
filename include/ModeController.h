#pragma once
#include "ConfigManager.h"
#include "BLEManager.h"
#include "WiFiManagerSimple.h"
#include "LedController.h"
#include "Log.h"

class ModeController {
    ConfigManager& cfg;
    BLEManager& ble;
    WiFiManagerSimple& wifi;
    LedController& led;
    bool buttonPressed=false; unsigned long lastButton=0; const unsigned long debounce=200; bool pending=false;
public:
    ModeController(ConfigManager& c, BLEManager& b, WiFiManagerSimple& w, LedController& l);
    void begin();
    void loop();

private:
    void attachCallbacks();
    void startCurrentMode();
    void toggleMode();
    void blinkConfirm();
    void handleButton();
    void applyCurrentLed();
    void restartForMode();
};
