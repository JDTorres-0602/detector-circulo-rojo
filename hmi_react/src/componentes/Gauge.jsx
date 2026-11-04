// Gauge.jsx — Reloj de aguja holográfico (velocidad / RPM)
const A0 = -225, A1 = 45;           // 270° de recorrido
const rad = (a) => (a * Math.PI) / 180;
const punto = (cx, cy, r, a) => [cx + r * Math.cos(rad(a)), cy + r * Math.sin(rad(a))];
function arco(cx, cy, r, a, b) {
  const [x1, y1] = punto(cx, cy, r, a), [x2, y2] = punto(cx, cy, r, b);
  return `M${x1} ${y1} A${r} ${r} 0 ${b - a > 180 ? 1 : 0} 1 ${x2} ${y2}`;
}

export default function Gauge({ titulo, valor, max, unidad, decimales = 0, sub }) {
  const v = Math.abs(valor || 0);
  const frac = Math.max(0, Math.min(1, v / (max || 1)));
  const a = A0 + frac * (A1 - A0);
  const cx = 70, cy = 70, R = 54;
  const marcas = [];
  for (let i = 0; i <= 10; i++) {
    const am = A0 + (i / 10) * (A1 - A0), mayor = i % 2 === 0;
    const [xa, ya] = punto(cx, cy, R + 4, am), [xb, yb] = punto(cx, cy, R + (mayor ? 11 : 8), am);
    marcas.push(<line key={i} x1={xa} y1={ya} x2={xb} y2={yb} className={mayor ? 'g-marca mayor' : 'g-marca'} />);
    if (mayor) {
      const [xt, yt] = punto(cx, cy, R - 10, am);
      marcas.push(<text key={'t' + i} x={xt} y={yt} className="g-num">{Math.round((max * i) / 10)}</text>);
    }
  }
  const [xn, yn] = punto(cx, cy, R - 4, a);
  return (
    <div className="gauge" role="img" aria-label={`${titulo}: ${v.toFixed(decimales)} ${unidad}`}>
      <svg viewBox="0 0 140 128">
        <path d={arco(cx, cy, R, A0, A1)} className="g-pista" />
        {frac > 0.002 && <path d={arco(cx, cy, R, A0, a)} className="g-valor" />}
        <circle cx={cx} cy={cy} r={R - 20} className="g-interior" />
        {marcas}
        <line x1={cx} y1={cy} x2={xn} y2={yn} className="g-aguja" />
        <circle cx={cx} cy={cy} r="3.5" className="g-eje" />
        <text x={cx} y={cy + 26} className="g-lectura">{v.toFixed(decimales)}</text>
        <text x={cx} y={cy + 38} className="g-unidad">{unidad}{valor < 0 ? ' · REVERSA' : ''}</text>
      </svg>
      <div className="gauge-titulo">{titulo}</div>
      {sub && <div className="gauge-sub">{sub}</div>}
    </div>
  );
}
