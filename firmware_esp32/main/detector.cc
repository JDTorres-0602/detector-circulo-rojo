// detector.cc — ver detector.h  (basado en el firmware de detección de personas del equipo)
#include "detector.h"

#include <stdlib.h>
#include <string.h>
#include <new>

#include "config.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "modelo_balon.h"
#include "postproceso.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

// El cuaderno escribe pruebas_vectores.h: imágenes de validación y la salida int8 que dio el
// modelo en Colab. Si existe, al arrancar se comprueba que la ESP32 calcula lo mismo.
#if __has_include("pruebas_vectores.h")
#include "pruebas_vectores.h"
#define HAY_VECTORES 1
#endif

static const char* TAG = "detector";

static tflite::MicroInterpreter* s_interp = nullptr;
static TfLiteTensor* s_in = nullptr;
static TfLiteTensor* s_out = nullptr;
static uint8_t* s_arena = nullptr;
static int8_t s_lut[256];   // pixel 0..255 -> valor int8 cuantizado de la entrada

static int autoprueba(void);

esp_err_t detector_iniciar(InfoDetector* info) {
    info->prueba = -1;
    if (g_modelo_balon_len == 0) {
        ESP_LOGW(TAG, "Todavía no hay modelo (modelo_balon.cc es el marcador): solo modo COLOR");
        return ESP_ERR_NOT_FOUND;
    }
    // El modelo es pequeño: copiarlo a RAM interna acelera la lectura de pesos
    const uint8_t* datos = g_modelo_balon;
    uint8_t* ram = (uint8_t*)heap_caps_aligned_alloc(16, g_modelo_balon_len, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (ram) {
        memcpy(ram, g_modelo_balon, g_modelo_balon_len);
        datos = ram;
    }
    const tflite::Model* modelo = tflite::GetModel(datos);
    if (modelo->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(TAG, "Versión de esquema %lu != %d", (unsigned long)modelo->version(), TFLITE_SCHEMA_VERSION);
        return ESP_FAIL;
    }

    // Solo las operaciones que puede usar el modelo. El cuaderno revisa que el modelo
    // no use otras; si cambias la arquitectura y aparece una nueva, agrégala aquí.
    static tflite::MicroMutableOpResolver<12> ops;
    ops.AddConv2D();
    ops.AddDepthwiseConv2D();
    ops.AddAdd();
    ops.AddLogistic();
    ops.AddPad();
    ops.AddQuantize();
    ops.AddDequantize();
    ops.AddReshape();
    ops.AddRelu6();
    ops.AddMaxPool2D();
    ops.AddConcatenation();
    ops.AddResizeNearestNeighbor();

    // Arena (memoria de trabajo). 1) Se mide cuánto necesita usando PSRAM.
    // 2) Si esa cantidad cabe en la RAM interna (mucho más rápida), se rehace allí.
    alignas(tflite::MicroInterpreter) static uint8_t mem_interp[sizeof(tflite::MicroInterpreter)];
    s_arena = (uint8_t*)heap_caps_aligned_alloc(16, ARENA_BYTES, MALLOC_CAP_SPIRAM);
    if (!s_arena) return ESP_ERR_NO_MEM;
    s_interp = new (mem_interp) tflite::MicroInterpreter(modelo, ops, s_arena, ARENA_BYTES);
    if (s_interp->AllocateTensors() != kTfLiteOk) {
        ESP_LOGE(TAG, "AllocateTensors falló (¿ARENA_BYTES muy pequeño? ¿falta una operación?)");
        return ESP_FAIL;
    }
    size_t necesaria = s_interp->arena_used_bytes() + 2048;
    info->arena_interna = false;
    // Solo si después quedan al menos 48 KB internos para WiFi, servidor web, etc.
    uint8_t* interna = nullptr;
    if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) > necesaria + 48 * 1024)
        interna = (uint8_t*)heap_caps_aligned_alloc(16, necesaria, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (interna) {
        s_interp->~MicroInterpreter();
        heap_caps_free(s_arena);
        s_arena = interna;
        s_interp = new (mem_interp) tflite::MicroInterpreter(modelo, ops, s_arena, necesaria);
        if (s_interp->AllocateTensors() != kTfLiteOk) return ESP_FAIL;
        info->arena_interna = true;
    }
    s_in = s_interp->input(0);
    s_out = s_interp->output(0);
    info->arena_usada = s_interp->arena_used_bytes();

    if (s_in->type != kTfLiteInt8 || s_in->bytes != MODELO_BYTES || s_out->bytes != MAPA_CELDAS) {
        ESP_LOGE(TAG, "Forma de tensores inesperada: entrada %d bytes (esperaba %d), salida %d (esperaba %d)",
                 (int)s_in->bytes, MODELO_BYTES, (int)s_out->bytes, MAPA_CELDAS);
        return ESP_FAIL;
    }
    // El modelo se entrenó con x = pixel/127.5 - 1. Cuantizado: q = x/escala + cero.
    // Se calcula una vez para los 256 valores posibles (tabla = LUT) y así no se hacen
    // 36 864 divisiones por cada imagen.
    for (int p = 0; p < 256; ++p) {
        float q = (p / 127.5f - 1.0f) / s_in->params.scale + s_in->params.zero_point;
        int qi = (int)(q < 0 ? q - 0.5f : q + 0.5f);
        s_lut[p] = (int8_t)(qi < -128 ? -128 : (qi > 127 ? 127 : qi));
    }
    ESP_LOGI(TAG, "Modelo listo: %u bytes, arena %u bytes en %s", (unsigned)g_modelo_balon_len,
             (unsigned)info->arena_usada, info->arena_interna ? "RAM interna" : "PSRAM");
    ESP_LOGI(TAG, "Entrada: escala %f, cero %d | Salida: escala %f, cero %d",
             s_in->params.scale, (int)s_in->params.zero_point,
             s_out->params.scale, (int)s_out->params.zero_point);
    info->prueba = autoprueba();
    return ESP_OK;
}

int64_t detector_ejecutar(const uint8_t* rgb, float* prob) {
    int64_t t0 = esp_timer_get_time();
    int8_t* in = s_in->data.int8;
    for (int i = 0; i < MODELO_BYTES; ++i) in[i] = s_lut[rgb[i]];
    if (s_interp->Invoke() != kTfLiteOk) return -1;
    int64_t dt = esp_timer_get_time() - t0;
    mapa_desde_int8(s_out->data.int8, s_out->params.scale, s_out->params.zero_point, prob);
    return dt;
}

static int autoprueba(void) {
#ifdef HAY_VECTORES
    // Las versiones optimizadas de las operaciones (ESP-NN) pueden redondear distinto que
    // las de referencia de Colab: se acepta una diferencia de hasta 2 en int8.
    float prob[MAPA_CELDAS];
    int peor = 0;
    for (int v = 0; v < N_VECTORES; ++v) {
        int64_t us = detector_ejecutar(VECTOR_ENTRADA[v], prob);
        const int8_t* q = s_out->data.int8;
        int maxdif = 0;
        for (int i = 0; i < MAPA_CELDAS; ++i) {
            int d = abs((int)q[i] - (int)VECTOR_SALIDA[v][i]);
            if (d > maxdif) maxdif = d;
        }
        ESP_LOGI(TAG, "Vector %d: diferencia máxima %d (%lld ms)", v, maxdif, us / 1000);
        if (maxdif > peor) peor = maxdif;
    }
    bool ok = peor <= 2;
    ESP_LOGI(TAG, "AUTOPRUEBA DEL MODELO: %s (diferencia máxima %d)", ok ? "OK" : "FALLÓ", peor);
    return ok ? 1 : 0;
#else
    ESP_LOGI(TAG, "Sin pruebas_vectores.h: se omite la autoprueba del modelo");
    return -1;
#endif
}
