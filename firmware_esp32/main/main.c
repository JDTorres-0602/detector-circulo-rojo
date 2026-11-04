// =====================================================================
//  main.c — Arranque y las dos tareas principales del carro
// =====================================================================
//  Tareas (FreeRTOS, corren "al mismo tiempo"):
//    camara   (camara.c)  captura JPEG sin parar
//    vision   (aquí)      JPEG -> 128x96 -> mapa 12x16 (color o modelo) -> blobs -> seguidor
//    control  (aquí)      10 veces/s: modo MANUAL o BÚSQUEDA AUTÓNOMA -> filtro de
//                         seguridad del ultrasonido -> "M izq der" y "V grados" al UNO
//    httpd    (web.c)     página, foto, estado y ajustes
//    uno_rx   (uno.c)     recibe la distancia del ultrasonido
// =====================================================================
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "camara.h"
#include "config.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "uno.h"
#include "usb_fotos.h"
#include "vision_color.h"
#include "web.h"
#include "wifi.h"

static const char* TAG = "main";

SemaphoreHandle_t g_mutex;
Ajustes g_ajustes;
Estado g_estado;
Seguidor g_seg;
Autonomo g_auto;
InfoDetector g_info;
bool g_modelo_ok = false;
uint8_t* g_entrada = NULL;
int g_manual_izq = 0, g_manual_der = 0;
uint32_t g_manual_t = 0;
int g_manual_servo = 90;

uint32_t ahora_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static void tarea_vision(void* arg) {
    uint8_t* jpeg = heap_caps_malloc(JPEG_MAX, MALLOC_CAP_SPIRAM);
    uint8_t* rgb = heap_caps_malloc(MODELO_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!rgb) rgb = heap_caps_malloc(MODELO_BYTES, MALLOC_CAP_SPIRAM);
    float mapa[MAPA_CELDAS];
    Blob blobs[MAX_BLOBS];
    uint32_t seq = 0;
    int64_t t_ultima = esp_timer_get_time();
    float det_por_s = 0;

    while (true) {
        size_t n = camara_ultimo_jpeg(jpeg, JPEG_MAX, &seq, 500);
        if (n == 0) continue;
        if (camara_preprocesar(jpeg, n, rgb) != ESP_OK) continue;

        xSemaphoreTake(g_mutex, portMAX_DELAY);
        Ajustes a = g_ajustes;
        memcpy(g_entrada, rgb, MODELO_BYTES);
        xSemaphoreGive(g_mutex);

        // 1) Imagen -> mapa 12x16. Los dos detectores entregan el mismo tipo de mapa.
        int64_t t0 = esp_timer_get_time();
        float umbral;
        if (a.modo == MODO_MODELO && g_modelo_ok) {
            if (detector_ejecutar(rgb, mapa) < 0) continue;
            umbral = a.umbral_modelo;
        } else {
            mapa_rojo(rgb, MODELO_ANCHO, MODELO_ALTO, a.delta_rojo, mapa);
            umbral = a.umbral_color;
        }
        // 2) Mapa -> blobs (manchas = balones candidatos)
        int nb = buscar_blobs(mapa, umbral, blobs, MAX_BLOBS);
        int64_t t1 = esp_timer_get_time();
        det_por_s = 0.8f * det_por_s + 0.2f * (1e6f / (float)(t1 - t_ultima));
        t_ultima = t1;

        // 3) Blobs -> seguidor (decide cuál es "el" balón)
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        int elegido = seguidor_nueva_deteccion(&g_seg, blobs, nb, ahora_ms());
        memcpy(g_estado.mapa, mapa, sizeof(mapa));
        memcpy(g_estado.blobs, blobs, sizeof(Blob) * nb);
        g_estado.n_blobs = nb;
        g_estado.elegido = elegido;
        g_estado.ms_deteccion = (t1 - t0) / 1000.0f;
        g_estado.det_por_s = det_por_s;
        xSemaphoreGive(g_mutex);
        // Ceder 1 tick: sin esto la tarea IDLE de este núcleo nunca corre y salta el watchdog
        vTaskDelay(1);
    }
}

// ---------------------------------------------------------------------
//  Filtro de seguridad (último eslabón antes de los motores, en CUALQUIER modo)
//  Si el ultrasonido mira al frente y hay algo más cerca que DIST_FRENO_CM, se
//  quita la parte "hacia adelante" del comando: el carro puede girar sobre su
//  eje o retroceder, pero no avanzar. Ver el objeto rojo no cambia esto.
// ---------------------------------------------------------------------
static bool filtro_seguridad(int* izq, int* der, int dist, bool frente_vigilado) {
    if (!frente_vigilado || dist <= 0 || dist >= DIST_FRENO_CM) return false;
    int avance = (*izq + *der) / 2;
    if (avance <= 0) return false;              // ya está quieto, girando o retrocediendo
    *izq -= avance;
    *der -= avance;
    if (abs(*izq) < 60) *izq = *der = 0;        // lo que queda es tan poco que solo zumbaría
    return true;
}

static void tarea_control(void* arg) {
    TickType_t prox = xTaskGetTickCount();
    int servo_enviado = -1;
    uint32_t t_servo = 0;                       // cuándo se movió el servo por última vez
    bool autonomo_antes = false;
    while (true) {
        vTaskDelayUntil(&prox, pdMS_TO_TICKS(PERIODO_CONTROL_MS));
        int dist = uno_distancia_cm();
        int izq = 0, der = 0, servo = 90;
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        uint32_t t = ahora_ms();

        // Cambio de modo (botones "entrar" / "salir" de la página)
        if (g_ajustes.autonomo != autonomo_antes) {
            auto_activar(&g_auto, &g_seg, g_ajustes.autonomo, t);
            g_manual_servo = 90;
            g_manual_t = 0;
            autonomo_antes = g_ajustes.autonomo;
        }

        if (g_ajustes.autonomo) {
            // BÚSQUEDA AUTÓNOMA: las órdenes manuales se ignoran
            auto_paso(&g_auto, &g_seg, t, dist, &izq, &der, &servo);
        } else {
            // MANUAL: motores y servo desde la página; si dejan de llegar órdenes, para
            if (g_manual_t && t - g_manual_t < EXPIRA_MANUAL_MS) {
                izq = g_manual_izq;
                der = g_manual_der;
            }
            servo = g_manual_servo;
        }

        if (servo != servo_enviado) t_servo = t;
        bool frente = abs(servo - 90) <= FRENTE_TOLERANCIA && t - t_servo >= FRENTE_ASENTAR_MS;
        bool obstaculo = filtro_seguridad(&izq, &der, dist, frente);

        g_estado.izq = izq;
        g_estado.der = der;
        g_estado.dist_cm = dist;
        g_estado.servo = servo;
        g_estado.frente_vigilado = frente;
        g_estado.obstaculo = obstaculo;
        xSemaphoreGive(g_mutex);

        if (servo != servo_enviado) {
            uno_servo(servo);
            servo_enviado = servo;
        }
        uno_motores(izq, der);           // se envía siempre (aunque sea 0 0): alimenta el watchdog del UNO
    }
}

void app_main(void) {
    esp_err_t r = nvs_flash_init();      // el WiFi guarda su calibración en NVS
    if (r == ESP_ERR_NVS_NO_FREE_PAGES || r == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    g_mutex = xSemaphoreCreateMutex();
    g_entrada = heap_caps_calloc(1, MODELO_BYTES, MALLOC_CAP_SPIRAM);
    g_ajustes = (Ajustes){
        .modo = MODO_COLOR, .autonomo = false,     // arranca en MANUAL
        .umbral_color = UMBRAL_COLOR_DEFECTO, .umbral_modelo = UMBRAL_MODELO_DEFECTO,
        .delta_rojo = DELTA_ROJO_DEFECTO,
    };
    ParamsSeguidor p;
    seguidor_params_defecto(&p);
    p.dist_min_cm = DIST_FRENO_CM;      // el seguidor tampoco avanza con algo encima
    seguidor_init(&g_seg, &p);

    ParamsAuto pa;
    auto_params_defecto(&pa);
    pa.servo_min = AUTO_SERVO_MIN;
    pa.servo_max = AUTO_SERVO_MAX;
    pa.servo_paso = AUTO_SERVO_PASO;
    pa.servo_izq_mayor = AUTO_SERVO_IZQ_MAYOR;
    pa.t_avance_ms = AUTO_AVANCE_MS;
    pa.vel_avance = AUTO_VEL_AVANCE;
    pa.vel_giro = AUTO_VEL_GIRO;
    pa.ms_por_grado = AUTO_MS_POR_GRADO;
    pa.dist_libre_cm = AUTO_DIST_LIBRE_CM;
    auto_init(&g_auto, &pa);

    uno_iniciar();
    if (!camara_iniciar()) {
        ESP_LOGE(TAG, "No respondió la cámara con ningún cableado. Revisa el cable plano y config.h");
        return;
    }
    camara_arrancar_tarea();
    usb_fotos_iniciar();
    wifi_iniciar_ap();
    web_iniciar();

    // El modelo va al final: así mide cuánta RAM interna quedó después del WiFi
    g_modelo_ok = detector_iniciar(&g_info) == ESP_OK;
    if (g_modelo_ok) g_ajustes.modo = MODO_MODELO;
    ESP_LOGI(TAG, "Modo inicial: %s | RAM interna libre: %u KB | PSRAM libre: %u KB",
             g_modelo_ok ? "MODELO" : "COLOR",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));

    xTaskCreatePinnedToCore(tarea_vision, "vision", 8192, NULL, 5, NULL, 1);
    xTaskCreate(tarea_control, "control", 4096, NULL, 7, NULL);
}
