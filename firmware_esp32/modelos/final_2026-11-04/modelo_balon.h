// Generado por el cuaderno detector_balon.ipynb — no editar a mano
#pragma once
#include <stdint.h>
#define MODELO_UMBRAL 0.480f   // falsa alarma <= 5% en las fotos sin balón de validación
extern const uint8_t g_modelo_balon[];
extern const unsigned int g_modelo_balon_len;
