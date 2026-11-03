// =====================================================================
//  web.h — Servidor web del carro (http://192.168.4.1)
// =====================================================================
//    /              la página (pagina.html)
//    /foto.jpg      último cuadro de la cámara, 320x240 (sirve para armar el dataset)
//    /entrada.bmp   la imagen 128x96 que de verdad entra a la visión
//    /estado        JSON: mapa 12x16, blobs, estado del seguidor, velocidades...
//    /ajustes?...   modo=color|modelo seguir=0|1 umbral= delta= area= vel= kp= kd=
//    /manual?izq=&der=   control manual (expira si no se repite)
// =====================================================================
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void web_iniciar(void);

#ifdef __cplusplus
}
#endif
