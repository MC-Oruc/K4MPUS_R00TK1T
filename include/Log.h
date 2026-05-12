// Simple logging macros. Define DEBUG_LOG before including to enable.
#pragma once
#include <Arduino.h>

// WiFi durum sabitleri bazı dosyalarda henüz dahil edilmemiş olabilir.
// Burada mümkünse WiFi.h'yi ekliyoruz; yoksa case'leri şartlı derliyoruz.
#if __has_include(<WiFi.h>)
#include <WiFi.h>
#endif

#ifndef LOG_LEVEL
#define LOG_LEVEL 1 // 0 = off, 1 = info, 2 = verbose
#endif

#if LOG_LEVEL > 0
  #define LOG(msg)        do { Serial.println(F(msg)); } while(0)
  #define LOGF(fmt, ...)  do { Serial.printf((const char*)F(fmt), __VA_ARGS__); } while(0)
#else
  #define LOG(msg)        do {} while(0)
  #define LOGF(fmt, ...)  do {} while(0)
#endif

#if LOG_LEVEL > 1
  #define VLOG(msg)       LOG(msg)
  #define VLOGF(fmt, ...) LOGF(fmt, __VA_ARGS__)
#else
  #define VLOG(msg)       do {} while(0)
  #define VLOGF(fmt, ...) do {} while(0)
#endif

inline const char* wifiStatusName(int s){
    switch(s){
#ifdef WL_IDLE_STATUS
  case WL_IDLE_STATUS: return "IDLE";
#endif
#ifdef WL_NO_SSID_AVAIL
  case WL_NO_SSID_AVAIL: return "NO_SSID";
#endif
#ifdef WL_SCAN_COMPLETED
  case WL_SCAN_COMPLETED: return "SCAN_DONE";
#endif
#ifdef WL_CONNECTED
  case WL_CONNECTED: return "CONNECTED";
#endif
#ifdef WL_CONNECT_FAILED
  case WL_CONNECT_FAILED: return "CONNECT_FAILED";
#endif
#ifdef WL_CONNECTION_LOST
  case WL_CONNECTION_LOST: return "CONNECTION_LOST";
#endif
#ifdef WL_DISCONNECTED
  case WL_DISCONNECTED: return "DISCONNECTED";
#endif
  // Fallback numeric mapping commonly used (0..6) if macros missing
  case 0: return "IDLE(0)";
  case 1: return "NO_SSID(1)";
  case 2: return "SCAN_DONE(2)";
  case 3: return "CONNECTED(3)";
  case 4: return "CONNECT_FAILED(4)";
  case 5: return "CONN_LOST(5)";
  case 6: return "DISCONNECTED(6)";
  default: return "UNKNOWN";
    }
}
