// =====================================================================
//  prueba_seguimiento.c — ¿Se queda centrado el carro mientras sigue el círculo?
// =====================================================================
//  Simula lo que hace inestable el seguimiento en el carro real:
//    - la cámara ve el pasado: un cuadro cada ~140 ms y el modelo tarda ~150 ms
//    - el modelo marca pocas celdas: el centro del blob salta de a una celda (1/16)
//    - los motores no giran por debajo de ~55 de PWM y a 120 el carro gira ~170°/s
//      (AUTO_MS_POR_GRADO = 6 en config.h)
//  y mide cuánto se aleja el círculo del centro, cuántas veces se sale de la
//  imagen y cuántas veces el carro cambia de sentido de giro ("baila").
//
//  Compilar y correr (desde esta carpeta, firmware_esp32/pruebas_pc):
//    gcc -O1 -Wall -I../main prueba_seguimiento.c ../main/seguidor.c -lm -o prueba_seguimiento && ./prueba_seguimiento
//  En Windows sin gcc:  pip install ziglang  y luego
//    python -m ziglang cc -O1 -Wall -I../main prueba_seguimiento.c ../main/seguidor.c -o prueba_seguimiento.exe
//    prueba_seguimiento.exe
// =====================================================================
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "seguidor.h"

#define FOV           60.0f   // campo de visión horizontal (grados)
#define PERIODO_CAM   140     // ms entre cuadros
static int LATENCIA = 150;          // ms desde que se toma el cuadro hasta tener el blob
#define PWM_MUERTO    55      // por debajo, la rueda no gira
static float GRADOS_S_120 = 167.0f;  // giro sobre su eje con PWM 120 (1000 / 6 ms por grado)

// Velocidad relativa de una rueda (1 = PWM 120), con la zona muerta del motor
static float rueda(int pwm) {
    int a = abs(pwm);
    if (a < PWM_MUERTO) return 0;
    float w = (float)(a - PWM_MUERTO) / (120 - PWM_MUERTO);
    return pwm < 0 ? -w : w;
}

typedef struct { float rumbo_inicial, objeto_se_mueve; unsigned semilla; } Escenario;

typedef struct { float error_medio, error_max; int salidas, cambios_sentido; } Resultado;

static Resultado simular(const ParamsSeguidor* p, Escenario e, int imprimir) {
    Seguidor s;
    seguidor_init(&s, p);
    srand(e.semilla);
    float rel = e.rumbo_inicial;         // dónde está el círculo respecto al frente (grados, + = derecha)
    float cola_rel[16];                  // cuadros "en proceso" (latencia)
    uint32_t cola_t[16];
    int n_cola = 0;
    uint32_t t_cuadro = 0;
    int izq = 0, der = 0, sentido_ant = 0;
    Resultado r = {0, 0, 0, 0};
    int n = 0, visible_ant = 1;

    seguidor_activar(&s, true, 0);
    for (uint32_t t = 0; t <= 12000; t += 10) {
        // Cámara: toma un cuadro y su resultado llega LATENCIA ms después
        if (t >= t_cuadro && n_cola < 16) {
            cola_rel[n_cola] = rel;
            cola_t[n_cola++] = t + LATENCIA;
            t_cuadro = t + PERIODO_CAM;
        }
        if (n_cola && t >= cola_t[0]) {
            float r0 = cola_rel[0];
            for (int i = 1; i < n_cola; ++i) { cola_rel[i - 1] = cola_rel[i]; cola_t[i - 1] = cola_t[i]; }
            n_cola--;
            Blob b = {0};
            int nb = 0;
            if (fabsf(r0) < FOV / 2) {
                // El blob del modelo: centro cuantizado a la celda (16 columnas) y a veces una celda corrido
                float cx = 0.5f + r0 / FOV;
                int celda = (int)(cx * 16);
                if (rand() % 4 == 0) celda += (rand() % 2) ? 1 : -1;
                if (celda < 0) celda = 0;
                if (celda > 15) celda = 15;
                b.cx = (celda + 0.5f) / 16.0f;
                b.cy = 0.4f;
                b.area = 3;
                b.pico = 0.8f;
                nb = 1;
            }
            seguidor_nueva_deteccion(&s, &b, nb, t);
        }
        // Control a 10 Hz (como tarea_control en main.c)
        if (t % 100 == 0) seguidor_paso(&s, t, 0, &izq, &der);
        // Física del giro: rueda izquierda más rápida => gira a la derecha => el círculo pasa a la izquierda
        float giro_gs = GRADOS_S_120 * (rueda(izq) - rueda(der)) / 2.0f;
        rel -= giro_gs * 0.010f;
        rel += e.objeto_se_mueve * 0.010f;   // el círculo camina de lado (grados/s)

        int sentido = giro_gs > 5 ? 1 : (giro_gs < -5 ? -1 : 0);
        if (sentido && sentido_ant && sentido != sentido_ant) r.cambios_sentido++;
        if (sentido) sentido_ant = sentido;
        int visible = fabsf(rel) < FOV / 2;
        if (!visible && visible_ant) r.salidas++;
        visible_ant = visible;
        if (t >= 2000) {                 // después de los primeros 2 s: ¿se mantiene centrado?
            r.error_medio += fabsf(rel);
            if (fabsf(rel) > r.error_max) r.error_max = fabsf(rel);
            n++;
        }
        if (imprimir && t % 500 == 0)
            printf("    t=%5.1fs círculo a %+6.1f°  motores %4d %4d  %s\n", t / 1000.0, rel, izq, der,
                   seguidor_nombre_estado(s.estado));
    }
    r.error_medio /= n;
    return r;
}

static void comparar(const char* nombre, Escenario e, const ParamsSeguidor* p) {
    Resultado r = simular(p, e, 0);
    printf("  %-38s error medio %5.1f°  máx %5.1f°  se salió de la imagen %d veces  cambios de sentido %d\n",
           nombre, r.error_medio, r.error_max, r.salidas, r.cambios_sentido);
}

int main(int argc, char** argv) {
    ParamsSeguidor p;
    seguidor_params_defecto(&p);
    p.area_objetivo = 1000;              // que no frene por tamaño: aquí solo importa el giro
    int detalle = 0;
    // Para probar otros valores sin recompilar: kp=60 kd=0 zm=0.15 gmax=90 tv=120 vel=140
    for (int i = 1; i < argc; ++i) {
        float v;
        if (sscanf(argv[i], "kp=%f", &v) == 1) p.kp_giro = v;
        else if (sscanf(argv[i], "kd=%f", &v) == 1) p.kd_giro = v;
        else if (sscanf(argv[i], "zm=%f", &v) == 1) p.zona_muerta = v;
        else if (sscanf(argv[i], "gmax=%f", &v) == 1) p.giro_max = (int)v;
        else if (sscanf(argv[i], "tv=%f", &v) == 1) p.t_giro_valido_ms = (uint32_t)v;
        else if (sscanf(argv[i], "vel=%f", &v) == 1) p.vel_max = (int)v;
        else if (sscanf(argv[i], "esg=%f", &v) == 1) p.error_solo_giro = v;
        else if (sscanf(argv[i], "gs=%f", &v) == 1) GRADOS_S_120 = v;           // otro carro: grados/s a PWM 120
        else if (sscanf(argv[i], "lat=%f", &v) == 1) LATENCIA = (int)v;          // otro retraso del modelo
        else detalle = 1;
    }
    Escenario escenarios[] = {
        {+20, 0, 1}, {-25, 0, 2}, {+5, 0, 3}, {0, 0, 4}, {+10, 8, 5}, {-15, -8, 6},
    };
    const char* nombres[] = {
        "círculo 20° a la derecha", "círculo 25° a la izquierda", "casi centrado (5°)",
        "centrado", "se mueve a la derecha 8°/s", "se mueve a la izquierda 8°/s",
    };
    printf("Parámetros: kp %.0f kd %.0f zona muerta %.2f giro máx %d pwm_min %d giro_valido %u ms\n",
           p.kp_giro, p.kd_giro, p.zona_muerta, p.giro_max, p.pwm_min, (unsigned)p.t_giro_valido_ms);
    float peor = 0;
    int salidas = 0;
    for (int i = 0; i < 6; ++i) {
        comparar(nombres[i], escenarios[i], &p);
        Resultado r = simular(&p, escenarios[i], 0);
        if (r.error_medio > peor) peor = r.error_medio;
        salidas += r.salidas;
    }
    if (detalle) {
        printf("\n  Detalle del primer escenario:\n");
        simular(&p, escenarios[0], 1);
    }
    int ok = peor < 8.0f && salidas == 0;
    printf("\n%s (peor error medio %.1f°, salidas de la imagen %d)\n", ok ? "SEGUIMIENTO ESTABLE" : "SEGUIMIENTO INESTABLE",
           peor, salidas);
    return ok ? 0 : 1;
}
