// =====================================================================
//  autonomo.h — Modo BÚSQUEDA AUTÓNOMA (C puro, se prueba en el PC)
// =====================================================================
//  La cámara y el ultrasonido van montados juntos sobre el servo, así que
//  cada barrido entrega DOS cosas: si se ve el objeto rojo en cada ángulo
//  y qué tan libre está el camino en ese ángulo.
//
//  Flujo:
//    (entrar al modo) -> ESCANEO: carro quieto, servo de SERVO_MIN a SERVO_MAX
//        ¿se vio el objeto?  sí -> ALINEANDO: servo al frente y el carro gira
//                                  su cuerpo hacia el ángulo donde se vio
//                               -> CONFIRMANDO: quieto, la cámara debe verlo
//                                  de frente (si no, vuelve a ESCANEO)
//                               -> SIGUIENDO: "Objeto fijado con éxito"; el
//                                  seguidor (seguidor.c) lo persigue. Si lo
//                                  pierde del todo -> ESCANEO
//                            no -> ¿frente libre?  sí -> AVANZANDO 3 s -> ESCANEO
//                                                  no -> ESQUIVANDO: gira hacia el
//                                                        ángulo más libre -> AVANZANDO
//                                                  todo bloqueado -> gira 180° -> ESCANEO
//    Mientras AVANZA también mira: si ve el objeto, va directo a SIGUIENDO.
//
//  JERARQUÍA: el ultrasonido manda. Este módulo solo avanza con el servo al
//  frente (así el ultrasonido mira hacia donde va el carro) y si el frente se
//  cierra deja de avanzar aunque vea el objeto. Además main.c aplica un filtro
//  de seguridad final y el UNO tiene su propio freno ("D cm").
// =====================================================================
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "seguidor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AUTO_MAX_PASOS 19   // posiciones del barrido (paso mínimo 10°)

typedef enum {
    AUTO_APAGADO = 0,
    AUTO_ESCANEO,
    AUTO_ALINEANDO,
    AUTO_CONFIRMANDO,
    AUTO_SIGUIENDO,
    AUTO_AVANZANDO,
    AUTO_ESQUIVANDO,
} EstadoAuto;

typedef struct {
    // Barrido
    int servo_min, servo_max, servo_paso;   // grados (90 = al frente)
    int servo_izq_mayor;      // 1 si ángulos > 90 miran a la IZQUIERDA del carro, 0 si a la derecha
    uint32_t t_asentar_ms;    // espera tras mover el servo (servo + imagen nueva)
    uint32_t t_observar_ms;   // ventana para detectar y medir en cada ángulo
    float fov_camara;         // campo de visión horizontal de la cámara (grados)
    // Movimiento
    uint32_t t_avance_ms;     // cuánto avanza cuando no encontró nada (3 s)
    int vel_avance;
    int vel_giro;
    float ms_por_grado;       // calibración: ms girando sobre su eje por cada grado
    int giro_min_grados;      // diferencias menores no se corrigen girando
    // Seguridad / navegación
    int dist_libre_cm;        // frente más cerca que esto = bloqueado (no avanza, esquiva)
    // Confirmación
    uint32_t t_confirmar_ms;  // tiempo para volver a ver el objeto de frente tras alinear
} ParamsAuto;

typedef struct {
    ParamsAuto p;
    EstadoAuto estado;
    uint32_t t_estado;        // cuándo se entró al estado actual
    int servo;                // ángulo que se pide al servo
    // Barrido
    int sentido;              // +1 sube el ángulo, -1 baja (se alterna en cada barrido)
    int paso;                 // índice del paso actual
    int n_pasos;
    uint32_t t_paso;
    bool limpio;              // ya se borraron las detecciones viejas en este paso/estado
    uint32_t t_limpio;        // desde cuándo cuentan las detecciones
    int perfil[AUTO_MAX_PASOS];     // distancia mínima medida en cada ángulo (999 = libre)
    int perfil_ang[AUTO_MAX_PASOS];
    // Giro en curso
    int giro_lado;            // -1 izquierda, +1 derecha (misma convención que el seguidor)
    uint32_t giro_ms;
    EstadoAuto tras_giro;
    // Objeto
    float obj_angulo;         // ángulo del servo al que se vio (corregido con la posición en la imagen)
    bool fijado;
    uint32_t eventos_fijado;  // sube cada vez que se fija el objeto (para avisos del HMI)
    uint32_t escaneos;
    bool bloqueado;           // el frente está cerrado
    const char* mensaje;      // último aviso para el HMI
} Autonomo;

void auto_params_defecto(ParamsAuto* p);
void auto_init(Autonomo* a, const ParamsAuto* p);
// Entrar (true) o salir (false) del modo. Entrar arranca con el escaneo inicial.
void auto_activar(Autonomo* a, Seguidor* s, bool activo, uint32_t t_ms);

// Llamar ~10 veces por segundo, con g_seg ya actualizado por la visión.
// dist_cm: lectura del ultrasonido (0 = sin eco o sin dato).
// Salidas: velocidades de los motores y el ángulo del servo.
void auto_paso(Autonomo* a, Seguidor* s, uint32_t t_ms, int dist_cm, int* izq, int* der, int* servo);

const char* auto_nombre_estado(EstadoAuto e);

#ifdef __cplusplus
}
#endif
