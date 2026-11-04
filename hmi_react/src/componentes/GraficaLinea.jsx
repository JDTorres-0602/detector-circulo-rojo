// GraficaLinea.jsx — Gráfica de líneas en canvas (últimos HISTORIA_S segundos) con cursor
import { useEffect, useRef, useState } from 'react';
import { HISTORIA_S } from '../api.js';

const MARGEN = { izq: 40, der: 10, arr: 8, abj: 18 };
const TINTA = '#9cc7da', TENUE = 'rgba(79,216,255,.10)', CERO = 'rgba(79,216,255,.35)';

// Paso "bonito" para las marcas del eje
function pasoBonito(rango, marcas) {
  const bruto = rango / marcas, mag = 10 ** Math.floor(Math.log10(bruto)), n = bruto / mag;
  return (n < 1.5 ? 1 : n < 3 ? 2 : n < 7 ? 5 : 10) * mag;
}

export default function GraficaLinea({ historia, version, series, unidad, minAbs = 1, fijo, alto, decimales = 1 }) {
  const cvRef = useRef(null);
  const [hover, setHover] = useState(null);     // x en px dentro del canvas
  const [tam, setTam] = useState({ w: 300, h: alto || 80 });

  useEffect(() => {
    const cv = cvRef.current;
    const obs = new ResizeObserver(() => setTam({ w: cv.clientWidth, h: cv.clientHeight }));
    obs.observe(cv);
    return () => obs.disconnect();
  }, []);

  useEffect(() => {
    const cv = cvRef.current, g = cv.getContext('2d');
    const dpr = window.devicePixelRatio || 1;
    const { w, h } = tam;
    if (cv.width !== Math.round(w * dpr)) { cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr); }
    g.setTransform(dpr, 0, 0, dpr, 0, 0);
    g.clearRect(0, 0, w, h);
    const ahora = Date.now(), t0 = ahora - HISTORIA_S * 1000;
    const datos = historia.current.filter((m) => m.t >= t0);
    const compacto = h < 72;                 // gráfica baja: sin etiquetas de tiempo
    const x0 = MARGEN.izq, x1 = w - MARGEN.der, y0 = compacto ? 4 : MARGEN.arr, y1 = h - (compacto ? 4 : MARGEN.abj);

    // Escala Y: fija o simétrica automática (mínimo ±minAbs)
    let lo, hi;
    if (fijo) [lo, hi] = fijo;
    else {
      let m = minAbs;
      for (const d of datos) for (const s of series) m = Math.max(m, Math.abs(d[s.k]));
      const p = pasoBonito(2 * m, 4);
      hi = Math.ceil(m / p) * p; lo = -hi;
    }
    const X = (t) => x0 + ((t - t0) / (HISTORIA_S * 1000)) * (x1 - x0);
    const Y = (v) => y1 - ((v - lo) / (hi - lo)) * (y1 - y0);

    // Rejilla y ejes (discretos)
    g.font = '10px ui-monospace, Menlo, Consolas, monospace';
    g.textAlign = 'right'; g.textBaseline = 'middle';
    const paso = pasoBonito(hi - lo, compacto ? 2 : 4);
    for (let v = Math.ceil(lo / paso) * paso; v <= hi + 1e-9; v += paso) {
      const y = Math.round(Y(v)) + 0.5;
      g.strokeStyle = Math.abs(v) < 1e-9 ? CERO : TENUE; g.lineWidth = 1;
      g.beginPath(); g.moveTo(x0, y); g.lineTo(x1, y); g.stroke();
      if (!compacto || h >= 56) { g.fillStyle = TINTA; g.fillText(+v.toFixed(2) + '', x0 - 6, y); }
    }
    // Gráfica muy baja: solo la escala "±máx" arriba (las marcas se encimarían)
    if (compacto && h < 56) { g.fillStyle = TINTA; g.textBaseline = 'top'; g.fillText(lo === -hi ? '±' + hi : hi + '', x0 - 6, y0); }
    g.textAlign = 'center'; g.textBaseline = 'top';
    for (let s = HISTORIA_S; s >= 0; s -= 5) {
      if (compacto) { const x = X(ahora - s * 1000); g.strokeStyle = TENUE; g.beginPath(); g.moveTo(x, y0); g.lineTo(x, y1); g.stroke(); continue; }
      const x = X(ahora - s * 1000);
      g.strokeStyle = TENUE; g.beginPath(); g.moveTo(x, y0); g.lineTo(x, y1); g.stroke();
      g.textAlign = s === HISTORIA_S ? 'left' : s === 0 ? 'right' : 'center';
      g.fillStyle = TINTA; g.fillText(s ? `-${s}s` : 'ahora', x, y1 + 4);
    }

    // Líneas (2 px, brillo suave)
    g.save(); g.beginPath(); g.rect(x0, y0 - 2, x1 - x0, y1 - y0 + 4); g.clip();
    for (const s of series) {
      g.strokeStyle = s.color; g.lineWidth = 2; g.lineJoin = 'round';
      g.shadowColor = s.color; g.shadowBlur = 6;
      g.beginPath();
      datos.forEach((d, i) => { const x = X(d.t), y = Y(d[s.k]); i ? g.lineTo(x, y) : g.moveTo(x, y); });
      g.stroke();
    }
    g.restore();

    // Cursor: línea vertical + puntos en la muestra más cercana
    if (hover != null && datos.length) {
      const tH = t0 + ((hover - x0) / (x1 - x0)) * HISTORIA_S * 1000;
      let mejor = datos[0];
      for (const d of datos) if (Math.abs(d.t - tH) < Math.abs(mejor.t - tH)) mejor = d;
      const x = X(mejor.t);
      g.strokeStyle = 'rgba(214,246,255,.5)'; g.lineWidth = 1;
      g.beginPath(); g.moveTo(x, y0); g.lineTo(x, y1); g.stroke();
      for (const s of series) {
        g.fillStyle = s.color; g.strokeStyle = '#04101b'; g.lineWidth = 2;
        g.beginPath(); g.arc(x, Y(mejor[s.k]), 4, 0, Math.PI * 2); g.fill(); g.stroke();
      }
    }
  }, [version, hover, tam, lo_hi_clave(fijo), series, minAbs]);

  const ult = historia.current[historia.current.length - 1];
  // Muestra bajo el cursor (se calcula aquí para que el tooltip no vaya un cuadro atrás)
  let tip = null;
  if (hover != null) {
    const ahora = Date.now(), t0 = ahora - HISTORIA_S * 1000;
    const x0 = MARGEN.izq, x1 = tam.w - MARGEN.der;
    const tH = t0 + ((hover - x0) / (x1 - x0)) * HISTORIA_S * 1000;
    let mejor = null;
    for (const d of historia.current) if (d.t >= t0 && (!mejor || Math.abs(d.t - tH) < Math.abs(mejor.t - tH))) mejor = d;
    if (mejor) tip = { x: x0 + ((mejor.t - t0) / (HISTORIA_S * 1000)) * (x1 - x0), d: mejor, hace: (ahora - mejor.t) / 1000 };
  }
  return (
    <div className="grafica">
      {series.length > 1 && (
        <div className="leyenda">
          {series.map((s) => (
            <span key={s.k} className="leyenda-item">
              <i style={{ background: s.color }} /> {s.nombre}
              <b>{ult ? ult[s.k].toFixed(decimales) : '—'}</b>
            </span>
          ))}
          <span className="tenue">{unidad}</span>
        </div>
      )}
      <div className="grafica-lienzo" style={alto ? { height: alto, flex: 'none' } : undefined}>
        <canvas ref={cvRef}
          onPointerMove={(e) => setHover(e.nativeEvent.offsetX)}
          onPointerLeave={() => setHover(null)} />
        {tip && (
          <div className="tooltip" style={{ left: Math.min(tip.x + 10, tam.w - 130) }}>
            <div className="tenue">hace {tip.hace.toFixed(1)} s</div>
            {series.map((s) => (
              <div key={s.k}><i style={{ background: s.color }} /> {s.nombre} <b>{tip.d[s.k].toFixed(decimales)}</b> {unidad}</div>
            ))}
          </div>
        )}
        {!historia.current.length && <div className="grafica-vacia">Sin datos de la IMU</div>}
      </div>
    </div>
  );
}

const lo_hi_clave = (f) => (f ? f.join(',') : '');
