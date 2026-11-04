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
// ---------------- Seguridad: el ultrasonido manda ----------------
// Si hay algo más cerca que esto AL FRENTE, el carro no avanza (sí puede girar o retroceder).
// Se aplica en DOS lugares: el filtro final de main.c y el freno propio del UNO ("D cm").
#define DIST_FRENO_CM       15
// El ultrasonido solo vigila el frente si el servo está a menos de esto de 90°
// y ya pasó FRENTE_ASENTAR_MS desde que se movió (si no, la lectura es de un lado).
#define FRENTE_TOLERANCIA   25
#define FRENTE_ASENTAR_MS   350

#define _STR(x) #x
#define STR(x) _STR(x)
// Se envían al arrancar: watchdog 600 ms, freno, telemetría cada 100 ms, servo al frente
#define UNO_CMD_INICIO      "W 600\nD " STR(DIST_FRENO_CM) "\nT 100\nV 90\n"

// ---------------- Modo BÚSQUEDA AUTÓNOMA (ver autonomo.h) ----------------
// Barrido del servo de la cabeza (cámara + ultrasonido)
#define AUTO_SERVO_MIN      0
#define AUTO_SERVO_MAX      180
#define AUTO_SERVO_PASO     30      // grados entre fotos del barrido
// Prueba: manda el servo a 150° desde la página. Si la cabeza mira a la IZQUIERDA del
// carro deja 1; si mira a la DERECHA pon 0.
#define AUTO_SERVO_IZQ_MAYOR 1
#define AUTO_AVANCE_MS      3000    // si no encuentra nada, avanza esto y vuelve a escanear
#define AUTO_VEL_AVANCE     130
#define AUTO_VEL_GIRO       120
// CALIBRAR: pon el carro a girar sobre su eje con AUTO_VEL_GIRO y mide cuánto tarda
// en dar una vuelta completa (360°). ms_por_grado = ese tiempo en ms / 360.
#define AUTO_MS_POR_GRADO   6.0f
#define AUTO_DIST_LIBRE_CM  30      // en búsqueda, frente más cerca que esto = bloqueado

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

// ---------------- Velocidad y RPM ESTIMADAS (el kit no tiene encoders) ----------------
// Se calculan desde el PWM: por debajo de PWM_ARRANQUE el motor no gira; a 255 da RPM_MAX.
// Para calibrar: marca una rueda, mándala a 255 y cuenta vueltas en 10 s (×6 = RPM).
#define RPM_MAX             200.0f
#define PWM_ARRANQUE        60
#define RUEDA_DIAMETRO_CM   6.5f

// ---------------- Tareas ----------------
#define PERIODO_CONTROL_MS  100        // 10 Hz: comando a los motores
#define EXPIRA_MANUAL_MS    600        // control manual: si no llegan órdenes, para
