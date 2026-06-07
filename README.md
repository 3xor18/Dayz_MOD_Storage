# 3xorStorage

Mod de almacenamiento para **DayZ** (cliente + servidor): barril de gran capacidad, **empaquetable y transportable**, con **virtualización de contenido** para mejorar el rendimiento del servidor y **anti-dupe** integrado.

> Inspirado en *ToFu Virtual Storage* (virtualización) y *MMG Base Storage* (empaquetado en cajas), combinando lo mejor de ambos: el loot guardado deja de cargar al servidor **y** el barril se puede mover de base fácilmente.

## Items

| Classname | Nombre en juego | Capacidad | Logo |
|---|---|---|---|
| `Exor_Barrel_500` | 3xor Barrel 500 | 500 slots (10×50) | "3xor" blanco |
| `Exor_Barrel_500_Packed` | 3xor Barrel 500 (empaquetado) | 5×5 en inventario | "3xor" blanco |

## Features

- ✅ **Empaquetar / Desplegar**: con el barril cerrado y vacío, acción *"Empaquetar barril"* → se convierte en una caja transportable. Con la caja en las manos, *"Desplegar barril"* lo coloca de nuevo. Transportá varios a la vez.
- ✅ **Indestructible y no lockeable**: el loot guardado nunca se destruye (explosiones/balas no lo afectan), pero el barril no acepta CodeLocks ni candados — quien llega hasta él, lo abre. La defensa es tu base, no el barril.
- 🔜 **Virtualización** (Fase 2): pasados X minutos sin interacción, el contenido se guarda en disco y los items desaparecen del mundo → menos entidades, menos lag, arranques más rápidos.
- 🔜 **Auto-cierre** (Fase 2): un barril dejado abierto se cierra solo pasados X minutos.
- 🔜 **Comida que dura más** (Fase 2): multiplicador configurable de duración dentro del barril (default: el doble).
- 🔜 **Anti-dupe** (Fase 3): ID único por barril, detección de duplicados al arranque, cooldowns y logs para admins.
- 🔜 **Mochilas/ropa con items adentro** (Fase 3): solo dentro de barriles 3xor, activable por config.
- 🔜 **Stacks de munición configurables** (Fase 3): una entrada por cada bala del juego, default 100. Solo balas sueltas/bolts/flechas (sin cajas ni granadas).
- 🔜 **Auto-stack al recoger** (Fase 3): las balas que levantás se fusionan solas con las pilas que ya tenés en el inventario.
- 🔜 **Cantidad de munición al spawnear** (Fase 3): rango aleatorio `{min, max}` configurable por bala para cada pila que spawnea como loot.
- 🔜 **Cobertura de vehículos** (Fase 2.5): los autos inactivos X minutos se cubren con una red de camuflaje y dejan de simularse (gran ahorro de recursos — los vehículos activos son de lo más pesado del server). Acción "Quitar la cobertura" para usarlos de nuevo. Todo configurable (`vehiculos_cubrir_minutos`, radio de jugadores, exclusiones).

Roadmap completo y decisiones de diseño: [`docs/PLAN.md`](docs/PLAN.md)

## Configuración del servidor

Al primer arranque se crea `<profile>/3xorStorage/settings.json` con los defaults (si ya existía de una versión vieja, se completa solo con los campos nuevos). Ejemplo completo en [`config-examples/settings.json`](config-examples/settings.json).

| Parámetro | Default | Qué hace | Activo desde |
|---|---|---|---|
| `virtualizar_minutos` | `10` | Minutos sin interacción (barril cerrado) para que el contenido se virtualice a disco y los items salgan del mundo. `0` = desactivado. Nota: más bajo = más ahorro, pero abrir un barril dormido tiene una micro-pausa de restauración, y la comida virtualizada congela su deterioro | Fase 2 |
| `auto_cerrar_minutos` | `5` | Si alguien deja el barril abierto, se cierra solo pasados estos minutos (recién ahí corre el timer de virtualización). `0` = desactivado | Fase 2 |
| `multiplicador_comida` | `2.0` | La comida dentro del barril se deteriora N veces más lento (`2.0` = dura el doble). Virtualizado, el deterioro queda congelado | Fase 2 |
| `permitir_ropa_con_items` | `true` | Permite guardar mochilas/ropa **con items adentro**, solo dentro de barriles 3xor (vanilla lo bloquea) | Fase 3 |
| `blacklist` | `[]` | Classnames que NO se pueden guardar en los barriles | Fase 3 |
| `cooldown_abrir_segundos` | `5` | Anti-dupe: segundos de espera para volver a abrir el mismo barril. `0` = sin cooldown | Fase 3 |
| `stack_municion` | auto | **Una entrada por cada bala del juego** → stack máximo. La lista se auto-completa al arrancar (Fase 3) con todas las municiones detectadas (vanilla + mods); después editás la que quieras | Fase 3 |
| `stack_municion_default` | `100` | Valor de relleno con el que se auto-agregan las balas a `stack_municion` (nuevas balas de updates/mods entran solas con este valor) | Fase 3 |
| `auto_stack` | `true` | Las balas que recogés se fusionan solas con las pilas que ya tenés | Fase 3 |
| `spawn_municion` | auto | **Una entrada por bala** → `{ "min": X, "max": Y }`: cada pila de loot spawnea con una cantidad aleatoria entre min y max. También se auto-completa con todas las balas | Fase 3 |
| `spawn_municion_min_default` / `spawn_municion_max_default` | `15` / `65` | Rango de relleno con el que se auto-agregan las balas a `spawn_municion` | Fase 3 |
| `municion_excluida` | 40mm + bengalas | Munición que NUNCA se toca (ni stack, ni auto-stack, ni spawn). Granadas de mano, humo, gas y cajas ya quedan fuera por diseño | Fase 3 |

> Los cambios en `settings.json` se aplican reiniciando el server.

## types.xml (spawn como loot)

Agregá esto al `types.xml` de tu misión para que el barril spawnee como loot. Recomendado: spawnear la versión **empaquetada** (es la que tiene gracia encontrar y llevarte); la desplegada dejala en `nominal=0` (solo existe cuando un jugador la despliega).

```xml
<!-- 3xorStorage: caja empaquetada (esta spawnea como loot) -->
<type name="Exor_Barrel_500_Packed">
    <nominal>4</nominal>
    <lifetime>14400</lifetime>
    <restock>1800</restock>
    <min>2</min>
    <quantmin>-1</quantmin>
    <quantmax>-1</quantmax>
    <cost>100</cost>
    <flags count_in_cargo="0" count_in_hoarder="0" count_in_map="1" count_in_player="0" crafted="0" deloot="0"/>
    <category name="containers"/>
    <usage name="Industrial"/>
    <usage name="Military"/>
</type>

<!-- 3xorStorage: barril desplegado (NO spawnea; persiste 45 dias como los barriles vanilla) -->
<type name="Exor_Barrel_500">
    <nominal>0</nominal>
    <lifetime>3888000</lifetime>
    <restock>0</restock>
    <min>0</min>
    <quantmin>-1</quantmin>
    <quantmax>-1</quantmax>
    <cost>100</cost>
    <flags count_in_cargo="0" count_in_hoarder="0" count_in_map="1" count_in_player="0" crafted="0" deloot="0"/>
    <category name="containers"/>
</type>
```

Ajustá `nominal`/`min` a gusto (cuántos querés que haya en el mapa a la vez). `lifetime 3888000` = 45 días sin interacción antes de despawnear, igual que un barril vanilla.

## Estructura del repo

```
├── src/ExorStorage/          # Fuente del PBO (prefix: ExorStorage)
│   ├── config.cpp            # CfgPatches / CfgMods / CfgVehicles (barril + caja)
│   ├── data/                 # Texturas .paa (generadas por el build)
│   └── scripts/
│       ├── 3_Game/           # Constantes + settings JSON
│       ├── 4_World/
│       │   ├── Entities/     # Clases del barril
│       │   ├── Actions/      # Empaquetar / Desplegar
│       │   └── ...           # Registro de acciones + PlayerBase
│       └── 5_Mission/        # Init del server (carga settings)
├── assets/textures/          # PNG fuente de las texturas (generadas)
├── tools/                    # Build: gen_textures.py, pack_pbo.py, build.ps1
│   └── (+ list_pbo.py / extract_pbo.py para inspeccionar PBOs vanilla)
├── mod/mod.cpp               # Metadata del mod para el launcher/Workshop
├── config-examples/          # settings.json de ejemplo
└── docs/PLAN.md              # Roadmap y decisiones de diseño
```

## Build

Requisitos: Windows, Python 3 con Pillow, [DayZ Tools](https://store.steampowered.com/app/830640/DayZ_Tools/) (para `ImageToPAA`).

```powershell
.\tools\build.ps1                # texturas + PBO -> dist\@3xorStorage
.\tools\build.ps1 -SkipTextures  # solo re-empaqueta el PBO
```

El resultado queda en `dist/@3xorStorage/` listo para copiar al server y al cliente.

## Instalación

**Servidor:**
1. Copiar `@3xorStorage` a la raíz del server.
2. Agregar `-mod=@3xorStorage` a los parámetros de arranque.
3. Agregar los items a `types.xml` para que spawneen como loot (o repartirlos por admin).
4. (Producción) Copiar la `.bikey` a la carpeta `keys/` del server.

**Cliente:** suscribirse al mod en Steam Workshop (cuando esté publicado) — el launcher lo carga solo.

## Estado

**v0.1.0 — Fase 1** (barril + empaquetado). En desarrollo activo; aún no publicado en Workshop.
