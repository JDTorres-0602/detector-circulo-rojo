# HMI del carro (React)

La ESP32 sirve este HMI ya compilado (`firmware_esp32/main/hmi.html.gz`), así que para **usarlo** no hay que instalar nada:

1. Enciende el carro.
2. Conéctate al WiFi `CarroBalon-XXXX` (clave `balon2026`).
3. Abre el navegador en **http://192.168.4.1**.

La página vieja de ajustes sigue en **http://192.168.4.1/debug**.

## Modificar el HMI

Requisito: Node.js LTS.

```
cd hmi_react
npm install
npm run dev     # http://localhost:5173 — recompila al guardar
npm run build   # genera dist/index.html y ../firmware_esp32/main/hmi.html.gz
```

- Con `npm run dev` y el PC conectado al WiFi del carro, el HMI le habla a 192.168.4.1.
- **Sin carro:** abre `http://localhost:5173/?demo`. Muestra datos y video simulados.
- Después de `npm run build` hay que volver a compilar y flashear la ESP32 (`idf.py build flash`), porque el HMI va dentro del firmware.
- `dist/index.html` también se puede abrir con doble clic en el PC cuando está conectado al WiFi del carro.

## Estructura

| Archivo | Qué hace |
|---|---|
| `src/api.js` | Pide `/estado` unas 10 veces por segundo, guarda la historia de la IMU y define los comandos (`/modo`, `/manual`, `/servo`, `/imu`) |
| `src/demo.js` | Carro simulado para `?demo` |
| `src/componentes/Video.jsx` | Video FPV (pide `/foto.jpg` en bucle) con HUD: objetivo fijado, candidatos, horizonte, rumbo, cabeza y distancia |
| `src/componentes/Telemetria.jsx` | Datos bajo el video y radar del último barrido |
| `src/componentes/Cruceta.jsx` | Control manual: se mantiene presionado; en PC con flechas o WASD |
| `src/componentes/ServoSlider.jsx` | Mueve la cabeza (servo) de izquierda a derecha |
| `src/componentes/GraficaLinea.jsx` | Gráficas de la IMU (últimos 20 s, con cursor) |
| `src/componentes/Gauge.jsx` | Relojes de velocidad y RPM |

La velocidad y las RPM son **estimadas a partir del PWM**, porque el kit no trae encoders. Se calibran en `config.h` con `RPM_MAX`, `PWM_ARRANQUE` y `RUEDA_DIAMETRO_CM`.
