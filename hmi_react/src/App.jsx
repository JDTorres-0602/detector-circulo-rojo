// App.jsx — Cabina de mando en UNA pantalla horizontal (sin desplazarse)
//   arriba: estado del enlace
//   centro: telemetría + radar | video FPV (con el mensaje del carro debajo)
//   abajo:  modos · cruceta · cabeza (servo)
//   derecha: IMU (aceleraciones, yaw, pitch, roll) y relojes de velocidad / RPM
import { useEffect, useState } from 'react';
import { useCarro, comando } from './api.js';
import BarraEstado from './componentes/BarraEstado.jsx';
import Panel from './componentes/Panel.jsx';
import Video from './componentes/Video.jsx';
import { Distancia, Detalles, Radar } from './componentes/Telemetria.jsx';
import Modos from './componentes/Modos.jsx';
import Cruceta from './componentes/Cruceta.jsx';
import ServoSlider from './componentes/ServoSlider.jsx';
import GraficaLinea from './componentes/GraficaLinea.jsx';
import Gauge from './componentes/Gauge.jsx';

// Colores de las 3 aceleraciones: paleta categórica validada para daltonismo sobre fondo oscuro
const ACELERACIONES = [
  { k: 'ax', nombre: 'X', color: '#3987e5' },
  { k: 'ay', nombre: 'Y', color: '#d95926' },
  { k: 'az', nombre: 'Z', color: '#199e70' },
];
const YAW = [{ k: 'yaw', nombre: 'Yaw', color: '#4fd8ff' }];
const PITCH = [{ k: 'pitch', nombre: 'Pitch', color: '#4fd8ff' }];
const ROLL = [{ k: 'roll', nombre: 'Roll', color: '#4fd8ff' }];
const RANGO_YAW = [-180, 180];

export default function App() {
  const { estado: s, conexion, latencia, historia, version } = useCarro();
  const ok = conexion === 'conectado';
  const manual = ok && s?.control !== 'auto';
  const velMax = s ? Math.round((s.rpm_max * Math.PI * 6.5) / 60) : 70;

  return (
    <div className="app">
      <div className="fondo" aria-hidden="true" />
      <BarraEstado conexion={conexion} latencia={latencia} estado={s} />

      <main className="cuerpo">
        <div className="principal">
          <div className="vista">
            <Panel titulo="TELEMETRÍA" className="p-tele">
              <Distancia s={s} />
              <Detalles s={s} />
              <Radar s={s} />
            </Panel>

            <Panel titulo="VISIÓN FPV" className="p-video"
              extra={<span className="panel-extra">{s ? (s.control === 'auto' ? 'AUTÓNOMO · ' + s.auto.toUpperCase() : 'MANUAL') : '—'}</span>}>
              <div className="video-zona"><Video estado={s} conexion={conexion} /></div>
              <div className={'video-msg' + (s?.fijado ? ' ok' : '') + (s?.obstaculo ? ' alerta' : '')}>
                <span className="tele-k">MENSAJE</span> {s ? s.msg : 'Esperando datos del carro…'}
              </div>
            </Panel>
          </div>

          <Panel className="p-mando">
            <Modos estado={s} deshabilitado={!ok} />
            <Cruceta habilitado={manual} />
            <ServoSlider estado={s} habilitado={manual} />
          </Panel>
        </div>

        <aside className="lateral">
          <Panel titulo="ACELERACIÓN" className="p-acc">
            <GraficaLinea historia={historia} version={version} series={ACELERACIONES} unidad="m/s²" minAbs={12} decimales={2} />
          </Panel>

          <Panel titulo="ORIENTACIÓN" className="p-ori"
            extra={<button className="chip chip-mini" disabled={!ok} onClick={() => comando.yaw0()} title="El yaw sale del giroscopio y se desvía con el tiempo">YAW A 0</button>}>
            <Angulo nombre="YAW" valor={s?.imu?.yaw}>
              <GraficaLinea historia={historia} version={version} series={YAW} unidad="°" fijo={RANGO_YAW} />
            </Angulo>
            <Angulo nombre="PITCH" valor={s?.imu?.pitch}>
              <GraficaLinea historia={historia} version={version} series={PITCH} unidad="°" minAbs={10} />
            </Angulo>
            <Angulo nombre="ROLL" valor={s?.imu?.roll}>
              <GraficaLinea historia={historia} version={version} series={ROLL} unidad="°" minAbs={10} />
            </Angulo>
          </Panel>

          <Panel titulo="PROPULSIÓN" className="p-prop" extra={<span className="panel-extra tenue">ESTIMADA</span>}>
            <div className="relojes">
              <Gauge titulo="VELOCIDAD" valor={s?.vel_cms} max={velMax} unidad="cm/s" />
              <Gauge titulo="RPM" valor={s ? (Math.abs(s.rpm_izq) + Math.abs(s.rpm_der)) / 2 : 0} max={s?.rpm_max || 200}
                unidad="rpm" sub={s ? `I ${s.rpm_izq} · D ${s.rpm_der}` : ''} />
            </div>
          </Panel>
        </aside>
      </main>

      <AvisoGirar />
    </div>
  );
}

function Angulo({ nombre, valor, children }) {
  return (
    <div className="angulo">
      <div className="angulo-cab">
        <span className="mini-titulo">{nombre}</span>
        <span className="angulo-valor">{valor != null ? valor.toFixed(1) + '°' : '—'}</span>
      </div>
      <div className="angulo-grafica">{children}</div>
    </div>
  );
}

// En un celular vertical la cabina no cabe: se pide girarlo (se puede seguir igual)
function AvisoGirar() {
  const [cerrado, setCerrado] = useState(false);
  const [vertical, setVertical] = useState(false);
  useEffect(() => {
    const mq = window.matchMedia('(orientation: portrait) and (max-width: 760px)');
    const f = () => setVertical(mq.matches);
    f(); mq.addEventListener('change', f);
    return () => mq.removeEventListener('change', f);
  }, []);
  if (!vertical || cerrado) return null;
  return (
    <div className="girar">
      <div className="girar-icono">⟲</div>
      <div className="girar-texto">GIRA EL CELULAR</div>
      <div className="nota">La cabina está pensada para usarse en horizontal.</div>
      <button className="chip" onClick={() => setCerrado(true)}>CONTINUAR EN VERTICAL</button>
    </div>
  );
}
