// =====================================================================
//  seguidor.h — Lógica para seguir el balón (C puro, se prueba en el PC)
// =====================================================================
//  Entradas:  blobs del detector (varias veces por segundo) y la distancia
//             del ultrasonido (10 veces por segundo, desde el UNO).
//  Salida:    velocidades izquierda/derecha (-255..255) para "M izq der".
//
//  Estados:
//    INACTIVO  -> el usuario no ha activado el modo seguir
//    ESPERANDO -> activo, pero no hay balón confirmado
//    SIGUIENDO -> hay balón: girar hacia él y acercarse hasta que se vea "grande"
//    BUSCANDO  -> se perdió hace poco: girar a pulsos hacia donde se fue
// =====================================================================
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "postproceso.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { SEG_INACTIVO = 0, SEG_ESPERANDO, SEG_SIGUIENDO, SEG_BUSCANDO } EstadoSeguidor;

typedef struct {
    // Detección
    int   confirmaciones;     // detecciones seguidas en el mismo lugar para engancharse
    float salto_max;          // cuánto puede moverse el balón entre detecciones (0..1)
    // Giro (controlador PD sobre el error horizontal, -1 = borde izq, +1 = borde der)
    float kp_giro;            // PWM por unidad de error
    float kd_giro;            // PWM por (unidad de error / s)
    float zona_muerta;        // error que se ignora (evita zigzag)
    int   giro_max;
    float error_solo_giro;    // con error mayor que esto, primero gira sin avanzar
    uint32_t t_giro_valido_ms;  // una detección solo ordena girar durante este tiempo; después
                                // se espera la siguiente (la imagen llega con retraso). 0 = siempre
    // Avance (proporcional a "qué tan lejos": se mide con el área del blob)
    int   vel_max;
    int   pwm_min;            // por debajo de esto los motores no se mueven
    float area_objetivo;      // área del blob (celdas) a la que se detiene: más = se acerca más
    int   dist_min_cm;        // ultrasonido: si hay algo más cerca que esto, no avanza (0 = no usar)
    // Pérdida y búsqueda
    uint32_t t_perdida_ms;    // sin ver el balón este tiempo => deja de avanzar y busca
    uint32_t t_busqueda_ms;   // tiempo total buscando antes de quedarse esperando
    int   giro_busqueda;
    uint32_t pulso_ms, pausa_ms;   // la búsqueda gira a pulsos para que la imagen no salga movida
} ParamsSeguidor;

typedef struct {
    ParamsSeguidor p;
    bool activo;
    EstadoSeguidor estado;
    bool tiene_objetivo;
    float cx, cy, area;       // objetivo filtrado
    float error, derivada;
    uint32_t t_visto, t_det_anterior;
    int ultimo_lado;          // -1 izquierda, +1 derecha
    float cand_cx, cand_cy;   // candidato a objetivo (aún sin confirmar)
    int cand_n;
    uint32_t cand_t;
    int izq, der;             // último comando calculado
} Seguidor;

void seguidor_params_defecto(ParamsSeguidor* p);
void seguidor_init(Seguidor* s, const ParamsSeguidor* p);
void seguidor_activar(Seguidor* s, bool activo, uint32_t t_ms);

// Llamar después de cada detección. Devuelve el índice del blob elegido (-1 si ninguno).
int seguidor_nueva_deteccion(Seguidor* s, const Blob* blobs, int n, uint32_t t_ms);

// Llamar ~10 veces por segundo. dist_cm = 0 si no hay lectura válida.
void seguidor_paso(Seguidor* s, uint32_t t_ms, int dist_cm, int* izq, int* der);

const char* seguidor_nombre_estado(EstadoSeguidor e);

#ifdef __cplusplus
}
#endif
