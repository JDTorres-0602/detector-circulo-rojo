// postproceso.c — ver postproceso.h  (del firmware de detección de personas del equipo)
#include "postproceso.h"

void mapa_desde_int8(const int8_t* q, float scale, int zero_point, float* prob) {
    for (int i = 0; i < MAPA_CELDAS; ++i) {
        float p = (float)(q[i] - zero_point) * scale;
        prob[i] = p < 0.f ? 0.f : (p > 1.f ? 1.f : p);
    }
}

int buscar_blobs(const float* mapa, float umbral, Blob* salida, int max) {
    uint8_t visto[MAPA_CELDAS] = {0};
    uint8_t pila[MAPA_CELDAS];
    int n = 0;

    for (int i = 0; i < MAPA_ALTO; ++i) {
        for (int j = 0; j < MAPA_ANCHO; ++j) {
            int k0 = i * MAPA_ANCHO + j;
            if (mapa[k0] < umbral || visto[k0]) continue;

            // Recorrido en profundidad (pila explícita, sin recursión)
            int tope = 0;
            pila[tope++] = (uint8_t)k0;
            visto[k0] = 1;
            float sw = 0, sx = 0, sy = 0, pico = 0;
            int area = 0, x0 = j, x1 = j, y0 = i, y1 = i;
            while (tope > 0) {
                int k = pila[--tope];
                int a = k / MAPA_ANCHO, b = k % MAPA_ANCHO;
                float p = mapa[k];
                sw += p; sx += p * (b + 0.5f); sy += p * (a + 0.5f);
                if (p > pico) pico = p;
                ++area;
                if (b < x0) x0 = b;
                if (b > x1) x1 = b;
                if (a < y0) y0 = a;
                if (a > y1) y1 = a;
                const int da[4] = {1, -1, 0, 0}, db[4] = {0, 0, 1, -1};
                for (int d = 0; d < 4; ++d) {
                    int na = a + da[d], nb = b + db[d];
                    if (na < 0 || na >= MAPA_ALTO || nb < 0 || nb >= MAPA_ANCHO) continue;
                    int nk = na * MAPA_ANCHO + nb;
                    if (!visto[nk] && mapa[nk] >= umbral) { visto[nk] = 1; pila[tope++] = (uint8_t)nk; }
                }
            }

            Blob bl = { sx / sw / MAPA_ANCHO, sy / sw / MAPA_ALTO, pico, sw, (uint8_t)area,
                        (uint8_t)x0, (uint8_t)y0, (uint8_t)x1, (uint8_t)y1 };

            // Inserción ordenada por área × pico (descendente), conservando los 'max' mejores
            float s = bl.area * bl.pico;
            int pos = n;
            while (pos > 0 && salida[pos - 1].area * salida[pos - 1].pico < s) --pos;
            if (pos >= max) continue;
            int ultimo = (n < max) ? n : max - 1;
            for (int t = ultimo; t > pos; --t) salida[t] = salida[t - 1];
            salida[pos] = bl;
            if (n < max) ++n;
        }
    }
    return n;
}
