// =====================================================================
//  vision_color.h — Detector "clásico" de balón rojo, SIN Machine Learning
// =====================================================================
//  Sirve para tres cosas:
//   1. Probar el carro completo (cámara, seguidor, motores) antes de tener modelo.
//   2. Comparar: ¿qué gana el modelo frente a una regla escrita a mano?
//   3. Ver sus límites: una camiseta roja o una caja roja también "son balón".
//
//  Entrega el mismo mapa 12x16 que el modelo: en cada celda, la FRACCIÓN de
//  píxeles rojos (0..1). Así el resto del firmware no sabe qué detector se usó.
//  C puro: se compila también en el PC (ver pruebas_pc/).
// =====================================================================
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Un píxel es "rojo" si R supera a G y a B por al menos 'delta' y además R >= ROJO_MIN.
#define ROJO_MIN 70
bool pixel_es_rojo(uint8_t r, uint8_t g, uint8_t b, int delta);

// rgb: imagen ancho x alto (RGB intercalado). ancho/alto deben ser 16 y 12 veces el tamaño de celda.
// mapa: MAPA_ALTO x MAPA_ANCHO valores 0..1.
void mapa_rojo(const uint8_t* rgb, int ancho, int alto, int delta, float* mapa);

#ifdef __cplusplus
}
#endif
