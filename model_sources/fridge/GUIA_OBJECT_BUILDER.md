# Guía Object Builder — Refrigerador retro (МОСКВА)

Este es el **único paso manual** del refri. El FBX→`.p3d` es GUI y no se automatiza.
Cuando termines y guardes, avisá y yo binarizo (hornea la animación) + build + test.

**Archivo fuente:** `model_sources\fridge\RetroFridge.fbx`
**Salida final:** `src\ExorStorage\data\models\fridge\fridge.p3d`
**Contrato de nombres (NO cambiar):** puerta = `lid` · eje = `lid_axis` · animación = `Lid`

---

## 0. Preparar P: (una vez por sesión)
- DayZ Tools → **Mount Work Drive** (P:). Necesario para que se vean texturas/rvmat.
- El material vive en `P:\ExorStorage\data\models\fridge\` (junction del repo). Si P: no
  resuelve `ExorStorage\...`, montá el mod o copiá la carpeta a P: temporalmente.

## 1. Importar el FBX
- File → Import → **Import FBX** → `RetroFridge.fbx`.
- **Master scale**: probá `1.0` primero. Si el refri sale gigante o minúsculo, deshacé e
  importá con `0.01` (FBX en cm) o `0.1`. Objetivo: alto ≈ **1.5 m** (un refri real).
- La base debe quedar apoyada en **Y = 0** (piso). Si flota o se hunde:
  Points → Select All → **Transform 3D → Move**, Y = ± lo necesario, *Apply to all lods*.

## 2. LOD visual (Resolution 0.0) — textura + material
- Select All (vértices/caras) → Faces → **Face Properties**.
- **Texture** = `ExorStorage\data\models\fridge\fridge_co.paa`
- **Material** = `ExorStorage\data\models\fridge\fridge.rvmat`
- Apply / OK. Debería verse la nevera blanca+roja (no rosa; el rosa = sin binarizar, se
  arregla solo al binarizar).

## 3. ⭐ Separar la PUERTA en su propia selección `lid`
El retro-fridge vino como **una sola malla** (la puerta no es objeto aparte). Para animarla
hay que marcar sus caras:
1. Rotá la vista para ver la puerta frontal.
2. Seleccioná **solo las caras de la puerta** (la mitad inferior roja que abre; incluí la
   manija). Usá selección por caras (no toques el cuerpo/carcasa).
3. Con esas caras seleccionadas: panel **Named Selections** → **New** → nombre exacto
   `lid` → asignar la selección actual.
4. Verificá: click en `lid` en la lista debe resaltar SOLO la puerta. Ni una cara del
   cuerpo adentro, ni una cara de la puerta afuera.
   > Si el modelo no tiene la puerta como pieza clara/separable, avisame: hacemos v1 SIN
   > animación (abre el inventario, la puerta no gira) y lo dejamos andando ya.

## 4. ⭐ Eje de bisagra `lid_axis` (Memory LOD)
1. Create → **New LOD** → Properties → tipo **Memory**.
2. En el Memory LOD, creá **2 puntos** sobre la **bisagra real** de la puerta (el borde
   vertical por donde gira, normalmente el lado izquierdo mirando de frente):
   - punto A abajo, punto B arriba, **misma X y misma Z** (línea perfectamente vertical).
   - Verificá en vista **Top**: los 2 puntos deben **superponerse** (= vertical).
3. Seleccioná los 2 puntos → Named Selections → New → nombre exacto `lid_axis`.
   > El ORDEN de los puntos define el sentido de giro. Si al testear abre al revés, lo
   > arreglo yo cambiando el signo del ángulo (no volvés a OB).

## 5. Geometry LOD (colisión) — simple
- Duplicá el LOD visual → cambiá tipo a **Geometry**.
- Structure → Topology → **Find Components** (o hacé una CAJA convexa simple que envuelva
  el refri — más estable que la malla con estantes).
- Selecciona todo → **E** (o Structure → Convexity → Component Convex Hull) si hay
  componentes cóncavos.
- Named Properties: agregá `class = "house"` y `mass = 40`.
  > Nota: como el barril 3xor, probablemente NO frene al jugador (limitación de items
  > DayZ). La colisión sirve para daño/placement, no para "chocar". No pelees con esto.

## 6. Propiedad del modelo
- En el LOD visual → Named Properties → agregá `autocenter = 0` (evita que el pivote se
  desplace y rompa el eje de la puerta).

## 7. Exportar
- **File → Save As** → `src\ExorStorage\data\models\fridge\fridge.p3d` (MLOD, editable).
- Guardá TAMBIÉN una copia de respaldo: `model_sources\fridge\fridge_MLOD_editable.p3d`.
- ⚠️ NO exportes a binarizado desde OB. Yo lo binarizo con `binarize.exe` + `model.cfg`
  (así hornea la animación de `lid` sobre `lid_axis`).

## 8. Avisame
Decime "listo el p3d" y yo hago: binarize (model.cfg + MLOD → ODOL) → copio a `src` →
build.ps1 → test in-game. Ajustes finos de la puerta (sentido/ángulo) los hago sin que
vuelvas a OB.

---
### Checklist antes de avisar
- [ ] Alto ≈ 1.5 m, base en Y=0.
- [ ] LOD visual con `fridge_co.paa` + `fridge.rvmat`.
- [ ] Selección `lid` = solo la puerta.
- [ ] `lid_axis` = 2 puntos verticales en la bisagra, en Memory LOD.
- [ ] Geometry LOD con class=house / mass=40.
- [ ] Guardado como `src\...\fridge.p3d` (MLOD) + backup en model_sources.
