# Etapa 3 — Datos: fotos y etiquetas

**Objetivo:** armar tu propio dataset, es decir, fotos tomadas **con la cámara del carro** y marcadas con dónde está el balón.

> En ML se dice "basura entra, basura sale". El modelo solo puede aprender lo que le muestran las fotos. Esta etapa es la que más decide el resultado final.

## ¿Cuántas y cuáles fotos?

Para empezar: **300 a 600 fotos** en **al menos 3 sesiones** (lugares o luces distintas).

| Tipo | Porcentaje | Por qué |
|---|---|---|
| Balón cerca, lejos, al centro y en los bordes | ~50 % | lo que debe encontrar |
| Balón a medias (cortado por el borde, tapado por una pata de silla) | ~10 % | pasa todo el tiempo al seguirlo |
| **Sin balón**, pero con **cosas rojas** (ropa, cajas, juguetes, cables) | ~20 % | enseña qué **no** es el balón |
| Sin balón, escenas normales (piso, paredes, muebles) | ~20 % | enseña el "fondo" |

Varía: luz de día, de noche, lámpara amarilla, sombras, distintos pisos, el balón quieto y rodando. **Toma las fotos desde el carro, en el piso**, porque así las verá el modelo.

## 1. Capturar

1. Carga el firmware (etapa 2) y conecta el PC al WiFi **CarroBalon-XXXX**.
2. En una terminal:

```bash
cd entrenamiento
python capturar_fotos.py --sesion sesion01
```

Se abre el video del carro. **ESPACIO** guarda una foto, **A** activa el modo automático (una foto cada 0.7 s, útil mientras mueves el carro con el celular desde la página) y **Q** sale. Las fotos quedan en `datos/fotos/sesion01_0001.jpg`, `..._0002.jpg`...

Cambia el nombre de sesión cada vez que cambies de lugar o de luz: `sesion02`, `sesion03`...

## 2. Etiquetar

```bash
python etiquetar.py
```

- **Clic en el centro del balón y arrastra hasta el borde**: queda un círculo verde.
- **N** (o →) guarda y pasa a la siguiente; **B** (o ←) vuelve.
- **0** marca "en esta foto NO hay balón". ¡Hazlo! Las fotos sin balón también son datos.
- **Z** borra el último círculo y **Q** guarda y sale. Puedes parar y seguir otro día: retoma donde quedaste.

Reglas: marca **todos** los balones de la foto; si el balón sale cortado, pon el centro donde estaría aunque quede fuera; si dudas, mejor saca esa foto del dataset (tecla **D**).

## 3. Revisar antes de entrenar

```bash
python probar_pc.py --modo color --fuente carpeta
```

Recorre tus fotos (N / B): verás tu etiqueta (círculo verde) y lo que detecta el modo COLOR. Sirve para encontrar etiquetas mal puestas y para ver **en qué fotos falla el color**. Esas fotos son las más valiosas para el modelo.

## 🧠 Por qué separar por sesión

El cuaderno usa unas sesiones para **entrenar** y otras para **validar** (medir). Dos fotos seguidas de la misma sesión son casi idénticas: si una queda para entrenar y otra para validar, el modelo "ya vio la respuesta" y la medida sale engañosamente buena. Separando por sesión, la validación mide lo que importa: **¿funciona en un lugar donde nunca entrenó?**

## 🧠 Cómo se convierte un círculo en lo que aprende el modelo

El modelo no predice círculos: predice un **mapa de 12×16** (una probabilidad por cada celda de 8×8 píxeles). `etiquetas_mapa()` en `comun.py` hace la traducción:

```
 foto 128x96 con un círculo         mapa 12x16 de la etiqueta
 ┌──────────────────────┐           . . . . . . . . . . . . . . . .
 │                      │           . . . . . . . . . . . . . . . .
 │            ●●●       │           . . . . . . . . . . ? ? ? . . .
 │           ●●●●●      │    ──►    . . . . . . . . . ? 1 1 1 ? . .
 │            ●●●       │           . . . . . . . . . ? 1 1 1 ? . .
 └──────────────────────┘           . . . . . . . . . . ? ? ? . . .
                                    1 = balón · ? = borde (se ignora) · . = no balón
```

## ✍️ Ejercicios

1. Antes de etiquetar todo, etiqueta 30 fotos y córrelas en `probar_pc.py --modo color`. ¿En cuántas acierta el color? Ese número es lo que el modelo debe superar.
2. Abre `datos/etiquetas.json` con un editor de texto. Explica qué significa cada número de una línea.
3. Piensa: si todas tus fotos fueran de día, ¿qué crees que le pasaría al modelo de noche? (Lo vas a comprobar en la etapa 6).

**Siguiente:** [Etapa 4 — Entrenar en Colab](04_entrenar.md)
