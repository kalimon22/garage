#include "ota_ctl.h"
#include <ArduinoOTA.h>
#include "logx.h"
#include "net.h"
#include "config.h"
#include "secrets.h"

static bool     otaOn   = false;
static uint32_t otaUntil = 0; // millis límite

static void publishOtaStatus() {
  net_mqtt_publish(TOPIC_OTA, String(otaOn ? "ON" : "OFF"), true);
}

void ota_enable_for(uint32_t minutes) {
  if (!otaOn) {
    ArduinoOTA.setHostname("esp32-garaje");
    ArduinoOTA.setPassword(ota_password);
    ArduinoOTA.onStart([](){ logPrintln("\n[OTA] Inicio de actualización"); });
    ArduinoOTA.onEnd([](){ logPrintln("\n[OTA] Fin. Reiniciando..."); });
    ArduinoOTA.onProgress([](unsigned int p, unsigned int t){
      static uint8_t last = 255; uint8_t pct = (p*100)/t;
      if (pct != last) { last = pct; logPrintf("\r[OTA] %u%%", pct); }
    });
    ArduinoOTA.onError([](ota_error_t e){ logPrintf("\n[OTA] Error %u\n", (unsigned)e); });
    ArduinoOTA.begin();
  }
  otaOn = true;
  otaUntil = millis() + minutes*60UL*1000UL;
  logPrintf("[OTA] ON %u min\n", (unsigned)minutes);
  publishOtaStatus();
}

void ota_disable() {
  otaOn = false; // con ESP32 basta con dejar de manejar OTA
  logPrintln("[OTA] OFF");
  publishOtaStatus();
}

void ota_tick() {
  uint32_t now = millis();
  if (!otaOn) return;
  if ((int32_t)(otaUntil - now) <= 0) { ota_disable(); return; }
  ArduinoOTA.handle();
}

bool ota_is_on() {
  return otaOn;
}

uint32_t ota_seconds_left() {
  if (!otaOn) return 0;
  uint32_t now = millis();
  if ((int32_t)(otaUntil - now) <= 0) return 0;
  return (otaUntil - now) / 1000UL;
}