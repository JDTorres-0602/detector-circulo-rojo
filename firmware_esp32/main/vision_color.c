// vision_color.c — ver vision_color.h. Mismo algoritmo que mapa_rojo() en entrenamiento/comun.py.
#include "vision_color.h"
#include "postproceso.h"

bool pixel_es_rojo(uint8_t r, uint8_t g, uint8_t b, int delta) {
    int mayor = g > b ? g : b;
    return r >= ROJO_MIN && (int)r - mayor >= delta;
}

void mapa_rojo(const uint8_t* rgb, int ancho, int alto, int delta, float* mapa) {
    const int cw = ancho / MAPA_ANCHO, ch = alto / MAPA_ALTO;   // 8 x 8 píxeles por celda
    const float por_celda = 1.0f / (float)(cw * ch);
    for (int i = 0; i < MAPA_ALTO; ++i) {
        for (int j = 0; j < MAPA_ANCHO; ++j) {
            int rojos = 0;
            for (int y = i * ch; y < (i + 1) * ch; ++y) {
                const uint8_t* p = rgb + (y * ancho + j * cw) * 3;
                for (int x = 0; x < cw; ++x, p += 3)
                    rojos += pixel_es_rojo(p[0], p[1], p[2], delta);
            }
            mapa[i * MAPA_ANCHO + j] = rojos * por_celda;
        }
    }
}
