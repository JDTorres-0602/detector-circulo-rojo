// api.js — Comunicación con el carro (HTTP a la ESP32) y hook useCarro()
import { useEffect, useRef, useState } from 'react';
import { Simulador } from './demo.js';

const params = new URLSearchParams(location.search);
export const DEMO = params.has('demo');

// Servido desde la ESP32 -> rutas relativas. Desde "npm run dev" (localhost) -> 192.168.4.1.
// ?carro=IP permite apuntar a otra dirección.
export const BASE = params.get('carro')
  ? 'http://' + params.get('carro')
  : ['localhost', '127.0.0.1'].includes(location.hostname) || location.protocol === 'file:'
    ? 'http://192.168.4.1'
    : '';

export const sim = DEMO ? new Simulador() : null;

async function pedir(ruta, ms = 1500) {
  const ctrl = new AbortController();
  const t = setTimeout(() => ctrl.abort(), ms);
  try {
    const r = await fetch(BASE + ruta, { signal: ctrl.signal, cache: 'no-store' });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    return r;
  } finally {
    clearTimeout(t);
  }
}

// Comandos (en modo demo van al simulador)
export const comando = {
  modo: (m) => (sim ? sim.modo(m) : pedir('/modo?m=' + m).catch(() => {})),
  manual: (izq, der) => (sim ? sim.manual(izq, der) : pedir(`/manual?izq=${izq}&der=${der}`, 600).catch(() => {})),
  servo: (ang) => (sim ? sim.servo(ang) : pedir('/servo?ang=' + ang, 600).catch(() => {})),
  yaw0: () => (sim ? sim.yaw0() : pedir('/imu?yaw0=1').catch(() => {})),
};

// Último cuadro de la cámara como Blob (en demo no se usa: el simulador dibuja la escena)
export async function pedirFoto() {
  const r = await pedir('/foto.jpg?t=' + Date.now(), 2000);
  return r.blob();
}

export const HISTORIA_S = 20;   // segundos que muestran las gráficas
const MAX_MUESTRAS = 400;

// Estado de conexión: 'conectando' | 'conectado' | 'sin-conexion'
export function useCarro() {
  const [estado, setEstado] = useState(null);
  const [conexion, setConexion] = useState('conectando');
  const [latencia, setLatencia] = useState(null);
  const [version, setVersion] = useState(0);         // sube con cada muestra nueva (redibuja gráficas)
  const historia = useRef([]);                       // [{t, ax, ay, az, yaw, pitch, roll}]
  const ultimoOk = useRef(0);

  useEffect(() => {
    let vivo = true;
    const inicio = performance.now();
    async function bucle() {
      while (vivo) {
        const t0 = performance.now();
        try {
          const s = sim ? sim.estado() : await (await pedir('/estado')).json();
          if (!vivo) return;
          const ahora = performance.now();
          ultimoOk.current = ahora;
          setLatencia(Math.round(ahora - t0));
          setEstado(s);
          setConexion('conectado');
          if (s.imu) {
            const h = historia.current;
            h.push({ t: Date.now(), ...s.imu });
            if (h.length > MAX_MUESTRAS) h.splice(0, h.length - MAX_MUESTRAS);
          }
          setVersion((v) => v + 1);
          await esperar(Math.max(0, 100 - (performance.now() - t0)));   // ~10 por segundo
        } catch {
          if (!vivo) return;
          // Reintenta siempre; tras 5 s sin respuesta pasa de "conectando" a "sin conexión"
          const desde = ultimoOk.current || inicio;
          setConexion(performance.now() - desde < 5000 ? 'conectando' : 'sin-conexion');
          await esperar(500);
        }
      }
    }
    bucle();
    return () => { vivo = false; };
  }, []);

  return { estado, conexion, latencia, historia, version };
}

export const esperar = (ms) => new Promise((r) => setTimeout(r, ms));
