# Glosario

| Término | Qué significa en este proyecto |
|---|---|
| **Arena** | Bloque de RAM que TFLite Micro usa para los resultados intermedios de cada capa (`ARENA_BYTES` en `config.h`) |
| **Aumento de datos** | Modificar al azar cada foto al entrenar (zoom, brillo, ruido...) para que el modelo no memorice |
| **Blob** | Grupo de celdas encendidas que se tocan. Cada blob es un balón candidato (`postproceso.c`) |
| **Calibración** | Fotos que el convertidor usa para elegir las escalas de la cuantización int8 |
| **Celda** | Cuadro de 8×8 píxeles de la imagen 128×96. Hay 12×16 = 192 |
| **Cuantización int8** | Pasar pesos y activaciones de decimales de 32 bits a enteros de 8 bits: 4 veces más pequeño y más rápido |
| **Dataset** | El conjunto de fotos con sus etiquetas |
| **Descenso de gradiente** | Cómo aprende la red: ajusta cada peso un poquito en la dirección que baja la pérdida |
| **Época** | Una vuelta de entrenamiento (aquí, `PASOS` lotes) |
| **Etiqueta** | La respuesta correcta de una foto: círculos en `etiquetas.json`, que se convierten en un mapa 12×16 |
| **Falsa alarma** | Ver un balón donde no hay. Se mide en fotos sin balón |
| **FOMO** | *Faster Objects, More Objects*: detector que predice una probabilidad por celda en vez de cajas. Ideal para microcontroladores |
| **FreeRTOS** | El sistema operativo de la ESP32: permite varias tareas a la vez (cámara, visión, control, web) |
| **Inferencia** | Usar el modelo ya entrenado para predecir (lo que hace la ESP32) |
| **Lote (batch)** | Grupo de fotos que se procesan juntas en un paso de entrenamiento (`LOTE = 32`) |
| **LUT** | Tabla precalculada: en `detector.cc` convierte cada valor de píxel 0..255 directamente al int8 que espera el modelo |
| **MobileNetV2** | Red neuronal diseñada para celulares, liviana y rápida. Usamos sus primeras capas |
| **Mutex** | Candado para que dos tareas no lean y escriban el mismo dato al mismo tiempo (`g_mutex`) |
| **Pérdida (loss)** | Número que mide qué tan mal predice el modelo. Entrenar es bajarlo. Aquí es la **pérdida focal** |
| **Pérdida focal** | Pérdida que pesa más los errores difíciles que los fáciles |
| **PD (control)** | Proporcional-Derivativo: gira según qué tan descentrado está el balón (P) y qué tan rápido se mueve (D) |
| **Peso** | Cada número interno de la red que se ajusta al entrenar |
| **PSRAM** | RAM externa de la ESP32 (varios MB), más lenta que la interna (~512 KB) |
| **PWM** | Prender y apagar un motor muy rápido para controlar su velocidad (`analogWrite`) |
| **Recall** | De todos los balones reales, qué fracción encuentra el detector |
| **Sobreajuste (overfitting)** | El modelo memoriza las fotos de entrenamiento y falla con fotos nuevas |
| **Sigmoide** | Función que convierte cualquier número en uno entre 0 y 1 (una probabilidad) |
| **TFLite Micro** | TensorFlow Lite para Microcontroladores: ejecuta modelos `.tflite` sin sistema operativo ni memoria dinámica |
| **Transfer learning** | Partir de un modelo ya entrenado (MobileNetV2 con ImageNet) y entrenar solo lo nuevo |
| **Umbral** | Valor desde el cual una celda se considera "balón". Se calcula para tener pocas falsas alarmas |
| **Validación** | Fotos que el modelo nunca usa para aprender. Sirven para medirlo honestamente |
| **Watchdog** | Vigilante: si deja de recibir órdenes, detiene los motores |
