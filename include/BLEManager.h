#pragma once
#include <Arduino.h>
#include <BTstackLib.h>
#include "Log.h"

class BLEManager {
    uint8_t advData[30] = {
        0x02,0x01,0x04,
        0x04,0xFF,0x47,0x00,0xFF,
        0x15,0x09,
        'T','R','A','K','Y','A',' ','U','N','I',' ','K','A','M','P','U','S','4','.','0'
    };
    bool active = false;
public:
    void begin(const char* name);
    void updateCompany(uint16_t companyId);
    void updateName(const char* name);
    void stop();
    void loop();
    bool isActive() const;
private:
    void updateAdv();
};
