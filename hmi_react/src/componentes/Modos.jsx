// Modos.jsx — Botones de modo
import { comando } from '../api.js';

export default function Modos({ estado, deshabilitado }) {
  const auto = estado?.control === 'auto';
  return (
    <div className="modos">
      <button className={'boton boton-auto' + (auto ? ' activo' : '')} disabled={deshabilitado || auto}
        onClick={() => comando.modo('auto')}>
        <span className="boton-icono">◎</span> <span>MODO AUTÓNOMO</span>
      </button>
      <button className="boton boton-salir" disabled={deshabilitado || !auto}
        onClick={() => { comando.modo('manual'); comando.manual(0, 0); }}>
        <span className="boton-icono">✕</span> <span>SALIR DE AUTÓNOMO</span>
      </button>
      <button className={'boton' + (!auto && estado ? ' activo' : '')} disabled={deshabilitado || !auto}
        onClick={() => comando.modo('manual')}>
        <span className="boton-icono">✥</span> <span>MODO MANUAL</span>
      </button>
    </div>
  );
}
