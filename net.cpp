#include "net.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <math.h>
#include "secrets.h"
#include "config.h"
#include "logx.h"
#include "ota_ctl.h"
#include "state.h"
#include "current.h"
#include "motor.h"
#include "hall.h"

// WiFi reconexión con backoff
static bool ipShown = false;
static uint32_t tNextRetry = 0;
static uint32_t retryDelayMs = 1000;
static const uint32_t RETRY_DELAY_MAX = 60 * 1000;

static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);

static bool mqttEnsureConnected();

bool net_wifi_connected() {
  return WiFi.status() == WL_CONNECTED;
}
bool net_mqtt_connected() {
  return mqtt.connected();
}

void net_mqtt_publish(const char *topic, const String &payload, bool retain) {
  if (!mqtt.connected())
    return;
  mqtt.publish(topic, payload.c_str(), retain);
}

static void handleCmd(const String &msg) {
  if (msg.startsWith("ota on")) {
    uint32_t minutos = 10;
    int sp = msg.indexOf(' ', 6);
    if (sp > 0) {
      String mins = msg.substring(sp + 1);
      mins.trim();
      if (mins.length())
        minutos = (uint32_t)mins.toInt();
      if (minutos == 0)
        minutos = 1;
    }
    ota_enable_for(minutos);
  } else if (msg == "ota off") {
    ota_disable();
  } else if (msg == "status") {
    String ip = net_wifi_connected() ? WiFi.localIP().toString() : "-";
    bool ota = ota_is_on();
    uint32_t left = ota_seconds_left();
    float ilimit = current_get_limit();

    String st = String("{\"wifi\":\"") + (net_wifi_connected() ? "ON" : "OFF") + "\",\"ip\":\"" + ip + "\",\"ota\":\"" + (ota ? "ON" : "OFF") + "\",\"left\":" + String(left) + ",\"ilimit\":" + String(ilimit, 2) + ",\"open_pulses\":" + String(hall_open_pulses) + "}";

    net_mqtt_publish(TOPIC_INFO, st, false);
  } else if (msg == "reboot") {
    logPrintln("[SYS] Reiniciando por MQTT...");
    delay(100);
    ESP.restart();
  } else {
    logPrintln("[MQTT] Comando no reconocido en TOPIC_CMD.");
  }
}

// Publica el límite actual en el topic de estado (retained)
static void publish_current_limit() {
  net_mqtt_publish(TOPIC_ILIMIT, String(current_get_limit(), 2), true);
}

static void mqttCallback(char *topic, byte *payload, unsigned int length) {
  String t(topic);
  String msg;
  msg.reserve(length);
  for (unsigned int i = 0; i < length; i++)
    msg += (char)payload[i];
  msg.trim();

  if (t == TOPIC_CMD) {
    logPrintf("[MQTT] cmd: %s\n", msg.c_str());
    handleCmd(msg);
  } else if (t == TOPIC_OPEN_CMD) {
    if (msg.equalsIgnoreCase("ON"))
      setEstado(ABRIENDO);
    else
      setEstado(DETENIDO);
  } else if (t == TOPIC_CLOSE_CMD) {
    if (msg.equalsIgnoreCase("ON"))
      setEstado(CERRANDO);
    else
      setEstado(DETENIDO);
  } else if (t == TOPIC_ILIMIT_CMD) {
    float nuevo = msg.toFloat();
    if (nuevo > 0.0f && isfinite(nuevo)) {
      float actual = current_get_limit();
      if (fabs(actual - nuevo) > 0.01f) {
        current_set_limit(nuevo);
        logPrintf("[MQTT] Nuevo límite de corriente: %.2f A\n", nuevo);
        publish_current_limit();
      }
    } else {
      logPrintf("[MQTT] Valor de límite inválido: '%s'\n", msg.c_str());
    }
  } else if (t == TOPIC_SPEED_CMD) {
    int nuevo = msg.toInt();
    if (nuevo >= 0 && nuevo <= 100) {
      int actual = motor_get_speed_target();
      if (actual != nuevo) {
        motor_set_speed_target(nuevo);  // cambia objetivo base (persistirá)
        net_mqtt_publish(TOPIC_SPEED, String(nuevo), true);
        logPrintf("[MQTT] Nueva velocidad objetivo: %d%%\n", nuevo);
      }
    } else {
      logPrintf("[MQTT] Valor de velocidad inválido: '%s'\n", msg.c_str());
    }
  } else if (t == TOPIC_MARK_CLOSED_CMD) {
    if (msg.equalsIgnoreCase("ON")) {
      hall_mark_closed();
      logPrintln("[MQTT] Marcado CERRADO en posición actual (encCount=0).");
    }
  } else if (t == TOPIC_MARK_OPEN_CMD) {
    if (msg.equalsIgnoreCase("ON")) {
      hall_mark_open();
      logPrintf("[MQTT] Marcado ABIERTO en posición actual (encCount=%ld).\n", hall_get_count());
    }
  } else if (t == TOPIC_HALL_EN_CMD) {
    if (msg.equalsIgnoreCase("ON")) {
      hall_set_enabled(true);
      net_mqtt_publish(TOPIC_HALL_EN_STATE, "ON", true);
      logPrintln("[HALL] enabled=ON");
    } else if (msg.equalsIgnoreCase("OFF")) {
      hall_set_enabled(false);
      net_mqtt_publish(TOPIC_HALL_EN_STATE, "OFF", true);
      logPrintln("[HALL] enabled=OFF");
    }
  }
}

static bool mqttEnsureConnected() {
  if (!net_wifi_connected())
    return false;
  if (mqtt.connected())
    return true;

  mqtt.setServer(mqtt_host, mqtt_port);
  mqtt.setCallback(mqttCallback);

  String clientId = String("esp32-garaje-") + String((uint32_t)ESP.getEfuseMac(), HEX);
  logPrint("[MQTT] Conectando...");
  bool ok;
  if (mqtt_user && strlen(mqtt_user))
    ok = mqtt.connect(clientId.c_str(), mqtt_user, mqtt_pass);
  else
    ok = mqtt.connect(clientId.c_str());

  if (ok) {
    logPrintln(" OK");
    mqtt.subscribe(TOPIC_CMD);
    mqtt.subscribe(TOPIC_OPEN_CMD);
    mqtt.subscribe(TOPIC_CLOSE_CMD);
    mqtt.subscribe(TOPIC_ILIMIT_CMD);
    mqtt.subscribe(TOPIC_ILIMIT);
    mqtt.subscribe(TOPIC_SPEED_CMD);
    mqtt.subscribe(TOPIC_SPEED);
    mqtt.subscribe(TOPIC_MARK_CLOSED_CMD);
    mqtt.subscribe(TOPIC_MARK_OPEN_CMD);
    mqtt.subscribe(TOPIC_HALL_EN_CMD);

    net_mqtt_publish(TOPIC_HALL_EN_STATE, hall_is_enabled() ? "ON" : "OFF", true);
    net_mqtt_publish(TOPIC_INFO, String("{\"boot\":true}"), false);
    net_mqtt_publish(TOPIC_SPEED, String(motor_get_speed_target()), true);
    publish_current_limit();
    logx_on_mqtt_connected();
    return true;
  } else {
    logPrintf(" fallo (%d)\n", mqtt.state());
    return false;
  }
}

void net_begin() {
  Serial.begin(115200);
  delay(50);
  logPrintln("\n[BOOT] ESP32 Garaje");

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  WiFi.begin(ssid, password);
  retryDelayMs = 1000;
  tNextRetry = millis() + retryDelayMs;
  logPrintln("[WiFi] Conectando...");
}

void net_tick() {
  uint32_t now = millis();

  // =========================
  // 1) CONTROL LOCAL (SIEMPRE)
  // =========================

  // a) Detectar inicio de movimiento -> blanking sobrecorriente
  static uint32_t tMoveSince = 0;
  static int prevEstadoInt = -1;
  int eNow = (int)getEstado();
  if (eNow != prevEstadoInt) {
    if (eNow == ABRIENDO || eNow == CERRANDO) {
      tMoveSince = now;
    }
    prevEstadoInt = eNow;
  }

  // b) Rampa del motor (local)
  static uint32_t tLastMotor = 0;
  if (now - tLastMotor >= MOTOR_TICK_MS) {
    motor_tick();
    tLastMotor = now;
  }

  // c) Guardia de sobrecorriente (local)
  static uint32_t tLastIcheck = 0;
  static bool overIEvent = false;  // para avisar por MQTT si hay broker
  if ((eNow == ABRIENDO || eNow == CERRANDO) && (now - tMoveSince) >= CURRENT_BLANKING_MS && (now - tLastIcheck) >= CURRENT_CHECK_PERIOD_MS) {
    if (current_guard_stop_if_over()) {
      overIEvent = true;  // marcaremos evento para publicar si hay MQTT
      Serial.println("[I] Corte por sobrecorriente");
    }
    tLastIcheck = now;
  }

  // =====================================
  // 2) CONECTIVIDAD (solo si hay WiFi/MQTT)
  // =====================================

  // WiFi reconexión con backoff
  if (WiFi.status() != WL_CONNECTED) {
    if (now >= tNextRetry) {
      logPrint(".");
      WiFi.reconnect();
      retryDelayMs = (retryDelayMs < RETRY_DELAY_MAX) ? retryDelayMs * 2 : RETRY_DELAY_MAX;
      tNextRetry = now + retryDelayMs;
    }
    ipShown = false;
    return;  // sin WiFi, no seguimos con MQTT/telemetría
  }

  // WiFi OK
  if (!ipShown) {
    ipShown = true;
    logPrintf("\n[WiFi] Conectado: %s\n", WiFi.localIP().toString().c_str());
  }

  // MQTT
  if (!mqttEnsureConnected()) return;
  mqtt.loop();

  // =========================
  // 3) TELEMETRÍA MQTT (opcional)
  // =========================

  // Publicar corriente cada 2s
  static uint32_t tLastCurrent = 0;
  if (now - tLastCurrent >= 2000) {
    float I = current_readA();
    net_mqtt_publish(TOPIC_IMEAS, String(I, 2), false);
    tLastCurrent = now;
  }

  // Encoder cada 500 ms
  static uint32_t tLastEnc = 0;
  if (now - tLastEnc >= 500) {
    long pos = hall_get_count();
    int dir = hall_get_dir();
    net_mqtt_publish(TOPIC_ENC_POS, String(pos), false);
    net_mqtt_publish(TOPIC_ENC_DIR, String(dir), false);
    tLastEnc = now;
  }

  // Aviso si hubo corte por sobrecorriente (una sola vez por evento)
  if (overIEvent) {
    net_mqtt_publish(TOPIC_LOG, String("[I] Corte por sobrecorriente"), false);
    overIEvent = false;
  }
}
