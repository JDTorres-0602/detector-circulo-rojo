// =====================================================================
//  prueba_logica.c — Prueba en el PC la lógica del carro (sin ESP32)
// =====================================================================
//  Visión por color, blobs y seguidor son C puro: se pueden probar en el
//  computador, que es mucho más rápido que flashear y mover el carro.
//
//  Compilar y correr (desde esta carpeta), con cualquier compilador de C:
//    gcc -I../main prueba_logica.c ../main/vision_color.c ../main/postproceso.c ../main/seguidor.c ../main/redimensionar.c -lm -o prueba && ./prueba
//  En Windows sin gcc:  pip install ziglang  y luego
//    python -m ziglang cc -I../main prueba_logica.c ../main/vision_color.c ../main/postproceso.c ../main/seguidor.c ../main/redimensionar.c -o prueba.exe
//    prueba.exe
// =====================================================================
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "postproceso.h"
#include "redimensionar.h"
#include "seguidor.h"
#include "vision_color.h"

static int fallos = 0;
#define REVISAR(cond, ...) do { if (cond) printf("  ok   "); else { printf("  FALLA "); ++fallos; } \
                                printf(__VA_ARGS__); printf("\n"); } while (0)

#define W 128
#define H 96
static uint8_t img[W * H * 3];

static void pintar_fondo(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < W * H; ++i) { img[3 * i] = r; img[3 * i + 1] = g; img[3 * i + 2] = b; }
}

static void pintar_circulo(int cx, int cy, int rad, uint8_t r, uint8_t g, uint8_t b) {
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= rad * rad) {
                uint8_t* p = img + 3 * (y * W + x);
                p[0] = r; p[1] = g; p[2] = b;
            }
}

static void prueba_color(void) {
    printf("Visión por color\n");
    float mapa[MAPA_CELDAS];
    REVISAR(pixel_es_rojo(200, 40, 30, 60), "rojo intenso es rojo");
    REVISAR(!pixel_es_rojo(200, 160, 30, 60), "naranja/amarillo no es rojo con delta 60");
    REVISAR(!pixel_es_rojo(60, 0, 0, 30), "rojo muy oscuro (R < ROJO_MIN) no cuenta");

    pintar_fondo(200, 200, 200);                       // pared gris
    pintar_circulo(96, 48, 12, 220, 30, 30);           // balón a la derecha
    mapa_rojo(img, W, H, 60, mapa);
    REVISAR(mapa[6 * MAPA_ANCHO + 12] > 0.9f, "la celda del centro del balón está llena (%.2f)", mapa[6 * MAPA_ANCHO + 12]);
    REVISAR(mapa[0] == 0.0f, "una esquina sin balón vale 0");

    Blob b[MAX_BLOBS];
    int n = buscar_blobs(mapa, 0.3f, b, MAX_BLOBS);
    REVISAR(n == 1, "un solo blob (hay %d)", n);
    if (n) REVISAR(fabsf(b[0].cx - 96.0f / W) < 0.04f && fabsf(b[0].cy - 48.0f / H) < 0.05f,
                   "centro del blob ~ centro del balón (%.2f, %.2f)", b[0].cx, b[0].cy);
}

static void prueba_blobs(void) {
    printf("Blobs\n");
    float mapa[MAPA_CELDAS] = {0};
    mapa[2 * MAPA_ANCHO + 2] = 0.9f;                               // blob pequeño arriba a la izquierda
    for (int i = 6; i < 9; ++i)
        for (int j = 10; j < 13; ++j) mapa[i * MAPA_ANCHO + j] = 0.8f;   // blob grande de 3x3
    mapa[0] = 0.2f;                                                // bajo el umbral: no cuenta
    Blob b[MAX_BLOBS];
    int n = buscar_blobs(mapa, 0.5f, b, MAX_BLOBS);
    REVISAR(n == 2, "dos blobs (hay %d)", n);
    if (n == 2) {
        REVISAR(b[0].area == 9 && b[1].area == 1, "el grande va primero (áreas %d y %d)", b[0].area, b[1].area);
        REVISAR(fabsf(b[0].cx - 11.5f / 16) < 1e-4f, "centro x del grande = %.4f", b[0].cx);
    }
}

static void prueba_seguidor(void) {
    printf("Seguidor\n");
    ParamsSeguidor p;
    seguidor_params_defecto(&p);
    Seguidor s;
    seguidor_init(&s, &p);
    int izq, der;
    uint32_t t = 1000;
    seguidor_paso(&s, t, 0, &izq, &der);
    REVISAR(s.estado == SEG_INACTIVO && izq == 0 && der == 0, "inactivo: no se mueve");

    seguidor_activar(&s, true, t);
    Blob derecha = {.cx = 0.8f, .cy = 0.6f, .pico = 0.9f, .area = 3};
    REVISAR(seguidor_nueva_deteccion(&s, &derecha, 1, t += 200) == -1, "1a detección: aún no se engancha");
    REVISAR(seguidor_nueva_deteccion(&s, &derecha, 1, t += 200) == 0, "2a detección en el mismo lugar: enganchado");
    seguidor_paso(&s, t += 50, 0, &izq, &der);
    REVISAR(s.estado == SEG_SIGUIENDO && izq > der, "balón a la derecha: gira a la derecha (izq %d > der %d)", izq, der);

    // El carro gira y el balón se va acercando al centro de la imagen poco a poco
    Blob centro_lejos = {.cx = 0.8f, .cy = 0.6f, .pico = 0.9f, .area = 2};
    for (int k = 0; k < 8; ++k) {
        centro_lejos.cx = k < 3 ? 0.7f - 0.1f * k : 0.5f;
        seguidor_nueva_deteccion(&s, &centro_lejos, 1, t += 200);
    }
    seguidor_paso(&s, t += 50, 0, &izq, &der);
    REVISAR(izq > 60 && der > 60 && abs(izq - der) < 30, "centrado y lejos: avanza recto (%d, %d)", izq, der);

    Blob centro_cerca = {.cx = 0.5f, .cy = 0.7f, .pico = 0.9f, .area = 20};
    for (int k = 0; k < 6; ++k) seguidor_nueva_deteccion(&s, &centro_cerca, 1, t += 200);
    seguidor_paso(&s, t += 50, 0, &izq, &der);
    REVISAR(izq == 0 && der == 0, "centrado y grande (área >= objetivo): se detiene (%d, %d)", izq, der);

    Blob otro = {.cx = 0.1f, .cy = 0.2f, .pico = 0.9f, .area = 20};
    REVISAR(seguidor_nueva_deteccion(&s, &otro, 1, t += 200) == -1, "otra cosa roja lejos del balón: se ignora");

    seguidor_paso(&s, t += 1500, 0, &izq, &der);
    REVISAR(s.estado == SEG_BUSCANDO, "sin verlo 1.5 s: buscando");
    seguidor_paso(&s, t += 8000, 0, &izq, &der);
    REVISAR(s.estado == SEG_ESPERANDO && izq == 0 && der == 0, "sin verlo mucho tiempo: espera quieto");
}

static void prueba_redimensionar(void) {
    printf("Redimensionar\n");
    static uint8_t src[160 * 120 * 3], dst[W * H * 3];
    memset(src, 77, sizeof(src));
    redimensionar_area_c(src, 160, 120, dst, W, H, 3);
    int iguales = 1;
    for (int i = 0; i < W * H * 3; ++i) iguales &= dst[i] == 77;
    REVISAR(iguales, "una imagen de un solo color sigue igual al reducirla");
}

int main(void) {
    prueba_color();
    prueba_blobs();
    prueba_seguidor();
    prueba_redimensionar();
    printf(fallos ? "\n%d PRUEBAS FALLARON\n" : "\nTODAS LAS PRUEBAS PASARON\n", fallos);
    return fallos != 0;
}
