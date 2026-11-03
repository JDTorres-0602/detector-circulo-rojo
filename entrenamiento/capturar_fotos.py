"""capturar_fotos.py — Toma fotos con la cámara DEL CARRO para armar el dataset.

Por qué con el carro y no con el celular: el modelo aprende de lo que ve. Si entrenas con
fotos del celular (otra cámara, otros colores, otra altura) y lo usas con la cámara del
carro, se equivoca más. Lo ideal es entrenar con el mismo sensor y desde la misma altura.

Uso (con el PC conectado a la red WiFi CarroBalon-XXXX):
    python capturar_fotos.py --sesion sesion01

Teclas en la ventana:
    ESPACIO  guardar la foto actual
    A        modo automático: guarda una foto cada --intervalo segundos (otra vez A para parar)
    Q / ESC  salir

Consejo: usa un nombre de sesión distinto cada vez que cambies de lugar o de luz
(sesion01 = sala de día, sesion02 = pasillo de noche...). El cuaderno separa
entrenamiento y validación POR SESIÓN para medir de forma honesta.
"""
import argparse
import glob
import os
import time
import urllib.request

import cv2
import numpy as np

AQUI = os.path.dirname(os.path.abspath(__file__))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--sesion', required=True, help='prefijo de los archivos, p. ej. sesion01 (sin "_")')
    ap.add_argument('--url', default='http://192.168.4.1/foto.jpg')
    ap.add_argument('--carpeta', default=os.path.join(AQUI, '..', 'datos', 'fotos'))
    ap.add_argument('--intervalo', type=float, default=0.7)
    a = ap.parse_args()
    if '_' in a.sesion:
        ap.error('el nombre de sesión no puede tener "_"')

    os.makedirs(a.carpeta, exist_ok=True)
    existentes = glob.glob(os.path.join(a.carpeta, f'{a.sesion}_*.jpg'))
    numeros = [os.path.basename(p)[len(a.sesion) + 1:-4] for p in existentes]
    k = 1 + max([int(n) for n in numeros if n.isdigit()] or [0])
    auto, t_ultima, guardadas = False, 0.0, 0
    print(f'Guardando en {os.path.abspath(a.carpeta)} desde {a.sesion}_{k:04d}.jpg')

    while True:
        try:
            datos = urllib.request.urlopen(a.url, timeout=3).read()
        except Exception as e:
            print('No responde el carro:', e, '(¿estás conectado a su WiFi?)')
            time.sleep(1)
            continue
        img = cv2.imdecode(np.frombuffer(datos, np.uint8), cv2.IMREAD_COLOR)
        if img is None:
            continue
        guardar = auto and time.time() - t_ultima >= a.intervalo
        vista = cv2.resize(img, None, fx=2, fy=2, interpolation=cv2.INTER_NEAREST)
        texto = f'{a.sesion}  guardadas: {guardadas}  ' + ('AUTO (A para parar)' if auto else 'ESPACIO = guardar, A = auto')
        cv2.putText(vista, texto, (8, 22), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 0), 3)
        cv2.putText(vista, texto, (8, 22), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)
        cv2.imshow('Captura (Q para salir)', vista)
        tecla = cv2.waitKey(1) & 0xFF
        if tecla in (ord('q'), 27):
            break
        if tecla == ord('a'):
            auto = not auto
        if tecla == ord(' ') or guardar:
            # Se guarda el JPEG tal cual llegó (sin recomprimir): son los mismos bytes que procesa la ESP32
            with open(os.path.join(a.carpeta, f'{a.sesion}_{k:04d}.jpg'), 'wb') as f:
                f.write(datos)
            k += 1
            guardadas += 1
            t_ultima = time.time()
    cv2.destroyAllWindows()
    print(f'Listo: {guardadas} fotos nuevas. Siguiente paso: python etiquetar.py')


if __name__ == '__main__':
    main()
