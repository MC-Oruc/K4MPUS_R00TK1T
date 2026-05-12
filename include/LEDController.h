#pragma once
#include <Arduino.h>

class LedController {
    int pin;
    unsigned long lastToggle = 0;
    bool state = false;
public:
    char mode[10] = "blink";
    int speed = 500;
    LedController(int p);
    void apply(const char* m, int s);
    void loop();
};
