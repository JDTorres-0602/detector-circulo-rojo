// Telemetria.jsx — Distancia, detalles y radar del último barrido (columna junto al video)
import { relServo } from './Video.jsx';

const ESTADOS = {
  apagado: 'En espera', escaneando: 'Escaneando', alineando: 'Alineando',
  confirmando: 'Confirmando', siguiendo: 'Siguiendo', avanzando: 'Avanzando', esquivando: 'Esquivando',
};

export function Distancia({ s }) {
  if (!s) return <div className="tele-distancia"><div className="tele-k">DISTANCIA AL FRENTE</div><div className="tele-grande">—</div></div>;
  const d = s.dist;
  const nivel = d > 0 && d < s.dist_freno ? 'critico' : d > 0 && d < s.dist_libre ? 'aviso' : 'ok';
  const pct = d > 0 ? Math.min(100, (d / 150) * 100) : 100;
  return (
    <div className={'tele-distancia nivel-' + nivel}>
      <div className="tele-k">DISTANCIA AL FRENTE</div>
      <div className="tele-grande">{d > 0 ? d : '>400'}<small> cm</small></div>
      <div className="barra-dist"><div style={{ width: pct + '%' }} />
        <span className="marca-umbral" style={{ left: (s.dist_freno / 150) * 100 + '%' }} title="freno" />
        <span className="marca-umbral suave" style={{ left: (s.dist_libre / 150) * 100 + '%' }} title="libre" />
      </div>
      <div className="tele-pie">
        {nivel === 'critico' ? '▲ FRENO ACTIVO' : nivel === 'aviso' ? '● CAMINO ESTRECHO' : '✓ CAMINO LIBRE'}
        {!s.frente && ' · frente sin vigilar'}
      </div>
    </div>
  );
}

export function Detalles({ s }) {
  if (!s) return null;
  const r = relServo(s);
  return (
    <div className="tele-rejilla">
      <Tile k="BÚSQUEDA" v={s.control === 'auto' ? ESTADOS[s.auto] || s.auto : 'Manual'} />
      <Tile k="OBJETIVO" v={s.fijado ? 'FIJADO' : s.obj ? 'Detectado' : 'Ninguno'} acento={s.fijado} />
      <Tile k="POSICIÓN" v={s.obj ? `x ${(s.obj[0] * 2 - 1).toFixed(2)} · a ${Math.round(s.obj[2])}` : '—'} />
      <Tile k="CABEZA" v={r === 0 ? `${s.servo}° frente` : `${s.servo}° ${Math.abs(r)}°${r > 0 ? 'D' : 'I'}`} />
      <Tile k="DETECCIÓN" v={`${s.det_s.toFixed(1)}/s`} />
      <Tile k="BARRIDOS" v={s.escaneos} />
    </div>
  );
}

function Tile({ k, v, acento }) {
  return (
    <div className="tile">
      <div className="tele-k">{k}</div>
      <div className={'tile-v' + (acento ? ' acento' : '')}>{v}</div>
    </div>
  );
}

// Radar: semicírculo con el frente hacia arriba. Rayo = hacia dónde mira la cabeza.
// Puntos = distancias del último barrido. Rombo = ángulo donde se vio el objetivo.
export function Radar({ s }) {
  const cx = 100, cy = 100, R = 86, MAX = 150;
  if (!s) return <div className="radar" />;
  const aXY = (rel, dcm) => {
    const r = (Math.min(dcm, MAX) / MAX) * R, a = (rel * Math.PI) / 180;
    return [cx + r * Math.sin(a), cy - r * Math.cos(a)];
  };
  const relDe = (ang) => (s.servo_izq_mayor ? 90 - ang : ang - 90);
  const puntos = (s.perfil || []).map(([ang, d]) => [relDe(ang), d >= 999 ? MAX : d, d >= 999]);
  const contorno = puntos.slice().sort((a, b) => a[0] - b[0]).map(([r, d]) => aXY(r, d).join(',')).join(' ');
  const [bx, by] = aXY(relServo(s), MAX);
  const [ox, oy] = aXY(relDe(s.obj_ang), MAX * 0.8);
  const arco = (r) => `M${cx - (r / MAX) * R} ${cy} A${(r / MAX) * R} ${(r / MAX) * R} 0 0 1 ${cx + (r / MAX) * R} ${cy}`;
  return (
    <div className="radar">
      <div className="tele-k">RADAR · ÚLTIMO BARRIDO</div>
      <svg viewBox="0 0 200 112" preserveAspectRatio="xMidYMax meet" role="img" aria-label="Radar del barrido del servo">
        {[50, 100, 150].map((r) => (
          <g key={r}>
            <path d={arco(r)} className="r-anillo" />
            <text x={cx + (r / MAX) * R} y={cy + 9} className="r-num">{r === MAX ? r + ' cm' : r}</text>
          </g>
        ))}
        <path d={arco(s.dist_freno)} className="r-freno" />
        {[-90, -45, 0, 45, 90].map((a) => { const [x, y] = aXY(a, MAX); return <line key={a} x1={cx} y1={cy} x2={x} y2={y} className="r-radio" />; })}
        {puntos.length > 1 && <polyline points={contorno} className="r-contorno" />}
        {puntos.map(([r, d, libre], i) => { const [x, y] = aXY(r, d); return <circle key={i} cx={x} cy={y} r="3" className={libre ? 'r-punto libre' : 'r-punto'} />; })}
        <line x1={cx} y1={cy} x2={bx} y2={by} className="r-rayo" />
        {s.fijado && <path d={`M${ox} ${oy - 6} L${ox + 6} ${oy} L${ox} ${oy + 6} L${ox - 6} ${oy} Z`} className="r-objetivo" />}
        <text x="4" y="10" className="r-lado" textAnchor="start">◀ IZQ</text>
        <text x="196" y="10" className="r-lado" textAnchor="end">DER ▶</text>
      </svg>
    </div>
  );
}
