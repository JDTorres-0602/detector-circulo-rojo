// ServoSlider.jsx — Mover la cabeza (servo con cámara + ultrasonido) de izquierda a derecha
import { useEffect, useRef, useState } from 'react';
import { comando } from '../api.js';
import { relServo } from './Video.jsx';

// rel: -90 (izquierda) .. +90 (derecha)  ->  ángulo del servo según cómo esté montado
const anguloDe = (rel, izqMayor) => (izqMayor ? 90 - rel : 90 + rel);

export default function ServoSlider({ estado, habilitado }) {
  const izqMayor = estado ? estado.servo_izq_mayor : 1;
  const [rel, setRel] = useState(0);
  const arrastrando = useRef(false);
  const ultimoEnvio = useRef(0);
  const pendiente = useRef(null);

  // En autónomo (o al no estar arrastrando) el control sigue la posición real del servo
  useEffect(() => {
    if (estado && (!habilitado || !arrastrando.current)) setRel(relServo(estado));
  }, [estado?.servo, habilitado]);

  const enviar = (r) => {
    const ahora = performance.now();
    clearTimeout(pendiente.current);
    if (ahora - ultimoEnvio.current > 90) { ultimoEnvio.current = ahora; comando.servo(anguloDe(r, izqMayor)); }
    else pendiente.current = setTimeout(() => { ultimoEnvio.current = performance.now(); comando.servo(anguloDe(r, izqMayor)); }, 100);
  };
  const mover = (r) => { setRel(r); enviar(r); };

  return (
    <div className={'servo' + (habilitado ? '' : ' bloqueada')}>
      <div className="servo-cab">
        <span className="mini-titulo">CÁMARA · SERVO{!habilitado && ' (lo mueve el barrido)'}</span>
        <span className="servo-valor">
          {Math.abs(rel) < 3 ? 'FRENTE' : `${Math.abs(rel)}° ${rel > 0 ? 'DERECHA' : 'IZQUIERDA'}`}
          <span className="tenue"> · servo {anguloDe(rel, izqMayor)}°</span>
        </span>
      </div>
      <div className="servo-fila">
        <button className="chip" disabled={!habilitado} onClick={() => mover(Math.max(-90, rel - 15))} aria-label="Cabeza a la izquierda">◀ IZQ</button>
        <div className="servo-pista">
          <input type="range" min="-90" max="90" step="5" value={rel} disabled={!habilitado}
            aria-label="Posición de la cámara"
            onPointerDown={() => { arrastrando.current = true; }}
            onPointerUp={() => { arrastrando.current = false; }}
            onChange={(e) => mover(+e.target.value)} />
          <div className="servo-marcas"><span>-90°</span><span>-45°</span><span>0°</span><span>45°</span><span>90°</span></div>
        </div>
        <button className="chip" disabled={!habilitado} onClick={() => mover(Math.min(90, rel + 15))} aria-label="Cabeza a la derecha">DER ▶</button>
        <button className="chip" disabled={!habilitado} onClick={() => mover(0)}>CENTRAR</button>
      </div>
    </div>
  );
}
