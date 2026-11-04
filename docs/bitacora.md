# Bitácora del proyecto

Registro de lo que se hizo, lo que se decidió y por qué. Lo más reciente va arriba.

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

---

## Pendiente para esta noche

- [ ] Etiquetar `cel01` y `cel02` y revisar con `python probar_pc.py --modo color --fuente carpeta`
- [ ] Subir `datos/` (fotos + `etiquetas.json`) a `Mi unidad/balon_rojo/`
- [ ] **Volver a subir el cuaderno** a Colab (el viejo no tiene `SESIONES_VAL`)
- [ ] Ronda 2 provisional: `SOLO_SINTETICOS = False`, `SESIONES_VAL = ['cel02']`, `EPOCAS = 30`, GPU T4
- [ ] Descargar `modelo_balon.cc`, `modelo_balon.h`, `pruebas_vectores.h` desde Drive (`balon_rojo/salida`, clic derecho → Descargar = zip) → copiar a `firmware_esp32/main/` y compilar. Este es el **modelo de respaldo** para mañana
- [ ] Instalar el **driver CH340** del UNO (si no aparece un puerto COM al conectarlo)
- [ ] **Cargar las baterías** del carro

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
