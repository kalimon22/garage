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
static int  slowLevel    = 0;     // 0=normal, 1=lento(50%), 2=ultra-lento(15%)

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

// Recalcula el objetivo efectivo (speedTarget) a partir de baseTarget y slowLevel
static void refresh_effective_target() {
  int eff = baseTarget;
  if (slowLevel == 1) {
    // Nivel 1: ralentización normal (usamos el factor de config)
    eff = (eff * MOTOR_SLOWDOWN_FACTOR_PERCENT) / 100;
  } else if (slowLevel == 2) {
    // Nivel 2: ultra-lento para "aterrizaje" (usamos el mismo que arranque suave)
    eff = MOTOR_SOFTSTART_MAX_PERCENT;
  }
  speedTarget = clamp01_100(eff);
}

bool motor_isSlowMode() {
  return (slowLevel > 0);
}

// -----------------------
// API pública (modo lento / velocidades)
// -----------------------
void motor_set_slow(int level) {
  slowLevel = level;
  refresh_effective_target(); // ajusta speedTarget en función del modo
}

// Alias para compatibilidad con código anterior que pasaba bool
void motor_set_slow(bool on) {
  motor_set_slow(on ? 1 : 0);
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
  return speedTarget; 
}

// -----------------------
// Control de pines / inicio
// -----------------------
void motor_begin() {
  // Cargar persistencia
  prefsMotor.begin(NVS_NS_MOTOR, false);
  baseTarget   = clamp01_100((int)prefsMotor.getInt(KEY_VEL_BASE, 0));
  speedTarget  = baseTarget;
  speedPercent = 0;   // siempre arrancar desde 0 para que el soft-start actúe
  slowLevel    = 0;
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
  desiredDir   = DIR_NONE;
  speedPercent = 0;   // resetear para que el próximo arranque empiece desde 0
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
// Ayudante: Lógica de retroceso por obstáculo
// -----------------------
static void handle_obstaculo_logic(uint32_t now) {
  static uint32_t tObstaculo = 0;
  static bool     retroStarted = false;

  if (tObstaculo == 0) {
    applyStopOutputs();
    actualDir = DIR_NONE;
    tDirChange = now;
    tObstaculo = now;
    retroStarted = false;
    Serial.println("[OBSTACULO] Motor detenido");
    return;
  }

  // Espera 200ms antes de invertir sentido
  if (!retroStarted && (now - tObstaculo) > 200) {
    motor_set_slow(2);             // Retroceso ultra-lento (15%) para seguridad
    applyOpenOutputs(speedTarget); // Liberar obstáculo
    actualDir = DIR_OPEN;
    retroStarted = true;
    Serial.println("[OBSTACULO] Retroceso de seguridad...");
  }

  // Detener tras el tiempo configurado de retroceso y volver a reposo
  if (retroStarted && (now - tObstaculo) > MOTOR_OBSTACLE_RETREAT_MS) {
    applyStopOutputs();
    actualDir = DIR_NONE;
    tObstaculo = 0;
    retroStarted = false;
    motor_set_slow(0);
    setEstado(DETENIDO);
    Serial.println("[OBSTACULO] Maniobra completada");
  }
}

// -----------------------
// Rampa + interlock (llamar cada MOTOR_TICK_MS ms)
// -----------------------
void motor_tick() {
  const uint32_t now = millis();
  static uint32_t tMoveStart = 0;

  // 1. GESTIÓN DE OBSTÁCULOS
  if (getEstado() == OBSTACULO) {
    handle_obstaculo_logic(now);
    return;
  }

  // 2. SINCRONIZACIÓN DE DIRECCIÓN
  // Traducimos el estado global a una intención local (Abrir, Cerrar o Quieto)
  Dir targetDir = DIR_NONE;
  EstadoPuerta e = getEstado();
  if (e == ABRIENDO)      targetDir = DIR_OPEN;
  else if (e == CERRANDO) targetDir = DIR_CLOSE;

  // 3. SEGURIDAD: TIEMPO MUERTO (DEAD-TIME)
  // Si cambiamos de sentido, forzamos parada y esperamos el tiempo muerto (protege el puente H)
  if (targetDir != actualDir) {
    applyStopOutputs();
    if (actualDir != DIR_NONE) {
      tDirChange = now;
      actualDir = DIR_NONE;
      speedPercent = 0; 
    }

    // Solo arrancamos si ha pasado el tiempo de seguridad (80ms recomendado)
    if (actualDir == DIR_NONE && targetDir != DIR_NONE) {
      if (now - tDirChange >= MOTOR_REVERSE_DEADTIME_MS) {
        actualDir = targetDir;
        tMoveStart = now;
        speedPercent = MOTOR_START_PEDESTAL_PERCENT; // El motor nace con un mínimo de fuerza
        logPrintf("[MOTOR] Arrancando %s (Kick %d%%)\n", 
                  (actualDir == DIR_OPEN ? "OPEN" : "CLOSE"), MOTOR_START_PEDESTAL_PERCENT);
      }
    }
  }

  // 4. CONTROL DE VELOCIDAD (RAMPA Y LÍMITES)
  if (actualDir != DIR_NONE) {
    // Objetivo según sensores (Normal, 50% o 15% para el aterrizaje final)
    int target = speedTarget;

    // Filtro de arranque: primer segundo limitado por MOTOR_SOFTSTART_MAX_PERCENT
    if (now - tMoveStart < MOTOR_SOFTSTART_MS) {
      target = min(target, (int)MOTOR_SOFTSTART_MAX_PERCENT);
    }

    // Rampa: Aplicamos el cambio progresivo (subir/bajar 1% cada 20ms)
    if (speedPercent < target)      speedPercent = min(speedPercent + MOTOR_RAMP_STEP_PERCENT, target);
    else if (speedPercent > target) speedPercent = max(speedPercent - MOTOR_RAMP_STEP_PERCENT, target);

    // 5. SALIDA FÍSICA A LOS PINES
    if (actualDir == DIR_OPEN) applyOpenOutputs(speedPercent);
    else                       applyCloseOutputs(speedPercent);
  } 
  else {
    // Si la orden es estar detenidos, aseguramos parada total y reset de rampa
    applyStopOutputs();
    speedPercent = 0;
  }
}
