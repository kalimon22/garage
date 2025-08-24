#pragma once

void hall_begin();
long hall_get_count();
int  hall_get_dir();

void hall_mark_closed();   // fija encCount = 0
void hall_mark_open();     // fija límite superior = encCount actual (y lo guarda en NVS)

void hall_tick(unsigned long now);

// Límite superior configurable (se carga/guarda en NVS)
extern long hall_open_pulses;
