// detector.h — El modelo del balón corriendo con TensorFlow Lite Micro
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    size_t arena_usada;      // bytes que realmente usa el intérprete
    bool arena_interna;      // true = RAM interna (rápida), false = PSRAM
    int prueba;              // autoprueba con pruebas_vectores.h: 1 = OK, 0 = FALLÓ, -1 = no había vectores
} InfoDetector;

// Carga el modelo. ESP_ERR_NOT_FOUND = todavía no hay modelo (modelo_balon.cc es el marcador).
esp_err_t detector_iniciar(InfoDetector* info);

// Ejecuta el modelo sobre la imagen 128x96 RGB (uint8 0..255). Llena 'prob' (12x16) con 0..1.
// Devuelve el tiempo de inferencia en microsegundos (negativo si falló).
int64_t detector_ejecutar(const uint8_t* rgb, float* prob);

#ifdef __cplusplus
}
#endif
