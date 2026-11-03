# Etapa 4 — Entrenar en Colab

**Objetivo:** entrenar el modelo, **entender las métricas** y decidir si sirve. Lo harás dos veces: primero con datos inventados, para conocer el proceso, y después con tus fotos.

## Ronda 1: sin fotos (para aprender el proceso)

1. En Google Drive crea la carpeta `balon_rojo` y sube ahí `entrenamiento/comun.py`.
2. Abre `entrenamiento/detector_balon.ipynb` en Colab (*Archivo → Subir cuaderno*).
3. *Entorno de ejecución → Cambiar tipo de entorno → **GPU T4***.
4. En la celda de configuración pon `SOLO_SINTETICOS = True`, `EPOCAS = 10`.
5. *Entorno de ejecución → Ejecutar todas*. Acepta el permiso de Drive.

Recorre el cuaderno **leyendo cada sección mientras corre**: cada una explica qué hace. Mira sobre todo:

- las imágenes sintéticas y sus etiquetas (sección 2);
- un lote con aumentos (sección 3): la misma foto nunca entra dos veces igual;
- las líneas de cada época (sección 6).

## Ronda 2: con tus fotos

1. Sube a `balon_rojo/` la carpeta `datos/` completa (`fotos/` + `etiquetas.json`).
2. `SOLO_SINTETICOS = False`, `EPOCAS = 30`. Ejecuta todo de nuevo.

## 🧠 Cómo leer una línea de entrenamiento

```
época 12/30 pérdida 0.0412 (val 0.0530) | umbral 0.46 | acierto 91.2% | falsa alarma 5.0% | recall 88.7% | manchas falsas por foto 0.08
```

| Valor | Qué significa | Quieres que... |
|---|---|---|
| pérdida | qué tan mal predice en las fotos de entrenamiento | baje |
| val | lo mismo en fotos que **nunca** usó para aprender | baje junto con la otra |
| umbral | probabilidad mínima para encender una celda, calculada para que en fotos **sin** balón haya alarma ≤ 5 % | — |
| acierto | en fotos con balón, la mancha principal cae sobre el balón (lo que seguirá el carro) | suba |
| falsa alarma | en fotos sin balón, ve un balón | quede ≈ 5 % (así se eligió el umbral) |
| recall | de todos los balones etiquetados, cuántos ve | suba |

## 🧠 Sobreajuste: la lección más importante

Al final, el cuaderno dibuja las curvas de pérdida:

```
pérdida                                    pérdida
 │\                                         │\
 │ \__ validación                           │ \      ___/ validación  ← ¡SUBE!
 │    \_____                                │  \____/
 │  \_____________ entrenamiento            │   \____________ entrenamiento
 └──────────── épocas                       └──────────── épocas
     BIEN: aprende cosas generales              MAL: memoriza tus fotos (overfitting)
```

Si te pasa lo de la derecha: **más fotos y más variadas** (lo mejor), más aumentos o menos épocas. El cuaderno guarda la **mejor época**, no la última, así que un poco de sobreajuste al final no daña el resultado.

## 🧠 ¿Qué hace el modelo, en una frase?

Las primeras capas de MobileNetV2 (que ya saben "ver" porque se entrenaron con 1.2 millones de fotos) convierten la imagen de 96×128 en 12×16 celdas, cada una con 96 números que describen bordes, colores y texturas de esa zona. Nuestra cabeza nueva aprende a leer esos 96 números y decir "aquí hay balón rojo" o "aquí no". Esto se llama **transfer learning**: reutilizar lo que otro modelo ya aprendió para entrenar con pocos datos.

## ✅ Cómo saber que el modelo sirve

En la sección 7 el cuaderno imprime:

```
MODELO: umbral 0.46 | acierto 93.0% | falsa alarma 5.0% | ...
COLOR:  umbral 0.30 | acierto 71.0% | falsa alarma 46.0% | ...
```

El modelo debe **ganarle claramente al color**, sobre todo en falsa alarma. Si no le gana, el problema casi siempre está en los datos: pocas fotos sin balón con cosas rojas, etiquetas malas o una sola sesión. Mira la figura **ERRORES**: te dice qué fotos te faltan.

## ✍️ Ejercicios (cambia **una** cosa a la vez y anota el resultado)

1. Entrena el modelo de la ronda 1 (solo sintético) y, en la sección 9, pruébalo con una foto real. ¿Qué pasa? ¿Por qué?
2. `FRAC_SINTETICOS = 0` contra `0.25` contra `0.5`. ¿Las sintéticas ayudan con tus fotos?
3. `FALSA_ALARMA_MAX = 0.01` contra `0.2`. ¿Qué umbral sale? Para el carro, ¿qué es peor: perseguir una caja roja o perder el balón un momento?
4. `ALPHA = 0.5`. ¿Mejora? (En la etapa 5 verás cuánto más lento corre en la ESP32).

**Siguiente:** [Etapa 5 — Embeber el modelo](05_embeber.md)
