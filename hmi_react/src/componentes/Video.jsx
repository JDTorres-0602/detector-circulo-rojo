// Video.jsx — Video FPV con HUD: objetivo, blobs, horizonte, rumbo, cabeza y distancia
import { useEffect, useRef, useState } from 'react';
import { DEMO, sim, pedirFoto, esperar } from '../api.js';

const CIAN = '#4fd8ff';
const ORO = '#ffb84d';
const ROJO = '#ff4d4d';
const cian = (a) => `rgba(79,216,255,${a})`;

// Lado de la cabeza: + = derecha, en grados respecto al frente
export const relServo = (s) => (s.servo_izq_mayor ? 90 - s.servo : s.servo - 90);

export default function Video({ estado, conexion }) {
  const imgRef = useRef(null);
  const capaRef = useRef(null);
  const estadoRef = useRef(estado);
  const evRef = useRef({ visto: null, t: 0 });
  const fpsRef = useRef(0);
  const [verMapa, setVerMapa] = useState(false);
  const mapaRef = useRef(false);
  const [sinVideo, setSinVideo] = useState(!DEMO);
  const sinVideoRef = useRef(!DEMO);
  estadoRef.current = estado;
  mapaRef.current = verMapa;
  sinVideoRef.current = sinVideo;

  // Aviso "objetivo fijado": se dispara cuando sube ev_fijado
  if (estado) {
    const ev = evRef.current;
    if (ev.visto !== null && estado.ev_fijado > ev.visto) ev.t = performance.now();
    ev.visto = estado.ev_fijado;
  }

  // Bucle de video: pide /foto.jpg una y otra vez
  useEffect(() => {
    if (DEMO) { fpsRef.current = 12; return; }
    let vivo = true;
    (async () => {
      let n = 0, t = performance.now();
      while (vivo) {
        try {
          const blob = await pedirFoto();
          const url = URL.createObjectURL(blob);
          const img = imgRef.current;
          const viejo = img.src;
          await new Promise((ok) => { img.onload = ok; img.onerror = ok; img.src = url; });
          if (viejo.startsWith('blob:')) URL.revokeObjectURL(viejo);
          n++;
          if (sinVideoRef.current) setSinVideo(false);
        } catch {
          if (!sinVideoRef.current) setSinVideo(true);
          await esperar(400);
        }
        const dt = performance.now() - t;
        if (dt > 1000) { fpsRef.current = (n * 1000) / dt; n = 0; t = performance.now(); }
      }
    })();
    return () => { vivo = false; };
  }, []);

  // HUD a 60 fps (las animaciones no dependen de que llegue un dato nuevo)
  useEffect(() => {
    const cv = capaRef.current;
    const g = cv.getContext('2d');
    let raf;
    const obs = new ResizeObserver(() => {
      const dpr = window.devicePixelRatio || 1;
      cv.width = Math.round(cv.clientWidth * dpr);
      cv.height = Math.round(cv.clientHeight * dpr);
    });
    obs.observe(cv);
    const bucle = (ts) => {
      const dpr = window.devicePixelRatio || 1;
      const w = cv.width / dpr, h = cv.height / dpr;
      g.setTransform(dpr, 0, 0, dpr, 0, 0);
      g.clearRect(0, 0, w, h);
      const s = estadoRef.current;
      if (DEMO && s) sim.dibujar(g, w, h, s.imu);
      dibujarHud(g, w, h, s, ts, {
        mapa: mapaRef.current, fps: fpsRef.current, sinVideo: sinVideoRef.current,
        tEvento: evRef.current.t,
      });
      raf = requestAnimationFrame(bucle);
    };
    raf = requestAnimationFrame(bucle);
    return () => { cancelAnimationFrame(raf); obs.disconnect(); };
  }, []);

  return (
    <div className="video">
      <img ref={imgRef} alt="" className={sinVideo ? 'oculto' : ''} />
      <canvas ref={capaRef} />
      <div className="video-barrido" />
      <div className="video-botones">
        <button className={'chip' + (verMapa ? ' activo' : '')} onClick={() => setVerMapa(!verMapa)}
          title="Muestra el mapa de detección 12×16 que ve la visión">
          MAPA DE CALOR
        </button>
      </div>
      {conexion !== 'conectado' && <div className="video-aviso">ENLACE {conexion === 'conectando' ? 'EN CURSO…' : 'PERDIDO'}</div>}
    </div>
  );
}

// ---------------------------------------------------------------- dibujo del HUD
function texto(g, t, x, y, { color = CIAN, tam = 11, alin = 'left', peso = 600, base = 'alphabetic' } = {}) {
  g.font = `${peso} ${tam}px ui-monospace, "SF Mono", Menlo, Consolas, monospace`;
  g.fillStyle = color;
  g.textAlign = alin;
  g.textBaseline = base;
  g.fillText(t, x, y);
}

function esquinas(g, x, y, w, h, l, color, grosor = 2) {
  g.strokeStyle = color; g.lineWidth = grosor;
  g.beginPath();
  g.moveTo(x, y + l); g.lineTo(x, y); g.lineTo(x + l, y);
  g.moveTo(x + w - l, y); g.lineTo(x + w, y); g.lineTo(x + w, y + l);
  g.moveTo(x + w, y + h - l); g.lineTo(x + w, y + h); g.lineTo(x + w - l, y + h);
  g.moveTo(x + l, y + h); g.lineTo(x, y + h); g.lineTo(x, y + h - l);
  g.stroke();
}

function dibujarHud(g, w, h, s, ts, op) {
  const cx = w / 2, cy = h / 2;
  const chico = w < 420;

  // Viñeta suave para que el HUD se lea sobre cualquier imagen
  const vin = g.createRadialGradient(cx, cy, Math.min(w, h) * 0.35, cx, cy, Math.max(w, h) * 0.75);
  vin.addColorStop(0, 'rgba(0,10,20,0)'); vin.addColorStop(1, 'rgba(0,10,20,.55)');
  g.fillStyle = vin; g.fillRect(0, 0, w, h);

  if (op.sinVideo && !DEMO) {
    // Estática
    for (let i = 0; i < 260; i++) {
      g.fillStyle = cian(Math.random() * 0.25);
      g.fillRect(Math.random() * w, Math.random() * h, 2 + Math.random() * 30, 1);
    }
    texto(g, 'SIN SEÑAL DE VIDEO', cx, cy - 6, { alin: 'center', tam: chico ? 13 : 16, peso: 700 });
    texto(g, 'conéctate al WiFi CarroBalon-XXXX', cx, cy + 16, { alin: 'center', tam: 11, color: cian(0.7), peso: 400 });
  }

  // Marco
  esquinas(g, 10, 10, w - 20, h - 20, 22, cian(0.8));
  if (!s) return;

  // Mapa de calor de la visión (12x16)
  if (op.mapa && s.mapa) {
    const cw = w / 16, ch = h / 12;
    for (let i = 0; i < 12; i++) for (let j = 0; j < 16; j++) {
      const v = s.mapa[i * 16 + j] / 100;
      if (v > 0.03) { g.fillStyle = cian(0.45 * v); g.fillRect(j * cw + 1, i * ch + 1, cw - 2, ch - 2); }
      if (v >= s.umbral) { g.strokeStyle = cian(0.8); g.lineWidth = 1; g.strokeRect(j * cw + 1.5, i * ch + 1.5, cw - 3, ch - 3); }
    }
  }

  // Horizonte artificial (pitch / roll de la IMU)
  if (s.imu) {
    const pxGrado = h / 60;
    g.save();
    g.translate(cx, cy + s.imu.pitch * pxGrado);
    g.rotate((-s.imu.roll * Math.PI) / 180);
    g.strokeStyle = cian(0.55); g.lineWidth = 1.5;
    const L = w * 0.22, hueco = 46;
    g.beginPath(); g.moveTo(-L - hueco, 0); g.lineTo(-hueco, 0); g.moveTo(hueco, 0); g.lineTo(L + hueco, 0); g.stroke();
    g.lineWidth = 1; g.strokeStyle = cian(0.3);
    for (const p of [-20, -10, 10, 20]) {
      const y = -p * pxGrado, l = 28;
      g.beginPath(); g.moveTo(-hueco - l, y); g.lineTo(-hueco, y); g.moveTo(hueco, y); g.lineTo(hueco + l, y); g.stroke();
      if (!chico) texto(g, String(p), hueco + l + 4, y + 4, { color: cian(0.45), tam: 9, peso: 400 });
    }
    g.restore();
  }

  // Retícula central
  g.strokeStyle = cian(0.7); g.lineWidth = 1.2;
  g.beginPath(); g.arc(cx, cy, 16, 0, Math.PI * 2); g.stroke();
  g.beginPath();
  g.moveTo(cx - 30, cy); g.lineTo(cx - 20, cy); g.moveTo(cx + 20, cy); g.lineTo(cx + 30, cy);
  g.moveTo(cx, cy - 30); g.lineTo(cx, cy - 20); g.moveTo(cx, cy + 20); g.lineTo(cx, cy + 30);
  g.stroke();
  g.fillStyle = CIAN; g.beginPath(); g.arc(cx, cy, 1.6, 0, Math.PI * 2); g.fill();

  // Cinta de rumbo (yaw)
  if (s.imu) {
    const ancho = Math.min(w * 0.5, 360), x0 = cx - ancho / 2, y = 30, pxG = ancho / 90;
    g.save(); g.beginPath(); g.rect(x0, y - 16, ancho, 30); g.clip();
    for (let ang = -175; ang <= 180; ang += 5) {
      let rel = ang - s.imu.yaw;
      while (rel > 180) rel -= 360; while (rel < -180) rel += 360;
      if (Math.abs(rel) > 50) continue;
      const x = cx - rel * pxG;   // yaw + = izquierda
      const mayor = ((ang % 30) + 30) % 30 === 0;
      g.strokeStyle = cian(mayor ? 0.75 : 0.35); g.lineWidth = 1;
      g.beginPath(); g.moveTo(x, y); g.lineTo(x, y - (mayor ? 9 : 5)); g.stroke();
      if (mayor) texto(g, String(ang), x, y + 11, { alin: 'center', tam: 9, color: cian(0.7), peso: 400 });
    }
    g.restore();
    g.fillStyle = CIAN;
    g.beginPath(); g.moveTo(cx, y + 1); g.lineTo(cx - 5, y - 7); g.lineTo(cx + 5, y - 7); g.closePath(); g.fill();
    texto(g, `YAW ${s.imu.yaw.toFixed(0)}°`, cx, y - 12, { alin: 'center', tam: 10 });
  }

  // Blobs candidatos y objetivo
  const cw = w / 16;
  (s.blobs || []).forEach((b, k) => {
    if (k === s.elegido) return;
    const r = Math.max(8, Math.sqrt(b[2]) * cw * 0.6);
    g.setLineDash([3, 4]); g.strokeStyle = cian(0.5); g.lineWidth = 1;
    g.beginPath(); g.arc(b[0] * w, b[1] * h, r, 0, Math.PI * 2); g.stroke(); g.setLineDash([]);
    texto(g, 'CANDIDATO', b[0] * w + r + 4, b[1] * h + 3, { tam: 9, color: cian(0.55), peso: 400 });
  });
  if (s.obj) {
    const [ox, oy, area] = s.obj;
    const x = ox * w, y = oy * h;
    const lado = Math.max(34, Math.sqrt(area) * cw * 1.3);
    const fijado = s.fijado;
    const col = fijado ? ORO : CIAN;
    // Línea de atención: del centro de la vista al objetivo
    g.strokeStyle = fijado ? 'rgba(255,184,77,.45)' : cian(0.35); g.lineWidth = 1; g.setLineDash([2, 5]);
    g.beginPath(); g.moveTo(cx, cy); g.lineTo(x, y); g.stroke(); g.setLineDash([]);
    // Caja de enganche con esquinas y anillo giratorio
    const pulso = fijado ? 0 : Math.sin(ts / 120) * 4;
    g.shadowColor = col; g.shadowBlur = 10;
    esquinas(g, x - lado / 2 - pulso, y - lado / 2 - pulso, lado + 2 * pulso, lado + 2 * pulso, lado * 0.28, col, 2);
    g.shadowBlur = 0;
    g.save(); g.translate(x, y); g.rotate(ts / 900);
    g.strokeStyle = fijado ? 'rgba(255,184,77,.7)' : cian(0.5); g.lineWidth = 1.2;
    for (let i = 0; i < 4; i++) { g.beginPath(); g.arc(0, 0, lado * 0.72, i * Math.PI / 2, i * Math.PI / 2 + 0.9); g.stroke(); }
    g.restore();
    g.fillStyle = col; g.beginPath(); g.arc(x, y, 2.5, 0, Math.PI * 2); g.fill();
    const etiqueta = fijado ? 'OBJETIVO FIJADO' : 'ADQUIRIENDO…';
    texto(g, etiqueta, x, y - lado / 2 - 12, { alin: 'center', tam: 11, color: col, peso: 700 });
    texto(g, `X ${(ox * 2 - 1).toFixed(2)}  ÁREA ${area.toFixed ? area.toFixed(0) : area}`, x, y + lado / 2 + 16,
      { alin: 'center', tam: 9, color: col, peso: 400 });
  }

  // Esquina superior izquierda: modo
  const auto = s.control === 'auto';
  texto(g, auto ? 'AUTÓNOMO' : 'MANUAL', 22, 34, { tam: chico ? 11 : 13, peso: 700, color: auto ? ORO : CIAN });
  if (auto) texto(g, '› ' + (s.auto || '').toUpperCase(), 22, 50, { tam: 10, color: cian(0.85) });
  // Esquina superior derecha: cuadros por segundo y parpadeo de "en vivo"
  const vivo = Math.floor(ts / 600) % 2 === 0;
  g.fillStyle = vivo ? ROJO : 'rgba(255,77,77,.25)';
  g.beginPath(); g.arc(w - 74, 30, 4, 0, Math.PI * 2); g.fill();
  texto(g, 'EN VIVO', w - 64, 34, { tam: 10, color: cian(0.85) });
  texto(g, `${op.fps.toFixed(1)} FPS`, w - 22, 50, { alin: 'right', tam: 10, color: cian(0.6), peso: 400 });

  // Indicador de la cabeza (servo): hacia dónde mira la cámara
  const rel = relServo(s);
  const ax = cx, ay = h - 26, R = Math.min(70, w * 0.16);
  g.strokeStyle = cian(0.35); g.lineWidth = 1;
  g.beginPath(); g.arc(ax, ay, R, Math.PI, 2 * Math.PI); g.stroke();
  for (const d of [-90, -45, 0, 45, 90]) {
    const a = ((d - 90) * Math.PI) / 180;
    g.beginPath(); g.moveTo(ax + Math.cos(a) * (R - 4), ay + Math.sin(a) * (R - 4)); g.lineTo(ax + Math.cos(a) * R, ay + Math.sin(a) * R); g.stroke();
  }
  const a = ((rel - 90) * Math.PI) / 180;
  g.strokeStyle = CIAN; g.lineWidth = 2; g.shadowColor = CIAN; g.shadowBlur = 8;
  g.beginPath(); g.moveTo(ax, ay); g.lineTo(ax + Math.cos(a) * R, ay + Math.sin(a) * R); g.stroke();
  g.shadowBlur = 0;
  const ladoTxt = Math.abs(rel) < 3 ? 'FRENTE' : `${Math.abs(rel).toFixed(0)}° ${rel > 0 ? 'DER' : 'IZQ'}`;
  texto(g, 'CABEZA ' + ladoTxt, ax, ay + 14, { alin: 'center', tam: 9, color: cian(0.8), peso: 400 });

  // Distancia (ultrasonido)
  const d = s.dist;
  const colD = d > 0 && d < s.dist_freno ? ROJO : d > 0 && d < s.dist_libre ? ORO : CIAN;
  const dTxt = d > 0 ? `${String(d).padStart(3, '0')} cm` : 'LIBRE';
  texto(g, 'DIST', 22, h - 40, { tam: 9, color: cian(0.6), peso: 400 });
  texto(g, dTxt, 22, h - 22, { tam: chico ? 14 : 18, color: colD, peso: 700 });
  if (!s.frente) texto(g, 'FRENTE SIN VIGILAR', 22, h - 52, { tam: 9, color: ORO, peso: 400 });

  // Motores
  texto(g, `I ${String(s.izq).padStart(4)}  D ${String(s.der).padStart(4)}`, w - 22, h - 22,
    { alin: 'right', tam: chico ? 10 : 12, color: cian(0.85) });
  texto(g, 'MOTORES PWM', w - 22, h - 40, { alin: 'right', tam: 9, color: cian(0.6), peso: 400 });

  // Obstáculo: marco rojo que late
  if (s.obstaculo) {
    const al = 0.45 + 0.35 * Math.sin(ts / 110);
    g.strokeStyle = `rgba(255,77,77,${al})`; g.lineWidth = 4; g.strokeRect(4, 4, w - 8, h - 8);
    texto(g, '⚠ OBSTÁCULO AL FRENTE · AVANCE BLOQUEADO', cx, 70, { alin: 'center', tam: chico ? 10 : 13, color: ROJO, peso: 700 });
  }

  // Aviso grande al fijar el objeto (2 s)
  const dt = performance.now() - op.tEvento;
  if (op.tEvento && dt < 2200) {
    const al = dt < 1700 ? 1 : 1 - (dt - 1700) / 500;
    g.save(); g.globalAlpha = al;
    g.fillStyle = 'rgba(40,24,0,.55)'; g.fillRect(0, cy - 30, w, 60);
    g.strokeStyle = ORO; g.lineWidth = 1; g.beginPath(); g.moveTo(0, cy - 30); g.lineTo(w, cy - 30); g.moveTo(0, cy + 30); g.lineTo(w, cy + 30); g.stroke();
    g.shadowColor = ORO; g.shadowBlur = 16;
    texto(g, 'OBJETO FIJADO CON ÉXITO', cx, cy, { alin: 'center', base: 'middle', tam: chico ? 15 : 22, color: ORO, peso: 800 });
    g.restore();
  }
}
