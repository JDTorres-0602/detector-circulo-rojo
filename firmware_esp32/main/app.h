// =====================================================================
//  app.h — Estado compartido entre las tareas (visión, control, web)
// =====================================================================
//  Todo lo que está aquí se lee y escribe con g_mutex tomado: varias
//  tareas de FreeRTOS lo usan al mismo tiempo.
// =====================================================================
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "detector.h"
#include "postproceso.h"
#include "seguidor.h"
#include "autonomo.h"

typedef enum { MODO_COLOR = 0, MODO_MODELO = 1 } ModoDeteccion;

typedef struct {
    ModoDeteccion modo;
    bool autonomo;            // false = modo MANUAL, true = modo BÚSQUEDA AUTÓNOMA
    float umbral_color;       // modo COLOR: fracción de píxeles rojos para encender una celda
    float umbral_modelo;      // modo MODELO: probabilidad mínima para encender una celda
    int delta_rojo;           // modo COLOR: cuánto debe superar R a G y B
} Ajustes;

typedef struct {
    float mapa[MAPA_CELDAS];  // último mapa 12x16 (0..1)
    Blob blobs[MAX_BLOBS];
    int n_blobs;
    int elegido;              // índice del blob que sigue el carro (-1 = ninguno)
    float ms_deteccion;       // tiempo de la última detección (preprocesado no incluido)
    float det_por_s;          // detecciones por segundo
    int izq, der;             // último comando a los motores
    int dist_cm;              // ultrasonido
    int servo;                // ángulo actual de la cabeza (cámara + ultrasonido)
    bool frente_vigilado;     // el ultrasonido está mirando al frente
    bool obstaculo;           // el filtro de seguridad está frenando el avance
} Estado;

extern SemaphoreHandle_t g_mutex;
extern Ajustes g_ajustes;
extern Estado g_estado;
extern Seguidor g_seg;
extern Autonomo g_auto;
extern InfoDetector g_info;
extern bool g_modelo_ok;
extern uint8_t* g_entrada;    // última imagen 128x96 RGB que entró a la visión (MODELO_BYTES)
extern int g_manual_izq, g_manual_der;
extern uint32_t g_manual_t;   // ms de la última orden manual
extern int g_manual_servo;    // ángulo del servo pedido en modo manual

uint32_t ahora_ms(void);
