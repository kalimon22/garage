// motor.h
#pragma once

// =====================================================
//   Inicialización y ciclo de control
// =====================================================
void motor_begin();   // Inicializa pines, PWM y recupera config NVS
void motor_tick();    // Llamar cada MOTOR_TICK_MS ms (rampa + interlock)

// =====================================================
//   Control de movimiento
// =====================================================
void motorOpen();     // Pide abrir (respeta rampas e interlocks)
void motorClose();    // Pide cerrar
void motorStop();     // Parada normal (log [MOTOR] STOP)
void motor_stop();    // Alias snake_case de motorStop()
void motor_emergency_stop(); // Parada de emergencia (corte inmediato, log [MOTOR] EMERGENCY STOP)

// =====================================================
//   Velocidad
// =====================================================
void motor_set_speed(int percent);        // fija velocidad actual y base (0..100) y persiste en NVS
int  motor_get_speed();                   // devuelve velocidad actual aplicada (0..100)

void motor_set_speed_target(int percent); // fija velocidad base (0..100) y persiste en NVS
int  motor_get_speed_target();            // devuelve velocidad efectiva (base o ralentizada)

// =====================================================
//   Modo lento (slow / ultra-slow)
// =====================================================
void motor_set_slow(int level);           // 0=normal, 1=lento, 2=ultra-lento
void motor_set_slow(bool on);             // alias: true=nivel 1, false=nivel 0
bool motor_isSlowMode();
