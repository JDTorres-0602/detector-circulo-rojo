// wifi.c — ver wifi.h
#include "wifi.h"
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"

static const char* TAG = "wifi";
static char s_ssid[33];

const char* wifi_iniciar_ap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t ini = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&ini));

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(s_ssid, sizeof(s_ssid), "%s%02X%02X", WIFI_PREFIJO_SSID, mac[4], mac[5]);

    wifi_config_t cfg = {0};
    memcpy(cfg.ap.ssid, s_ssid, strlen(s_ssid));
    cfg.ap.ssid_len = strlen(s_ssid);
    strncpy((char*)cfg.ap.password, WIFI_CLAVE, sizeof(cfg.ap.password) - 1);
    cfg.ap.channel = WIFI_CANAL;
    cfg.ap.max_connection = WIFI_MAX_CLIENTES;
    cfg.ap.authmode = strlen(WIFI_CLAVE) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);   // sin ahorro de energía: video más fluido
    ESP_LOGI(TAG, "Red WiFi \"%s\" (clave \"%s\") -> abre http://192.168.4.1", s_ssid, WIFI_CLAVE);
    return s_ssid;
}
