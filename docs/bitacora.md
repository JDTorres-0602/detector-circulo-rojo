# Bitácora del proyecto

Registro de lo que se hizo, lo que se decidió y por qué. Lo más reciente va arriba.

---

## 2026-11-04 — Laboratorio: carro real, modelo final y merge con el HMI

### Hardware (primer arranque real)
- ESP32-S3 **QFN56 con 8 MB de PSRAM integrada** (modo OCT correcto), cámara **OV3660** (no OV2640), puerto **COM12**.
- La primera carga necesitó **BOOT + RST** (el firmware de fábrica no deja reiniciar por USB); las siguientes son automáticas.
- Arreglos: `vTaskDelay(1)` en la tarea de visión (saltaba el watchdog de la tarea IDLE del núcleo 1).
- **Fotos por cable USB** (`usb_fotos.c` + `capturar_fotos.py --usb COM12`): el WiFi era demasiado lento desde el PC.

### Autoprueba del modelo
La diferencia máxima de 29 no era un error: las funciones optimizadas (`esp-nn`) redondean distinto que las de referencia de Colab, y en el PC con funciones optimizadas da lo mismo (23). Ahora la autoprueba exige **diferencia media ≤ 3 y ≤ 3 celdas que cambien de decisión**. Resultado en el carro: media 0.52, 0 cambios → **OK**.

### Dataset y modelo final
- `cel03` (36 fotos, cámara del carro, laboratorio) y `cel04` (26 fotos, cámara del carro, otro salón). Total: 133 fotos etiquetadas.
- Sin GPU de Colab (cuota agotada): **entrenado en el PC** (Ryzen 7, ~20 s por época) con `entrenamiento/.venv` + TensorFlow 2.21.
- Entrena `cel01`+`cel02`+`cel03`, valida `cel04`: **MODELO 88.9 % vs COLOR 66.7 %** de acierto (falsa alarma 5.9 % los dos); int8: 77.8 % / 0 %. Umbral 0.48. Copia en `firmware_esp32/modelos/final_2026-11-04/`.
- El modelo marca con seguridad el **centro** del círculo: la mancha mide pocas celdas aunque esté cerca. **Calibrar "Área objetivo"** con el tablero (`probar_pc.py --modo modelo --fuente usb`) a la distancia de frenado deseada.

### Merge con el trabajo del compañero (HMI + búsqueda autónoma + IMU)
Respaldo previo en `C:\proyectos\respaldo_antes_merge_2026-11-04`. Su versión partía de la nuestra de hoy (modelo, autoprueba y USB idénticos).
- **Tomado tal cual:** `autonomo.c/h` (escaneo con la cabeza → alinear → confirmar → seguir; si no ve nada avanza 3 s; esquiva; el ultrasonido manda), modos MANUAL / AUTÓNOMO (reemplazan "Seguir"), servo manual, filtro de seguridad en `main.c` (`DIST_FRENO_CM` = 15), IMU, RPM estimadas, `/modo`, `/servo`, `/imu`, CORS, HMI React en `/` y la página vieja en `/debug`.
- **Arduino:** su versión (IMU MPU6050, mediana de 3 lecturas, freno solo con la cabeza al frente, aviso `F`) **+ lo que se había perdido de la nuestra:** `INTERCAMBIAR_LADOS`, `W 0` / `T 0` para apagar watchdog y telemetría al probar a mano, y el mensaje `carro_uno listo`.
- **HMI:** `hmi_react/` en la raíz; `build.mjs` corregido (apuntaba a `main_ESP32_carrito/` y armaba la ruta con `pathname`, que falla en Windows). El `hmi.html.gz` embebido es idéntico a su `dist/index.html`.
- **Prueba:** `firmware_esp32/pruebas_pc/prueba_autonomo.c` (se compila con `python -m ziglang cc ...`): los 7 escenarios pasan.
- Compilan: ESP32 1.37 MB (57 % libre), UNO 38 % de flash / 27 % de RAM.

### Pendiente
- [ ] Cargar el firmware del merge en la ESP32 y `carro_uno.ino` en el UNO (interruptor en **Upload** al subir, luego **Cam**)
- [ ] Probar motores (guía 01), servo (`AUTO_SERVO_IZQ_MAYOR`: con 150° la cabeza debe mirar a la izquierda) y calibrar `AUTO_MS_POR_GRADO`
- [ ] Calibrar umbral y área objetivo; fijarlos en el firmware

---

## 2026-11-03 — De la nube al PC, círculo rojo impreso, primer entrenamiento

### Decisiones importantes

| Decisión | Por qué |
|---|---|
| **El objetivo ya no es un balón: es un círculo rojo impreso** | No hay balón físico para la demo; se usará una hoja impresa. Para el modelo da igual: la etiqueta sigue siendo un círculo `[cx, cy, r]`. El código y las guías todavía dicen "balón" |
| **Distractores del mismo rojo, cada uno en su hoja con margen blanco**: cuadrado, franja (y opcional triángulo) | Si solo cambia la forma, el modelo aprende "rojo + redondo". Si el cuadrado fuera de otro color, aprendería solo "rojo" (eso ya lo hace el modo COLOR) |
| **No usar los datasets de Roboflow** | `red-circles-detection` (244 imgs): círculos dibujados sobre fondo gris, nada que el generador sintético no haga. `red-circle-syjgu` (52 fotos reales): útil solo como complemento de entrenamiento (vista desde arriba, una sola sesión, sin distractores). Quedó pendiente; no se descargó |
| **Repo en `C:\proyectos\detector-balon-rojo`**, fuera de OneDrive | OneDrive bloquea archivos de `build/` y la ruta larga choca con el límite de 260 caracteres de Windows. La copia vieja de `OneDrive\Desktop\carritoESP` ya no se usa |
| **La sesión de validación la elige uno** (`SESIONES_VAL` en el cuaderno) | Debe ser la sesión más parecida a la demo, para que las métricas y el umbral midan ese escenario |

### Entorno del PC (verificado)

- **ESP-IDF v5.5.6** en `C:\esp\v5.5.6\esp-idf` (herramientas en `C:\Espressif`). Se usa desde VS Code (extensión ESP-IDF, target `esp32s3`) o con el acceso directo **IDF_v5.5.6_Powershell** del escritorio. En una PowerShell normal `idf.py` no existe.
- **Arduino IDE 2** con `arduino:avr 1.8.8`. Se instaló la librería **Servo 1.3.0**, que faltaba.
- **Python 3.14** (el normal) y **Python 3.12** en `entrenamiento/.venv` con `ai-edge-litert`, para correr el `.tflite` en el PC:
  `entrenamiento\.venv\Scripts\python.exe probar_pc.py --modo modelo ...`
- Google Drive: carpeta `Mi unidad/balon_rojo/` con `comun.py` (versión nueva) y `salida/`.

### Cambios en el código (commits)

| Commit | Qué |
|---|---|
| `230ba60` | `comun.balon_sintetico()` dibuja **círculos rojos planos sobre hoja blanca**, a veces aplastados (hoja de lado), y **distractores impresos** (cuadrado, franja). Nuevo `importar_fotos.py` (fotos del celular → 4:3, 320×240, `SESION_NNNN.jpg`). `.gitignore` ignora `.clangd` |
| `92aea4f` | `detector.cc`: `schema_generated.h` se movió en **esp-tflite-micro 1.4** → se incluye con `__has_include`. Primera compilación completa del firmware |
| `09b84dd` | Cuaderno: opción `SESIONES_VAL` |

### Ronda 1 en Colab (solo sintéticas, 10 épocas) — modelo de práctica

- Pérdida entrenamiento 0.0022 / validación 0.0043: bajan juntas, **sin sobreajuste**.
- Umbral 0.27 → acierto **98.7 %**, falsa alarma **5.6 %**, recall 97.3 %.
- **COLOR** en las mismas imágenes: acierto 65.9 %, **falsa alarma 81.7 %** (persigue cada cuadrado y franja roja).
- **int8 vs float: sin pérdida** (diferencia media por celda 0.0074). Modelo de **67 240 bytes**. Operaciones: ADD, CONV_2D, DEPTHWISE_CONV_2D, LOGISTIC (las 4 ya están en `detector.cc`).
- Con umbral 0.40 la falsa alarma baja a 1.4 % y el acierto solo a 96.1 %: para la demo puede convenir un umbral más alto.
- ⚠️ El 98.7 % es sobre imágenes del mismo generador: **no dice nada del mundo real**.

### Firmware: primera compilación (sin placa)

- `idf.py set-target esp32s3` + `idf.py build`: **OK** con el modelo de práctica dentro.
- Binario 1.06 MB de 3 MB (65 % libre). RAM interna: 149 KB usados, **188 KB libres**. IRAM 100 % (normal en la S3).
- El modelo de práctica está en `firmware_esp32/main/` (`modelo_balon.cc/.h`, `pruebas_vectores.h`) **sin confirmar a propósito**: no subir a git.
- `carro_uno.ino` compila: 4916 bytes (15 %) de flash, 289 bytes de RAM.

### Dataset (en `datos/fotos/`, sin confirmar en git)

| Sesión | Fotos | Sin círculo | Notas |
|---|---|---|---|
| `cel01` | 28 | 11 | Escritorio y pared, casi todas **desde arriba y de cerca**, verticales (se pierde ~44 % por el recorte 4:3). Buenos distractores: cuadrado, franja, **camiseta roja** |
| `cel02` | 44 | 15 | Sala con puerta, **cámara cerca del piso**, círculos **pequeños y lejanos**, círculo + distractores juntos; sillas anaranjadas y cojines rojos como negativos. Las 15 últimas (30–44) son horizontales |

El rojo impreso mide RGB ≈ (148, 62, 60): R − max(G,B) = 86, así que el modo COLOR (delta 60) sí lo detecta… y también detecta todos los distractores.

### Guía de etiquetado (`python etiquetar.py`)

| Fotos | Acción |
|---|---|
| `cel01` 1–8, 16, 21–28 | Marcar el círculo (la 21 está cortada arriba: centro donde estaría) |
| `cel01` 9–15, 17–20 | Tecla **0** |
| `cel02` 1–29 excepto 8, 9, 11, 13 | Marcar el círculo (son pequeños) |
| `cel02` 8, 9, 11, 13, 30–39, 44 | Tecla **0** |
| `cel02` 40–43 | Marcar el círculo (la 42 está cortada a la izquierda) |

Nunca marcar el cuadrado, la franja ni la camiseta. Círculo visto de lado: arrastrar hacia arriba o abajo (la medida que no se achica).

### Noche: etiquetado y ronda 2 provisional (fotos reales)

- **Etiquetas**: 71 fotos (45 con círculo, 26 sin círculo). Se sacó `cel01_0021` (medio círculo con el centro fuera de la foto: no se puede marcar en `etiquetar.py`).
- Se arregló el PATH de Windows: el instalador de Python 3.12 se había puesto primero y `python` dejó de tener OpenCV. Ahora `python` = 3.14 y la 3.12 solo con `py -3.12` o `.venv`.

| Corrida | Entrena con | Valida con | Resultado | Lección |
|---|---|---|---|---|
| A | `cel01` (de cerca, desde arriba) | `cel02` | acierto **31 %**, falsa alarma 6.7 %; COLOR 41 % / 86.7 %. Los 21 errores: **todos círculos lejanos** | El modelo solo sabe lo que hay en sus datos: nunca vio un círculo real pequeño |
| B | `cel02` (lejos, cámara baja) | `cel01` | 100 % / 0 %, pero el modelo guardado era el de la **época 6** (val 0.14): marcaba solo los **bordes** de los círculos grandes; con umbral 0.30, 50 % | Una métrica perfecta con pocas fotos se **satura** y engaña: mirar la pérdida de validación y las imágenes |
| C | `cel02` | `cel01` | Con el arreglo del desempate (commit `24d2141`): guardó la **época 17** (val 0.0067). **100 % / 0 % hasta umbral 0.80**, X en el centro de los círculos, mapa azul en los distractores | ✅ **Modelo de respaldo** |

**Modelo de respaldo (corrida C)**: `MODELO_UMBRAL 0.200`, 67 240 bytes. Copia en `firmware_esp32/modelos/respaldo_2026-11-03/` y ya está en `firmware_esp32/main/`; el firmware compila (1.2 MB, 60 % libre).
- En el carro **subir el umbral a ~0.5** con el deslizador "Umbral de celda" de la página (con 0.20 aparecen manchas falsas pequeñas en los bordes del cuadrado).
- No está medido **de lejos** (validó con `cel01`, de cerca), aunque aprendió con círculos lejanos.
- Si mañana el modelo final sale peor o no hay tiempo: copiar los 3 archivos de `modelos/respaldo_2026-11-03/` a `main/` y compilar.

---

## Pendiente para esta noche

- [x] Etiquetar `cel01` y `cel02` y revisar
- [x] Subir `datos/` (fotos + `etiquetas.json`) a `Mi unidad/balon_rojo/`
- [x] Volver a subir el cuaderno a Colab
- [x] Ronda 2 provisional → modelo de respaldo (corrida C)
- [x] Copiar el modelo de respaldo a `firmware_esp32/main/` y compilar
- [ ] Instalar el **driver CH340** del UNO (si no aparece un puerto COM al conectarlo)
- [ ] **Cargar las baterías** del carro

**Mañana, entrenamiento final**: subir `cel03` a Drive (fotos nuevas + `etiquetas.json` actualizado) y usar `SESIONES_VAL = ['cel03']`: entrena con `cel01` + `cel02` (cerca **y** lejos).

## Plan para mañana (8:30 – 14:00, en el laboratorio)

| Hora | Tarea | Resultado esperado |
|---|---|---|
| 8:30 – 9:00 | **UNO**: cargar `arduino_uno/carro_uno/carro_uno.ino` (placa *Arduino Uno*), probar sentido de los motores (guía 01) | Motores giran bien |
| 9:00 – 10:00 | **ESP32**: `idf.py -p COMx flash monitor` en `firmware_esp32`. Revisar en el monitor: PSRAM, cámara, WiFi `CarroBalon-XXXX`, `AUTOPRUEBA DEL MODELO: OK` | Página web con video |
| 10:00 – 10:30 | **ESP32 + UNO**: el carro sigue el círculo en **modo COLOR** | ✅ **Demo de respaldo lista** |
| 10:30 – 11:00 | **Sesión `cel03` con la cámara del carro**: `python capturar_fotos.py --sesion cel03` (PC conectado al WiFi del carro) | ~40 fotos, ≥12 sin círculo |
| 11:00 – 11:20 | Etiquetar `cel03` y revisar | |
| 11:20 – 11:50 | Colab final: `SESIONES_VAL = ['cel03']`, 30 épocas | Modelo le gana al COLOR |
| 11:50 – 12:15 | Copiar el modelo, `idf.py build flash monitor`, autoprueba OK | Botón **Modo MODELO** en la página |
| 12:15 – 13:30 | Pruebas COLOR vs MODELO, ajustar umbral y velocidad (guía 06) | |
| 13:30 – 14:00 | Margen para imprevistos | |

### Cómo debe ser `cel03` (la sesión de validación)

- En el **lugar de la demo** (o lo más parecido), con otra luz que `cel01`/`cel02`.
- **Cámara del carro** (o celular en el piso a 10–15 cm, en horizontal, mirando recto).
- Círculo **de pie en el piso o a la altura de la rodilla** (a la altura del pecho queda fuera de cuadro).
- ~40 fotos: ~12 a distintas distancias (0.3 → 2.5 m), ~6 en distintas posiciones/cortado, ~3 de lado, ~6 con distractores, **~8 solo distractores y ~4 solo fondo** (mínimo 10–12 sin círculo: con menos, el umbral no se puede calcular).
- Truco: secuencia acercándose 30 cm por foto, como si el carro lo siguiera.

### Problemas probables y solución rápida

| Síntoma | Causa | Solución |
|---|---|---|
| No aparece puerto COM | Cable solo de carga o falta driver CH340 | Otro cable de datos / instalar driver |
| `PSRAM ID read error` o `PSRAM chip is not connected` | El módulo tiene PSRAM QUAD, no OCT | `idf.py menuconfig` → Component config → ESP PSRAM → Mode: QUAD; recompilar |
| Imagen de cabeza o en espejo en la página | Orientación de la cámara | En `config.h`, cambiar `CAMARA_VOLTEAR_V` (de cabeza) o `CAMARA_ESPEJO_H` (espejo) y recompilar |
| El carro retrocede o gira al revés | Sentido de los motores | Invertir en `carro_uno.ino` o cambiar cables |
| `AllocateTensors falló` | Arena chica u operación faltante | Subir `ARENA_BYTES` en `config.h` |
| `AUTOPRUEBA … FALLÓ` | `modelo_balon.cc` y `pruebas_vectores.h` de entrenamientos distintos | Copiar los 3 archivos del mismo entrenamiento |
| Sin internet en el laboratorio | — | Datos del celular, o usar el modelo de respaldo de esta noche |

### Llevar

Portátil y cargador · **2–3 cables USB de datos** · baterías cargadas · cartones (círculo, cuadrado, franja) · camiseta roja · sesión de Google iniciada en el portátil.

### Comandos de referencia

```bash
# Fotos del celular -> dataset (una vez por carpeta/sesión)
cd C:\proyectos\detector-balon-rojo\entrenamiento
python importar_fotos.py --origen C:\ruta\a\las\fotos --sesion cel03

# Fotos con la cámara del carro (conectado a su WiFi)
python capturar_fotos.py --sesion cel03

# Etiquetar y revisar
python etiquetar.py
python probar_pc.py --modo color --fuente carpeta

# Probar el .tflite en el PC (Python 3.12)
.venv\Scripts\python.exe probar_pc.py --modo modelo --fuente carpeta

# Firmware (en la terminal ESP-IDF)
cd C:\proyectos\detector-balon-rojo\firmware_esp32
idf.py build
idf.py -p COMx flash monitor
```
