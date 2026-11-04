// BarraEstado.jsx — Arriba: estado del enlace con el carro
import { useEffect, useState } from 'react';
import { DEMO } from '../api.js';

const TEXTO = { conectado: 'CONECTADO', conectando: 'CONECTANDO…', 'sin-conexion': 'SIN CONEXIÓN' };

export default function BarraEstado({ conexion, latencia, estado }) {
  const [hora, setHora] = useState(() => new Date());
  useEffect(() => { const t = setInterval(() => setHora(new Date()), 1000); return () => clearInterval(t); }, []);
  const ok = conexion === 'conectado';
  return (
    <header className="barra">
      <div className="marca">
        <svg viewBox="0 0 40 40" className="marca-logo" aria-hidden="true">
          <circle cx="20" cy="20" r="17" />
          <circle cx="20" cy="20" r="11" />
          <path d="M20 3v6M20 31v6M3 20h6M31 20h6" />
        </svg>
        <div>
          <div className="marca-nombre">C.A.R.R.O.</div>
        </div>
      </div>

      <div className={'enlace enlace-' + conexion} role="status" aria-live="polite">
        <span className="enlace-punto" />
        <span className="enlace-texto">{TEXTO[conexion]}</span>
        {DEMO && <span className="enlace-demo">SIMULACIÓN</span>}
      </div>

      <div className="barra-datos">
        <Dato k="LATENCIA" v={ok && latencia != null ? latencia + ' ms' : '—'} />
        <Dato k="ARDUINO UNO" v={ok ? (estado?.uno ? 'ENLAZADO' : 'SIN RESPUESTA') : '—'} mal={ok && !estado?.uno} />
        <Dato k="IMU" v={ok ? (estado?.imu ? 'EN LÍNEA' : 'NO DETECTADA') : '—'} mal={ok && !estado?.imu} />
        <Dato k="HORA" v={hora.toLocaleTimeString('es-CO', { hour12: false })} />
      </div>
    </header>
  );
}

function Dato({ k, v, mal }) {
  return (
    <div className="dato">
      <span className="dato-k">{k}</span>
      <span className={'dato-v' + (mal ? ' mal' : '')}>{v}</span>
    </div>
  );
}
