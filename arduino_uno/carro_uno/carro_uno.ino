// =====================================================================
//  carro_uno.ino — Arduino UNO del carro: motores, servo y ultrasonido
// =====================================================================
//  El UNO NO decide nada: obedece a la ESP32 por serial (9600 baudios) y
//  le devuelve la distancia. Lo único que decide por su cuenta es la
//  SEGURIDAD (segunda capa, por si la ESP32 se cuelga o se equivoca):
//    - Watchdog: si no llega "M" en W ms, para los motores.
//    - Freno: si el ultrasonido mira al frente y hay algo a menos de D cm,
//      no deja avanzar (sí deja girar sobre su eje y retroceder).
//
//  Protocolo (una orden por línea, igual que uno.h de la ESP32):
//    ESP32 -> UNO:  M izq der   velocidades -255..255
//                   S           parar
//                   W ms        watchdog
//                   D cm        distancia de freno
//                   T ms        cada cuánto mandar la distancia
//                   V grados    servo de la cabeza (90 = al frente)
//                   Y           pone el yaw de la IMU en 0
//    UNO -> ESP32:  U cm        distancia (0 = sin eco)
//                   I ax ay az yaw pitch roll   IMU: aceleraciones en milésimas de g,
//                               ángulos en décimas de grado (solo si hay IMU)
//                   F           aviso: frenó el avance por un obstáculo
//
//  Pines: placa ELEGOO Smart Robot Car V4.0 (driver TB6612). Si tu
//  carro es otro, cambia solo esta sección.
//
//  IMPORTANTE al subir este programa: la ESP32 usa los pines 0/1 (Serial)
//  del UNO. Desconecta el cable de la cámara (o pon el interruptor del
//  shield en "Upload") mientras subes el código, y vuelve a conectarlo.
// =====================================================================
#include <Servo.h>
#include <Wire.h>

// ---------------- Pines ----------------
const uint8_t PIN_STBY  = 3;    // habilita el driver de motores
const uint8_t PIN_PWMA  = 5;    // motor A = lado DERECHO
const uint8_t PIN_AIN1  = 7;
const uint8_t PIN_PWMB  = 6;    // motor B = lado IZQUIERDO
const uint8_t PIN_BIN1  = 8;
const uint8_t PIN_SERVO = 10;   // servo de la cabeza (cámara + ultrasonido)
const uint8_t PIN_TRIG  = 13;
const uint8_t PIN_ECHO  = 12;

// Prueba: manda "M 150 150" (o usa la flecha ▲ de la página). Si un lado gira hacia atrás,
// cambia su INVERTIR. Si con "M 120 0" gira la rueda DERECHA en vez de la izquierda,
// los lados están cruzados: pon INTERCAMBIAR_LADOS en true.
const bool INVERTIR_IZQ = false;
const bool INVERTIR_DER = false;
const bool INTERCAMBIAR_LADOS = false;

// El ultrasonido solo vigila el frente con el servo cerca de 90° y ya quieto
const int FRENTE_TOLERANCIA = 25;          // mismo valor que config.h de la ESP32
const unsigned long SERVO_ASENTAR_MS = 350;

// IMU MPU6050 (I2C: A4 = SDA, A5 = SCL). Se calibra al encender: deja el carro QUIETO
// los primeros 2 segundos. Si la IMU está montada girada, cambia los signos aquí.
const uint8_t MPU_DIR = 0x68;
const int SIGNO_PITCH = 1, SIGNO_ROLL = 1, SIGNO_YAW = 1;

// ---------------- Estado ----------------
Servo cabeza;
unsigned long watchdog_ms = 600;
int freno_cm = 15;
unsigned long telemetria_ms = 100;

int pedido_izq = 0, pedido_der = 0;        // lo último que pidió la ESP32
unsigned long t_ultimo_M = 0;
int servo_ang = 90;
unsigned long t_servo = 0;
int dist_cm = 0;                            // mediana de las 3 últimas lecturas (0 = sin eco)
int lecturas[3] = {0, 0, 0};
uint8_t i_lectura = 0;
unsigned long t_medida = 0, t_telemetria = 0, t_imu = 0;
bool frenando = false;

bool imu_ok = false;
float gyro_bias[3] = {0, 0, 0};
float acc_g[3] = {0, 0, 1};          // aceleración filtrada (g)
float yaw = 0, pitch = 0, roll = 0;  // grados
unsigned long t_imu_us = 0;

char linea[24];
uint8_t n_linea = 0;

// ---------------- Motores ----------------
void motor(uint8_t pin_pwm, uint8_t pin_dir, int v, bool invertir) {
  if (invertir) v = -v;
  v = constrain(v, -255, 255);
  digitalWrite(pin_dir, v >= 0 ? HIGH : LOW);
  analogWrite(pin_pwm, abs(v));
}

bool frente_vigilado() {
  return abs(servo_ang - 90) <= FRENTE_TOLERANCIA && millis() - t_servo >= SERVO_ASENTAR_MS;
}

// Aplica el pedido de la ESP32 pasando por el freno del ultrasonido
void aplicar_motores() {
  int izq = pedido_izq, der = pedido_der;
  bool freno = false;
  if (frente_vigilado() && dist_cm > 0 && dist_cm < freno_cm) {
    int avance = (izq + der) / 2;
    if (avance > 0) {                 // quita solo la parte "hacia adelante"
      izq -= avance;
      der -= avance;
      if (abs(izq) < 60) izq = der = 0;
      freno = true;
    }
  }
  if (freno && !frenando) Serial.println("F");
  frenando = freno;
  if (INTERCAMBIAR_LADOS) { int t = izq; izq = der; der = t; }
  motor(PIN_PWMB, PIN_BIN1, izq, INVERTIR_IZQ);
  motor(PIN_PWMA, PIN_AIN1, der, INVERTIR_DER);
}

// ---------------- Ultrasonido ----------------
int medir_cm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long us = pulseIn(PIN_ECHO, HIGH, 25000UL);   // máx. ~4 m
  return us == 0 ? 0 : (int)(us / 58);
}

int mediana3(int a, int b, int c) {
  // Las lecturas "sin eco" (0) cuentan como muy lejos
  long x = a ? a : 9999, y = b ? b : 9999, z = c ? c : 9999;
  long m = max(min(x, y), min(max(x, y), z));
  return m >= 9999 ? 0 : (int)m;
}

// ---------------- IMU (MPU6050, sin librerías) ----------------
bool mpu_escribir(uint8_t reg, uint8_t valor) {
  Wire.beginTransmission(MPU_DIR);
  Wire.write(reg);
  Wire.write(valor);
  return Wire.endTransmission() == 0;
}

// d = ax, ay, az, temperatura, gx, gy, gz (valores crudos)
bool mpu_leer(int16_t* d) {
  Wire.beginTransmission(MPU_DIR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(MPU_DIR, (uint8_t)14) != 14) return false;
  for (uint8_t i = 0; i < 7; i++) {
    int16_t alto = Wire.read();
    d[i] = (alto << 8) | Wire.read();
  }
  return true;
}

void imu_iniciar() {
  Wire.begin();
  Wire.setClock(400000);
#if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(3000, true);   // si el bus I2C se cuelga, no bloquea el carro
#endif
  if (!mpu_escribir(0x6B, 0x00)) return;   // despertar; si no responde, no hay IMU
  mpu_escribir(0x1A, 0x03);                 // filtro pasa-bajos ~44 Hz
  mpu_escribir(0x1B, 0x00);                 // giroscopio ±250 °/s  (131 por °/s)
  mpu_escribir(0x1C, 0x00);                 // acelerómetro ±2 g    (16384 por g)
  delay(100);
  // Calibración del giroscopio: promedio con el carro quieto
  long suma[3] = {0, 0, 0};
  int n = 0;
  int16_t d[7];
  for (int i = 0; i < 300; i++) {
    if (mpu_leer(d)) {
      suma[0] += d[4]; suma[1] += d[5]; suma[2] += d[6];
      n++;
    }
    delay(3);
  }
  if (n < 100) return;
  for (int k = 0; k < 3; k++) gyro_bias[k] = (float)suma[k] / n;
  if (mpu_leer(d)) {                        // ángulos iniciales desde la gravedad
    float ax = d[0] / 16384.0, ay = d[1] / 16384.0, az = d[2] / 16384.0;
    pitch = atan2(-ax, sqrt(ay * ay + az * az)) * 57.2958;
    roll = atan2(ay, az) * 57.2958;
  }
  imu_ok = true;
  t_imu_us = micros();
}

void imu_actualizar() {
  int16_t d[7];
  if (!mpu_leer(d)) return;
  unsigned long ahora = micros();
  float dt = (ahora - t_imu_us) / 1e6;
  t_imu_us = ahora;
  if (dt <= 0 || dt > 0.2) return;          // pausa larga: no integrar basura

  float ax = d[0] / 16384.0, ay = d[1] / 16384.0, az = d[2] / 16384.0;
  float gx = (d[4] - gyro_bias[0]) / 131.0;
  float gy = (d[5] - gyro_bias[1]) / 131.0;
  float gz = (d[6] - gyro_bias[2]) / 131.0;
  acc_g[0] += 0.3 * (ax - acc_g[0]);
  acc_g[1] += 0.3 * (ay - acc_g[1]);
  acc_g[2] += 0.3 * (az - acc_g[2]);

  // Filtro complementario: el giroscopio es rápido pero se desvía, la gravedad es lenta pero estable
  float pitch_acc = atan2(-ax, sqrt(ay * ay + az * az)) * 57.2958;
  float roll_acc = atan2(ay, az) * 57.2958;
  pitch = 0.98 * (pitch + gy * dt) + 0.02 * pitch_acc;
  roll = 0.98 * (roll + gx * dt) + 0.02 * roll_acc;
  // El yaw solo sale del giroscopio (no hay brújula): se desvía con el tiempo. "Y" lo pone en 0.
  if (fabs(gz) > 0.4) yaw += gz * dt;
  if (yaw > 180) yaw -= 360;
  if (yaw < -180) yaw += 360;
}

void enviar_imu() {
  Serial.print("I ");
  Serial.print((int)(acc_g[0] * 1000)); Serial.print(' ');
  Serial.print((int)(acc_g[1] * 1000)); Serial.print(' ');
  Serial.print((int)(acc_g[2] * 1000)); Serial.print(' ');
  Serial.print((int)(SIGNO_YAW * yaw * 10)); Serial.print(' ');
  Serial.print((int)(SIGNO_PITCH * pitch * 10)); Serial.print(' ');
  Serial.println((int)(SIGNO_ROLL * roll * 10));
}

// ---------------- Órdenes de la ESP32 ----------------
void procesar(char* l) {
  char orden = l[0];
  char* p = l + 1;
  long a = strtol(p, &p, 10);
  long b = strtol(p, &p, 10);
  switch (orden) {
    case 'M':
      pedido_izq = constrain(a, -255, 255);
      pedido_der = constrain(b, -255, 255);
      t_ultimo_M = millis();
      break;
    case 'S':
      pedido_izq = pedido_der = 0;
      break;
    // W 0 apaga el watchdog y T 0 la telemetría (útil para probar a mano desde el Monitor Serie)
    case 'W': watchdog_ms = a == 0 ? 0 : constrain(a, 100, 5000); break;
    case 'Y': yaw = 0; break;
    case 'D': freno_cm = constrain(a, 0, 200); break;
    case 'T': telemetria_ms = a == 0 ? 0 : constrain(a, 50, 2000); break;
    case 'V': {
      int g = constrain(a, 0, 180);
      if (g != servo_ang) {
        servo_ang = g;
        cabeza.write(g);
        t_servo = millis();
      }
      break;
    }
  }
}

void leer_serial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      linea[n_linea] = 0;
      if (n_linea) procesar(linea);
      n_linea = 0;
    } else if (n_linea < sizeof(linea) - 1) {
      linea[n_linea++] = c;
    }
  }
}

// ---------------- Programa ----------------
void setup() {
  pinMode(PIN_STBY, OUTPUT);
  pinMode(PIN_PWMA, OUTPUT);
  pinMode(PIN_AIN1, OUTPUT);
  pinMode(PIN_PWMB, OUTPUT);
  pinMode(PIN_BIN1, OUTPUT);
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  digitalWrite(PIN_STBY, HIGH);
  motor(PIN_PWMA, PIN_AIN1, 0, false);
  motor(PIN_PWMB, PIN_BIN1, 0, false);

  cabeza.attach(PIN_SERVO, 500, 2400);
  cabeza.write(90);
  t_servo = millis();

  Serial.begin(9600);
  imu_iniciar();
  Serial.println(imu_ok ? F("carro_uno listo (IMU OK)") : F("carro_uno listo (sin IMU)"));
}

void loop() {
  unsigned long t = millis();
  leer_serial();

  // Watchdog: sin órdenes de la ESP32 => parar
  if (watchdog_ms && t - t_ultimo_M > watchdog_ms) pedido_izq = pedido_der = 0;

  // Ultrasonido cada 50 ms (menos se confunde con ecos del disparo anterior)
  if (t - t_medida >= 50) {
    t_medida = t;
    lecturas[i_lectura] = medir_cm();
    i_lectura = (i_lectura + 1) % 3;
    dist_cm = mediana3(lecturas[0], lecturas[1], lecturas[2]);
  }

  if (imu_ok && t - t_imu >= 10) {          // IMU a ~100 Hz
    t_imu = t;
    imu_actualizar();
  }

  aplicar_motores();

  if (telemetria_ms && t - t_telemetria >= telemetria_ms) {
    t_telemetria = t;
    Serial.print("U ");
    Serial.println(dist_cm);
    if (imu_ok) enviar_imu();
  }
}
