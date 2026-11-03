"""etiquetar.py — Marca dónde está el balón en cada foto (crea datos/etiquetas.json).

Etiquetar = darle al modelo la "respuesta correcta" de cada foto. Es el trabajo más
aburrido y el que más decide si el modelo sale bueno: una etiqueta mal puesta le
enseña algo falso.

Uso:
    python etiquetar.py                  # empieza en la primera foto sin etiquetar
    python etiquetar.py --desde 1        # empieza desde la primera foto

Ratón:  clic en el CENTRO del balón y arrastra hasta su BORDE -> círculo
Teclas:
    N  o  ->     guardar y pasar a la siguiente
    B  o  <-     volver a la anterior
    0            "en esta foto NO hay balón" (también es una etiqueta útil) y siguiente
    Z            borrar el último círculo
    C            borrar todos los círculos de esta foto
    D            quitar la etiqueta de esta foto (vuelve a "sin etiquetar")
    Q  o  ESC    guardar y salir

Reglas para etiquetar bien:
  - Marca TODOS los balones rojos de la foto, también los lejanos y los que están a medias.
  - Si el balón sale cortado por el borde, pon el centro donde estaría (aunque quede fuera).
  - Si no estás seguro de si algo es el balón, mejor no uses esa foto (D y siguiente).
"""
import argparse
import glob
import json
import os

import cv2

AQUI = os.path.dirname(os.path.abspath(__file__))
IZQ, DER = 2424832, 2555904   # flechas en Windows (cv2.waitKeyEx)


class Editor:
    def __init__(self, carpeta):
        self.fotos = sorted(glob.glob(os.path.join(carpeta, 'fotos', '*.jpg')))
        self.ruta_json = os.path.join(carpeta, 'etiquetas.json')
        self.et = {}
        if os.path.exists(self.ruta_json):
            with open(self.ruta_json, encoding='utf-8') as f:
                self.et = json.load(f)
        self.circ = []          # círculos de la foto actual, en píxeles de la foto
        self.arrastre = None    # (x, y) del centro mientras se arrastra
        self.cursor = (0, 0)
        self.escala = 1.0

    def guardar(self):
        with open(self.ruta_json, 'w', encoding='utf-8') as f:
            json.dump(dict(sorted(self.et.items())), f, indent=1)

    def cargar(self, i):
        self.img = cv2.imread(self.fotos[i])
        h, w = self.img.shape[:2]
        self.escala = min(3.0, 960 / w)
        nombre = os.path.basename(self.fotos[i])
        self.circ = [(cx * w, cy * h, r * w) for cx, cy, r in self.et.get(nombre, [])]

    def fijar(self, i):
        """Escribe los círculos de la foto i en el json (normalizados)."""
        h, w = self.img.shape[:2]
        self.et[os.path.basename(self.fotos[i])] = [[round(x / w, 4), round(y / h, 4), round(r / w, 4)]
                                                    for x, y, r in self.circ]
        self.guardar()

    def raton(self, ev, x, y, flags, _):
        x, y = x / self.escala, y / self.escala
        self.cursor = (x, y)
        if ev == cv2.EVENT_LBUTTONDOWN:
            self.arrastre = (x, y)
        elif ev == cv2.EVENT_LBUTTONUP and self.arrastre:
            cx, cy = self.arrastre
            r = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5
            if r >= 2:
                self.circ.append((cx, cy, r))
            self.arrastre = None

    def dibujar(self, i):
        v = cv2.resize(self.img, None, fx=self.escala, fy=self.escala, interpolation=cv2.INTER_NEAREST)
        s = self.escala
        for x, y, r in self.circ:
            cv2.circle(v, (int(x * s), int(y * s)), max(1, int(r * s)), (0, 255, 0), 2)
            cv2.drawMarker(v, (int(x * s), int(y * s)), (0, 255, 0), cv2.MARKER_CROSS, 8)
        if self.arrastre:
            cx, cy = self.arrastre
            r = ((self.cursor[0] - cx) ** 2 + (self.cursor[1] - cy) ** 2) ** 0.5
            cv2.circle(v, (int(cx * s), int(cy * s)), max(1, int(r * s)), (0, 255, 255), 1)
        nombre = os.path.basename(self.fotos[i])
        hechas = sum(os.path.basename(p) in self.et for p in self.fotos)
        if nombre in self.et:
            estado = f'{len(self.et[nombre])} balon(es)' if self.et[nombre] else 'SIN balon'
        else:
            estado = 'sin etiquetar'
        txt = f'{i + 1}/{len(self.fotos)}  {nombre}  [{estado}]  etiquetadas: {hechas}'
        cv2.rectangle(v, (0, 0), (v.shape[1], 26), (0, 0, 0), -1)
        cv2.putText(v, txt, (6, 18), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (255, 255, 255), 1)
        cv2.imshow('Etiquetar', v)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--datos', default=os.path.join(AQUI, '..', 'datos'))
    ap.add_argument('--desde', type=int, default=0, help='número de foto (1 = la primera). Por defecto: la primera sin etiquetar')
    a = ap.parse_args()
    ed = Editor(a.datos)
    if not ed.fotos:
        print('No hay fotos en', os.path.abspath(os.path.join(a.datos, 'fotos')), '-> primero: python capturar_fotos.py')
        return
    if a.desde:
        i = min(a.desde, len(ed.fotos)) - 1
    else:
        i = next((k for k, p in enumerate(ed.fotos) if os.path.basename(p) not in ed.et), len(ed.fotos) - 1)
    cv2.namedWindow('Etiquetar')
    cv2.setMouseCallback('Etiquetar', ed.raton)
    ed.cargar(i)
    while True:
        ed.dibujar(i)
        t = cv2.waitKeyEx(20)
        if t == -1:
            continue
        c = chr(t & 0xFF).lower() if t < 256 else ''
        nombre = os.path.basename(ed.fotos[i])
        if c in ('q', '\x1b'):
            if ed.circ or nombre in ed.et:
                ed.fijar(i)
            break
        elif c == 'n' or t == DER:
            if ed.circ or nombre in ed.et:
                ed.fijar(i)
            elif nombre not in ed.et:
                print(f'{nombre}: sin círculos -> queda SIN etiquetar (si no hay balón, usa 0)')
            if i + 1 < len(ed.fotos):
                i += 1
                ed.cargar(i)
        elif c == 'b' or t == IZQ:
            if ed.circ or nombre in ed.et:
                ed.fijar(i)
            if i > 0:
                i -= 1
                ed.cargar(i)
        elif c == '0':
            ed.circ = []
            ed.fijar(i)
            if i + 1 < len(ed.fotos):
                i += 1
                ed.cargar(i)
        elif c == 'z' and ed.circ:
            ed.circ.pop()
        elif c == 'c':
            ed.circ = []
        elif c == 'd':
            ed.circ = []
            ed.et.pop(nombre, None)
            ed.guardar()
    cv2.destroyAllWindows()
    con = sum(1 for v in ed.et.values() if v)
    print(f'Guardado {ed.ruta_json}: {len(ed.et)} fotos etiquetadas ({con} con balón, {len(ed.et) - con} sin balón)')


if __name__ == '__main__':
    main()
