// =====================================================================
//  config.h — Parámetros del firmware de la ESP32 (cámara del carro)
// =====================================================================
#pragma once
#include "sdkconfig.h"

// ---------------- WiFi (punto de acceso propio del carro) ----------------
#define WIFI_PREFIJO_SSID   "CarroBalon-"   // + 4 dígitos de la MAC, p. ej. CarroBalon-3F2A
#define WIFI_CLAVE          "balon2026"     // mínimo 8 caracteres ("" = red abierta)
#define WIFI_CANAL          6
#define WIFI_MAX_CLIENTES   3
// La IP del carro es siempre 192.168.4.1

// ---------------- Enlace serial con el Arduino UNO ----------------
// Mismos pines y velocidad que el firmware de fábrica de ELEGOO.
#define UNO_UART            UART_NUM_2
#define UNO_BAUDIOS         9600
#if CONFIG_IDF_TARGET_ESP32S3
// Módulo nuevo ESP32-S3-WROOM-1: conector de 4 pines RX/TX/GND/5V
#define UNO_PIN_RX          3
#define UNO_PIN_TX          40
#else
// Módulo anterior ESP32-WROVER
#define UNO_PIN_RX          33
#define UNO_PIN_TX          4
#endif
// Se envían al arrancar: watchdog 600 ms, freno a 12 cm, telemetría cada 100 ms, servo al frente
#define UNO_CMD_INICIO      "W 600\nD 12\nT 100\nV 90\n"

// ---------------- Cámara ----------------
#define CAMARA_XCLK_HZ      20000000
#define CAMARA_CALIDAD_JPEG 12            // 0-63, menor = mejor calidad
// Prueba de orientación: pon el balón a la DERECHA del carro (mirando hacia donde mira el carro);
// en la página debe verse a la DERECHA. Si sale al revés, cambia CAMARA_ESPEJO_H.
// Si se ve de cabeza, cambia CAMARA_VOLTEAR_V.
#if CONFIG_IDF_TARGET_ESP32S3
#define CAMARA_VOLTEAR_V    1
#define CAMARA_ESPEJO_H     0
#else
#define CAMARA_VOLTEAR_V    0
#define CAMARA_ESPEJO_H     0
#endif
// La cámara captura QVGA 320x240 en JPEG; la visión usa una versión reducida a 128x96.

// ---------------- Imagen que entra a la visión ----------------
#define MODELO_ALTO     96
#define MODELO_ANCHO    128
#define MODELO_CANALES  3                 // RGB
#define MODELO_BYTES    (MODELO_ALTO * MODELO_ANCHO * MODELO_CANALES)
#define ARENA_BYTES     (400 * 1024)      // memoria de trabajo de TFLite Micro (se mide al arrancar)

// ---------------- Detección: valores iniciales (se cambian desde la página) ----------------
// Modo COLOR: un píxel es "rojo" si R supera a G y a B por al menos DELTA_ROJO.
#define DELTA_ROJO_DEFECTO      60
// Una celda (8x8 px) se "enciende" si al menos esta fracción de sus píxeles es roja.
#define UMBRAL_COLOR_DEFECTO    0.30f
// Modo MODELO: probabilidad mínima por celda. modelo_balon.h (lo escribe el cuaderno) trae
// el umbral calculado en validación; si no, 0.5.
#include "modelo_balon.h"
#ifdef MODELO_UMBRAL
#define UMBRAL_MODELO_DEFECTO   MODELO_UMBRAL
#else
#define UMBRAL_MODELO_DEFECTO   0.5f
#endif

// ---------------- Tareas ----------------
#define PERIODO_CONTROL_MS  100        // 10 Hz: comando a los motores
#define EXPIRA_MANUAL_MS    600        // control manual: si no llegan órdenes, para
