# Etapa 6 — Seguir el balón y ajustar

**Objetivo:** entender cómo el carro convierte "el balón está en x = 0.7" en velocidades de motor, afinar el control y aprender el **ciclo de mejora** que se repite en todo proyecto de ML.

## 🧠 La máquina de estados (`seguidor.c`)

```
             activar                2 detecciones en el mismo lugar
 INACTIVO ───────────► ESPERANDO ───────────────────────────────► SIGUIENDO
                           ▲                                        │   ▲
                           │ 6 s sin verlo                1 s sin   │   │ lo vuelve
                           │                              verlo     ▼   │ a ver
                           └──────────────────────────────────── BUSCANDO
                                               (gira a pulsos hacia donde lo vio por última vez)
```

- **Confirmaciones:** un reflejo rojo de un solo cuadro no mueve el carro; el balón tiene que aparecer 2 veces seguidas en el mismo lugar.
- **Seguimiento por cercanía:** ya enganchado, en cada cuadro elige el blob **más cercano** al balón anterior, no el más grande. Así no salta a una caja roja que aparezca en la imagen.
- **Filtro:** la posición nueva pesa 70 % y la anterior 30 %, lo que suaviza el temblor de la detección.

## 🧠 El control PD

```
error = (cx − 0.5) × 2          → −1 = borde izquierdo, 0 = centro, +1 = borde derecho
giro  = Kp × error + Kd × (qué tan rápido cambia el error)
avance = vel_max × (1 − área / área_objetivo)      → rápido si se ve pequeño, cero al llegar
izquierda = avance + giro        derecha = avance − giro
```

- **Kp** (proporcional): cuánto gira por estar descentrado. Si es muy bajo, el carro tarda en girar y pierde el balón. Si es muy alto, zigzaguea.
- **Kd** (derivativo): frena el giro cuando el error ya está bajando rápido. Quita el zigzag.
- **Zona muerta:** errores pequeños se ignoran para no corregir sin parar.
- **Si el error es grande** (balón muy a un lado), primero gira sin avanzar.

## Afinar en el carro (desde la página, sin recompilar)

1. **Área objetivo:** pon el balón a la distancia a la que quieres que se detenga y mira "área=..." en el estado. Ese número es tu área objetivo.
2. **Kp:** empieza en 60 y súbelo hasta que el carro gire con decisión sin pasarse. Si zigzaguea, bájalo un 30 %.
3. **Kd:** súbelo de a poco hasta que desaparezca el zigzag que queda.
4. **Velocidad máx.:** empieza baja (100). Con más velocidad hace falta más Kd.

Cuando encuentres buenos valores, escríbelos en `seguidor_params_defecto()` (y `pwm_min` de la etapa 1) para que queden como predeterminados.

## 🧠 El ciclo de mejora (el trabajo real del ML)

```
  usar el carro ──► encontrar dónde falla ──► tomar fotos de ESAS situaciones
        ▲                                                │
        │                                                ▼
  embeber y comparar ◄── entrenar ◄── etiquetar (sesión nueva)
```

Cada vuelta, el modelo mejora **justo donde lo necesitas**. Lleva una tabla:

| Versión | Fotos | Sesiones | Acierto | Falsa alarma | Qué cambió |
|---|---|---|---|---|---|
| v1 | 350 | 3 | | | primer modelo |
| v2 | 480 | 5 | | | + fotos de noche y con ropa roja |

Guarda cada versión con git (`git commit -m "modelo v2: + fotos de noche"`): así sabes qué datos produjeron cada modelo.

## ✍️ Ejercicios

1. Pon `kd = 0` y súbele mucho el `kp`. Describe el zigzag. Ahora sube `kd` hasta que desaparezca.
2. ¿Qué pasa si el balón rueda rápido y se sale de la imagen? Mira el estado BUSCANDO. ¿Hacia qué lado gira y por qué? (Pista: `ultimo_lado`).
3. Haz una vuelta completa del ciclo de mejora: encuentra **una** situación donde el modelo falle, toma 50 fotos de ella, re-entrena y mide si mejoró **sin empeorar** lo demás.
4. Reto: el ultrasonido ya mide la distancia (`dist` en el estado). Modifica `seguidor_paso()` para usar la distancia real en vez del área cuando el balón está centrado y a menos de 1 m. (Pista: el seguidor de personas del equipo hace algo parecido).
5. Reto grande: agrega un segundo objeto (por ejemplo, un cono azul que el carro deba **evitar**). ¿Qué tendrías que cambiar en las etiquetas, en la última capa del modelo y en `postproceso.c`?
