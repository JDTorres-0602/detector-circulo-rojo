// Cruceta.jsx — Control manual: mantener presionado para mover (también flechas / WASD)
import { useEffect, useRef, useState } from 'react';
import { comando } from '../api.js';

const VELOCIDADES = [{ k: 'BAJA', v: 110 }, { k: 'MEDIA', v: 160 }, { k: 'ALTA', v: 220 }];
const TECLAS = {
  ArrowUp: 'arriba', KeyW: 'arriba', ArrowDown: 'abajo', KeyS: 'abajo',
  ArrowLeft: 'izq', KeyA: 'izq', ArrowRight: 'der', KeyD: 'der',
};

// Dirección(es) presionadas -> velocidades de cada rueda
function mezclar(p, v) {
  const f = (p.arriba ? 1 : 0) - (p.abajo ? 1 : 0);
  const g = (p.der ? 1 : 0) - (p.izq ? 1 : 0);
  if (!f && !g) return [0, 0];
  if (!f) return [Math.round(g * v * 0.85), Math.round(-g * v * 0.85)];        // girar sobre su eje
  const curva = g * v * 0.45;                                                   // adelante/atrás en curva
  const lim = (x) => Math.max(-255, Math.min(255, Math.round(x)));
  return [lim(f * v + curva), lim(f * v - curva)];
}

export default function Cruceta({ habilitado }) {
  const [pres, setPres] = useState({});
  const [nivel, setNivel] = useState(1);
  const presRef = useRef(pres);
  presRef.current = pres;
  const vel = VELOCIDADES[nivel].v;

  // Mientras haya algo presionado, se repite la orden (la ESP32 para si dejan de llegar)
  useEffect(() => {
    const [i, d] = mezclar(pres, vel);
    if (!habilitado) return;
    comando.manual(i, d);
    if (!i && !d) return;
    const t = setInterval(() => comando.manual(i, d), 150);
    return () => clearInterval(t);
  }, [pres, vel, habilitado]);

  useEffect(() => { if (!habilitado) setPres({}); }, [habilitado]);

  // Teclado (PC)
  useEffect(() => {
    const abajo = (e) => {
      if (!habilitado || e.target.tagName === 'INPUT') return;
      if (e.code === 'Space') { e.preventDefault(); setPres({}); return; }
      if (/^Digit[123]$/.test(e.code)) { setNivel(+e.code.slice(5) - 1); return; }
      const dir = TECLAS[e.code];
      if (dir) { e.preventDefault(); if (!presRef.current[dir]) setPres((p) => ({ ...p, [dir]: true })); }
    };
    const arriba = (e) => {
      const dir = TECLAS[e.code];
      if (dir) setPres((p) => { const n = { ...p }; delete n[dir]; return n; });
    };
    const soltarTodo = () => setPres({});
    window.addEventListener('keydown', abajo);
    window.addEventListener('keyup', arriba);
    window.addEventListener('blur', soltarTodo);
    return () => { window.removeEventListener('keydown', abajo); window.removeEventListener('keyup', arriba); window.removeEventListener('blur', soltarTodo); };
  }, [habilitado]);

  const tecla = (dir, simbolo, etiqueta) => ({
    className: 'cruz-btn cruz-' + dir + (pres[dir] ? ' presionado' : ''),
    disabled: !habilitado,
    'aria-label': etiqueta,
    onPointerDown: (e) => { e.preventDefault(); e.currentTarget.setPointerCapture?.(e.pointerId); setPres((p) => ({ ...p, [dir]: true })); },
    onPointerUp: () => setPres((p) => { const n = { ...p }; delete n[dir]; return n; }),
    onPointerCancel: () => setPres((p) => { const n = { ...p }; delete n[dir]; return n; }),
    onContextMenu: (e) => e.preventDefault(),
    children: <span>{simbolo}</span>,
  });

  const [i, d] = mezclar(pres, vel);
  return (
    <div className={'cruceta-zona' + (habilitado ? '' : ' bloqueada')}
      title="Mantén presionado. Combina ▲ + ◀/▶ para curvas. PC: flechas o WASD, espacio = parar, 1-2-3 = potencia.">
      <div className="cruceta">
        <button {...tecla('arriba', '▲', 'Adelante')} />
        <button {...tecla('izq', '◀', 'Girar a la izquierda')} />
        <button className="cruz-btn cruz-centro" disabled={!habilitado} onClick={() => setPres({})} aria-label="Parar">
          <span>STOP</span>
        </button>
        <button {...tecla('der', '▶', 'Girar a la derecha')} />
        <button {...tecla('abajo', '▼', 'Atrás')} />
      </div>
      <div className="cruceta-info">
        <div className="mini-titulo">POTENCIA</div>
        <div className="segmentos">
          {VELOCIDADES.map((x, k) => (
            <button key={x.k} className={'segmento' + (k === nivel ? ' activo' : '')} disabled={!habilitado}
              onClick={() => setNivel(k)}>{x.k}</button>
          ))}
        </div>
                <div className="orden">I <b>{i}</b> · D <b>{d}</b></div>
        {!habilitado && <div className="nota">Bloqueado: modo autónomo</div>}
      </div>
    </div>
  );
}
