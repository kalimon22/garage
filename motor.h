// motor.h
#pragma once
#include <stdint.h>

void motor_begin();
void motorOpen();
void motorClose();
void motorStop();
void motor_set_speed(int percent);
int  motor_get_speed();
void motor_set_speed_target(int percent); // velocidad objetivo 0..100
int  motor_get_speed_target();

void motor_tick(); // llámalo cada ~20 ms desde tu loop o net_tick

// Opcional: si quieres poder ajustar potencias en tiempo real, añade esto
// (y su implementación) más adelante.
// void motorSetPower(uint8_t dutyOpen, uint8_t dutyClose);
