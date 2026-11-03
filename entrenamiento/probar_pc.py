"""probar_pc.py — Prueba la detección en el PC, igual que la haría la ESP32.

Modos:
    --modo color    detector clásico (umbral de color). Solo necesita numpy y opencv.
    --modo modelo   el modelo int8 (.tflite) que exporta el cuaderno. Necesita además
                    ai-edge-litert o tensorflow (ver requirements.txt).
Fuentes:
    --fuente carpeta   fotos de datos/fotos (N / B para avanzar). Si están etiquetadas,
                       el círculo verde es la etiqueta: compárala con lo que detecta.
    --fuente carro     en vivo desde http://192.168.4.1/foto.jpg (conectado al WiFi del carro)
    --fuente webcam    la cámara del PC (otra cámara: sirve para jugar, no para medir)

Ejemplos:
    python probar_pc.py --modo color --fuente carpeta
    python probar_pc.py --modo modelo --tflite modelo_balon_int8.tflite --fuente carro
"""
import argparse
import json
import os
import time
import urllib.request

import cv2
import numpy as np

import comun

AQUI = os.path.dirname(os.path.abspath(__file__))
ZOOM = 4


class Modelo:
    def __init__(self, ruta):
        try:
            from ai_edge_litert.interpreter import Interpreter
        except ImportError:
            try:
                from tflite_runtime.interpreter import Interpreter
            except ImportError:
                import tensorflow as tf
                Interpreter = tf.lite.Interpreter
        self.it = Interpreter(model_path=ruta)
        self.it.allocate_tensors()
        self.ent, self.sal = self.it.get_input_details()[0], self.it.get_output_details()[0]
        s, z = self.ent['quantization']
        # La misma tabla (LUT) que arma detector.cc
        self.lut = np.clip(np.round((np.arange(256) / 127.5 - 1.0) / s + z), -128, 127).astype(np.int8)
        self.s_out, self.z_out = self.sal['quantization']

    def mapa(self, rgb):
        self.it.set_tensor(self.ent['index'], self.lut[rgb][None])
        self.it.invoke()
        q = self.it.get_tensor(self.sal['index'])[0, ..., 0].astype(np.float32)
        return np.clip((q - self.z_out) * self.s_out, 0, 1)


def mostrar(rgb, mapa, umbral, etiqueta, titulo):
    v = cv2.resize(rgb[..., ::-1], None, fx=ZOOM, fy=ZOOM, interpolation=cv2.INTER_NEAREST)
    c = comun.CELDA * ZOOM
    capa = v.copy()
    for i in range(comun.GH):
        for j in range(comun.GW):
            p = float(mapa[i, j])
            if p > 0.02:
                cv2.rectangle(capa, (j * c, i * c), ((j + 1) * c, (i + 1) * c), (0, 220, 255), -1)
    alfa = np.repeat(cv2.resize(mapa, (v.shape[1], v.shape[0]), interpolation=cv2.INTER_NEAREST)[..., None], 3, 2) * 0.55
    v = (v * (1 - alfa) + capa * alfa).astype(np.uint8)
    for i in range(comun.GH):
        for j in range(comun.GW):
            if mapa[i, j] >= umbral:
                cv2.rectangle(v, (j * c, i * c), ((j + 1) * c - 1, (i + 1) * c - 1), (60, 60, 255), 1)
    for cx, cy, r in etiqueta:   # etiqueta (si la hay): verde
        cv2.circle(v, (int(cx * v.shape[1]), int(cy * v.shape[0])), int(r * v.shape[1]), (0, 255, 0), 2)
    blobs = comun.decodificar(mapa, umbral)
    for k, b in enumerate(blobs):
        cv2.drawMarker(v, (int(b['cx'] * v.shape[1]), int(b['cy'] * v.shape[0])),
                       (255, 255, 0) if k == 0 else (0, 255, 255), cv2.MARKER_TILTED_CROSS, 18, 2)
    texto = titulo + f' | blobs: {len(blobs)}' + (f" | principal x={blobs[0]['cx']:.2f} area={blobs[0]['area']}" if blobs else '')
    cv2.rectangle(v, (0, 0), (v.shape[1], 24), (0, 0, 0), -1)
    cv2.putText(v, texto, (6, 17), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
    cv2.imshow('Prueba (Q = salir)', v)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--modo', choices=['color', 'modelo'], default='color')
    ap.add_argument('--fuente', choices=['carpeta', 'carro', 'webcam'], default='carpeta')
    ap.add_argument('--tflite', default=os.path.join(AQUI, 'modelo_balon_int8.tflite'))
    ap.add_argument('--umbral', type=float, default=None, help='por defecto 0.30 (color) o 0.5 (modelo)')
    ap.add_argument('--delta', type=int, default=60, help='modo color: cuánto debe superar R a G y B')
    ap.add_argument('--datos', default=os.path.join(AQUI, '..', 'datos'))
    ap.add_argument('--url', default='http://192.168.4.1/foto.jpg')
    a = ap.parse_args()
    umbral = a.umbral if a.umbral is not None else (0.30 if a.modo == 'color' else 0.5)
    modelo = Modelo(a.tflite) if a.modo == 'modelo' else None

    etiquetas = {}
    ruta_et = os.path.join(a.datos, 'etiquetas.json')
    if os.path.exists(ruta_et):
        with open(ruta_et, encoding='utf-8') as f:
            etiquetas = json.load(f)
    fotos = sorted(os.path.join(a.datos, 'fotos', n) for n in os.listdir(os.path.join(a.datos, 'fotos'))
                   if n.lower().endswith('.jpg')) if a.fuente == 'carpeta' else []
    if a.fuente == 'carpeta' and not fotos:
        print('No hay fotos en datos/fotos. Prueba con --fuente carro o --fuente webcam')
        return
    cam = cv2.VideoCapture(0) if a.fuente == 'webcam' else None
    i = 0
    while True:
        etiqueta = []
        if a.fuente == 'carpeta':
            bgr = cv2.imread(fotos[i])
            nombre = os.path.basename(fotos[i])
            _, etiqueta = comun.preparar_base(bgr, etiquetas.get(nombre, []))
            titulo = f'{i + 1}/{len(fotos)} {nombre}'
        elif a.fuente == 'carro':
            try:
                datos = urllib.request.urlopen(a.url, timeout=3).read()
            except Exception as e:
                print('No responde el carro:', e)
                time.sleep(1)
                continue
            bgr = cv2.imdecode(np.frombuffer(datos, np.uint8), cv2.IMREAD_COLOR)
            titulo = 'carro'
        else:
            ok, bgr = cam.read()
            if not ok:
                print('No se pudo leer la webcam')
                return
            titulo = 'webcam'
        rgb = comun.preparar(bgr)
        t0 = time.perf_counter()
        mapa = modelo.mapa(rgb) if modelo else comun.mapa_rojo(rgb, a.delta)
        ms = (time.perf_counter() - t0) * 1000
        mostrar(rgb, mapa, umbral, etiqueta, f'{titulo} | {a.modo} {ms:.1f} ms (en el PC) | umbral {umbral:.2f}')
        t = cv2.waitKeyEx(0 if a.fuente == 'carpeta' else 1)
        c = chr(t & 0xFF).lower() if 0 <= t < 256 else ''
        if c in ('q', '\x1b'):
            break
        if a.fuente == 'carpeta':
            if c == 'b' or t == 2424832:
                i = max(0, i - 1)
            else:
                i = min(len(fotos) - 1, i + 1)
    cv2.destroyAllWindows()


if __name__ == '__main__':
    main()
