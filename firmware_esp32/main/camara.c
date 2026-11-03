// camara.c — ver camara.h  (basado en el firmware de detección de personas del equipo)
#include "camara.h"
#include "sdkconfig.h"
#include <string.h>
#include "config.h"
#include "esp_camera.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "jpeg_decoder.h"
#include "redimensionar.h"

static const char* TAG = "camara";

typedef struct {
    const char* nombre;
    int pwdn, reset, xclk, sda, scl, d7, d6, d5, d4, d3, d2, d1, d0, vsync, href, pclk;
} Cableado;

// Cableados posibles de la placa de cámara del ELEGOO V4.0.
//                pwdn reset xclk sda scl  d7  d6  d5  d4  d3  d2  d1  d0 vsync href pclk
static const Cableado CABLEADOS[] = {
#if CONFIG_IDF_TARGET_ESP32S3
    // Módulo nuevo ESP32-S3-WROOM-1: mismos pines que el ESP32-S3-EYE
    {"ESP32S3_ELEGOO", -1, -1, 15, 4, 5, 16, 17, 18, 12, 10, 8, 9, 11, 6, 7, 13},
#else
    // Módulo anterior ESP32-WROVER. M5STACK_WIDE es compatible con los pines 33/4 hacia el UNO.
    {"M5STACK_WIDE", -1, 15, 27, 22, 23, 19, 36, 18, 39, 5, 34, 35, 32, 25, 26, 21},
    {"WROVER_KIT",   -1, -1, 21, 26, 27, 35, 34, 39, 36, 19, 18, 5, 4, 25, 23, 22},
#endif
};

static uint8_t* s_jpeg = NULL;
static size_t s_len = 0;
static volatile uint32_t s_seq = 0;
static SemaphoreHandle_t s_mutex;
static float s_fps = 0;

static esp_err_t probar(const Cableado* c) {
    camera_config_t cfg = {
        .pin_pwdn = c->pwdn, .pin_reset = c->reset, .pin_xclk = c->xclk,
        .pin_sccb_sda = c->sda, .pin_sccb_scl = c->scl,
        .pin_d7 = c->d7, .pin_d6 = c->d6, .pin_d5 = c->d5, .pin_d4 = c->d4,
        .pin_d3 = c->d3, .pin_d2 = c->d2, .pin_d1 = c->d1, .pin_d0 = c->d0,
        .pin_vsync = c->vsync, .pin_href = c->href, .pin_pclk = c->pclk,
        .xclk_freq_hz = CAMARA_XCLK_HZ,
        .ledc_timer = LEDC_TIMER_0, .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_QVGA,
        .jpeg_quality = CAMARA_CALIDAD_JPEG,
        .fb_count = 2,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_LATEST,
    };
    return esp_camera_init(&cfg);
}

const char* camara_iniciar(void) {
    for (size_t i = 0; i < sizeof(CABLEADOS) / sizeof(CABLEADOS[0]); ++i) {
        ESP_LOGI(TAG, "Probando cableado %s...", CABLEADOS[i].nombre);
        if (probar(&CABLEADOS[i]) == ESP_OK) {
            sensor_t* s = esp_camera_sensor_get();
            if (s) {
                s->set_vflip(s, CAMARA_VOLTEAR_V);    // ver config.h
                s->set_hmirror(s, CAMARA_ESPEJO_H);
            }
            ESP_LOGI(TAG, "Cámara lista con cableado %s", CABLEADOS[i].nombre);
            return CABLEADOS[i].nombre;
        }
        esp_camera_deinit();
    }
    return NULL;
}

static void tarea_camara(void* arg) {
    int64_t t0 = esp_timer_get_time();
    int cuadros = 0;
    while (true) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
        if (fb->len <= JPEG_MAX) {
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            memcpy(s_jpeg, fb->buf, fb->len);
            s_len = fb->len;
            ++s_seq;
            xSemaphoreGive(s_mutex);
        }
        esp_camera_fb_return(fb);
        if (++cuadros == 20) {
            int64_t t = esp_timer_get_time();
            s_fps = 20.0f * 1e6f / (float)(t - t0);
            t0 = t; cuadros = 0;
        }
        vTaskDelay(1);
    }
}

void camara_arrancar_tarea(void) {
    s_mutex = xSemaphoreCreateMutex();
    s_jpeg = heap_caps_malloc(JPEG_MAX, MALLOC_CAP_SPIRAM);
    xTaskCreatePinnedToCore(tarea_camara, "camara", 4096, NULL, 6, NULL, 0);
}

size_t camara_ultimo_jpeg(uint8_t* dst, size_t cap, uint32_t* seq, int espera_ms) {
    int esperado = 0;
    while (s_seq == *seq) {
        if (esperado >= espera_ms) return 0;
        vTaskDelay(pdMS_TO_TICKS(5));
        esperado += 5;
    }
    size_t n = 0;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_len <= cap) { memcpy(dst, s_jpeg, s_len); n = s_len; }
    *seq = s_seq;
    xSemaphoreGive(s_mutex);
    return n;
}

float camara_fps(void) { return s_fps; }

// ---------------------------------------------------------------- preprocesado
// IMPORTANTE: esto debe ser IGUAL a preparar() en entrenamiento/comun.py.
// Si el modelo se entrena con imágenes preparadas de otra forma, en el carro "ve otra cosa".
#define DEC_ANCHO 160
#define DEC_ALTO  120
static uint8_t* s_rgb = NULL;     // 160x120x3 (PSRAM)

esp_err_t camara_preprocesar(const uint8_t* jpeg, size_t len, uint8_t* rgb) {
    if (!s_rgb) {
        s_rgb = heap_caps_malloc(DEC_ANCHO * DEC_ALTO * 3, MALLOC_CAP_SPIRAM);
        if (!s_rgb) return ESP_ERR_NO_MEM;
    }
    // 1) Decodificar a la mitad de resolución: 320x240 -> 160x120 (el decodificador lo hace casi gratis)
    esp_jpeg_image_cfg_t cfg = {
        .indata = (uint8_t*)jpeg, .indata_size = len,
        .outbuf = s_rgb, .outbuf_size = DEC_ANCHO * DEC_ALTO * 3,
        .out_format = JPEG_IMAGE_FORMAT_RGB888,
        .out_scale = JPEG_IMAGE_SCALE_1_2,
    };
    esp_jpeg_image_output_t info;
    esp_err_t r = esp_jpeg_decode(&cfg, &info);
    if (r != ESP_OK) return r;
    if (info.width != DEC_ANCHO || info.height != DEC_ALTO) return ESP_ERR_INVALID_SIZE;

    // 2) 160x120 RGB -> 128x96 RGB por promedio de área (igual que cv2.INTER_AREA)
    redimensionar_area_c(s_rgb, DEC_ANCHO, DEC_ALTO, rgb, MODELO_ANCHO, MODELO_ALTO, 3);
    return ESP_OK;
}
