#include <Arduino.h>
#include "motor.h"

// Configuración PWM (ajusta pines y canal)
const int motorPinA = 25; // ejemplo pin dirección A
const int motorPinB = 26; // ejemplo pin dirección B
const int pwmChannel = 0;
const int freq = 1000;
const int resolution = 8;
const int dutyCycleOpen = 200;  // ajusta potencia apertura
const int dutyCycleClose = 200; // ajusta potencia cierre

void motorSetup() {
  ledcSetup(pwmChannel, freq, resolution);
  ledcAttachPin(motorPinA, pwmChannel);
  pinMode(motorPinB, OUTPUT);
}

void motorOpen() {
  digitalWrite(motorPinB, LOW);
  ledcWrite(pwmChannel, dutyCycleOpen);
}

void motorClose() {
  digitalWrite(motorPinB, HIGH);
  ledcWrite(pwmChannel, dutyCycleClose);
}

void motorStop() {
  ledcWrite(pwmChannel, 0);
}