// =====================================================================
//  web.h — Servidor web del carro (http://192.168.4.1)
// =====================================================================
//    /              el HMI en React (hmi.html.gz, fuente en hmi_react/)
//    /debug         la página de depuración y ajuste fino (pagina.html)
//    /foto.jpg      último cuadro de la cámara, 320x240 (sirve para armar el dataset)
//    /entrada.bmp   la imagen 128x96 que de verdad entra a la visión
//    /estado        JSON: mapa 12x16, blobs, estado del seguidor, velocidades...
//    /ajustes?...   modo=color|modelo umbral= delta= area= vel= kp= kd=
//                   avance_ms= ms_grado= paso=   (búsqueda autónoma)
//    /modo?m=auto|manual  entrar / salir de la BÚSQUEDA AUTÓNOMA
//    /manual?izq=&der=    control manual de motores (expira si no se repite; ignorado en autónomo)
//    /servo?ang=0..180    cabeza (cámara + ultrasonido) en manual; ignorado en autónomo
//    /imu?yaw0=1          pone el yaw en 0
//
//  /estado trae para el HMI: control (manual|auto), auto (escaneando, alineando,
//  confirmando, siguiendo, avanzando, esquivando), fijado, ev_fijado (sube cada vez
//  que se fija el objeto: sirve para mostrar el aviso UNA vez), msg, servo, dist,
//  frente, obstaculo, perfil [[ángulo, cm], ...] del último barrido, imu {ax, ay,
//  az en m/s²; yaw, pitch, roll en grados} o null, rpm_izq, rpm_der, vel_cms (estimadas).
// =====================================================================
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void web_iniciar(void);

#ifdef __cplusplus
}
#endif
