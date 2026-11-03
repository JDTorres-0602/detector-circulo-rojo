# datos/

- `fotos/` — las fotos del dataset, con nombre `SESION_NNNN.jpg` (por ejemplo `sesion01_0007.jpg`). Las crea `entrenamiento/capturar_fotos.py`.
- `etiquetas.json` — dónde está el balón en cada foto. Lo crea `entrenamiento/etiquetar.py`.

Formato de `etiquetas.json`:

```json
{
 "sesion01_0007.jpg": [[0.62, 0.71, 0.08]],
 "sesion01_0008.jpg": []
}
```

Cada balón es un círculo `[cx, cy, r]`: `cx = x_centro / ancho`, `cy = y_centro / alto`, `r = radio_en_píxeles / ancho`. Una lista vacía significa "en esta foto NO hay balón", y también es una etiqueta útil. Las fotos que no aparecen en el archivo no se usan.

**¿Subir las fotos a GitHub?** Unos cientos de fotos de 320×240 pesan pocos MB, así que sí se pueden versionar. Tener el dataset en el repositorio te deja saber con qué datos se entrenó cada modelo.
