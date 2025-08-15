#pragma once
#include <Arduino.h>

void logPrint(const String& s);
void logPrintln(const String& s);
void logPrintf(const char* fmt, ...);

// Llamar desde net cuando MQTT conecta
void logx_on_mqtt_connected();
