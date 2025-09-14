#pragma once
#include <Arduino.h>

void light_begin();
void light_tick(uint32_t now);

void light_on();
void light_off();
void light_toggle();
bool light_is_on();

void light_set_level(uint16_t level);   // 0..(2^res-1)
uint16_t light_get_level();
void light_set_percent(uint8_t pct);    // 0..100

// Nuevas funciones para modo respiración a full brightness
void light_set_breath_mode(bool enable);
bool light_get_breath_mode();