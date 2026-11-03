// =====================================================================
//  uno.h — Enlace serial con el Arduino UNO (que maneja motores y ultrasonido)
// =====================================================================
//  Protocolo de texto, una orden por línea (ver arduino_uno/carro_uno/carro_uno.ino):
//    ESP32 -> UNO:  "M izq der"  velocidades -255..255
//                   "S"          parar
//                   "W ms"       watchdog: si no llega "M" en ese tiempo, el UNO para
//                   "D cm"       freno: no avanzar si el ultrasonido ve algo más cerca
//                   "T ms"       cada cuánto el UNO manda la distancia
//                   "V grados"   servo de la cabeza (90 = al frente)
//    UNO -> ESP32:  "U cm"       distancia del ultrasonido (0 = sin eco)
// =====================================================================
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void uno_iniciar(void);
void uno_enviar(const char* texto);          // texto con '\n' al final de cada orden
void uno_motores(int izq, int der);
int  uno_distancia_cm(void);                  // última distancia recibida (0 = sin dato)
uint32_t uno_ms_desde_ultimo_dato(void);      // para saber si el UNO está vivo

#ifdef __cplusplus
}
#endif
