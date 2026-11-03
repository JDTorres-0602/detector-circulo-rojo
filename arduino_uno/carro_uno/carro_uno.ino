// =====================================================================
//  carro_uno.ino — Arduino UNO del ELEGOO Smart Robot Car V4.0
// =====================================================================
//  El UNO NO piensa: solo obedece a la ESP32 y le cuenta lo que mide.
//  Recibe órdenes de texto por el puerto serie (pines 0/1, donde se
//  conecta el módulo de cámara) y maneja motores, servo y ultrasonido.
//
//  Órdenes (una por línea, terminadas en '\n'):
//    M izq der   velocidades -255..255 (negativo = hacia atrás)
//    S           parar
//    W ms        watchdog: si no llega una "M" en ese tiempo, para (0 = apagado)
//    D cm        freno: no avanzar si el ultrasonido ve algo más cerca (0 = apagado)
//    T ms        cada cuánto enviar la distancia (0 = no enviar)
//    V grados    servo de la cabeza (90 = al frente)
//  Respuesta periódica:
//    U cm        distancia del ultrasonido (0 = sin eco)
//
//  Para probarlo SIN la ESP32: Monitor Serie a 9600 baudios, "Nueva línea",
//  y escribe por ejemplo "W 0" y luego "M 120 120".
//
//  OJO al subir el programa: los pines 0/1 los comparte la cámara. Pon el
//  interruptor de la placa en "Upload" (o desconecta el cable de la cámara)
//  y vuélvelo a "Cam" al terminar.
// =====================================================================
#include <Servo.h>

// ---------------- Pines del escudo ELEGOO V4.0 (driver TB6612) ----------------
const int PIN_STBY = 3;     // habilita el driver
const int PIN_PWMA = 5;     // velocidad motores A (lado DERECHO)
const int PIN_AIN1 = 7;     // dirección motores A
const int PIN_PWMB = 6;     // velocidad motores B (lado IZQUIERDO)
const int PIN_BIN1 = 8;     // dirección motores B
const int PIN_TRIG = 13;    // ultrasonido HC-SR04
const int PIN_ECHO = 12;
const int PIN_SERVO = 10;

// Si al mandar "M 120 0" gira la rueda DERECHA en vez de la izquierda, pon 1 aquí.
#define INTERCAMBIAR_LADOS 0
// Si un lado va hacia atrás cuando debería ir hacia adelante, invierte su sentido.
#define INVERTIR_IZQ 0
#define INVERTIR_DER 0

Servo servo;
unsigned long t_ultima_m = 0, t_ultima_tele = 0;
unsigned long watchdog_ms = 600;
int freno_cm = 0, tele_ms = 0;
int vel_izq = 0, vel_der = 0;
int dist_cm = 0;

void motor(int pin_pwm, int pin_dir, int v, bool invertir) {
  if (invertir) v = -v;
  digitalWrite(pin_dir, v >= 0 ? HIGH : LOW);   // HIGH = adelante en el escudo ELEGOO
  analogWrite(pin_pwm, constrain(abs(v), 0, 255));
}

void aplicar_motores() {
  int izq = vel_izq, der = vel_der;
  // Freno de seguridad: si hay algo muy cerca, no se permite avanzar (sí girar o retroceder)
  if (freno_cm > 0 && dist_cm > 0 && dist_cm < freno_cm) {
    if (izq > 0 && der > 0) izq = der = 0;
  }
#if INTERCAMBIAR_LADOS
  int t = izq; izq = der; der = t;
#endif
  digitalWrite(PIN_STBY, (izq || der) ? HIGH : LOW);
  motor(PIN_PWMB, PIN_BIN1, izq, INVERTIR_IZQ);
  motor(PIN_PWMA, PIN_AIN1, der, INVERTIR_DER);
}

int medir_cm() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long us = pulseIn(PIN_ECHO, HIGH, 20000UL);   // máx. 20 ms (~3.4 m)
  return us == 0 ? 0 : (int)(us / 58);
}

void procesar(char* l) {
  char c = l[0];
  char* resto = l + 1;
  if (c == 'M') {
    char* i = strtok(resto, " ");
    if (!i) return;
    vel_izq = constrain(atoi(i), -255, 255);
    char* d = strtok(NULL, " ");
    vel_der = d ? constrain(atoi(d), -255, 255) : vel_izq;
    t_ultima_m = millis();
  } else if (c == 'S') {
    vel_izq = vel_der = 0;
  } else if (c == 'W') {
    watchdog_ms = atol(resto);
  } else if (c == 'D') {
    freno_cm = atoi(resto);
  } else if (c == 'T') {
    tele_ms = atoi(resto);
  } else if (c == 'V') {
    servo.write(constrain(atoi(resto), 0, 180));
  }
}

void setup() {
  pinMode(PIN_STBY, OUTPUT);
  pinMode(PIN_PWMA, OUTPUT); pinMode(PIN_AIN1, OUTPUT);
  pinMode(PIN_PWMB, OUTPUT); pinMode(PIN_BIN1, OUTPUT);
  pinMode(PIN_TRIG, OUTPUT); pinMode(PIN_ECHO, INPUT);
  servo.attach(PIN_SERVO);
  servo.write(90);
  Serial.begin(9600);
  Serial.println(F("carro_uno listo"));
}

void loop() {
  // 1) Leer órdenes (sin bloquear: se arma la línea carácter por carácter)
  static char linea[32];
  static int n = 0;
  while (Serial.available()) {
    char ch = Serial.read();
    if (ch == '\r') continue;
    if (ch == '\n') { linea[n] = 0; if (n) procesar(linea); n = 0; }
    else if (n < (int)sizeof(linea) - 1) linea[n++] = ch;
  }
  // 2) Watchdog: si la ESP32 deja de mandar órdenes (se colgó, se cayó el WiFi...), parar
  if (watchdog_ms && millis() - t_ultima_m > watchdog_ms) vel_izq = vel_der = 0;

  // 3) Medir distancia (si hay freno o telemetría) y avisar a la ESP32
  unsigned long periodo = tele_ms ? (unsigned long)tele_ms : 100;
  if ((tele_ms || freno_cm) && millis() - t_ultima_tele >= periodo) {
    t_ultima_tele = millis();
    dist_cm = medir_cm();
    if (tele_ms) {
      Serial.print(F("U "));
      Serial.println(dist_cm);
    }
  }
  aplicar_motores();
}
