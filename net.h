#pragma once
#include <Arduino.h>

void net_begin();              // iniciar WiFi + MQTT (siempre ON)
void net_tick();               // reconexiones, mqtt.loop()

bool net_wifi_connected();
bool net_mqtt_connected();

void net_mqtt_publish(const char* topic, const String& payload, bool retain=false);