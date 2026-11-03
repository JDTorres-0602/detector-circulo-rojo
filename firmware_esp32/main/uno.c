// uno.c — ver uno.h
#include "uno.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "uno";
static volatile int s_dist = 0;
static volatile int64_t s_t_dato = 0;

static void tarea_rx(void* arg) {
    char linea[32];
    int n = 0;
    uint8_t c;
    while (true) {
        if (uart_read_bytes(UNO_UART, &c, 1, pdMS_TO_TICKS(100)) != 1) continue;
        if (c == '\r') continue;
        if (c != '\n') {
            if (n < (int)sizeof(linea) - 1) linea[n++] = (char)c;
            continue;
        }
        linea[n] = 0;
        n = 0;
        if (linea[0] == 'U' && linea[1] == ' ') {
            s_dist = atoi(linea + 2);
            s_t_dato = esp_timer_get_time();
        } else if (linea[0]) {
            ESP_LOGI(TAG, "UNO dice: %s", linea);   // mensajes de texto del UNO (p. ej. al arrancar)
        }
    }
}

void uno_iniciar(void) {
    uart_config_t cfg = {
        .baud_rate = UNO_BAUDIOS,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(UNO_UART, 512, 512, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UNO_UART, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UNO_UART, UNO_PIN_TX, UNO_PIN_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    xTaskCreate(tarea_rx, "uno_rx", 3072, NULL, 5, NULL);
    vTaskDelay(pdMS_TO_TICKS(200));
    uno_enviar(UNO_CMD_INICIO);
    ESP_LOGI(TAG, "Enlace con el UNO listo (TX %d, RX %d, %d baudios)", UNO_PIN_TX, UNO_PIN_RX, UNO_BAUDIOS);
}

void uno_enviar(const char* texto) {
    uart_write_bytes(UNO_UART, texto, strlen(texto));
}

void uno_motores(int izq, int der) {
    char b[24];
    snprintf(b, sizeof(b), "M %d %d\n", izq, der);
    uno_enviar(b);
}

int uno_distancia_cm(void) {
    return uno_ms_desde_ultimo_dato() < 500 ? s_dist : 0;
}

uint32_t uno_ms_desde_ultimo_dato(void) {
    if (s_t_dato == 0) return UINT32_MAX;
    return (uint32_t)((esp_timer_get_time() - s_t_dato) / 1000);
}
