#pragma once
#include <Arduino.h>

// ============================
// Configuración básica (en config.h)
// ============================
// #define HALL_PULSE_PIN   35        // Pin del ESP32 donde conectas el cable de PULSOS
// #define HALL_DIR_PIN     36        // Pin del ESP32 donde conectas el cable de DIRECCIÓN
// #define HALL_OPEN_PULSES_DEFAULT 5000   // Pulsos esperados para un ciclo completo (ajústalo a tu puerta)
// #define HALL_SLOWDOWN_THRESHOLD_PERCENT 10 // % al inicio/fin para ir en "lento"
// #define HALL_DIR_ACTIVE_HIGH_CLOSE 1  // 1 = nivel alto = CERRANDO ; 0 = nivel alto = ABRIENDO
// =================================

// Inicializa el sistema Hall (lee valores guardados en NVS, configura pines e interrupciones)
void hall_begin();

// Debe llamarse en loop() con millis() para:
// - Detener al llegar a los extremos virtuales
// - Activar/desactivar modo lento según posición
void hall_tick(unsigned long now);

// Devuelve el contador de pulsos actual
// - 0        = tope de CERRADO
// - max      = tope de ABIERTO
// - valores intermedios = posición relativa
long hall_get_count();

// Devuelve la última dirección detectada:
// +1 = ABRIENDO
// -1 = CERRANDO
//  0 = sin movimiento/pulsos
int hall_get_dir();

// Marca que se alcanzó el extremo CERRADO
// - pone encCount=0
// - guarda en NVS que el último extremo válido fue CERRADO
void hall_mark_closed();

// Marca que se alcanzó el extremo ABIERTO
// - pone encCount=posición actual
// - actualiza en NVS el número total de pulsos como "open_pulses"
// - guarda en NVS que el último extremo válido fue ABIERTO
void hall_mark_open();

// Límite superior configurable (se carga/guarda en NVS)
extern long hall_open_pulses;

// Nuevo: control de habilitación
void hall_set_enabled(bool on);
bool hall_is_enabled();

