# Etapa 0 — Preparar el entorno

**Objetivo:** tener instaladas las cuatro herramientas del proyecto y entender para qué sirve cada una.

| Herramienta | Para qué | Dónde corre |
|---|---|---|
| **Arduino IDE 2** | Programar el Arduino UNO (motores) | Tu PC → UNO por USB |
| **ESP-IDF** (con VS Code) | Compilar y cargar el firmware de la ESP32 (cámara + visión) | Tu PC → ESP32 por USB |
| **Python 3** + numpy + OpenCV | Tomar fotos, etiquetarlas y probar la detección | Tu PC |
| **Google Colab** | Entrenar el modelo con una GPU gratis | La nube de Google |

---

## 1. Arduino IDE

1. Descárgalo de arduino.cc (versión 2.x) e instálalo.
2. Conecta el UNO del carro por USB. En *Herramientas → Placa* elige **Arduino Uno**, y en *Puerto* el COM que aparezca.

✅ **Funcionó si:** abres *Archivo → Ejemplos → 01.Basics → Blink*, lo subes y el LED "L" del UNO parpadea.

> Si no aparece ningún puerto COM, falta el driver USB (el UNO de ELEGOO usa CH340): búscalo como "CH340 driver Windows".

## 2. ESP-IDF (el entorno oficial de Espressif)

La ESP32 **no** se programa aquí con el IDE de Arduino. Se usa **ESP-IDF**, el entorno profesional de Espressif: da control total de la memoria (PSRAM, RAM interna), de las tareas de FreeRTOS y de librerías como TensorFlow Lite Micro.

1. Instala **VS Code** y, dentro de VS Code, la extensión **ESP-IDF** (de Espressif).
2. Abre la paleta (`Ctrl+Shift+P`) → **ESP-IDF: Configure ESP-IDF Extension** → *Express* → elige la versión **v5.4** (o cualquier v5.x estable) → *Install*. Tarda un buen rato y descarga ~2 GB.
3. Para usar la terminal: paleta → **ESP-IDF: Open ESP-IDF Terminal**. Ahí funciona el comando `idf.py`.

✅ **Funcionó si:** en esa terminal `idf.py --version` muestra `ESP-IDF v5.x`.

> Alternativa sin VS Code: el *ESP-IDF Tools Installer* para Windows crea un acceso directo "ESP-IDF PowerShell" con todo listo.

## 3. Python (para las herramientas del PC)

En una terminal normal, dentro de la carpeta del repositorio:

```bash
cd entrenamiento
pip install -r requirements.txt
```

✅ **Funcionó si:** `python -c "import cv2, numpy; print('ok')"` imprime `ok`.

> Para correr el modelo `.tflite` en el PC (`probar_pc.py --modo modelo`) hace falta además `ai-edge-litert`, que necesita Python 3.10–3.12. Si tu Python es más nuevo, no pasa nada: el cuaderno de Colab también prueba el modelo.

## 4. Google Colab

Solo necesitas tu cuenta de Google: entra a colab.research.google.com y abre (*Archivo → Subir cuaderno*) `entrenamiento/detector_balon.ipynb`. Colab presta gratis una GPU NVIDIA T4, que entrena unas 20 a 50 veces más rápido que tu CPU.

---

## 🧠 Por qué hay tanto software

En TinyML (ML en microcontroladores) el trabajo se reparte según dónde sobra la potencia:

- **Entrenar** necesita millones de operaciones repetidas miles de veces → GPU en la nube.
- **Ejecutar** el modelo ya entrenado (la *inferencia*) es mucho más barato → cabe en la ESP32.
- **Los motores** necesitan tiempos precisos y muchos pines → el UNO, que no hace nada más.

## ✍️ Ejercicio

Abre `firmware_esp32/main/config.h` y responde: ¿qué tamaño tiene la imagen que entra a la visión? ¿Cuántos bytes ocupa? (Pista: `MODELO_BYTES`). ¿Por qué crees que no se usa la imagen completa de 320×240?

**Siguiente:** [Etapa 1 — Arduino UNO y motores](01_arduino_uno.md)
