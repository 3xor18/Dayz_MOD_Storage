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
| `Exor_Barrel_1000` | 3xor Barrel 1000 | 10×100 = 1000 slots | Rojas |
| `Exor_Barrel_500_Packed` | 3xor Barrel 500 (empaquetado) | — (5×5 en inventario) | Blancas |
| `Exor_Barrel_1000_Packed` | 3xor Barrel 1000 (empaquetado) | — (5×5 en inventario) | Rojas |

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
- **Stack de munición** global: `stack_municion_default` (100) + override por classname en `stack_municion`. Solo balas sueltas, bolts y flechas. Excluidos: cajas de munición, granadas 40 mm, granadas de mano (explosiva/humo/gas). Técnica: override de `GetQuantityMax` leyendo el JSON; plan B = valores fijos en config.cpp si el enfoque dinámico da problemas de split/persistencia.

### Fase 4 — Test de carga y release
- Test en server local con cientos de items.
- Publicación en Steam Workshop (DayZ Tools Publisher, cuenta del dueño).
- Firmado de PBOs (DSUtils) + distribución de la `.bikey` al server.

## Decisiones pendientes

- **Balance de raideo**: ¿el barril es lockeable (CodeLock/candado)? ¿Es destruible y en qué horario? Con NoWallDamage los muros son inmunes fuera de horario — un barril de 1000 slots lockeado e indestructible sería loot 100% seguro. Decidir antes de salir a producción.
- Posición de los logos "3xor" en la textura: el tileo actual es provisional; se ajusta cuando veamos el UV real in-game.

## Limitaciones conocidas (heredadas del enfoque, igual que ToFu)

- Items con estado especial complejo pueden necesitar blacklist.
- El loot virtualizado es invisible para el CE (economía) del juego.
- El despliegue Fase 1 es "frente al jugador" sin preview de holograma; se puede mejorar a colocación con preview más adelante.
