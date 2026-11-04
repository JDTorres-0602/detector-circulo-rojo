// prueba_autonomo.c — Simula el carro en el PC para probar autonomo.c + seguidor.c
// Compilar y correr (desde esta carpeta, firmware_esp32/pruebas_pc):
//   gcc -O1 -Wall -I../main prueba_autonomo.c ../main/autonomo.c ../main/seguidor.c -lm -o prueba_autonomo && ./prueba_autonomo
//  En Windows sin gcc:  pip install ziglang  y luego
//   python -m ziglang cc -O1 -Wall -I../main prueba_autonomo.c ../main/autonomo.c ../main/seguidor.c -o prueba_autonomo.exe
//   prueba_autonomo.exe
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "autonomo.h"

// Mundo: rumbo en grados (positivo = izquierda). Servo > 90 mira a la izquierda.
typedef struct {
    float rumbo, x, y;       // carro
    float obj_x, obj_y;      // objeto rojo (si hay)
    bool hay_obj;
    float pared_y;           // pared delante en y (si > 0)
    float ms_por_grado;      // giro real del carro
} Mundo;

static float ang_a(float dx, float dy) { return atan2f(dx, dy) * -180.0f / (float)M_PI; } // 0 = +y, izq +
static float norm(float a) { while (a > 180) a -= 360; while (a < -180) a += 360; return a; }

static int distancia(const Mundo* m, int servo) {
    float dir = m->rumbo + (servo - 90);
    float best = 0;
    if (m->pared_y > 0) {   // pared horizontal en y = pared_y
        float r = dir * (float)M_PI / 180.0f;
        float dy = cosf(r);
        if (dy > 0.2f) best = (m->pared_y - m->y) / dy;
    }
    if (m->hay_obj) {
        float d = hypotf(m->obj_x - m->x, m->obj_y - m->y);
        float rel = norm(ang_a(m->obj_x - m->x, m->obj_y - m->y) - dir);
        if (fabsf(rel) < 15 && (best == 0 || d < best)) best = d;
    }
    return best > 0 && best < 400 ? (int)best : 0;
}

// Devuelve 1 blob si el objeto está en el campo de visión
static int ver(const Mundo* m, int servo, Blob* b) {
    if (!m->hay_obj) return 0;
    float dir = m->rumbo + (servo - 90);
    float rel = norm(ang_a(m->obj_x - m->x, m->obj_y - m->y) - dir);
    if (fabsf(rel) > 28) return 0;
    float d = hypotf(m->obj_x - m->x, m->obj_y - m->y);
    memset(b, 0, sizeof(*b));
    b->cx = 0.5f - rel / 60.0f;
    b->cy = 0.6f;
    b->area = (uint8_t)fminf(60, 600.0f / (d + 1));
    b->pico = 0.9f;
    return 1;
}

static bool filtro(int* izq, int* der, int dist, bool frente) {
    if (!frente || dist <= 0 || dist >= 15) return false;
    int a = (*izq + *der) / 2;
    if (a <= 0) return false;
    *izq -= a; *der -= a;
    if (abs(*izq) < 60) *izq = *der = 0;
    return true;
}

static void correr(const char* nombre, Mundo m, uint32_t dur_ms) {
    printf("\n===== %s =====\n", nombre);
    Seguidor s; ParamsSeguidor ps; seguidor_params_defecto(&ps); ps.dist_min_cm = 15; seguidor_init(&s, &ps);
    Autonomo a; ParamsAuto pa; auto_params_defecto(&pa); auto_init(&a, &pa);
    uint32_t t = 1000;
    auto_activar(&a, &s, true, t);
    int servo_real = 90, servo_cmd = 90; uint32_t t_servo = t;
    EstadoAuto antes = AUTO_APAGADO; uint32_t ev = 0; int frenos = 0; float dmin = 1e9;
    for (; t < 1000 + dur_ms; t += 10) {
        // servo físico: llega en ~250 ms
        if (servo_real != servo_cmd) { int d = servo_cmd - servo_real; int st = d > 0 ? (d > 7 ? 7 : d) : (d < -7 ? -7 : d); servo_real += st; }
        if (t % 200 == 0) {   // visión a 5 Hz con ~100 ms de retraso (usa el servo de antes)
            Blob b; int n = ver(&m, servo_real, &b);
            seguidor_nueva_deteccion(&s, &b, n, t);
        }
        if (t % 100 == 0) {   // control a 10 Hz
            int dist = distancia(&m, servo_real);
            int izq, der, sv;
            auto_paso(&a, &s, t, dist, &izq, &der, &sv);
            if (sv != servo_cmd) t_servo = t;
            servo_cmd = sv;
            bool frente = abs(sv - 90) <= 25 && t - t_servo >= 350;
            if (filtro(&izq, &der, dist, frente)) frenos++;
            // cinemática simple: 100 ms
            float giro = (der - izq) / 2.0f, av = (izq + der) / 2.0f;
            if (fabsf(giro) > 1) m.rumbo += (giro / pa.vel_giro) * 100.0f / m.ms_por_grado;
            float r = m.rumbo * (float)M_PI / 180.0f;
            float paso = av / 130.0f * 3.0f;              // 3 cm por tick a vel 130
            m.x += -sinf(r) * paso; m.y += cosf(r) * paso;
            if (m.hay_obj) { float d = hypotf(m.obj_x - m.x, m.obj_y - m.y); if (d < dmin) dmin = d; }
            if (m.pared_y > 0 && m.y > m.pared_y - 3) { printf("  !!! CHOQUE con la pared en t=%u\n", t); return; }
            if (a.estado != antes || a.eventos_fijado != ev) {
                printf("  t=%5.1fs %-12s servo=%3d rumbo=%6.1f pos=(%.0f,%.0f) dist=%3d | %s\n",
                       (t - 1000) / 1000.0, auto_nombre_estado(a.estado), sv, m.rumbo, m.x, m.y, dist, a.mensaje);
                antes = a.estado; ev = a.eventos_fijado;
            }
        }
    }
    printf("  -> fijado %u veces, frenos de seguridad %d, distancia mínima al objeto %.0f cm, estado final %s\n",
           a.eventos_fijado, frenos, m.hay_obj ? dmin : -1, auto_nombre_estado(a.estado));
}

int main(void) {
    Mundo base = {.rumbo = 0, .x = 0, .y = 0, .hay_obj = false, .pared_y = 0, .ms_por_grado = 6.0f};

    Mundo m1 = base; m1.hay_obj = true; m1.obj_x = -80; m1.obj_y = 80;   // 45° a la izquierda
    correr("1. Objeto a la izquierda: escanea, alinea, fija y sigue", m1, 20000);

    Mundo m2 = base; m2.hay_obj = true; m2.obj_x = 120; m2.obj_y = 20;   // casi 80° a la derecha
    correr("2. Objeto a la derecha, giro mal calibrado (+30%)", (m2.ms_por_grado = 7.8f, m2), 25000);

    Mundo m3 = base; m3.pared_y = 22;                                     // pared enfrente
    correr("3. Sin objeto y pared enfrente: esquiva", m3, 20000);

    Mundo m4 = base; m4.hay_obj = true; m4.obj_x = 0; m4.obj_y = 300;    // muy lejos: no se ve al inicio?
    correr("4. Objeto lejos al frente", m4, 30000);

    Mundo m5 = base;
    correr("5. Nada: escanea, avanza 3 s, repite", m5, 22000);
    Mundo m6 = base; m6.pared_y = 70;
    correr("6. Pared aparece mientras avanza: se detiene antes", m6, 15000);

    Mundo m7 = base; m7.hay_obj = true; m7.obj_x = 0; m7.obj_y = 150; m7.pared_y = 60;
    correr("7. Objeto visible DETRÁS de un obstáculo: el ultrasonido manda", m7, 20000);
    return 0;
}
