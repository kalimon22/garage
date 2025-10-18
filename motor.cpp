#include <Arduino.h>
#include <Preferences.h>
#include "motor.h"
#include "config.h"
#include "state.h"
#include "logx.h"   // <-- para logPrintf

// -----------------------
// Persistencia (NVS)
// -----------------------
static Preferences prefsMotor;
static const char* NVS_NS_MOTOR = "motor";
static const char* KEY_VEL_BASE = "velBase";  // 0..100

// -----------------------
// Estado interno del motor
// -----------------------
static int  speedPercent = 0;   // velocidad actual aplicada (0..100)
static int  speedTarget  = 0;   // objetivo efectivo (0..100) tras aplicar modo lento
static int  baseTarget   = 0;   // objetivo base (0..100) antes de factor de ralentización
static bool slowMode     = false; // si true, se aplica el factor de ralentización al target

// FSM de sentido con interlock (dead-time)
enum Dir : uint8_t { DIR_NONE=0, DIR_OPEN=1, DIR_CLOSE=2 };
static Dir desiredDir = DIR_NONE;     // lo que se quiere (según estado)
static Dir actualDir  = DIR_NONE;     // lo que está aplicado a los pines
static uint32_t tDirChange = 0;       // marca de tiempo de última transición a DIR_NONE

// -----------------------
// Utilidades
// -----------------------
static int clamp01_100(int percent) {
  if (percent < 0)   return 0;
  if (percent > 100) return 100;
  return percent;
}

static int calcDuty(int percent) {
  percent = clamp01_100(percent);
  const int maxDuty = (1 << MOTOR_PWM_RES) - 1;  // p.ej., 255 si resolución=8
  return (percent * maxDuty) / 100;
}

// Quita PWM de ambos canales (helper local)
static void applyStopOutputs() {
  ledcWrite(MOTOR_RPWM_PIN, 0);
  ledcWrite(MOTOR_LPWM_PIN, 0);
}

// Aplica PWM al canal de abrir
static void applyOpenOutputs(int percent) {
  ledcWrite(MOTOR_RPWM_PIN, calcDuty(percent));
  ledcWrite(MOTOR_LPWM_PIN, 0);
}

// Aplica PWM al canal de cerrar
static void applyCloseOutputs(int percent) {
  ledcWrite(MOTOR_RPWM_PIN, 0);
  ledcWrite(MOTOR_LPWM_PIN, calcDuty(percent));
}

// Recalcula el objetivo efectivo (speedTarget) a partir de baseTarget y slowMode
static void refresh_effective_target() {
  int eff = baseTarget;
  if (slowMode) {
    // velocidad efectiva = base * (MOTOR_SLOWDOWN_FACTOR_PERCENT / 100)
    eff = (eff * MOTOR_SLOWDOWN_FACTOR_PERCENT) / 100;
  }
  speedTarget = clamp01_100(eff);
}

bool motor_isSlowMode() {
  return slowMode;
}

// -----------------------
// API pública (modo lento / velocidades)
// -----------------------
void motor_set_slow(bool on) {
  slowMode = on;
  refresh_effective_target(); // ajusta speedTarget en función del modo
}

// ¡IMPORTANTE! Ya NO escribimos PWM directo aquí; solo actualizamos estado.
// El tick se encarga de aplicar salidas respetando interlock.
void motor_set_speed(int percent) {
  percent      = clamp01_100(percent);
  speedPercent = percent;
  baseTarget   = percent;

  // Persistir base SIEMPRE que cambie
  prefsMotor.putInt(KEY_VEL_BASE, baseTarget);

  refresh_effective_target();
}

int motor_get_speed() {
  return speedPercent;
}

void motor_set_speed_target(int percent) {
  baseTarget = clamp01_100(percent);

  // Persistir base
  prefsMotor.putInt(KEY_VEL_BASE, baseTarget);

  refresh_effective_target();
}

int motor_get_speed_target() {
  return speedTarget; // (en boot slowMode=false, equivale a base)
}

// -----------------------
// Control de pines / inicio
// -----------------------
void motor_begin() {
  // Cargar persistencia
  prefsMotor.begin(NVS_NS_MOTOR, false);
  baseTarget   = clamp01_100((int)prefsMotor.getInt(KEY_VEL_BASE, 0));
  speedTarget  = baseTarget;
  speedPercent = baseTarget;   // opcional: igualamos al objetivo al arrancar
  slowMode     = false;
  refresh_effective_target();

  pinMode(MOTOR_REN_PIN, OUTPUT);
  pinMode(MOTOR_LEN_PIN, OUTPUT);
  digitalWrite(MOTOR_REN_PIN, HIGH); // habilita driver lado R
  digitalWrite(MOTOR_LEN_PIN, HIGH); // habilita driver lado L

  // Configura PWM en ambos canales (API core 3.x ESP32)
  ledcAttach(MOTOR_RPWM_PIN, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
  ledcAttach(MOTOR_LPWM_PIN, MOTOR_PWM_FREQ, MOTOR_PWM_RES);

  // Arranque en stop
  desiredDir = DIR_NONE;
  actualDir  = DIR_NONE;
  tDirChange = millis();
  applyStopOutputs();
}

// -----------------------
// Acciones directas (solo fijan "deseado")
// -----------------------
void motorOpen()  { desiredDir = DIR_OPEN;  }
void motorClose() { desiredDir = DIR_CLOSE; }

// Parada normal (llamada “camelCase” ya existente)
void motorStop()  {
  desiredDir = DIR_NONE;
  // Aplicamos parada inmediata y arrancamos dead-time
  if (actualDir != DIR_NONE) {
    applyStopOutputs();
    actualDir  = DIR_NONE;
    tDirChange = millis();
  }
  logPrintln("[MOTOR] STOP");
}

// Alias en snake_case para compatibilidad con otros módulos
void motor_stop() {
  motorStop();
}

// Parada de emergencia: corta salidas YA, limpia estados y deja rampa a 0
void motor_emergency_stop() {
  desiredDir   = DIR_NONE;   // nadie desea mover
  actualDir    = DIR_NONE;   // reflejo inmediato
  applyStopOutputs();        // PWM a 0 en ambos canales

  // Si quisieras “corte duro” del puente H, descomenta:
  // digitalWrite(MOTOR_REN_PIN, LOW);
  // digitalWrite(MOTOR_LEN_PIN, LOW);

  tDirChange   = millis();   // arranca dead-time para un próximo arranque
  speedPercent = 0;          // la rampa parte de 0 tras emergencia
  logPrintln("[MOTOR] EMERGENCY STOP");
}

// -----------------------
// Rampa + interlock (llamar cada MOTOR_TICK_MS ms)
// -----------------------
void motor_tick() {
  static uint32_t tObstaculo = 0;
  static bool retroStarted = false;
  const uint32_t now = millis();

  // --- NUEVO BLOQUE: manejo del estado OBSTÁCULO ---
  if (getEstado() == OBSTACULO) {
    if (tObstaculo == 0) {
      // Primer tick en estado OBSTÁCULO → parar el motor
      applyStopOutputs();
      actualDir = DIR_NONE;
      desiredDir = DIR_NONE;
      tDirChange = now;
      tObstaculo = now;
      retroStarted = false;
      Serial.println("[OBSTACULO] Motor detenido por sobrecorriente");
      return; // salimos del tick normal
    }

    // Espera breve antes de invertir sentido
    if (!retroStarted && (now - tObstaculo) > 200) {
      motor_set_slow(true);          // opcional: retroceso lento
      applyOpenOutputs(speedTarget); // abrir un poco
      actualDir = DIR_OPEN;
      desiredDir = DIR_OPEN;
      retroStarted = true;
      Serial.println("[OBSTACULO] Retroceso iniciado");
      return; // seguimos retrocediendo en próximos ticks
    }

    // Después de ~1000 ms de retroceso, detener
    if (retroStarted && (now - tObstaculo) > 2000) {
      applyStopOutputs();
      actualDir = DIR_NONE;
      desiredDir = DIR_NONE;
      tDirChange = now;
      tObstaculo = 0;
      retroStarted = false;
      motor_set_slow(false);         // volver a velocidad normal
      setEstado(DETENIDO);
      Serial.println("[OBSTACULO] Retroceso completado, motor detenido");
      return;
    }

    // Mientras esté en OBSTÁCULO, no se ejecuta nada más
    return;
  }
  // --- FIN BLOQUE NUEVO -
  // 1) Sincroniza "desiredDir" con el estado global, por si alguien llamó setEstado()
  switch (getEstado()) {
    case ABRIENDO:  desiredDir = DIR_OPEN;  break;
    case CERRANDO:  desiredDir = DIR_CLOSE; break;
    default:        desiredDir = DIR_NONE;  break;
  }

  // 2) Rampa hacia el objetivo efectivo
  if (speedPercent != speedTarget) {
    if (speedPercent < speedTarget) {
      speedPercent = min(speedPercent + MOTOR_RAMP_STEP_PERCENT, speedTarget);
    } else {
      speedPercent = max(speedPercent - MOTOR_RAMP_STEP_PERCENT, speedTarget);
    }
  }

  // 3) Interlock de inversión: para > espera dead-time > aplica nuevo sentido

  if (desiredDir != actualDir) {
    if (actualDir != DIR_NONE) {
      // Estábamos aplicando un sentido: soltar primero
      applyStopOutputs();
      actualDir  = DIR_NONE;
      tDirChange = now;
      return; // damos una vuelta de tick en stop antes de seguir
    }

    // Aquí estamos en DIR_NONE; esperamos dead-time antes de enganchar el nuevo
    if ((now - tDirChange) < MOTOR_REVERSE_DEADTIME_MS) {
      applyStopOutputs();
      return;
    }

    // Ya pasó el dead-time: engancha nuevo sentido
    if (desiredDir == DIR_OPEN) {
      applyOpenOutputs(speedPercent);
      actualDir = DIR_OPEN;
    } else if (desiredDir == DIR_CLOSE) {
      applyCloseOutputs(speedPercent);
      actualDir = DIR_CLOSE;
    } else {
      applyStopOutputs();
      actualDir = DIR_NONE;
    }
    return;
  }

  // 4) Mantener salidas según sentido actual
  if (actualDir == DIR_OPEN) {
    applyOpenOutputs(speedPercent);
  } else if (actualDir == DIR_CLOSE) {
    applyCloseOutputs(speedPercent);
  } else {
    applyStopOutputs();
  }
}
