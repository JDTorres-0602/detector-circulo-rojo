// redimensionar.h — Reducción de imagen por promedio de área (igual a cv2.INTER_AREA)
// C puro: se compila también en el PC (ver pruebas_pc/).
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Reduce src (w1 x h1, nc canales intercalados) a dst (w2 x h2). Requiere w2 <= w1 < 2*w2 (igual en alto).
void redimensionar_area_c(const uint8_t* src, int w1, int h1, uint8_t* dst, int w2, int h2, int nc);

#ifdef __cplusplus
}
#endif
