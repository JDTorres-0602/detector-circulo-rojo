# Etapa 5 — Embeber el modelo en la ESP32

**Objetivo:** meter el modelo entrenado dentro del firmware, comprobar que la ESP32 calcula **exactamente** lo mismo que Colab y compararlo con el modo COLOR en el carro.

## 🧠 Qué significa "embeber" un modelo

Un modelo entrenado es una lista de números (los *pesos*) más la receta de qué operaciones hacer con ellos. Para que corra en un microcontrolador:

1. **Se cuantiza a int8** (sección 8 del cuaderno). Cada peso pasa de un decimal de 4 bytes (`0.0731...`) a un entero de 1 byte (`−128..127`) más una *escala* y un *cero* por capa: `valor_real ≈ (entero − cero) × escala`. El modelo queda 4 veces más pequeño y la ESP32 calcula mucho más rápido con enteros. A cambio pierde un poco de precisión: el cuaderno mide cuánto.
2. **Se convierte a un arreglo de C** (`modelo_balon.cc`): el archivo `.tflite` escrito byte por byte como `0x1c, 0x00, ...`. Así queda dentro del firmware, en la memoria flash, sin sistema de archivos.
3. **TensorFlow Lite Micro** (`detector.cc`) lo lee y ejecuta las operaciones una por una. Necesita una zona de RAM de trabajo, la **arena**, para los resultados intermedios de cada capa.

## Pasos

1. El cuaderno descargó `modelo_balon.cc`, `modelo_balon.h` y `pruebas_vectores.h`. Cópialos a `firmware_esp32/main/` (los dos primeros reemplazan a los marcadores vacíos).
2. Copia `modelo_balon_int8.tflite` a `entrenamiento/`.
3. Compila y carga:

```bash
cd firmware_esp32
idf.py build flash monitor
```

## ✅ Qué debe decir el monitor

```
detector: Modelo listo: 67240 bytes, arena ... bytes en RAM interna
detector: Entrada: escala 0.007843, cero -1 | Salida: escala 0.003906, cero -128
detector: Vector 0: diferencia máxima 0 (95 ms)
detector: Vector 1: diferencia máxima 1 (95 ms)
detector: Vector 2: diferencia máxima 0 (95 ms)
detector: AUTOPRUEBA DEL MODELO: OK (diferencia máxima 1)
main: Modo inicial: MODELO | ...
```

(Tus números serán otros). La **autoprueba** pasa por el modelo las mismas imágenes que Colab y compara la salida celda por celda. Si da OK, el modelo embebido es el mismo que mediste. Si **FALLA**, algo cambió en el camino: un archivo viejo, o `modelo_balon.cc` y `pruebas_vectores.h` de entrenamientos distintos.

| Si ves... | Significa |
|---|---|
| `AllocateTensors falló` | la arena no alcanza (sube `ARENA_BYTES` en `config.h`) o el modelo usa una operación no registrada |
| `Didn't find op for builtin opcode 'XXX'` | agrega `ops.AddXxx()` en `detector.cc` y súbele 1 al tamaño de `MicroMutableOpResolver<...>` |
| `Forma de tensores inesperada` | el modelo no es de 96×128×3 → 12×16 |

## Comparar en el carro

En la página ahora se habilita **Modo MODELO**. Cambia entre COLOR y MODELO en las mismas situaciones de los ejercicios de la etapa 2 (camiseta roja, poca luz, balón naranja) y anota:

| Situación | COLOR | MODELO |
|---|---|---|
| balón solo | | |
| balón + caja roja | | |
| poca luz | | |
| detecciones/s y ms | | |

También puedes probar el modelo en el PC con el video del carro:

```bash
cd entrenamiento
python probar_pc.py --modo modelo --fuente carro
```

## 🧠 La regla de oro, otra vez

El modelo se entrenó con imágenes preparadas por `comun.preparar()`: JPEG → 160×120 → 128×96 por promedio de área, en RGB, con píxeles escalados a [−1, 1]. En la ESP32, `camara.c` y `redimensionar.c` hacen exactamente lo mismo, y la tabla `s_lut` de `detector.cc` hace la escala más la cuantización de un solo paso. Si cambias cualquier lado (por ejemplo, la cámara a escala de grises), el modelo recibe algo que nunca vio y falla **sin dar ningún error**. Por eso existen la autoprueba y el botón "Ver entrada 128x96".

## ✍️ Ejercicios

1. Del monitor, anota: tamaño del modelo, arena usada, ms por inferencia. ¿Qué porcentaje de la RAM interna libre ocupa la arena?
2. En `config.h`, baja `ARENA_BYTES` a un valor menor que la arena usada y mira qué pasa al arrancar. Vuelve a dejarlo como estaba.
3. Entrena con `ALPHA = 0.5`, embébelo y compara los ms por inferencia. ¿Vale la pena el aumento de acierto?
4. En el cuaderno, compara las métricas "float" e "int8" de la sección 8. ¿Cuánto se perdió al cuantizar?

**Siguiente:** [Etapa 6 — Seguir y ajustar](06_seguir_y_ajustar.md)
