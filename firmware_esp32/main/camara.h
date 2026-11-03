// camara.h — Captura de la OV2640 y preparación de la imagen para la visión
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JPEG_MAX (48 * 1024)   // tamaño máximo de un cuadro JPEG 320x240

// Inicia la cámara probando los cableados conocidos de la placa ELEGOO.
// Devuelve el nombre del cableado que funcionó, o NULL si ninguno.
const char* camara_iniciar(void);

// Arranca la tarea que captura cuadros JPEG continuamente.
void camara_arrancar_tarea(void);

// Copia el último JPEG si es más nuevo que *seq. Espera hasta 'espera_ms'.
// Devuelve el tamaño copiado (0 si no hubo cuadro nuevo o no cabe).
size_t camara_ultimo_jpeg(uint8_t* dst, size_t capacidad, uint32_t* seq, int espera_ms);

// JPEG 320x240 -> imagen 128x96 RGB intercalado (R,G,B,R,G,B...). 'rgb' debe tener MODELO_BYTES.
esp_err_t camara_preprocesar(const uint8_t* jpeg, size_t len, uint8_t* rgb);

float camara_fps(void);

#ifdef __cplusplus
}
#endif
