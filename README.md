# Controlador Inteligente de Puerta de Garaje (ESP32)

Este proyecto implementa un controlador avanzado y seguro para una puerta de garaje automatizada utilizando un ESP32. Incluye control de motor con rampas de aceleración, monitorización de corriente para detección de obstáculos, lectura de encoder (sensor Hall) para posicionamiento, integración MQTT para domótica, control de iluminación y actualizaciones inalámbricas (OTA).

## Características Principales

- **Control de Motor (IBT-2 / BTS7960)**: 
  - Control de giro bidireccional mediante PWM.
  - Rampas de aceleración y deceleración ajustables.
  - *Dead-time* de protección al invertir el giro.
  - Velocidad reducida en los tramos finales del recorrido para un cierre y apertura suaves.
- **Odometría con Sensor Hall**:
  - Lectura de pulsos y dirección (encoder) para conocer la posición exacta de la puerta.
  - Calibración de recorrido (pulsos totales entre abierto y cerrado).
- **Seguridad Activa y Pasiva**:
  - **Sensor de Corriente (ACS712)**: Configuración de límites de sobrecorriente para parada de emergencia en caso de detección de obstáculos.
  - **Plausibilidad de señales**: Detección de motor en movimiento sin consumo, o consumo sin lectura del encoder (atasco mecánico temporal).
- **Integración Domótica (MQTT / Wi-Fi)**:
  - Comandos para Abrir, Cerrar, Detener y controlar luces remotamente.
  - Publicación en tiempo real del estado de la puerta, posición, lecturas de corriente y velocidad.
  - Tópicos *retained* para fácil integración con Home Assistant o Node-RED.
- **Control de Iluminación**:
  - Gestión de luces LED PWM (hasta 2 tiras).
  - Efectos visuales de encendido suave y modo "Respiración".
  - Apagado automático transcurrido un tiempo programado.
- **Interfaz y Control Local**:
  - Display matricial (soporte para biblioteca Parola) para mostrar el estado y animaciones visuales (por ejemplo, flechas de apertura/cierre).
  - Entrada de botón pulsador local para el control manual alternativo (abrir-parar-cerrar-parar).
- **Mantenimiento (OTA)**:
  - Soporte de actualizaciones Over-the-Air previa habilitación por red o al encendido, permitiendo mejora del firmware sin acceso físico al ESP32.

---

## Requisitos de Hardware

- Espressif **ESP32**
- Driver de motor de alta potencia **IBT-2 / BTS7960**.
- Sensor de corriente **ACS712** o compatible.
- Sensor **Hall / Encoder digital** instalado en el eje de tracción de la puerta.
- Matriz de LEDs basada en MAX7219 (opcional para el display decorativo).
- Tira de luces o módulos LED (Controlados por transistores N-MOSFET).
- Pulsador local (Normalmente Abierto conectado a un pin).
- Fuente de alimentación a medida según el motor y periféricos.

---

## Archivos y Módulo del Proyecto

| Fichero         | Descripción                                                                 |
|-----------------|-----------------------------------------------------------------------------|
| `puerta.ino`    | Core de Arduino. Máquina de estado inicial y blucle (loop) principal.       |
| `config.h`      | Archivo central de configuración: Pines, constantes de velocidad, MQTT, etc.|
| `motor.cpp/.h`  | Abstracción del hardware del puente H, frecuencias PWM y control de rampa.  |
| `hall.cpp/.h`   | Lógica de lectura de encoder (interrupciones/polling) y paradas suaves.     |
| `current.cpp/.h`| Lectura del ADC del ESP32 para interpretar el consumo del motor y topes.    |
| `safety.cpp/.h` | Validaciones de fallos (motor accionado pero sin pulsos o baja corriente).  |
| `net.cpp/.h`    | Conexión WiFi, configuración del cliente MQTT y publicación de estados.     |
| `light.cpp/.h`  | Control PWM de la iluminación del garaje, temporizadores y efectos.         |
| `display.cpp/.h`| Animaciones en la matriz de LEDs mediante la biblioteca Parola.             |
| `ota_ctl.cpp/.h`| Funciones para habilitar/deshabilitar actualizaciones OTA de forma segura.  |
| `secrets.cpp/.h`| (*No incluido en el repo*) Credenciales de WiFi y Broker MQTT.              |

---

## Configuración y Despliegue

1. **Credenciales**:
   Crea los ficheros `secrets.h` y `secrets.cpp` para alojar tus datos sensibles de conexión:
   ```cpp
   // secrets.h
   #pragma once
   extern const char* WIFI_SSID;
   extern const char* WIFI_PASS;
   extern const char* MQTT_SERVER;
   extern const uint16_t MQTT_PORT;
   extern const char* MQTT_USER;
   extern const char* MQTT_PASS;
   ```
   ```cpp
   // secrets.cpp
   #include "secrets.h"
   const char* WIFI_SSID   = "Tu_Red";
   const char* WIFI_PASS   = "Tu_Password";
   const char* MQTT_SERVER = "192.168.1.100";
   const uint16_t MQTT_PORT= 1883;
   const char* MQTT_USER   = "usuario";
   const char* MQTT_PASS   = "password";
   ```

2. **Personalización hardware**:
   Revisa en el fichero `config.h` los pines asignados:
   - Señales de motor: pines 25, 26, 32 y 33.
   - Entradas del Hall: Pines 35 y 36.
   - Pin Botón local: Pin 14.
   - Señales de Luces: Pines 27 y 13.
   Ajusta características de rampa (`MOTOR_RAMP_STEP_PERCENT`), umbrales limitadores de corriente (`SAFETY_MIN_CURRENT_A`), etc., según tu modelo de motor.

3. **Compilación y carga**:
   Compila usando el Arduino IDE o PlatformIO y súbelo inicialmente por puerto serie. 
   Posteriores subidas pueden hacerse vía OTA habilitando la ventana mediante MQTT o en los primeros 5 minutos de inicio (`BOOT_OTA_MINUTES`).

4. **Calibración inicial**:
   - Ajusta empíricamente los valores de las protecciones de sobreintensidad de corriente desde el panel MQTT (`garage/current/limit/cmd`).
   - Define las fronteras de final de carrera enviando peticiones de marcado al tópico correspondiente: `garage/door/mark_closed/cmd` y `garage/door/mark_open/cmd`.

---

## Interfaz MQTT (Tópicos Principales)

| Tópico (Suscritos)               | Acción                                             |
|----------------------------------|----------------------------------------------------|
| `garage/door/cmd`                | Comandos del sistema (ej: `reboot`, `status`, `ota on`) |
| `garage/door/open/cmd` o `close/cmd` | Activa la apertura o cierre manual del portón. |
| `garage/light/cmd`               | Comandos de luz principal (`ON`, `OFF`, `TOGGLE`). |
| `garage/current/limit/cmd`       | Ajustar el nivel de tolerancia de sobrecorriente.  |

| Tópico (Publicados)              | Información                                        |
|----------------------------------|----------------------------------------------------|
| `garage/door/state`              | Estado actual: `ABRIENDO`, `CERRANDO`, `DETENIDO`. |
| `garage/encoder/pos`             | Posición actual cruda (basado en pulsos leídos).   |
| `garage/current/value`           | Corriente eléctrica instantánea sensada en Amperios|
| `garage/door/info`               | Payload JSON con IP, versión y métricas del sistema|

---
**Nota sobre seguridad electromecánica**:
El autor del código no se hace responsable de daños causados por configuraciones erróneas. Asegúrese de realizar pruebas iniciales sin el portón acoplado o manteniendo siempre un mecanismo físico que detenga el motor o lo desconecte.
