#include <Arduino.h>
#include "motor.h"
#include "config.h"
#include "state.h"   // para saber ABRIENDO/CERRANDO

static int speedPercent = 0;       // velocidad actual (0..100)
static int speedTarget  = 0;       // velocidad objetivo (0..100)

static int calcDuty(int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  int maxDuty = (1 << MOTOR_PWM_RES) - 1;  // p. ej. 255
  return (percent * maxDuty) / 100;
}

void motor_set_speed(int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  speedPercent = percent;
  speedTarget  = percent; // mantén objetivo igual
  // aplica de inmediato al canal activo
  auto e = getEstado();
  if (e == ABRIENDO) {
    ledcWrite(MOTOR_RPWM_PIN, calcDuty(speedPercent));
    ledcWrite(MOTOR_LPWM_PIN, 0);
  } else if (e == CERRANDO) {
    ledcWrite(MOTOR_RPWM_PIN, 0);
    ledcWrite(MOTOR_LPWM_PIN, calcDuty(speedPercent));
  }
}

int  motor_get_speed()        { return speedPercent; }
void motor_set_speed_target(int percent) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  speedTarget = percent;
}
int  motor_get_speed_target() { return speedTarget; }

void motor_begin() {
  pinMode(MOTOR_REN_PIN, OUTPUT);
  pinMode(MOTOR_LEN_PIN, OUTPUT);
  digitalWrite(MOTOR_REN_PIN, HIGH);
  digitalWrite(MOTOR_LEN_PIN, HIGH);

  ledcAttach(MOTOR_RPWM_PIN, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
  ledcAttach(MOTOR_LPWM_PIN, MOTOR_PWM_FREQ, MOTOR_PWM_RES);

  motorStop();
}

void motorOpen() {
  ledcWrite(MOTOR_RPWM_PIN, calcDuty(speedPercent));
  ledcWrite(MOTOR_LPWM_PIN, 0);
}

void motorClose() {
  ledcWrite(MOTOR_RPWM_PIN, 0);
  ledcWrite(MOTOR_LPWM_PIN, calcDuty(speedPercent));
}

void motorStop() {
  ledcWrite(MOTOR_RPWM_PIN, 0);
  ledcWrite(MOTOR_LPWM_PIN, 0);
}

// Llama cada ~20 ms para hacer la rampa
void motor_tick() {
  if (speedPercent == speedTarget) return;

  if (speedPercent < speedTarget) speedPercent = min(speedPercent + MOTOR_RAMP_STEP_PERCENT, speedTarget);
  else                            speedPercent = max(speedPercent - MOTOR_RAMP_STEP_PERCENT, speedTarget);

  auto e = getEstado();
  if (e == ABRIENDO) {
    ledcWrite(MOTOR_RPWM_PIN, calcDuty(speedPercent));
    ledcWrite(MOTOR_LPWM_PIN, 0);
  } else if (e == CERRANDO) {
    ledcWrite(MOTOR_RPWM_PIN, 0);
    ledcWrite(MOTOR_LPWM_PIN, calcDuty(speedPercent));
  }
  // En DETENIDO/ABIERTO/CERRADO no aplicamos duty; motorStop() ya dejó 0.
}
