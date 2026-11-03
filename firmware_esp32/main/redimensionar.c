// redimensionar.c — ver redimensionar.h  (del firmware de detección de personas del equipo)
#include "redimensionar.h"

// Pesos de un eje: la celda de salida i cubre [i*s, (i+1)*s) en la entrada.
// Como la escala es < 2, cada salida toca como máximo 3 entradas.
typedef struct { int ini; int n; float w[3]; } Tramo;

static void pesos_eje(int n_in, int n_out, Tramo* t) {
    float s = (float)n_in / (float)n_out;
    for (int i = 0; i < n_out; ++i) {
        float a = i * s, b = (i + 1) * s;
        int k0 = (int)a;
        t[i].ini = k0;
        t[i].n = 0;
        for (int k = k0; k < n_in && k < b && t[i].n < 3; ++k) {
            float lo = a > k ? a : (float)k;
            float hi = b < k + 1 ? b : (float)(k + 1);
            t[i].w[t[i].n++] = (hi - lo) / s;
        }
    }
}

void redimensionar_area_c(const uint8_t* src, int w1, int h1, uint8_t* dst, int w2, int h2, int nc) {
    static Tramo tx[256], ty[256];
    static int cw1 = 0, ch1 = 0, cw2 = 0, ch2 = 0;
    if (w1 != cw1 || w2 != cw2) { pesos_eje(w1, w2, tx); cw1 = w1; cw2 = w2; }
    if (h1 != ch1 || h2 != ch2) { pesos_eje(h1, h2, ty); ch1 = h1; ch2 = h2; }

    for (int y = 0; y < h2; ++y) {
        for (int x = 0; x < w2; ++x) {
            for (int c = 0; c < nc; ++c) {
                float acc = 0.f;
                for (int a = 0; a < ty[y].n; ++a) {
                    const uint8_t* fila = src + ((ty[y].ini + a) * w1 + tx[x].ini) * nc + c;
                    float sf = 0.f;
                    for (int b = 0; b < tx[x].n; ++b) sf += tx[x].w[b] * fila[b * nc];
                    acc += ty[y].w[a] * sf;
                }
                int v = (int)(acc + 0.5f);
                if (v > 255) v = 255;
                dst[(y * w2 + x) * nc + c] = (uint8_t)v;
            }
        }
    }
}
