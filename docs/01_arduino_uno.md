# Etapa 1 — Arduino UNO y motores

**Objetivo:** cargar `arduino_uno/carro_uno/carro_uno.ino` y mover el carro escribiendo órdenes a mano. Así entiendes el "idioma" que después hablará la ESP32.

## El reparto de trabajo

```
 ESP32 (cámara)  ──"M 120 80\n"──►  Arduino UNO  ──PWM──► driver TB6612 ──► 4 motores
                 ◄──"U 57\n"─────                ◄──eco── ultrasonido HC-SR04
```

La ESP32 **decide** y el UNO **obedece**. Se hablan por el puerto serie (pines 0 y 1 del UNO, 9600 baudios) con texto simple: una orden por línea.

| Orden | Significado |
|---|---|
| `M izq der` | velocidad de cada lado, de −255 a 255 (negativo = atrás) |
| `S` | parar |
| `W ms` | *watchdog*: si no llega ninguna `M` en ese tiempo, el UNO para solo (0 = desactivado) |
| `D cm` | freno: no avanzar si el ultrasonido ve algo más cerca |
| `T ms` | cada cuánto el UNO responde `U distancia` |
| `V grados` | servo de la cabeza (90 = al frente) |

## Pasos

1. Abre `carro_uno.ino` en el Arduino IDE.
2. **Importante:** los pines 0/1 los comparte el módulo de cámara. Antes de subir, pon el interruptor del escudo en la posición de carga (*upload*) o desconecta el cable de la cámara. Si no, la carga falla con `avrdude: stk500_getsync()`.
3. Sube el programa. Vuelve el interruptor a su posición normal.
4. **Levanta el carro** para que las ruedas no toquen la mesa. Abre el *Monitor Serie* a **9600** baudios con fin de línea **"Nueva línea"**.
5. Debe aparecer `carro_uno listo (IMU OK)` (o `(sin IMU)` si el escudo no tiene el MPU6050). Deja el carro **quieto** al encender: los primeros segundos se calibra el giroscopio. Escribe, una por una:

```
W 0
M 120 120
S
M 120 0
M 0 120
T 500
```

## ✅ Cómo saber que funcionó

- Con `M 120 120` las cuatro ruedas giran **hacia adelante**.
- Con `M 120 0` giran solo las **izquierdas**. Si giran las derechas, pon `INTERCAMBIAR_LADOS 1` al inicio del `.ino`. Si un lado gira hacia atrás, usa `INVERTIR_IZQ` o `INVERTIR_DER`.
- Con `T 500` aparece `U 34` (o el número que corresponda) dos veces por segundo. Acerca la mano al sensor y mira cómo cambia. Si hay IMU, también aparece `I ax ay az yaw pitch roll` (aceleraciones en milésimas de g, ángulos en décimas de grado). `V 150` mueve la cabeza; `Y` pone el yaw en 0.
- El freno del UNO (`D cm`) solo actúa con la cabeza mirando al frente (servo a ±25° de 90): si mira a un lado, la distancia no es la del camino. Cuando frena, avisa con `F`.
- Con `W 600` (lo que manda la ESP32 al arrancar) y luego `M 120 120`, las ruedas giran solo **0.6 s** y paran. Ese es el watchdog funcionando.

## 🧠 Por qué funciona así

- **Watchdog:** si la ESP32 se reinicia o se cae el WiFi en plena marcha, el carro **para solo**. Es la regla de seguridad más importante de un robot: ante la duda, quieto.
- **PWM:** `analogWrite(pin, 120)` no entrega "medio voltaje": prende y apaga el motor muy rápido, encendido 120/255 del tiempo. Por debajo de ~70 los motores no alcanzan a vencer la fricción y solo zumban. Por eso `seguidor.c` tiene `pwm_min`.
- **Lectura sin bloquear:** `loop()` arma la línea carácter por carácter en vez de usar `Serial.readStringUntil`, que se queda esperando y congelaría el watchdog.

## ✍️ Ejercicios

1. Encuentra el `pwm_min` de **tu** carro: prueba `M 50 50`, `M 60 60`, `M 70 70`... sobre el piso. ¿Desde qué valor arranca? Anótalo, lo usarás en la etapa 6.
2. En el código, ¿qué pasa si mandas `D 20` y luego `M 120 120` con la mano a 10 cm del sensor? ¿Y con `M -120 -120`? ¿Por qué el freno no bloquea la marcha atrás?
3. Reto: agrega una orden `B` que haga sonar el buzzer del escudo (busca su pin en la documentación de ELEGOO).

**Siguiente:** [Etapa 2 — Firmware de la ESP32 en modo COLOR](02_firmware_modo_color.md)
