// demo.js — Carro simulado para probar el HMI sin hardware (abrir con ?demo)
// Entrega el mismo JSON que /estado de la ESP32 y dibuja una escena falsa de cámara.

const FOV = 60;
const norm = (a) => { while (a > 180) a -= 360; while (a < -180) a += 360; return a; };
const lim = (v, a, b) => Math.max(a, Math.min(b, v));
const ruido = (a) => (Math.random() - 0.5) * 2 * a;

export class Simulador {
  constructor() {
    this.t0 = performance.now();
    this.tPrev = this.t0;
    this.control = 'manual';
    this.rumbo = 0;               // grados, + = izquierda
    this.balon = { ang: 70, dist: 160 };   // respecto al rumbo inicial
    this.servoAng = 90;
    this.izq = 0; this.der = 0;
    this.tManual = 0; this.mIzq = 0; this.mDer = 0;
    this.yawOff = 0;
    this.velPrev = 0;
    this.ev = 0;
    this.auto = 'apagado';
    this.tFase = 0;
    this.paso = 0;
    this.sentido = 1;
    this.perfil = [];
    this.msg = 'Modo manual';
    this.fijado = false;
    this.escaneos = 0;
    this.tVisto = 0;
  }
  modo(m) {
    const a = m === 'auto';
    if (a && this.control !== 'auto') { this.control = 'auto'; this.iniciarEscaneo(); this.msg = 'Escaneo inicial'; this.escaneos = 1; }
    if (!a) { this.control = 'manual'; this.auto = 'apagado'; this.fijado = false; this.msg = 'Modo manual'; this.servoAng = 90; }
  }
  manual(izq, der) { if (this.control === 'manual') { this.mIzq = izq; this.mDer = der; this.tManual = performance.now(); } }
  servo(a) { if (this.control === 'manual') this.servoAng = lim(+a, 0, 180); }
  yaw0() { this.yawOff = this.rumbo; }

  iniciarEscaneo() {
    this.auto = 'escaneando'; this.tFase = performance.now(); this.paso = 0;
    this.perfil = [];
    this.fijado = false; this.msg = 'Escaneando el área';
  }
  // Dirección del balón respecto a donde mira la cámara (+ = izquierda)
  relCamara() { return norm(this.balon.ang - this.rumbo - (this.servoAng - 90)); }
  visible() { return Math.abs(this.relCamara()) < FOV / 2 - 2 && this.balon.dist < 260; }
  distEn(servo) {   // "paredes" falsas
    const dir = norm(this.rumbo + servo - 90);
    return Math.round(70 + 55 * Math.sin(dir / 25) + 25 * Math.cos(dir / 9));
  }

  paso10Hz(ahora) {
    const ms = ahora - this.tFase;
    let izq = 0, der = 0;
    if (this.control === 'manual') {
      if (ahora - this.tManual < 600) { izq = this.mIzq; der = this.mDer; }
    } else {
      const vis = this.visible();
      if (vis) this.tVisto = ahora;
      switch (this.auto) {
        case 'escaneando': {
          const angs = [0, 30, 60, 90, 120, 150, 180];
          const lista = this.sentido > 0 ? angs : [...angs].reverse();
          const i = Math.min(lista.length - 1, Math.floor(ms / 850));
          this.servoAng = lista[i];
          this.perfil[i] = [lista[i], this.distEn(lista[i])];
          if (vis && ms % 850 > 350) { this.auto = 'alineando'; this.tFase = ahora; this.msg = 'Objeto detectado: alineando'; this.objAng = this.servoAng; break; }
          if (ms > 850 * lista.length) { this.sentido *= -1; this.auto = 'avanzando'; this.tFase = ahora; this.msg = 'Sin objeto: avanzando'; }
          break;
        }
        case 'alineando': {
          this.servoAng = 90;
          const rel = norm(this.balon.ang - this.rumbo);
          if (Math.abs(rel) > 8) { const g = rel > 0 ? -120 : 120; izq = g; der = -g; }
          else { this.auto = 'confirmando'; this.tFase = ahora; }
          break;
        }
        case 'confirmando':
          this.servoAng = 90;
          if (ms > 700) {
            if (vis) { this.auto = 'siguiendo'; this.tFase = ahora; this.fijado = true; this.ev++; this.msg = 'Objeto fijado con éxito'; }
            else { this.escaneos++; this.iniciarEscaneo(); }
          }
          break;
        case 'siguiendo': {
          this.servoAng = 90;
          const rel = this.relCamara();
          const giro = lim(-rel * 4, -130, 130);
          const avance = this.balon.dist > 30 && Math.abs(rel) < 15 ? 130 : 0;
          izq = avance + giro; der = avance - giro;   // balón a la izquierda (rel > 0) => rueda derecha más rápida
          if (ahora - this.tVisto > 3000) { this.msg = 'Objeto perdido: escaneando de nuevo'; this.escaneos++; this.iniciarEscaneo(); }
          break;
        }
        case 'avanzando':
          this.servoAng = 90;
          if (vis && ms > 350) { this.auto = 'siguiendo'; this.fijado = true; this.ev++; this.msg = 'Objeto fijado con éxito'; this.tFase = ahora; break; }
          if (ms < 3350) { izq = der = 130; } else { this.escaneos++; this.iniciarEscaneo(); this.msg = 'Fin del avance: escaneando'; }
          break;
      }
    }
    this.izq = Math.round(izq); this.der = Math.round(der);
  }

  avanzar(ahora) {
    const dt = Math.min(0.2, (ahora - this.tPrev) / 1000);
    this.tPrev = ahora;
    const giro = (this.der - this.izq) / 2;          // + = izquierda
    const av = (this.izq + this.der) / 2;
    this.rumbo = norm(this.rumbo + (giro / 120) * 160 * dt);
    const vel = (av / 130) * 30;                     // cm/s
    // El balón se acerca si el carro avanza mirando hacia él; y se mueve solo, despacio
    const rel = norm(this.balon.ang - this.rumbo);
    if (vel > 0) this.balon.dist = Math.max(20, this.balon.dist - vel * dt * Math.cos(rel * Math.PI / 180));
    if (vel < 0) this.balon.dist = Math.min(300, this.balon.dist - vel * dt);
    this.balon.ang = norm(this.balon.ang + Math.sin(ahora / 3000) * 6 * dt);
    if (this.balon.dist < 40 && this.control === 'manual') this.balon.dist += 0.5;
    this.acc = (vel - this.velPrev) / Math.max(dt, 0.01) / 100;   // m/s²
    this.velPrev = vel;
    this.vel = vel;
    this.giroRad = (giro / 120) * 160 * Math.PI / 180;
  }

  estado() {
    const ahora = performance.now();
    this.avanzar(ahora);
    this.paso10Hz(ahora);
    const vis = this.visible();
    const rel = this.relCamara();
    const blobs = [], mapa = new Array(192).fill(0);
    let obj = null;
    if (vis) {
      const cx = 0.5 - rel / FOV, cy = 0.62;
      const area = Math.round(lim(900 / this.balon.dist, 1, 50));
      blobs.push([+cx.toFixed(3), cy, area, 0.92]);
      obj = [cx, cy, area];
      const r = Math.sqrt(area) / 2;
      for (let i = 0; i < 12; i++) for (let j = 0; j < 16; j++) {
        const d = Math.hypot(j + 0.5 - cx * 16, i + 0.5 - cy * 12);
        if (d < r + 1) mapa[i * 16 + j] = Math.round(lim(100 * (1.2 - d / (r + 1)), 0, 100));
      }
    }
    // Reflejo rojo falso de vez en cuando (blob sin elegir)
    if (Math.sin(ahora / 1700) > 0.7) blobs.push([0.15, 0.3, 2, 0.4]);
    const dist = this.servoAng === 90 && vis && Math.abs(rel) < 8 ? Math.round(this.balon.dist) : this.distEn(this.servoAng);
    const pwmRpm = (p) => (Math.abs(p) <= 60 ? 0 : Math.sign(p) * 200 * (Math.abs(p) - 60) / 195);
    const ri = pwmRpm(this.izq), rd = pwmRpm(this.der);
    const s = ahora / 1000;
    return {
      modo: 'modelo', modelo: true, prueba: 1,
      control: this.control, auto: this.auto, fijado: this.fijado, ev_fijado: this.ev, msg: this.msg,
      servo: Math.round(this.servoAng), frente: Math.abs(this.servoAng - 90) <= 25, obstaculo: dist < 15 && this.izq + this.der > 0,
      bloqueado: false, escaneos: this.escaneos, obj_ang: this.objAng ?? 90,
      avance_ms: 3000, ms_grado: 6, paso: 30, perfil: this.perfil.filter(Boolean),
      rpm_izq: Math.round(ri), rpm_der: Math.round(rd), rpm_max: 200, vel_cms: +((ri + rd) / 2 * Math.PI * 6.5 / 60).toFixed(1),
      dist_freno: 15, dist_libre: 30, servo_izq_mayor: 1,
      imu: {
        ax: +((this.acc || 0) + ruido(0.25)).toFixed(2),
        ay: +((this.vel || 0) / 100 * (this.giroRad || 0) * 3 + ruido(0.25) + 0.3 * Math.sin(s * 0.7)).toFixed(2),
        az: +(9.81 + ruido(0.3)).toFixed(2),
        yaw: +norm(this.rumbo - this.yawOff).toFixed(1),
        pitch: +(1.5 * Math.sin(s * 0.9) + ruido(0.4) - (this.acc || 0) * 0.6).toFixed(1),
        roll: +(2 * Math.sin(s * 0.6) + ruido(0.4) + (this.giroRad || 0) * 2).toFixed(1),
      },
      umbral: 0.5, delta: 60, area: 14, vel: 140, kp: 120, kd: 12,
      estado: this.auto === 'siguiendo' ? 'siguiendo' : 'esperando', izq: this.izq, der: this.der, dist, uno: true,
      fps: 12 + ruido(0.8), det_s: 6.5 + ruido(0.5), ms: 92 + ruido(5),
      obj, elegido: vis ? 0 : -1, blobs, mapa,
    };
  }

  // Escena falsa de cámara (suelo, horizonte, balón rojo)
  dibujar(g, w, h, imu) {
    const roll = (imu?.roll || 0) * Math.PI / 180;
    const horizonte = h * 0.42 + (imu?.pitch || 0) * 3;
    g.save();
    g.translate(w / 2, horizonte); g.rotate(-roll); g.translate(-w / 2, -horizonte);
    const cielo = g.createLinearGradient(0, -h, 0, horizonte);
    cielo.addColorStop(0, '#2b3a4a'); cielo.addColorStop(1, '#6d7f8f');
    g.fillStyle = cielo; g.fillRect(-w, -h, 3 * w, horizonte + h);
    const suelo = g.createLinearGradient(0, horizonte, 0, h * 1.5);
    suelo.addColorStop(0, '#5a5048'); suelo.addColorStop(1, '#2c2622');
    g.fillStyle = suelo; g.fillRect(-w, horizonte, 3 * w, h * 2);
    // Baldosas que se mueven con el rumbo
    g.strokeStyle = 'rgba(0,0,0,.25)'; g.lineWidth = 1;
    const desp = ((this.rumbo + this.servoAng - 90) / FOV) * w;
    for (let k = -20; k <= 20; k++) {
      const x = w / 2 + ((k * 80 + desp) % 1600);
      g.beginPath(); g.moveTo(w / 2 + (x - w / 2) * 0.08, horizonte); g.lineTo(x, h * 1.4); g.stroke();
    }
    for (let k = 1; k < 9; k++) { const y = horizonte + (h - horizonte) * (k * k) / 64; g.beginPath(); g.moveTo(-w, y); g.lineTo(2 * w, y); g.stroke(); }
    // Pared lejana
    g.fillStyle = '#4a5560'; g.fillRect(-w, horizonte - h * 0.12, 3 * w, h * 0.12);
    g.restore();
    if (this.visible()) {
      const rel = this.relCamara();
      const x = (0.5 - rel / FOV) * w, r = lim(2200 / this.balon.dist, 6, 90);
      const y = h * 0.62;
      const grad = g.createRadialGradient(x - r * 0.35, y - r * 0.35, r * 0.1, x, y, r);
      grad.addColorStop(0, '#ff8a80'); grad.addColorStop(0.5, '#e02020'); grad.addColorStop(1, '#6a0808');
      g.fillStyle = 'rgba(0,0,0,.35)'; g.beginPath(); g.ellipse(x, y + r * 0.95, r * 0.9, r * 0.22, 0, 0, Math.PI * 2); g.fill();
      g.fillStyle = grad; g.beginPath(); g.arc(x, y, r, 0, Math.PI * 2); g.fill();
    }
  }
}
