#include "LEDController.h"

LedController::LedController(int p) : pin(p) {}

void LedController::apply(const char* m, int s){
    if(strcmp(mode, m)==0 && speed==s) return; // no change
    strlcpy(mode, m, sizeof(mode)); speed = s;
    if(strcmp(mode,"on")==0) digitalWrite(pin,HIGH);
    else if(strcmp(mode,"off")==0) digitalWrite(pin,LOW);
}

void LedController::loop(){
    unsigned long now = millis();
    if(strcmp(mode, "blink")==0){
        if(now - lastToggle >= (unsigned long) speed){ state = !state; digitalWrite(pin, state?HIGH:LOW); lastToggle = now; }
    } else if(strcmp(mode, "pulse")==0){
        if(!state){ if(now - lastToggle >= (unsigned long) speed){ state = true; digitalWrite(pin,HIGH); lastToggle = now; }}
        else { if(now - lastToggle >= 100){ state = false; digitalWrite(pin,LOW); lastToggle = now; }}
    }
}
