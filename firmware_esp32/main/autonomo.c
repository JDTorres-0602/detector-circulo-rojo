// autonomo.c — ver autonomo.h
#include "autonomo.h"
#include <math.h>
#include <stdlib.h>

#define LIBRE 999   // distancia que se usa cuando el ultrasonido no tiene eco (nada cerca)

static uint32_t transcurrido(uint32_t t, uint32_t antes) {
    return (int32_t)(t - antes) < 0 ? 0 : t - antes;
}

// ¿El seguidor confirmó un objeto con imágenes tomadas después de 'desde'?
static bool visto_desde(const Seguidor* s, uint32_t desde) {
    return s->tiene_objetivo && (int32_t)(s->t_visto - desde) >= 0;
}

void auto_params_defecto(ParamsAuto* p) {
    p->servo_min = 0;
    p->servo_max = 180;
    p->servo_paso = 30;          // con ~60° de campo de visión, 30° deja traslape entre fotos
    p->servo_izq_mayor = 1;
    p->t_asentar_ms = 350;
    p->t_observar_ms = 500;
    p->fov_camara = 60.0f;
    p->t_avance_ms = 3000;
    p->vel_avance = 130;
    p->vel_giro = 120;
    p->ms_por_grado = 6.0f;      // CALIBRAR: ver config.h
    p->giro_min_grados = 12;
    p->dist_libre_cm = 30;
    p->t_confirmar_ms = 1500;
}

void auto_init(Autonomo* a, const ParamsAuto* p) {
    a->p = *p;
    a->estado = AUTO_APAGADO;
    a->t_estado = 0;
    a->servo = 90;
    a->sentido = 1;
    a->paso = a->n_pasos = 0;
    a->t_paso = 0;
    a->limpio = false;
    a->t_limpio = 0;
    for (int i = 0; i < AUTO_MAX_PASOS; ++i) { a->perfil[i] = LIBRE; a->perfil_ang[i] = 90; }
    a->giro_lado = 1;
    a->giro_ms = 0;
    a->tras_giro = AUTO_ESCANEO;
    a->obj_angulo = 90;
    a->fijado = false;
    a->eventos_fijado = 0;
    a->escaneos = 0;
    a->bloqueado = false;
    a->mensaje = "Modo manual";
}

// Borra lo que el seguidor tenía (p. ej. un objeto visto con el servo en otro ángulo)
// para que solo cuenten detecciones nuevas.
static void limpiar_seguidor(Seguidor* s, uint32_t t) { seguidor_activar(s, true, t); }

static void entrar(Autonomo* a, EstadoAuto e, uint32_t t) {
    a->estado = e;
    a->t_estado = t;
    a->t_paso = t;
    a->paso = 0;
    a->limpio = false;
}

// Desde este momento solo cuentan detecciones nuevas (se hace una vez por paso/estado,
// cuando el servo ya llegó y la cámara ve lo que hay en esa dirección).
static void abrir_ventana(Autonomo* a, Seguidor* s, uint32_t t) {
    if (a->limpio) return;
    limpiar_seguidor(s, t);
    a->limpio = true;
    a->t_limpio = t;
}

static int angulo_paso(const Autonomo* a, int i) {
    return a->sentido > 0 ? a->p.servo_min + i * a->p.servo_paso
                          : a->p.servo_max - i * a->p.servo_paso;
}

static void iniciar_escaneo(Autonomo* a, Seguidor* s, uint32_t t) {
    int paso = a->p.servo_paso < 10 ? 10 : a->p.servo_paso;
    a->p.servo_paso = paso;
    int n = (a->p.servo_max - a->p.servo_min) / paso + 1;
    a->n_pasos = n < 1 ? 1 : (n > AUTO_MAX_PASOS ? AUTO_MAX_PASOS : n);
    for (int i = 0; i < AUTO_MAX_PASOS; ++i) a->perfil[i] = LIBRE;
    for (int i = 0; i < a->n_pasos; ++i) a->perfil_ang[i] = angulo_paso(a, i);
    entrar(a, AUTO_ESCANEO, t);
    a->servo = a->perfil_ang[0];
    a->escaneos++;
    a->fijado = false;
    a->bloqueado = false;
    a->mensaje = "Escaneando el área";
    limpiar_seguidor(s, t);
}

static void iniciar_avance(Autonomo* a, Seguidor* s, uint32_t t) {
    entrar(a, AUTO_AVANZANDO, t);
    a->servo = 90;
    limpiar_seguidor(s, t);
}

static void fijar(Autonomo* a, uint32_t t) {
    entrar(a, AUTO_SIGUIENDO, t);
    a->servo = 90;
    a->fijado = true;
    a->eventos_fijado++;
    a->mensaje = "Objeto fijado con éxito";
}

// Gira el cuerpo del carro 'grados' hacia 'lado' (con el servo al frente) y luego pasa a 'tras'.
static void girar(Autonomo* a, uint32_t t, EstadoAuto como, int lado, float grados, EstadoAuto tras) {
    entrar(a, como, t);
    a->servo = 90;
    a->giro_lado = lado;
    a->giro_ms = grados < a->p.giro_min_grados ? 0 : (uint32_t)(grados * a->p.ms_por_grado);
    a->tras_giro = tras;
}

// Lado hacia el que queda un ángulo del servo: -1 izquierda, +1 derecha, 0 al frente
static int lado_de(const Autonomo* a, float angulo) {
    float d = angulo - 90.0f;
    if (fabsf(d) < 1.0f) return 0;
    bool izquierda = a->p.servo_izq_mayor ? d > 0 : d < 0;
    return izquierda ? -1 : 1;
}

// Terminó un barrido sin ver el objeto: ¿avanzar, esquivar o dar la vuelta?
static void decidir(Autonomo* a, Seguidor* s, uint32_t t) {
    a->sentido = -a->sentido;           // el próximo barrido va al revés (el servo no regresa en vano)
    int frente = 0, mejor = 0;
    for (int i = 0; i < a->n_pasos; ++i) {
        if (abs(a->perfil_ang[i] - 90) < abs(a->perfil_ang[frente] - 90)) frente = i;
        if (a->perfil[i] > a->perfil[mejor]) mejor = i;
    }
    if (a->perfil[frente] >= a->p.dist_libre_cm) {
        a->mensaje = "Sin objeto: avanzando";
        iniciar_avance(a, s, t);
    } else if (a->perfil[mejor] >= a->p.dist_libre_cm) {
        a->bloqueado = true;
        a->mensaje = "Obstáculo al frente: esquivando";
        float ang = (float)a->perfil_ang[mejor];
        girar(a, t, AUTO_ESQUIVANDO, lado_de(a, ang), fabsf(ang - 90.0f), AUTO_AVANZANDO);
    } else {
        a->bloqueado = true;
        a->mensaje = "Camino bloqueado: dando la vuelta";
        girar(a, t, AUTO_ESQUIVANDO, 1, 180.0f, AUTO_ESCANEO);
    }
}

void auto_activar(Autonomo* a, Seguidor* s, bool activo, uint32_t t) {
    if (activo) {
        if (a->estado != AUTO_APAGADO) return;   // ya estaba en autónomo
        a->sentido = 1;
        a->escaneos = 0;
        iniciar_escaneo(a, s, t);
        a->mensaje = "Escaneo inicial";
    } else {
        entrar(a, AUTO_APAGADO, t);
        a->servo = 90;
        a->fijado = false;
        a->bloqueado = false;
        a->mensaje = "Modo manual";
        seguidor_activar(s, false, t);
    }
}

void auto_paso(Autonomo* a, Seguidor* s, uint32_t t, int dist_cm, int* izq, int* der, int* servo) {
    const ParamsAuto* p = &a->p;
    int vi = 0, vd = 0;
    int dist = dist_cm > 0 ? dist_cm : LIBRE;
    uint32_t en_estado = transcurrido(t, a->t_estado);

    switch (a->estado) {
    case AUTO_APAGADO:
        break;

    case AUTO_ESCANEO: {
        // El primer paso espera el doble: el servo puede venir desde el otro extremo
        uint32_t asentar = a->paso == 0 ? 2 * p->t_asentar_ms : p->t_asentar_ms;
        uint32_t en_paso = transcurrido(t, a->t_paso);
        a->servo = a->perfil_ang[a->paso];
        if (en_paso < asentar) break;                       // el servo todavía se mueve
        abrir_ventana(a, s, t);                             // inicio de la ventana de observación
        if (dist < a->perfil[a->paso]) a->perfil[a->paso] = dist;   // ultrasonido mirando a este ángulo

        if (visto_desde(s, a->t_limpio)) {
            // El objeto está en la imagen: corregir el ángulo con su posición horizontal
            float corr = (s->cx - 0.5f) * p->fov_camara;    // > 0: a la derecha de la imagen
            a->obj_angulo = a->servo + (p->servo_izq_mayor ? -corr : corr);
            a->mensaje = "Objeto detectado: alineando";
            girar(a, t, AUTO_ALINEANDO, lado_de(a, a->obj_angulo), fabsf(a->obj_angulo - 90.0f),
                  AUTO_CONFIRMANDO);
            break;
        }
        if (en_paso >= asentar + p->t_observar_ms) {
            if (a->paso + 1 < a->n_pasos) {
                a->paso++;
                a->t_paso = t;
                a->limpio = false;
                a->servo = a->perfil_ang[a->paso];
            } else {
                decidir(a, s, t);
            }
        }
        break;
    }

    case AUTO_ALINEANDO:
    case AUTO_ESQUIVANDO:
        // Giro sobre su eje: no avanza, así que no depende del ultrasonido
        a->servo = 90;
        if (en_estado < a->giro_ms) {
            vi = a->giro_lado * p->vel_giro;
            vd = -vi;
        } else if (a->tras_giro == AUTO_CONFIRMANDO) {
            entrar(a, AUTO_CONFIRMANDO, t);
        } else if (a->tras_giro == AUTO_AVANZANDO) {
            iniciar_avance(a, s, t);
        } else {
            iniciar_escaneo(a, s, t);
        }
        break;

    case AUTO_CONFIRMANDO:
        // Quieto, cámara al frente: ¿se ve el objeto otra vez?
        a->servo = 90;
        if (en_estado < p->t_asentar_ms) break;
        abrir_ventana(a, s, t);
        if (visto_desde(s, a->t_limpio)) {
            fijar(a, t);
        } else if (en_estado > p->t_asentar_ms + p->t_confirmar_ms) {
            iniciar_escaneo(a, s, t);
            a->mensaje = "No se confirmó el objeto: escaneando de nuevo";
        }
        break;

    case AUTO_AVANZANDO:
        a->servo = 90;
        if (en_estado < p->t_asentar_ms) break;            // esperar a que el ultrasonido mire al frente
        abrir_ventana(a, s, t);
        // 1) El ultrasonido manda: frente cerrado => parar, aunque se vea el objeto
        if (dist < p->dist_libre_cm) {
            a->bloqueado = true;
            iniciar_escaneo(a, s, t);
            a->mensaje = "Obstáculo al frente: detenido, escaneando";
            break;
        }
        a->bloqueado = false;
        // 2) Si aparece el objeto mientras avanza, se fija directamente
        if (visto_desde(s, a->t_limpio)) {
            fijar(a, t);
            break;
        }
        // 3) Avanzar el tiempo indicado y volver a escanear
        if (en_estado < p->t_asentar_ms + p->t_avance_ms) {
            vi = vd = p->vel_avance;
        } else {
            iniciar_escaneo(a, s, t);
            a->mensaje = "Fin del avance: escaneando";
        }
        break;

    case AUTO_SIGUIENDO:
        a->servo = 90;
        seguidor_paso(s, t, dist_cm, &vi, &vd);
        if (s->estado == SEG_ESPERANDO) {                  // el seguidor lo perdió del todo
            iniciar_escaneo(a, s, t);
            a->mensaje = "Objeto perdido: escaneando de nuevo";
            vi = vd = 0;
        } else {
            a->mensaje = s->estado == SEG_BUSCANDO ? "Objeto fuera de vista: buscándolo"
                                                   : "Objeto fijado con éxito";
        }
        break;
    }

    *izq = vi;
    *der = vd;
    *servo = a->servo;
}

const char* auto_nombre_estado(EstadoAuto e) {
    switch (e) {
        case AUTO_ESCANEO:     return "escaneando";
        case AUTO_ALINEANDO:   return "alineando";
        case AUTO_CONFIRMANDO: return "confirmando";
        case AUTO_SIGUIENDO:   return "siguiendo";
        case AUTO_AVANZANDO:   return "avanzando";
        case AUTO_ESQUIVANDO:  return "esquivando";
        default:               return "apagado";
    }
}
