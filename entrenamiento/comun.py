"""comun.py — Lo que comparten el cuaderno, las herramientas del PC y (en C) el firmware.

Regla de oro del ML embebido: lo que el modelo ve al ENTRENAR debe ser idéntico a lo
que ve en el CARRO. Por eso estas funciones tienen su gemela en el firmware:

    preparar()        <->  camara_preprocesar()   (camara.c + redimensionar.c)
    mapa_rojo()       <->  mapa_rojo()            (vision_color.c)
    decodificar()     <->  buscar_blobs()         (postproceso.c)

Si cambias una, cambia la otra.

Formato de las etiquetas (datos/etiquetas.json):
    {"sesion01_0003.jpg": [[cx, cy, r], ...],   # un círculo por balón
     "sesion01_0004.jpg": []}                   # foto SIN balón (también sirve: enseña qué NO es)
    cx = x_centro / ancho_foto, cy = y_centro / alto_foto, r = radio_px / ancho_foto
"""
import json
import os

import cv2
import numpy as np

ALTO, ANCHO = 96, 128           # entrada del modelo (proporción 4:3, como la cámara)
BASE_ALTO, BASE_ANCHO = 120, 160  # paso intermedio: la ESP32 decodifica el JPEG a la mitad
CELDA = 8
GH, GW = ALTO // CELDA, ANCHO // CELDA   # mapa de salida: 12 x 16
ROJO_MIN = 70                   # igual que en vision_color.h


# ----------------------------------------------------------------------------- imagen
def recorte_43(h, w):
    """Recorte central con proporción 4:3 -> (y0, x0, alto, ancho)."""
    if h * 4 >= w * 3:
        cw, ch = w, w * 3 // 4
    else:
        ch, cw = h, h * 4 // 3
    return (h - ch) // 2, (w - cw) // 2, ch, cw


def preparar_base(bgr, circulos=()):
    """Foto de OpenCV (BGR, cualquier tamaño) -> 160x120 RGB + círculos ajustados al recorte.
    Las fotos del carro (320x240) no se recortan: solo se reducen a la mitad."""
    h, w = bgr.shape[:2]
    y0, x0, ch, cw = recorte_43(h, w)
    im = cv2.resize(bgr[y0:y0 + ch, x0:x0 + cw], (BASE_ANCHO, BASE_ALTO), interpolation=cv2.INTER_AREA)
    c = [((cx * w - x0) / cw, (cy * h - y0) / ch, r * w / cw) for cx, cy, r in circulos]
    return np.ascontiguousarray(im[..., ::-1]), np.array(c, np.float32).reshape(-1, 3)


def base_a_entrada(base_rgb):
    """160x120 -> 128x96 por promedio de área (redimensionar_area_c en la ESP32)."""
    return cv2.resize(base_rgb, (ANCHO, ALTO), interpolation=cv2.INTER_AREA)


def preparar(bgr):
    """Foto BGR -> exactamente lo que entra a la visión en el carro: 128x96 RGB uint8."""
    return base_a_entrada(preparar_base(bgr)[0])


def a_entrada_modelo(rgb_uint8):
    """uint8 [0,255] -> float [-1,1] (lo que espera MobileNetV2). En la ESP32 es una tabla (LUT)."""
    return rgb_uint8.astype(np.float32) / 127.5 - 1.0


# ----------------------------------------------------------------------------- visión clásica
def mapa_rojo(rgb, delta=60):
    """Detector SIN ML: fracción de píxeles rojos en cada celda 8x8 -> mapa (12, 16) 0..1."""
    x = rgb.astype(np.int16)
    r, mayor = x[..., 0], np.maximum(x[..., 1], x[..., 2])
    rojo = (r >= ROJO_MIN) & (r - mayor >= delta)
    return rojo.reshape(GH, CELDA, GW, CELDA).mean(axis=(1, 3)).astype(np.float32)


# ----------------------------------------------------------------------------- etiquetas
def etiquetas_mapa(circulos, radio_min_px=2.0):
    """Círculos (cx, cy, r) normalizados a la imagen 128x96 -> (12, 16, 2): [etiqueta, peso].

    - Celda POSITIVA (1): su centro cae dentro del balón. Así el área del blob crece cuando
      el balón está cerca, igual que en el modo color (el seguidor usa esa área).
    - Celda IGNORADA (peso 0): el borde del balón (ambiguo), balones diminutos, o balones
      cuyo centro quedó fuera de la imagen (solo se ve un pedacito).
    - Todo lo demás: NEGATIVO (0) con peso 1.
    """
    lab = np.zeros((GH, GW), np.float32)
    peso = np.ones((GH, GW), np.float32)
    py, px = np.meshgrid((np.arange(GH) + 0.5) * CELDA, (np.arange(GW) + 0.5) * CELDA, indexing='ij')
    borde = CELDA * 0.75                      # margen ambiguo alrededor del balón (px)
    for cx, cy, r in np.asarray(circulos, np.float32).reshape(-1, 3):
        x, y, rp = cx * ANCHO, cy * ALTO, r * ANCHO
        d = np.hypot(px - x, py - y)
        dentro_img = 0 <= cx < 1 and 0 <= cy < 1
        if rp < radio_min_px or not dentro_img:
            peso[d <= rp + borde] = 0.0
            continue
        nucleo = d <= rp
        nucleo[min(int(y // CELDA), GH - 1), min(int(x // CELDA), GW - 1)] = True   # al menos la celda del centro
        peso[(d <= rp + borde) & ~nucleo & (lab == 0)] = 0.0
        lab[nucleo] = 1.0
        peso[nucleo] = 1.0
    return np.stack([lab, peso], -1)


# ----------------------------------------------------------------------------- postproceso
def decodificar(mapa, umbral):
    """Manchas 4-conexas sobre el mapa (12, 16) -> lista de blobs, la más fuerte primero.
    Mismo algoritmo que buscar_blobs() en postproceso.c."""
    activo = mapa >= umbral
    visto = np.zeros_like(activo)
    blobs = []
    for i in range(GH):
        for j in range(GW):
            if not activo[i, j] or visto[i, j]:
                continue
            pila = [(i, j)]
            visto[i, j] = True
            sw = sx = sy = 0.0
            area, pico = 0, 0.0
            while pila:
                a, b = pila.pop()
                p = float(mapa[a, b])
                sw += p; sx += p * (b + .5); sy += p * (a + .5)
                area += 1; pico = max(pico, p)
                for da, db in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    na, nb = a + da, b + db
                    if 0 <= na < GH and 0 <= nb < GW and activo[na, nb] and not visto[na, nb]:
                        visto[na, nb] = True
                        pila.append((na, nb))
            blobs.append(dict(cx=sx / sw / GW, cy=sy / sw / GH, area=area, pico=pico))
    blobs.sort(key=lambda b: -b['area'] * b['pico'])
    return blobs


# ----------------------------------------------------------------------------- datos
def cargar_etiquetas(carpeta_datos):
    """-> lista de (ruta_foto, circulos) solo de las fotos que ya están etiquetadas."""
    ruta = os.path.join(carpeta_datos, 'etiquetas.json')
    if not os.path.exists(ruta):
        return []
    with open(ruta, encoding='utf-8') as f:
        et = json.load(f)
    fotos = os.path.join(carpeta_datos, 'fotos')
    return [(os.path.join(fotos, n), c) for n, c in sorted(et.items()) if os.path.exists(os.path.join(fotos, n))]


def sesion_de(ruta):
    """'sesion03_0012.jpg' -> 'sesion03'. Las fotos de una misma sesión se parecen mucho."""
    return os.path.basename(ruta).split('_')[0]


# ----------------------------------------------------------------------------- datos sintéticos
def balon_sintetico(r, con_distractores=True):
    """Imagen 128x96 RGB inventada + círculos. Sirve para probar TODO el proceso antes de
    tener fotos reales (y como complemento). Ojo: un modelo entrenado SOLO con esto no
    funcionará bien en el carro: el mundo real es mucho más variado."""
    H, W = ALTO * 2, ANCHO * 2               # se dibuja al doble y se reduce (bordes suaves)
    im = np.ones((H, W, 3), np.float32) * r.uniform(40, 220, 3)
    piso = int(H * r.uniform(0.35, 0.75))     # "pared" arriba y "piso" abajo
    im[piso:] = r.uniform(40, 220, 3)
    im += np.linspace(r.uniform(-30, 30), r.uniform(-30, 30), W)[None, :, None]
    for _ in range(r.integers(0, 6)):        # muebles / objetos de colores al azar
        x0, y0 = r.integers(0, W), r.integers(0, H)
        cv2.rectangle(im, (int(x0), int(y0)), (int(x0 + r.integers(10, 90)), int(y0 + r.integers(10, 90))),
                      r.uniform(0, 255, 3).tolist(), -1)
    if con_distractores:
        for _ in range(r.integers(0, 3)):    # cosas ROJAS que NO son balón (cajas, ropa)
            x0, y0 = int(r.integers(0, W)), int(r.integers(0, H))
            rojo = [r.uniform(150, 255), r.uniform(0, 60), r.uniform(0, 60)]
            if r.random() < 0.5:
                cv2.rectangle(im, (x0, y0), (x0 + int(r.integers(12, 70)), y0 + int(r.integers(12, 70))), rojo, -1)
            else:
                pts = np.array([[x0 + r.integers(-40, 40), y0 + r.integers(-40, 40)] for _ in range(5)], np.int32)
                cv2.fillPoly(im, [pts], rojo)
        for _ in range(r.integers(0, 2)):    # balones de OTRO color: tampoco son el objetivo
            otros = ([40, 90, 220], [40, 180, 60], [230, 210, 40], [235, 235, 235])   # azul, verde, amarillo, blanco
            c = (np.array(otros[r.integers(len(otros))]) + r.uniform(-25, 25, 3)).tolist()
            cv2.circle(im, (int(r.integers(0, W)), int(r.integers(0, H))), int(r.integers(6, 40)), c, -1)
    circulos = []
    for _ in range(r.choice([0, 1, 1, 1, 2])):
        rad = float(r.uniform(5, 50))
        x, y = float(r.uniform(0, W)), float(r.uniform(H * 0.2, H))
        base = np.array([r.uniform(150, 255), r.uniform(0, 50), r.uniform(0, 50)])
        y0, y1 = max(0, int(y - rad - 1)), min(H, int(y + rad + 2))      # solo el cuadrado del balón
        x0, x1 = max(0, int(x - rad - 1)), min(W, int(x + rad + 2))
        yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
        d = np.hypot(xx - x, yy - y) / rad
        luz = np.hypot(xx - (x - rad * .35), yy - (y - rad * .35)) / rad   # brillo arriba a la izquierda
        color = base[None, None] * (1.15 - 0.45 * np.clip(d, 0, 1))[..., None] + 90 * np.exp(-luz ** 2 * 6)[..., None]
        im[y0:y1, x0:x1] = np.where((d <= 1)[..., None], color, im[y0:y1, x0:x1])
        circulos.append([x / W, y / H, rad / W])
    im += r.normal(0, r.uniform(2, 8), im.shape)
    im = cv2.resize(np.clip(im, 0, 255).astype(np.uint8), (ANCHO, ALTO), interpolation=cv2.INTER_AREA)
    return im, np.array(circulos, np.float32).reshape(-1, 3)
