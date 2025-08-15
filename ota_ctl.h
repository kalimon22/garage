#pragma once
#include <stdint.h>

void ota_enable_for(uint32_t minutes);   // activa OTA X minutos
void ota_disable();                       // desactiva OTA
void ota_tick();   