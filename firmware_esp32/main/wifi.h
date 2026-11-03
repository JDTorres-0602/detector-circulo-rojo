// wifi.h — El carro crea su propia red WiFi (punto de acceso) en 192.168.4.1
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Devuelve el nombre de la red creada (CarroBalon-XXXX)
const char* wifi_iniciar_ap(void);

#ifdef __cplusplus
}
#endif
