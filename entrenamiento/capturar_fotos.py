"""capturar_fotos.py — Toma fotos con la cámara DEL CARRO para armar el dataset.

Por qué con el carro y no con el celular: el modelo aprende de lo que ve. Si entrenas con
fotos del celular (otra cámara, otros colores, otra altura) y lo usas con la cámara del
carro, se equivoca más. Lo ideal es entrenar con el mismo sensor y desde la misma altura.

Uso (con el PC conectado a la red WiFi CarroBalon-XXXX):
    python capturar_fotos.py --sesion sesion01

Por cable USB (más rápido; el carro conectado al PC por el USB del módulo de cámara):
    python capturar_fotos.py --sesion sesion01 --usb COM12

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


class FotosUSB:
    """Pide fotos por el USB del ESP32-S3 (ver firmware_esp32/main/usb_fotos.h)."""

    def __init__(self, puerto):
        import serial   # pip install pyserial
        self.s = serial.Serial()
        self.s.port, self.s.baudrate, self.s.timeout = puerto, 115200, 2
        # DTR/RTS apagados: en el USB del S3 esas líneas reinician el chip o lo dejan en modo de carga
        self.s.dtr = self.s.rts = False
        self.s.open()

    def foto(self):
        self.s.reset_input_buffer()
        self.s.write(b'F')
        buf = b''
        while b'FOTO' not in buf:          # antes de la marca pueden venir mensajes de registro
            parte = self.s.read(64)
            if not parte or len(buf) > 65536:
                raise IOError('el carro no respondió por USB')
            buf += parte
        buf = buf[buf.index(b'FOTO') + 4:]
        while len(buf) < 4:
            buf += self.s.read(4 - len(buf))
        n = int.from_bytes(buf[:4], 'little')
        datos = buf[4:]
        while len(datos) < n:
            parte = self.s.read(n - len(datos))
            if not parte:
                raise IOError('foto incompleta')
            datos += parte
        datos = datos[:n]
        if not (datos[:2] == b'\xff\xd8' and datos[-2:] == b'\xff\xd9'):
            raise IOError('JPEG dañado (se pide otra)')
        return datos


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--sesion', required=True, help='prefijo de los archivos, p. ej. sesion01 (sin "_")')
    ap.add_argument('--url', default='http://192.168.4.1/foto.jpg')
    ap.add_argument('--carpeta', default=os.path.join(AQUI, '..', 'datos', 'fotos'))
    ap.add_argument('--intervalo', type=float, default=0.7)
    ap.add_argument('--usb', metavar='COMx', help='pedir las fotos por el cable USB en vez del WiFi')
    a = ap.parse_args()
    if '_' in a.sesion:
        ap.error('el nombre de sesión no puede tener "_"')

    os.makedirs(a.carpeta, exist_ok=True)
    existentes = glob.glob(os.path.join(a.carpeta, f'{a.sesion}_*.jpg'))
    numeros = [os.path.basename(p)[len(a.sesion) + 1:-4] for p in existentes]
    k = 1 + max([int(n) for n in numeros if n.isdigit()] or [0])
    auto, t_ultima, guardadas = False, 0.0, 0
    print(f'Guardando en {os.path.abspath(a.carpeta)} desde {a.sesion}_{k:04d}.jpg')
    usb = FotosUSB(a.usb) if a.usb else None

    while True:
        try:
            datos = usb.foto() if usb else urllib.request.urlopen(a.url, timeout=3).read()
        except Exception as e:
            print('No responde el carro:', e, '(¿cable USB conectado?)' if usb else '(¿estás conectado a su WiFi?)')
            time.sleep(0.2 if usb else 1)
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
