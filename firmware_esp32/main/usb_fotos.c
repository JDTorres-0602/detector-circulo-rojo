// usb_fotos.c — ver usb_fotos.h
#include "usb_fotos.h"

#include <stdbool.h>
#include "camara.h"
#include "driver/usb_serial_jtag.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "usb_fotos";

static bool escribir(const void* datos, size_t n) {
    const uint8_t* p = datos;
    while (n > 0) {
        int w = usb_serial_jtag_write_bytes(p, n, pdMS_TO_TICKS(1000));
        if (w <= 0) return false;    // nadie está leyendo del otro lado
        p += w;
        n -= w;
    }
    return true;
}

static void tarea_usb_fotos(void* arg) {
    uint8_t* jpeg = heap_caps_malloc(JPEG_MAX, MALLOC_CAP_SPIRAM);
    uint32_t seq = 0;
    while (true) {
        uint8_t c;
        if (usb_serial_jtag_read_bytes(&c, 1, portMAX_DELAY) != 1 || c != 'F') continue;
        // Un cuadro más nuevo que el último enviado; si la cámara no tiene uno en 1 s, el que haya
        size_t n = camara_ultimo_jpeg(jpeg, JPEG_MAX, &seq, 1000);
        if (n == 0) {
            seq = 0;
            n = camara_ultimo_jpeg(jpeg, JPEG_MAX, &seq, 0);
        }
        uint8_t cab[8] = {'F', 'O', 'T', 'O', n & 0xFF, (n >> 8) & 0xFF, (n >> 16) & 0xFF, (n >> 24) & 0xFF};
        if (escribir(cab, sizeof(cab))) escribir(jpeg, n);
    }
}

void usb_fotos_iniciar(void) {
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    cfg.tx_buffer_size = 16 * 1024;
    if (usb_serial_jtag_driver_install(&cfg) != ESP_OK) {
        ESP_LOGW(TAG, "No se pudo iniciar el USB: sin fotos por cable");
        return;
    }
    xTaskCreate(tarea_usb_fotos, "usb_fotos", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "Fotos por USB listas (capturar_fotos.py --usb COMx)");
}
