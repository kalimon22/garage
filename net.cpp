#include "net.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"
#include "config.h"
#include "logx.h"
#include "ota_ctl.h"
#include "motor.h"
#include "display_ctl.h"  // nuevo para mostrar en pantalla

// WiFi reconexión con backoff
static bool     ipShown      = false;
static uint32_t tNextRetry   = 0;
static uint32_t retryDelayMs = 1000;
static const uint32_t RETRY_DELAY_MAX = 60 * 1000;

static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);

static bool mqttEnsureConnected();

bool net_wifi_connected() { return WiFi.status() == WL_CONNECTED; }
bool net_mqtt_connected() { return mqtt.connected(); }

void net_mqtt_publish(const char* topic, const String& payload, bool retain) {
  if (!mqtt.connected()) return;
  mqtt.publish(topic, payload.c_str(), retain);
}

static void setAndReportState(const char* state) {
  net_mqtt_publish(TOPIC_STATE, String(state), true);
  display_show_state(state); // actualiza el texto en la pantalla
}

static void handleCmd(const String& msg) {
  if (msg.startsWith("ota on")) {
    uint32_t minutos = 10;
    int sp = msg.indexOf(' ', 6);
    if (sp > 0) {
      String mins = msg.substring(sp+1); mins.trim();
      if (mins.length()) minutos = (uint32_t)mins.toInt();
      if (minutos == 0) minutos = 1;
    }
    ota_enable_for(minutos);
  }
  else if (msg == "ota off") {
    ota_disable();
  }
  else if (msg == "status") {
    String ip = net_wifi_connected() ? WiFi.localIP().toString() : "-";
    String st = String("{\"wifi\":\"") + (net_wifi_connected()?"ON":"OFF") +
                "\",\"ip\":\"" + ip + "\"}";
    net_mqtt_publish(TOPIC_INFO, st, false);
  }
  else if (msg == "reboot") {
    logPrintln("[SYS] Reiniciando por MQTT...");
    delay(100);
    ESP.restart();
  }
  else if (msg == "open_on") {
    motorOpen();
    setAndReportState("ABRIENDO");
  }
  else if (msg == "open_off") {
    motorStop();
    setAndReportState("DETENIDO");
  }
  else if (msg == "close_on") {
    motorClose();
    setAndReportState("CERRANDO");
  }
  else if (msg == "close_off") {
    motorStop();
    setAndReportState("DETENIDO");
  }
  else {
    logPrintln("[MQTT] Comando no reconocido.");
  }
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String t(topic);
  String msg; msg.reserve(length);
  for (unsigned int i=0;i<length;i++) msg += (char)payload[i];
  msg.trim();

  if (t == TOPIC_CMD) {
    logPrintf("[MQTT] cmd: %s\n", msg.c_str());
    handleCmd(msg);
  }
  else if (t == TOPIC_OPEN_CMD) {
    if (msg == "ON") {
      motorOpen();
      setAndReportState("ABRIENDO");
    } else {
      motorStop();
      setAndReportState("DETENIDO");
    }
  }
  else if (t == TOPIC_CLOSE_CMD) {
    if (msg == "ON") {
      motorClose();
      setAndReportState("CERRANDO");
    } else {
      motorStop();
      setAndReportState("DETENIDO");
    }
  }
}

static bool mqttEnsureConnected() {
  if (!net_wifi_connected()) return false;
  if (mqtt.connected()) return true;

  mqtt.setServer(mqtt_host, mqtt_port);
  mqtt.setCallback(mqttCallback);

  String clientId = String("esp32-garaje-") + String((uint32_t)ESP.getEfuseMac(), HEX);
  logPrint("[MQTT] Conectando...");
  bool ok;
  if (mqtt_user && strlen(mqtt_user)) ok = mqtt.connect(clientId.c_str(), mqtt_user, mqtt_pass);
  else                                 ok = mqtt.connect(clientId.c_str());

  if (ok) {
    logPrintln(" OK");
    mqtt.subscribe(TOPIC_CMD);
    mqtt.subscribe(TOPIC_OPEN_CMD);
    mqtt.subscribe(TOPIC_CLOSE_CMD);
    net_mqtt_publish(TOPIC_INFO, String("{\"boot\":true}"), false);
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

  if (WiFi.status() != WL_CONNECTED && now >= tNextRetry) {
    logPrint(".");
    WiFi.reconnect();
    retryDelayMs = (retryDelayMs < RETRY_DELAY_MAX) ? retryDelayMs * 2 : RETRY_DELAY_MAX;
    tNextRetry = now + retryDelayMs;
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (!ipShown) { ipShown = true; logPrintf("\n[WiFi] Conectado: %s\n", WiFi.localIP().toString().c_str()); }
    if (mqttEnsureConnected()) mqtt.loop();
  } else {
    ipShown = false;
  }
}