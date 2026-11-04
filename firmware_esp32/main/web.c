// web.c — ver web.h
#include "web.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "camara.h"
#include "config.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "uno.h"

static const char* TAG = "web";

// pagina.html va dentro del firmware (EMBED_TXTFILES en CMakeLists.txt)
extern const char pagina_inicio[] asm("_binary_pagina_html_start");
extern const char pagina_fin[] asm("_binary_pagina_html_end");
// hmi.html.gz: el HMI en React ya comprimido (EMBED_FILES: sin 0 al final)
extern const uint8_t hmi_inicio[] asm("_binary_hmi_html_gz_start");
extern const uint8_t hmi_fin[] asm("_binary_hmi_html_gz_end");

// El servidor atiende una petición a la vez, así que estos búferes se pueden reutilizar
static uint8_t* s_jpeg;
static uint8_t* s_bmp;
static char s_json[4096];

// ---------------------------------------------------------------- utilidades
static int pos_json;
static void json(const char* fmt, ...) {
    if (pos_json >= (int)sizeof(s_json)) return;
    va_list ap;
    va_start(ap, fmt);
    pos_json += vsnprintf(s_json + pos_json, sizeof(s_json) - pos_json, fmt, ap);
    va_end(ap);
}

// Lee un parámetro de la URL (?clave=valor). Devuelve true si estaba.
static bool parametro(httpd_req_t* req, const char* clave, char* valor, size_t cap) {
    char q[160];
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) != ESP_OK) return false;
    return httpd_query_key_value(q, clave, valor, cap) == ESP_OK;
}

static bool param_float(httpd_req_t* req, const char* clave, float* v) {
    char b[16];
    if (!parametro(req, clave, b, sizeof(b))) return false;
    *v = strtof(b, NULL);
    return true;
}

static float limitar(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Permite usar el HMI desde otro origen (p. ej. "npm run dev" en el PC conectado al carro)
static void cors(httpd_req_t* req) { httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*"); }

// RPM estimadas desde el PWM (no hay encoders). Con signo: negativo = hacia atrás.
static float rpm_estimada(int pwm) {
    int a = abs(pwm);
    if (a <= PWM_ARRANQUE) return 0;
    float r = RPM_MAX * (float)(a - PWM_ARRANQUE) / (float)(255 - PWM_ARRANQUE);
    return pwm < 0 ? -r : r;
}

// ---------------------------------------------------------------- manejadores
static esp_err_t h_hmi(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    return httpd_resp_send(req, (const char*)hmi_inicio, hmi_fin - hmi_inicio);
}

static esp_err_t h_pagina(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, pagina_inicio, pagina_fin - pagina_inicio - 1);  // -1: sin el 0 final
}

static esp_err_t h_foto(httpd_req_t* req) {
    cors(req);
    uint32_t seq = UINT32_MAX;       // "cualquier cuadro": devuelve el último sin esperar
    size_t n = camara_ultimo_jpeg(s_jpeg, JPEG_MAX, &seq, 300);
    if (n == 0) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "sin cuadro");
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, (const char*)s_jpeg, n);
}

// BMP de 24 bits: cabecera de 54 bytes + filas de abajo hacia arriba en orden B,G,R
static esp_err_t h_entrada(httpd_req_t* req) {
    const int w = MODELO_ANCHO, h = MODELO_ALTO, fila = w * 3;   // 384, ya es múltiplo de 4
    const uint32_t datos = fila * h, total = 54 + datos;
    uint8_t* c = s_bmp;
    memset(c, 0, 54);
    c[0] = 'B'; c[1] = 'M';
    memcpy(c + 2, &total, 4);
    uint32_t v = 54; memcpy(c + 10, &v, 4);
    v = 40; memcpy(c + 14, &v, 4);
    int32_t wi = w, hi = h; memcpy(c + 18, &wi, 4); memcpy(c + 22, &hi, 4);
    c[26] = 1; c[28] = 24;
    memcpy(c + 34, &datos, 4);
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    for (int y = 0; y < h; ++y) {
        const uint8_t* src = g_entrada + (h - 1 - y) * fila;
        uint8_t* dst = c + 54 + y * fila;
        for (int x = 0; x < w; ++x) {
            dst[3 * x] = src[3 * x + 2];
            dst[3 * x + 1] = src[3 * x + 1];
            dst[3 * x + 2] = src[3 * x];
        }
    }
    xSemaphoreGive(g_mutex);
    httpd_resp_set_type(req, "image/bmp");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, (const char*)c, total);
}

static esp_err_t h_estado(httpd_req_t* req) {
    cors(req);
    ImuDatos imu;
    bool hay_imu = uno_imu(&imu);
    pos_json = 0;
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    const Ajustes* a = &g_ajustes;
    const Estado* e = &g_estado;
    const ParamsSeguidor* p = &g_seg.p;
    const Autonomo* au = &g_auto;
    json("{\"modo\":\"%s\",\"modelo\":%s,\"prueba\":%d,",
         a->modo == MODO_MODELO ? "modelo" : "color", g_modelo_ok ? "true" : "false", g_info.prueba);
    // Modo de control y búsqueda autónoma (lo que necesita el HMI)
    json("\"control\":\"%s\",\"auto\":\"%s\",\"fijado\":%s,\"ev_fijado\":%u,\"msg\":\"%s\",",
         a->autonomo ? "auto" : "manual", auto_nombre_estado(au->estado), au->fijado ? "true" : "false",
         (unsigned)au->eventos_fijado, au->mensaje);
    json("\"servo\":%d,\"frente\":%s,\"obstaculo\":%s,\"bloqueado\":%s,\"escaneos\":%u,\"obj_ang\":%.0f,",
         e->servo, e->frente_vigilado ? "true" : "false", e->obstaculo ? "true" : "false",
         au->bloqueado ? "true" : "false", (unsigned)au->escaneos, au->obj_angulo);
    json("\"avance_ms\":%u,\"ms_grado\":%.1f,\"paso\":%d,\"perfil\":[",
         (unsigned)au->p.t_avance_ms, au->p.ms_por_grado, au->p.servo_paso);
    for (int i = 0; i < au->n_pasos; ++i)
        json("%s[%d,%d]", i ? "," : "", au->perfil_ang[i], au->perfil[i]);
    json("],");
    // Velocidad / RPM estimadas, límites de distancia e IMU
    float ri = rpm_estimada(e->izq), rd = rpm_estimada(e->der);
    float vel = (ri + rd) / 2.0f * 3.14159f * RUEDA_DIAMETRO_CM / 60.0f;   // cm/s
    json("\"rpm_izq\":%.0f,\"rpm_der\":%.0f,\"rpm_max\":%.0f,\"vel_cms\":%.1f,",
         ri, rd, RPM_MAX, vel);
    json("\"dist_freno\":%d,\"dist_libre\":%d,\"servo_izq_mayor\":%d,",
         DIST_FRENO_CM, au->p.dist_libre_cm, au->p.servo_izq_mayor);
    if (hay_imu)
        json("\"imu\":{\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f,\"yaw\":%.1f,\"pitch\":%.1f,\"roll\":%.1f},",
             imu.ax, imu.ay, imu.az, imu.yaw, imu.pitch, imu.roll);
    else
        json("\"imu\":null,");
    json("\"zm\":%.2f,\"tgiro\":%u,", p->zona_muerta, (unsigned)p->t_giro_valido_ms);
    json("\"umbral\":%.2f,\"delta\":%d,\"area\":%.1f,\"vel\":%d,\"kp\":%.0f,\"kd\":%.0f,",
         a->modo == MODO_MODELO ? a->umbral_modelo : a->umbral_color, a->delta_rojo,
         p->area_objetivo, p->vel_max, p->kp_giro, p->kd_giro);
    json("\"estado\":\"%s\",\"izq\":%d,\"der\":%d,\"dist\":%d,\"uno\":%s,",
         seguidor_nombre_estado(g_seg.estado), e->izq, e->der, e->dist_cm,
         uno_ms_desde_ultimo_dato() < 1000 ? "true" : "false");
    json("\"fps\":%.1f,\"det_s\":%.1f,\"ms\":%.1f,", camara_fps(), e->det_por_s, e->ms_deteccion);
    if (g_seg.tiene_objetivo)
        json("\"obj\":[%.3f,%.3f,%.1f],", g_seg.cx, g_seg.cy, g_seg.area);
    else
        json("\"obj\":null,");
    json("\"elegido\":%d,\"blobs\":[", e->elegido);
    for (int i = 0; i < e->n_blobs; ++i)
        json("%s[%.3f,%.3f,%d,%.2f]", i ? "," : "", e->blobs[i].cx, e->blobs[i].cy,
             e->blobs[i].area, e->blobs[i].pico);
    json("],\"mapa\":[");
    for (int i = 0; i < MAPA_CELDAS; ++i) json("%s%d", i ? "," : "", (int)(e->mapa[i] * 100 + 0.5f));
    json("]}");
    xSemaphoreGive(g_mutex);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, s_json, pos_json < (int)sizeof(s_json) ? pos_json : (int)sizeof(s_json) - 1);
}

static esp_err_t h_ajustes(httpd_req_t* req) {
    cors(req);
    char b[16];
    float v;
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    Ajustes* a = &g_ajustes;
    if (parametro(req, "modo", b, sizeof(b)))
        a->modo = (strcmp(b, "modelo") == 0 && g_modelo_ok) ? MODO_MODELO : MODO_COLOR;
    if (param_float(req, "umbral", &v)) {   // el umbral que se cambia es el del modo actual
        v = limitar(v, 0.02f, 0.98f);
        if (a->modo == MODO_MODELO) a->umbral_modelo = v; else a->umbral_color = v;
    }
    if (param_float(req, "delta", &v)) a->delta_rojo = (int)limitar(v, 0, 255);
    if (param_float(req, "area", &v)) g_seg.p.area_objetivo = limitar(v, 1, MAPA_CELDAS);
    if (param_float(req, "vel", &v)) g_seg.p.vel_max = (int)limitar(v, 0, 255);
    if (param_float(req, "kp", &v)) g_seg.p.kp_giro = limitar(v, 0, 500);
    if (param_float(req, "kd", &v)) g_seg.p.kd_giro = limitar(v, 0, 200);
    if (param_float(req, "zm", &v)) g_seg.p.zona_muerta = limitar(v, 0, 0.5f);
    if (param_float(req, "tgiro", &v)) g_seg.p.t_giro_valido_ms = (uint32_t)limitar(v, 0, 1000);   // 0 = siempre
    // Búsqueda autónoma
    if (param_float(req, "avance_ms", &v)) g_auto.p.t_avance_ms = (uint32_t)limitar(v, 500, 10000);
    if (param_float(req, "ms_grado", &v)) g_auto.p.ms_por_grado = limitar(v, 1, 30);
    if (param_float(req, "paso", &v)) g_auto.p.servo_paso = (int)limitar(v, 10, 90);   // vale desde el próximo barrido
    xSemaphoreGive(g_mutex);
    return h_estado(req);
}

// /modo?m=auto  entra a BÚSQUEDA AUTÓNOMA (arranca con el escaneo inicial)
// /modo?m=manual  sale y vuelve a MANUAL. El cambio lo aplica la tarea de control.
static esp_err_t h_modo(httpd_req_t* req) {
    cors(req);
    char b[16];
    if (parametro(req, "m", b, sizeof(b))) {
        xSemaphoreTake(g_mutex, portMAX_DELAY);
        g_ajustes.autonomo = strcmp(b, "auto") == 0;
        xSemaphoreGive(g_mutex);
    }
    return h_estado(req);
}

// /servo?ang=0..180  solo en MANUAL (en autónomo el servo lo maneja el barrido)
static esp_err_t h_servo(httpd_req_t* req) {
    cors(req);
    float ang = 90;
    if (!param_float(req, "ang", &ang)) return httpd_resp_sendstr(req, "falta ang");
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    bool manual = !g_ajustes.autonomo;
    if (manual) g_manual_servo = (int)limitar(ang, 0, 180);
    xSemaphoreGive(g_mutex);
    return httpd_resp_sendstr(req, manual ? "ok" : "ignorado: modo autonomo");
}

// /imu?yaw0=1  pone el yaw en 0 (el giroscopio se desvía con el tiempo)
static esp_err_t h_imu(httpd_req_t* req) {
    cors(req);
    char b[8];
    if (parametro(req, "yaw0", b, sizeof(b))) uno_enviar("Y\n");
    return httpd_resp_sendstr(req, "ok");
}

static esp_err_t h_manual(httpd_req_t* req) {
    cors(req);
    float izq = 0, der = 0;
    param_float(req, "izq", &izq);
    param_float(req, "der", &der);
    xSemaphoreTake(g_mutex, portMAX_DELAY);
    if (g_ajustes.autonomo) {            // en autónomo los motores no se controlan a mano
        xSemaphoreGive(g_mutex);
        return httpd_resp_sendstr(req, "ignorado: modo autonomo");
    }
    g_manual_izq = (int)limitar(izq, -255, 255);
    g_manual_der = (int)limitar(der, -255, 255);
    g_manual_t = ahora_ms();
    xSemaphoreGive(g_mutex);
    return httpd_resp_sendstr(req, "ok");
}

void web_iniciar(void) {
    s_jpeg = heap_caps_malloc(JPEG_MAX, MALLOC_CAP_SPIRAM);
    s_bmp = heap_caps_malloc(54 + MODELO_BYTES, MALLOC_CAP_SPIRAM);
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 8192;
    cfg.max_uri_handlers = 12;
    cfg.lru_purge_enable = true;
    httpd_handle_t srv = NULL;
    if (httpd_start(&srv, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo iniciar el servidor web");
        return;
    }
    const httpd_uri_t rutas[] = {
        {.uri = "/", .method = HTTP_GET, .handler = h_hmi},
        {.uri = "/debug", .method = HTTP_GET, .handler = h_pagina},
        {.uri = "/foto.jpg", .method = HTTP_GET, .handler = h_foto},
        {.uri = "/entrada.bmp", .method = HTTP_GET, .handler = h_entrada},
        {.uri = "/estado", .method = HTTP_GET, .handler = h_estado},
        {.uri = "/ajustes", .method = HTTP_GET, .handler = h_ajustes},
        {.uri = "/manual", .method = HTTP_GET, .handler = h_manual},
        {.uri = "/modo", .method = HTTP_GET, .handler = h_modo},
        {.uri = "/servo", .method = HTTP_GET, .handler = h_servo},
        {.uri = "/imu", .method = HTTP_GET, .handler = h_imu},
    };
    for (size_t i = 0; i < sizeof(rutas) / sizeof(rutas[0]); ++i) httpd_register_uri_handler(srv, &rutas[i]);
    ESP_LOGI(TAG, "Servidor web listo");
}
