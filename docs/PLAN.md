# 3xorStorage — Plan de desarrollo

Mod cliente+servidor para DayZ inspirado en **ToFu Virtual Storage** (virtualización de contenido) y **MMG Base Storage** (muebles empaquetables en cajas).

## Objetivos

1. Reducir el lag del servidor sacando del mundo el loot guardado (menos entidades vivas).
2. Prevenir el dupeo de items guardados.
3. Que los barriles sean fáciles de transportar (empaquetado → caja → desplegar), la gran limitación de ToFu.

## Items

| Classname | Visible en juego | Capacidad | Letras |
|---|---|---|---|
| `Exor_Barrel_500` | 3xor Barrel 500 | 10×50 = 500 slots | Blancas |
| `Exor_Barrel_500_Packed` | 3xor Barrel 500 (empaquetado) | — (5×5 en inventario) | Blancas |

> **Decisión 7-jun-2026:** se eliminó el barril de 1000 slots (`Exor_Barrel_1000`) — 1000 era demasiado para el balance del server. Queda solo el de 500. Si algún día se quiere de vuelta, está en el historial de git (hasta el commit `beff3a1`).

Modelo base: barril vanilla `\dz\gear\containers\55galDrum.p3d`, retexturizado vía `hiddenSelections = {"camoGround"}` con camo procedural + logo "3xor".

Obtención: spawn como loot (`types.xml`) + entrega por admins/eventos. **No** se craftea.

## Fases

### ✅ Fase 1 — Barriles + empaquetado (v0.1.0)
- [x] Estructura del mod (config.cpp, módulos 3_Game / 4_World / 5_Mission)
- [x] Barriles 500/1000 con texturas camo "3xor"
- [x] Acción **Empaquetar barril** (solo cerrado, sano y vacío — incluye líquidos)
- [x] Acción **Desplegar barril** (con el paquete en las manos)
- [x] Settings JSON en `$profile:3xorStorage\settings.json` (se crea solo con defaults)
- [ ] Test en server local

### Fase 2 — Virtualización
- Tras `virtualizar_minutos` sin interacción, el contenido se serializa (recursivo: attachments, cargadores, cantidades, cargo anidado) a `$profile:3xorStorage\storage\<id>.json` y los items se borran del mundo.
- **Auto-cierre** (`auto_cerrar_minutos`, default 5): si alguien deja el barril abierto, se cierra solo pasados X min sin interacción; recién ahí arranca el timer de virtualización (un barril abierto nunca virtualiza).
- Al abrir el barril, se restaura todo.
- El JSON se escribe **antes** de borrar items (crash-safe: nunca se pierde loot).
- Comida: multiplicador de duración `multiplicador_comida` mientras el barril está activo. Virtualizado = deterioro congelado (aceptado por diseño).

### Fase 3 — Anti-dupe + reglas de guardado + munición
- ID persistente único por barril; JSON ligado a ese ID; se elimina al restaurar.
- Caja dupeada ⇒ el segundo barril restaura vacío + log de alerta para admins.
- Chequeo al arranque del server: JSONs huérfanos / IDs duplicados → log + cuarentena.
- Cooldown configurable de abrir/cerrar (`cooldown_abrir_segundos`).
- `blacklist`: items que no entran al barril.
- `permitir_ropa_con_items`: mochilas/ropa con cosas adentro, solo en barriles 3xor (configurable).
- **Stack de munición**: `stack_municion` = un mapa con **una entrada por cada bala del juego** → stack máximo. Se **auto-completa al arrancar** con todas las municiones detectadas en config (vanilla + mods), usando `stack_municion_default` (100) como relleno; balas nuevas de updates entran solas. Solo balas sueltas, bolts y flechas. Técnica: override de `GetQuantityMax` leyendo el JSON; plan B = valores fijos en config.cpp si el enfoque dinámico da problemas de split/persistencia.
- **Auto-stack al recoger** (`auto_stack`, default true): cuando una pila de balas entra al inventario del jugador, el server la fusiona automáticamente con las pilas existentes del mismo tipo que tengan espacio (hasta el stack máximo). Mismo alcance que el punto anterior: solo balas sueltas/bolts/flechas. Técnica: hook en `OnInventoryEnter` de las pilas de munición (server-side).
- **Cantidad de munición al spawnear**: `spawn_municion` = un mapa con una entrada por bala → rango `{min, max}`; cada pila de loot spawnea con cantidad **aleatoria entre min y max**. Auto-completado igual que `stack_municion` (relleno: `spawn_municion_min_default`=15, `spawn_municion_max_default`=65). El resultado se recorta al stack máximo del tipo. Técnica: hook `EEOnCECreate` (server-side). Mismo alcance: solo balas sueltas/bolts/flechas.
- **Alcance del paquete de munición (regla dura, aplica a los 3 puntos anteriores)**: SOLO pilas de munición simple (`Ammunition_Base`): balas sueltas, cartuchos, bolts y flechas. EXCLUIDO todo lo demás: granadas de mano (explosivas/humo/gas), granadas 40 mm del lanzagranadas, viales/tóxicos, cajas de munición, bengalas. Doble seguro: filtro por clase + lista `municion_excluida` configurable en el JSON (pre-poblada con 40 mm y similares) por si un update o mod mete algo que se cuele.

### Fase 4 — Test de carga y release
- Test en server local con cientos de items.
- Publicación en Steam Workshop (DayZ Tools Publisher, cuenta del dueño).
- Firmado de PBOs (DSUtils) + distribución de la `.bikey` al server.

## Decisiones tomadas

- **Balance de raideo (RESUELTO 7-jun-2026)**: el barril es **indestructible** (`SetAllowDamage(false)`, el loot nunca se pierde) pero **NO lockeable** (`CanReceiveAttachment` bloquea codelocks/candados de cualquier mod). Quien llega físicamente al barril, lo abre — la protección es la base (y con NoWallDamage, raidear la base = romper puertas en horario de raid). La caja empaquetada también es indestructible.

## Decisiones pendientes
- Posición de los logos "3xor" en la textura: el tileo actual es provisional; se ajusta cuando veamos el UV real in-game.

## Limitaciones conocidas (heredadas del enfoque, igual que ToFu)

- Items con estado especial complejo pueden necesitar blacklist.
- El loot virtualizado es invisible para el CE (economía) del juego.
- El despliegue Fase 1 es "frente al jugador" sin preview de holograma; se puede mejorar a colocación con preview más adelante.
