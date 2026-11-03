# Etapa 2 — Firmware de la ESP32 en modo COLOR

**Objetivo:** compilar y cargar el firmware, abrir la página del carro y hacer que siga el balón **sin Machine Learning**. Vas a ver qué tan lejos llega una regla escrita a mano y dónde falla. Eso es justo lo que el modelo tendrá que resolver.

## 1. Compilar y cargar

Conecta el módulo de cámara por USB a tu PC. En la **terminal de ESP-IDF**:

```bash
cd firmware_esp32
idf.py set-target esp32s3
idf.py build
idf.py -p COM10 flash monitor
```

- `set-target` se hace **una sola vez** (usa `esp32` si tu módulo es el WROVER viejo). Crea el archivo `sdkconfig` a partir de `sdkconfig.defaults`.
- La primera compilación descarga las librerías de `main/idf_component.yml` (cámara, JPEG, TFLite Micro) y tarda varios minutos. Las siguientes son rápidas.
- Cambia `COM10` por el puerto de tu placa (míralo en el Administrador de dispositivos). Para salir del monitor: `Ctrl+]`.
- Si no logra conectar ("Failed to connect"): mantén presionado **BOOT** en el módulo mientras empieza a cargar.

## 2. ✅ Qué debe decir el monitor

```
camara: Cámara lista con cableado ESP32S3_ELEGOO
wifi: Red WiFi "CarroBalon-3F2A" (clave "balon2026") -> abre http://192.168.4.1
web: Servidor web listo
detector: Todavía no hay modelo (modelo_balon.cc es el marcador): solo modo COLOR
main: Modo inicial: COLOR | RAM interna libre: ... KB | PSRAM libre: ... KB
uno: UNO dice: carro_uno listo        <- solo si el UNO está conectado y encendido
```

| Si ves... | Revisa |
|---|---|
| `No respondió la cámara con ningún cableado` | el cable plano de la cámara, o que el target (s3 / esp32) sea el de tu módulo |
| `PSRAM ID read error` | en `sdkconfig.defaults.esp32s3` cambia OCT por QUAD (lo explica el archivo) |
| `UNO SIN RESPUESTA` en la página | interruptor del escudo, cable de 4 pines, o el UNO sin el programa de la etapa 1 |

## 3. La página del carro

1. Conecta tu celular o PC a la red **CarroBalon-XXXX** (clave `balon2026`).
2. Abre **http://192.168.4.1**.

Vas a ver el video con una cuadrícula de **12×16 celdas** encima. Cada celda pinta en amarillo **qué fracción de sus píxeles es roja**, y las que pasan el umbral se marcan con borde rojo. Los círculos son *blobs* (grupos de celdas encendidas); el verde es el que sigue el carro.

Prueba esto, en orden:

1. **Orientación:** pon el balón a la derecha del carro. Debe verse a la derecha en la página. Si no, cambia `CAMARA_ESPEJO_H` o `CAMARA_VOLTEAR_V` en `config.h`.
2. **"Ver entrada 128x96":** así ve el mundo la visión. Ese es el tamaño con el que trabajará el modelo.
3. **Delta rojo:** súbelo y bájalo mirando el mapa. Busca un valor donde el balón se encienda completo y el resto de la escena no.
4. **Control manual:** mantén presionadas las flechas.
5. **Seguir: SÍ** con el carro en el piso. Mueve el balón despacio.

## 🧠 Qué está pasando por dentro

```
camara.c      JPEG 320x240 ──decodificar a la mitad──► 160x120 ──promedio de área──► 128x96 RGB
vision_color.c  para cada celda 8x8: ¿cuántos píxeles cumplen R ≥ 70 y R − max(G,B) ≥ delta?
postproceso.c   celdas ≥ umbral que se tocan → un blob (centro, área, pico)
seguidor.c      ¿es el mismo blob de antes? → error = qué tan a un lado está → giro PD + avance
main.c          10 veces por segundo: "M izq der" al UNO
```

Hay **tres tareas de FreeRTOS** trabajando a la vez: la cámara captura sin parar, la visión procesa el último cuadro y el control manda órdenes al UNO a ritmo fijo. Comparten datos con un **mutex** (`g_mutex` en `app.h`) para que nadie lea un dato a medio escribir.

## ✍️ Ejercicios: encuentra los límites del modo COLOR

Anota los resultados: son tu **línea base**.

1. Pon una camiseta, una caja o un juguete **rojo** al lado del balón. ¿Qué sigue el carro?
2. Prueba con poca luz y a contraluz (frente a una ventana). ¿Qué tienes que hacer con el delta?
3. Prueba un balón **naranja** o **rosado**. ¿Lo confunde?
4. Con el carro quieto mirando el balón, anota "detección X/s, Y ms" de la página. Más adelante lo compararás con el modelo.
5. Lee `vision_color.c` (son 25 líneas) y explica con tus palabras por qué un objeto rojo cuadrado "es balón" para este detector.

> La conclusión esperada: el color solo no basta. Hace falta reconocer **forma + color + contexto**, y eso es lo que aprende una red neuronal con ejemplos.

**Siguiente:** [Etapa 3 — Datos: fotos y etiquetas](03_datos.md)
