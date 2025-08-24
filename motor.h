// motor.h
#pragma once

void motor_begin();
void motor_tick();

void motorOpen();
void motorClose();
void motorStop();

void motor_set_speed(int percent);        // fija velocidad actual y base (0..100)
int  motor_get_speed();

void motor_set_speed_target(int percent); // fija velocidad base (0..100)
int  motor_get_speed_target();

// NUEVO: activar/desactivar modo lento (aplica factor sobre la base)
void motor_set_slow(bool on);
