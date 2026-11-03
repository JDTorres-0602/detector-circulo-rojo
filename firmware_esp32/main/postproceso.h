// =====================================================================
//  postproceso.h — Del mapa de calor 12x16 a "balones" (blobs)
// =====================================================================
//  C puro (sin ESP-IDF): se compila igual en el PC para las pruebas.
//
//  Los DOS detectores (color y modelo) entregan el mismo tipo de mapa:
//  12x16 celdas, cada celda = 8x8 px de la imagen de 96x128, con un valor
//  0..1 que dice "qué tanto balón hay aquí". Las celdas encendidas que se
//  tocan (vecindad de 4) se agrupan en un blob = un balón.
// =====================================================================
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAPA_ALTO   12
#define MAPA_ANCHO  16
#define MAPA_CELDAS (MAPA_ALTO * MAPA_ANCHO)
#define MAX_BLOBS   8

typedef struct {
    float cx, cy;     // centroide ponderado, normalizado 0..1 (0 = izquierda/arriba)
    float pico;       // valor máximo dentro del blob
    float suma;       // suma de valores (≈ "masa" del balón)
    uint8_t area;     // número de celdas (más grande = más cerca)
    uint8_t x0, y0, x1, y1;   // caja en celdas (inclusive)
} Blob;

// Convierte la salida int8 cuantizada del modelo a probabilidades 0..1.
// p = (q - zero_point) * scale
void mapa_desde_int8(const int8_t* q, float scale, int zero_point, float* prob);

// Agrupa celdas con valor >= umbral. Devuelve cuántos blobs llenó (<= max),
// ordenados de mayor a menor (área × pico).
// Mismo algoritmo que decodificar() en entrenamiento/comun.py.
int buscar_blobs(const float* mapa, float umbral, Blob* salida, int max);

#ifdef __cplusplus
}
#endif
