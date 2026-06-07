# 3xorStorage

Mod de almacenamiento para **DayZ** (cliente + servidor): barriles de gran capacidad, **empaquetables y transportables**, con **virtualización de contenido** para mejorar el rendimiento del servidor y **anti-dupe** integrado.

> Inspirado en *ToFu Virtual Storage* (virtualización) y *MMG Base Storage* (empaquetado en cajas), combinando lo mejor de ambos: el loot guardado deja de cargar al servidor **y** el barril se puede mover de base fácilmente.

## Items

| Classname | Nombre en juego | Capacidad | Logo |
|---|---|---|---|
| `Exor_Barrel_500` | 3xor Barrel 500 | 500 slots (10×50) | "3xor" blanco |
| `Exor_Barrel_1000` | 3xor Barrel 1000 | 1000 slots (10×100) | "3xor" rojo |
| `Exor_Barrel_500_Packed` | 3xor Barrel 500 (empaquetado) | 5×5 en inventario | "3xor" blanco |
| `Exor_Barrel_1000_Packed` | 3xor Barrel 1000 (empaquetado) | 5×5 en inventario | "3xor" rojo |

## Features

- ✅ **Empaquetar / Desplegar**: con el barril cerrado y vacío, acción *"Empaquetar barril"* → se convierte en una caja transportable. Con la caja en las manos, *"Desplegar barril"* lo coloca de nuevo. Transportá varios a la vez.
- 🔜 **Virtualización** (Fase 2): pasados X minutos sin interacción, el contenido se guarda en disco y los items desaparecen del mundo → menos entidades, menos lag, arranques más rápidos.
- 🔜 **Comida que dura más** (Fase 2): multiplicador configurable de duración dentro del barril (default: el doble).
- 🔜 **Anti-dupe** (Fase 3): ID único por barril, detección de duplicados al arranque, cooldowns y logs para admins.
- 🔜 **Mochilas/ropa con items adentro** (Fase 3): solo dentro de barriles 3xor, activable por config.
- 🔜 **Stacks de munición configurables** (Fase 3): default 100 por tipo, ajustable por classname. Solo balas sueltas/bolts/flechas (sin cajas ni granadas).

Roadmap completo y decisiones de diseño: [`docs/PLAN.md`](docs/PLAN.md)

## Configuración del servidor

Al primer arranque se crea `<profile>/3xorStorage/settings.json` con los defaults. Ejemplo completo en [`config-examples/settings.json`](config-examples/settings.json):

```json
{
    "virtualizar_minutos": 30,
    "multiplicador_comida": 2.0,
    "permitir_ropa_con_items": true,
    "blacklist": [],
    "cooldown_abrir_segundos": 5,
    "stack_municion_default": 100,
    "stack_municion": { "Ammo_762x39": 100 }
}
```

## Estructura del repo

```
├── src/ExorStorage/          # Fuente del PBO (prefix: ExorStorage)
│   ├── config.cpp            # CfgPatches / CfgMods / CfgVehicles (los 4 items)
│   ├── data/                 # Texturas .paa (generadas por el build)
│   └── scripts/
│       ├── 3_Game/           # Constantes + settings JSON
│       ├── 4_World/
│       │   ├── Entities/     # Clases de los barriles
│       │   ├── Actions/      # Empaquetar / Desplegar
│       │   └── ...           # Registro de acciones
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

**v0.1.0 — Fase 1** (barriles + empaquetado). En desarrollo activo; aún no publicado en Workshop.
