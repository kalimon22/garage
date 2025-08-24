#pragma once
#include <Arduino.h>

// Inicialización y calibración
void current_begin();

// Lectura de corriente instantánea (Amperios)
float current_readA();

// Verificación de sobrecorriente, para y cambia estado si excede límite
bool current_guard_stop_if_over();

// Configuración del límite de sobrecorriente (por MQTT o código)
void current_set_limit(float amps);

// Lectura del límite actual
float current_get_limit();