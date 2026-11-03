"""importar_fotos.py — Trae fotos del CELULAR (o de cualquier cámara) al dataset.

Lo ideal es capturar con la cámara del carro (capturar_fotos.py). Mientras no tengas el
carro, esto deja las fotos del celular con el mismo formato que las del carro:

    1. recorte central a proporción 4:3 (la del sensor del carro),
    2. reducción a 320x240 por promedio de área (el tamaño que manda la ESP32),
    3. nombre SESION_NNNN.jpg, que es lo que esperan etiquetar.py y el cuaderno.

La orientación (foto vertical u horizontal) se corrige sola: OpenCV lee la etiqueta EXIF
que guarda el celular. Toma las fotos en HORIZONTAL y a la altura del piso: así las verá
el carro. Las fotos HEIC de iPhone no se leen: en Ajustes > Cámara > Formatos elige
"Más compatible" (JPG) o conviértelas antes.

Uso:
    python importar_fotos.py --origen "C:/Users/yo/Pictures/sala_de_dia" --sesion cel01

Consejo: una carpeta de origen por lugar o luz, y un nombre de sesión por carpeta
(cel01 = sala de día, cel02 = cuarto de noche...). El cuaderno separa entrenamiento y
validación POR SESIÓN.
"""
import argparse
import glob
import os

import cv2

import comun

AQUI = os.path.dirname(os.path.abspath(__file__))
EXTENSIONES = ('.jpg', '.jpeg', '.png', '.bmp', '.webp')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--origen', required=True, help='carpeta con las fotos del celular')
    ap.add_argument('--sesion', required=True, help='prefijo de los archivos, p. ej. cel01 (sin "_")')
    ap.add_argument('--carpeta', default=os.path.join(AQUI, '..', 'datos', 'fotos'))
    ap.add_argument('--calidad', type=int, default=85, help='calidad JPEG (la ESP32 manda alrededor de 85)')
    a = ap.parse_args()
    if '_' in a.sesion:
        ap.error('el nombre de sesión no puede tener "_"')

    fuentes = sorted(p for p in glob.glob(os.path.join(a.origen, '*')) if p.lower().endswith(EXTENSIONES))
    if not fuentes:
        ap.error(f'no hay fotos {EXTENSIONES} en {a.origen}')

    os.makedirs(a.carpeta, exist_ok=True)
    existentes = glob.glob(os.path.join(a.carpeta, f'{a.sesion}_*.jpg'))
    numeros = [os.path.basename(p)[len(a.sesion) + 1:-4] for p in existentes]
    k = 1 + max([int(n) for n in numeros if n.isdigit()] or [0])
    print(f'Guardando en {os.path.abspath(a.carpeta)} desde {a.sesion}_{k:04d}.jpg')

    guardadas, verticales = 0, 0
    for ruta in fuentes:
        img = cv2.imread(ruta, cv2.IMREAD_COLOR)   # aplica la rotación EXIF del celular
        if img is None:
            print('  no se pudo leer:', os.path.basename(ruta))
            continue
        h, w = img.shape[:2]
        verticales += h > w
        y0, x0, ch, cw = comun.recorte_43(h, w)
        img = cv2.resize(img[y0:y0 + ch, x0:x0 + cw], (320, 240), interpolation=cv2.INTER_AREA)
        cv2.imwrite(os.path.join(a.carpeta, f'{a.sesion}_{k:04d}.jpg'), img, [cv2.IMWRITE_JPEG_QUALITY, a.calidad])
        k += 1
        guardadas += 1

    if verticales:
        print(f'Ojo: {verticales} fotos eran verticales; se recortó una franja horizontal del centro.')
    print(f'Listo: {guardadas} fotos nuevas. Siguiente paso: python etiquetar.py')


if __name__ == '__main__':
    main()
