// seguidor.c — ver seguidor.h  (adaptado del seguidor de personas del equipo)
#include "seguidor.h"
#include <math.h>

static float limitar(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Milisegundos desde 'antes' hasta 't'. Si 'antes' quedó un poco en el futuro
// (otra tarea leyó el reloj después), cuenta 0 en vez de un número enorme.
static uint32_t transcurrido(uint32_t t, uint32_t antes) {
    return (int32_t)(t - antes) < 0 ? 0 : t - antes;
}

// Distancia entre dos posiciones en la imagen (lo vertical pesa la mitad)
static float distancia(float ax, float ay, float bx, float by) {
    return fabsf(ax - bx) + 0.5f * fabsf(ay - by);
}

void seguidor_params_defecto(ParamsSeguidor* p) {
    p->confirmaciones = 2;
    p->salto_max = 0.30f;
    p->kp_giro = 120.0f;
    p->kd_giro = 12.0f;
    p->zona_muerta = 0.08f;
    p->giro_max = 130;
    p->error_solo_giro = 0.50f;
    p->vel_max = 140;
    p->pwm_min = 70;
    p->area_objetivo = 14.0f;
    p->dist_min_cm = 0;          // el UNO ya frena solo con el comando "D" (ver config.h)
    p->t_perdida_ms = 1000;
    p->t_busqueda_ms = 6000;
    p->giro_busqueda = 110;
    p->pulso_ms = 200;
    p->pausa_ms = 600;
}

void seguidor_init(Seguidor* s, const ParamsSeguidor* p) {
    s->p = *p;
    s->activo = false;
    s->estado = SEG_INACTIVO;
    s->tiene_objetivo = false;
    s->cx = 0.5f; s->cy = 0.5f; s->area = 0;
    s->error = s->derivada = 0;
    s->t_visto = s->t_det_anterior = 0;
    s->ultimo_lado = 1;
    s->cand_cx = s->cand_cy = 0;
    s->cand_n = 0;
    s->cand_t = 0;
    s->izq = s->der = 0;
}

void seguidor_activar(Seguidor* s, bool activo, uint32_t t_ms) {
    s->activo = activo;
    s->tiene_objetivo = false;
    s->cand_n = 0;
    s->estado = activo ? SEG_ESPERANDO : SEG_INACTIVO;
    s->t_visto = t_ms;
}

int seguidor_nueva_deteccion(Seguidor* s, const Blob* b, int n, uint32_t t) {
    bool reciente = s->tiene_objetivo && transcurrido(t, s->t_visto) < s->p.t_perdida_ms + 400;
    int elegido = -1;

    if (reciente) {
        // Seguimiento: el blob más cercano al balón anterior (no el más grande: así
        // no salta a otra cosa roja que aparezca en la imagen)
        float mejor = 1e9f;
        for (int i = 0; i < n; ++i) {
            float d = distancia(b[i].cx, b[i].cy, s->cx, s->cy);
            if (d < s->p.salto_max && d < mejor) { mejor = d; elegido = i; }
        }
    } else {
        // Nadie enganchado: el blob principal debe aparecer en el mismo lugar en varias
        // detecciones seguidas. Un reflejo rojo de un solo cuadro no mueve el carro.
        if (n == 0) return -1;
        elegido = 0;
        bool mismo = s->cand_n > 0 && transcurrido(t, s->cand_t) < 1000 &&
                     distancia(b[0].cx, b[0].cy, s->cand_cx, s->cand_cy) < s->p.salto_max;
        s->cand_n = mismo ? s->cand_n + 1 : 1;
        s->cand_cx = b[0].cx;
        s->cand_cy = b[0].cy;
        s->cand_t = t;
        if (s->cand_n < s->p.confirmaciones) return -1;
        s->cand_n = 0;
    }
    if (elegido < 0) return -1;

    const Blob* o = &b[elegido];
    float a = reciente ? 0.7f : 1.0f;     // filtro: 70% nuevo, 30% anterior (suaviza el ruido)
    s->cx = a * o->cx + (1 - a) * s->cx;
    s->cy = a * o->cy + (1 - a) * s->cy;
    s->area = a * o->area + (1 - a) * s->area;

    float e = (s->cx - 0.5f) * 2.0f;      // -1 = borde izquierdo, 0 = centro, +1 = borde derecho
    float dt = transcurrido(t, s->t_det_anterior) / 1000.0f;
    s->derivada = (reciente && dt > 0.05f && dt < 2.0f) ? (e - s->error) / dt : 0.0f;
    s->error = e;
    s->t_det_anterior = t;
    s->t_visto = t;
    s->tiene_objetivo = true;
    if (fabsf(e) > 0.15f) s->ultimo_lado = e > 0 ? 1 : -1;
    return elegido;
}

// Los motores no se mueven con PWM bajo. Valores pequeños se anulan y los
// medianos se suben al mínimo útil (evita que un lado "zumbe" sin girar).
static int con_minimo(float v, int pwm_min) {
    if (fabsf(v) < 0.5f * pwm_min || fabsf(v) < 1.0f) return 0;
    int s = v > 0 ? 1 : -1;
    float m = fabsf(v) < pwm_min ? pwm_min : fabsf(v);
    return s * (int)(m > 255 ? 255 : m);
}

void seguidor_paso(Seguidor* s, uint32_t t, int dist_cm, int* izq, int* der) {
    float vi = 0, vd = 0;
    const ParamsSeguidor* p = &s->p;
    uint32_t sin_ver = transcurrido(t, s->t_visto);

    if (!s->activo) {
        s->estado = SEG_INACTIVO;
    } else if (s->tiene_objetivo && sin_ver < p->t_perdida_ms) {
        s->estado = SEG_SIGUIENDO;
        // Giro: controlador PD. P = "qué tan descentrado está", D = "qué tan rápido se mueve"
        float e = s->error;
        float giro = (fabsf(e) < p->zona_muerta) ? 0.0f : p->kp_giro * e + p->kd_giro * s->derivada;
        giro = limitar(giro, -p->giro_max, p->giro_max);

        // Avance: rápido si el balón se ve pequeño (lejos), cero al llegar a area_objetivo
        float avance = 0;
        if (fabsf(e) < p->error_solo_giro) {
            avance = p->vel_max * (1.0f - s->area / p->area_objetivo);
            avance = limitar(avance, 0, p->vel_max) * (1.0f - fabsf(e));
            if (p->dist_min_cm > 0 && dist_cm > 0 && dist_cm < p->dist_min_cm) avance = 0;
        }
        vi = avance + giro;      // balón a la derecha (e > 0) => rueda izquierda más rápida
        vd = avance - giro;
    } else if (s->tiene_objetivo && sin_ver < p->t_busqueda_ms) {
        s->estado = SEG_BUSCANDO;
        uint32_t fase = transcurrido(sin_ver, p->t_perdida_ms) % (p->pulso_ms + p->pausa_ms);
        if (fase < p->pulso_ms) {
            vi = s->ultimo_lado * p->giro_busqueda;
            vd = -vi;
        }
    } else {
        s->estado = SEG_ESPERANDO;
        s->tiene_objetivo = false;
    }

    s->izq = con_minimo(vi, p->pwm_min);
    s->der = con_minimo(vd, p->pwm_min);
    *izq = s->izq;
    *der = s->der;
}

const char* seguidor_nombre_estado(EstadoSeguidor e) {
    switch (e) {
        case SEG_SIGUIENDO: return "siguiendo";
        case SEG_BUSCANDO:  return "buscando";
        case SEG_ESPERANDO: return "esperando";
        default:            return "inactivo";
    }
}
