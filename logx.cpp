#include "logx.h"
#include <stdarg.h>
#include "net.h"
#include "config.h"

// Buffer circular sencillo para cuando MQTT aún no conecta
static const size_t LOG_BUFFER_MAX = 50;
static String logBuffer[LOG_BUFFER_MAX];
static size_t logHead = 0, logTail = 0;
static inline bool logBufEmpty() { return logHead == logTail; }
static inline bool logBufFull()  { return ((logHead + 1) % LOG_BUFFER_MAX) == logTail; }
static void logBufPush(const String& s) { if (logBufFull()) logTail = (logTail + 1) % LOG_BUFFER_MAX; logBuffer[logHead] = s; logHead = (logHead + 1) % LOG_BUFFER_MAX; }
static bool logBufPop(String& out) { if (logBufEmpty()) return false; out = logBuffer[logTail]; logTail = (logTail + 1) % LOG_BUFFER_MAX; return true; }

void logPrint(const String& s) {
  Serial.print(s);
  if (net_mqtt_connected()) net_mqtt_publish(TOPIC_LOG, s, false);
  else logBufPush(s);
}

void logPrintln(const String& s) {
  Serial.println(s);
  String out = s; out += "\n";
  if (net_mqtt_connected()) net_mqtt_publish(TOPIC_LOG, out, false);
  else logBufPush(out);
}

void logPrintf(const char* fmt, ...) {
  char buf[256];
  va_list args; va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  logPrint(String(buf));
}

void logx_on_mqtt_connected() {
  // Volcar lo pendiente
  if (!net_mqtt_connected()) return;
  String line;
  while (logBufPop(line)) net_mqtt_publish(TOPIC_LOG, line, false);
}