#pragma once
#include <stdint.h>

void ota_enable_for(uint32_t minutes);   // activa OTA X minutos
void ota_disable();                       // desactiva OTA
void ota_tick();   
// NUEVO:
bool     ota_is_on();
uint32_t ota_seconds_left();  // 0 si está OFF