// usb_fotos.h — Fotos por el cable USB (sin WiFi), para armar el dataset
//
// Protocolo (USB serie/JTAG nativo del ESP32-S3):
//   PC -> carro:  'F'
//   carro -> PC:  "FOTO" + largo (4 bytes, little-endian) + JPEG 320x240
// Los mensajes de registro (ESP_LOG) también salen por este puerto: el PC
// busca la marca "FOTO" y comprueba que el JPEG empiece en FF D8 y termine en FF D9.
// Lo usa entrenamiento/capturar_fotos.py --usb COMx
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void usb_fotos_iniciar(void);

#ifdef __cplusplus
}
#endif
