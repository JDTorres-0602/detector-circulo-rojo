# Carro que sigue un balón rojo (ESP32 + Arduino UNO + TinyML)

Un carro ELEGOO Smart Robot Car V4.0 que **ve un balón rojo con su cámara y lo sigue**. La detección corre **dentro de la ESP32**, sin computador ni internet.

El proyecto tiene **dos detectores** que entregan el mismo resultado, para que puedas compararlos:

| Detector | Cómo funciona | Qué necesitas |
|---|---|---|
| **COLOR** (visión clásica) | Una regla escrita a mano: "un píxel es rojo si R supera a G y a B por X" | Nada: funciona desde el primer día |
| **MODELO** (Machine Learning) | Una red neuronal (MobileNetV2 + FOMO) entrenada con **tus** fotos | Fotos etiquetadas y entrenar en Colab |

Está basado en el proyecto de detección de personas del equipo y conserva su estructura.

---

## Cómo funciona todo junto

```
        EN EL PC / COLAB (una vez por modelo)                    EN EL CARRO (≈5-10 veces por segundo)
 ┌───────────────────────────────────────────┐        ┌──────────────────────────────────────────────────┐
 │ capturar_fotos.py → datos/fotos/*.jpg     │        │ Cámara OV2640 → JPEG 320x240                      │
 │ etiquetar.py      → datos/etiquetas.json  │        │   camara.c: JPEG → 160x120 → 128x96 RGB           │
 │ detector_balon.ipynb (Colab, GPU):        │        │   ┌──────────────┐     ┌──────────────────┐       │
 │   etiquetas → mapa 12x16                  │        │   │ vision_color │ ó   │ detector.cc      │       │
 │   entrenar MobileNetV2 + FOMO             │        │   │ (sin ML)     │     │ (TFLite Micro)   │       │
 │   medir, elegir umbral                    │        │   └──────┬───────┘     └────────┬─────────┘       │
 │   cuantizar a int8                        │        │          └── mapa 12x16 (0..1) ─┘                 │
 │   → modelo_balon.cc/.h ───────────────────┼──────► │   postproceso.c: celdas encendidas → blobs        │
 └───────────────────────────────────────────┘        │   seguidor.c: control PD → velocidad izq/der      │
                                                      │   uno.c: "M izq der" por serie ─────────┐         │
                                                      └─────────────────────────────────────────┼─────────┘
                                                                                                ▼
                                                      Arduino UNO (carro_uno.ino): motores, servo, ultrasonido
```

---

## Estructura

```
detector-balon-rojo/
├── docs/                      ← LAS GUÍAS: empieza aquí
├── arduino_uno/carro_uno/     ← programa del UNO (motores, ultrasonido, servo)
├── firmware_esp32/            ← proyecto ESP-IDF de la cámara
│   ├── main/
│   │   ├── main.c             arranque y tareas (visión y control)
│   │   ├── config.h           pines, WiFi, valores iniciales
│   │   ├── camara.c/.h        captura y preprocesado (JPEG → 128x96)
│   │   ├── redimensionar.c/.h reducción por área (igual a OpenCV INTER_AREA)
│   │   ├── vision_color.c/.h  detector SIN ML (fracción de píxeles rojos por celda)
│   │   ├── detector.cc/.h     detector CON ML (TensorFlow Lite Micro, int8)
│   │   ├── modelo_balon.cc/.h el modelo (marcador vacío hasta que entrenes uno)
│   │   ├── postproceso.c/.h   mapa 12x16 → blobs
│   │   ├── seguidor.c/.h      máquina de estados + control PD
│   │   ├── uno.c/.h           protocolo serie con el UNO
│   │   ├── wifi.c, web.c, pagina.html   red propia y página de control
│   │   └── idf_component.yml  librerías (cámara, JPEG, TFLite Micro)
│   └── pruebas_pc/            pruebas de la lógica en el computador
├── entrenamiento/
│   ├── detector_balon.ipynb   cuaderno de Colab: entrenar y exportar
│   ├── comun.py               funciones compartidas (gemelas de las del firmware)
│   ├── capturar_fotos.py      tomar fotos con la cámara del carro
│   ├── etiquetar.py           marcar el balón en cada foto
│   └── probar_pc.py           probar color o modelo en el PC
└── datos/                     tus fotos y etiquetas
```

---

## Ruta de aprendizaje

Cada guía tiene: **qué vas a hacer**, **cómo saber que funcionó** ✅, **por qué funciona así** 🧠 y **ejercicios** ✍️. Hazlas en orden: cada etapa usa lo de la anterior.

| Etapa | Guía | Resultado |
|---|---|---|
| 0 | [Preparar el entorno](docs/00_preparar_entorno.md) | ESP-IDF, Arduino IDE, Python y Colab listos |
| 1 | [Arduino UNO y motores](docs/01_arduino_uno.md) | Mueves el carro escribiendo órdenes en el Monitor Serie |
| 2 | [Firmware de la ESP32 en modo COLOR](docs/02_firmware_modo_color.md) | El carro sigue el balón **sin ML**, y ves sus límites |
| 3 | [Datos: fotos y etiquetas](docs/03_datos.md) | Un dataset propio de unos cientos de fotos |
| 4 | [Entrenar en Colab](docs/04_entrenar.md) | Un modelo medido y mejor que el detector por color |
| 5 | [Embeber el modelo](docs/05_embeber.md) | El modelo corriendo en la ESP32, con autoprueba OK |
| 6 | [Seguir y ajustar](docs/06_seguir_y_ajustar.md) | Control afinado y el ciclo de mejora de datos |

¿Una palabra rara? Está en el [glosario](docs/glosario.md).

---

## Prueba rápida de la lógica (sin carro)

```bash
cd firmware_esp32/pruebas_pc
pip install ziglang
python -m ziglang cc -I../main prueba_logica.c ../main/vision_color.c ../main/postproceso.c ../main/seguidor.c ../main/redimensionar.c -o prueba.exe
./prueba.exe
```

Debe terminar con `TODAS LAS PRUEBAS PASARON`.
