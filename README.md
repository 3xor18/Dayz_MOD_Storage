# 3xor_Vanilla_Optimization

Mod **todo-en-uno** para DayZ (cliente + servidor), **100% standalone**. Combina sistemas de **comunidad/PvP** (party, territorio, anti-raid, spawns, chat, killfeed, VIP) con **optimización del servidor** (barriles virtualizados, vehículos que se duermen, stacks de munición) — menos entidades vivas = menos lag.

> Antes llamado "3xorStorage"; se renombró porque dejó de ser solo storage.

## Dependencias

**NINGUNA.** El mod **no requiere ningún otro mod** de Steam Workshop. Solo usa addons **vanilla** de DayZ:

```
requiredAddons[] = { DZ_Data, DZ_Scripts, DZ_Gear_Containers, DZ_Weapons_Ammunition, DZ_Gear_Camping };
```

No depende de Expansion, CF (Community Framework) ni ningún framework externo. Puede convivir con otros mods, pero **ojo**: si corrés Expansion-Groups/Territories al mismo tiempo, los sistemas de grupo/territorio van a chocar (este mod asume ser el único de party/territorio).

---

## Features

### Party + Territorio (por mástil)
- **Reclamar territorio**: al colocar el kit de bandera vanilla (soga + 3 palos), la bandera se auto-construye, reclama el territorio y crea el party (quien la pone = líder).
- **Party** (mirando el mástil, scroll): *Invitar a grupo* (abre invitación 10 min) → el otro ve *Unirse al grupo* · *Cancelar invitación* · *Administrar party* (lista de miembros + expulsar + salir).
- **Bandera izar/bajar** (cast de 15s): izar = solo miembros; bajar = según config. La bandera abajo puede bloquear el respawn en base.
- **Bandera blanca** (opcional): protección de N minutos reales tras reclamar (no se puede cambiar la tela); al vencer se libera o se cambia por la bandera configurada.

### Anti-raid (gateado por el radio del mástil ajeno)
- **No desmantelar** muros/torres en territorio ajeno si no sos del party.
- **No construir** (deployables, kits, **campos de plantación**) en territorio ajeno — bloqueado en cliente y reforzado server-side.
- **Logs forenses** (`raidlog/raid_AAAA-MM-DD.txt`, auto-purga): robo de items, deslogueo y expulsión en base ajena, con steamid + nombre + pos + hora.
- **Anti-combat-log**: al reconectar dentro de territorio ajeno, te teletransporta al borde.

### Spawns
- Pantalla de selección al morir / primer login, con puntos configurables + "Mi base".
- **Cooldown por punto** y por base, con cuenta regresiva en vivo (gris + rojo cuando no disponible).
- Respawn en base (cooldown configurable, requiere bandera arriba).

### VIP
- **Spawn en base + Equipamiento**: reemplaza la ropa por un loadout (pantalón/camisa/zapato/bolso + items extra) y gasta 1 uso.
- **Usos por jugador y por mes**, con el ciclo anclado a la **fecha de ingreso** de cada VIP (entró el 15 → resetea el 15, no el 1°).

### HUD / UI
- HUD de party (barra de vida + nombre + distancia), **nameplates 3D**, **mapa** (M) con tu posición + la de tu grupo, **marcas** (T pone / Y limpia).
- **Killfeed** (arriba-derecha) PvP/suicidio.
- **Panel de info del server** (ESC → "Server Info"): tabs General / Reglas (texto editable) y **Score** (ranking kills/deaths/suicidios/distancia).
- **Tooltip de items**: pastilla de rareza por tier + barra de durabilidad.

### Chat
- **2 canales**: GLOBAL (a todos) y ZONA (proximity, radio configurable). Tecla **`.`** cambia el canal.
- Panel abajo-izquierda (nombre azul + mensaje blanco), líneas con duración configurable. **Reemplaza el chat vanilla.**
- **Anti-spam**: cooldown entre mensajes + bloqueo de mensaje repetido.

### Vehículos
- Conductor en 3ª persona / pasajeros forzados a 1ª; ver tu inventario + el del auto; quitar daño (toggle).
- **Sueño automático**: autos inactivos X min dejan de simular física (gran ahorro) y despiertan solos cuando alguien se acerca.
- **Voltear vehículo** (acción con hold, anti-abuso).

### Storage (barriles 3xor)
- `Exor_Barrel_500` (500 slots) + su versión **empaquetable** (caja transportable).
- **Virtualización** del contenido a disco (menos entidades), **auto-cierre**, **anti-dupe**, comida que dura más, mochilas/ropa con items adentro.
- **Indestructible y no lockeable** (la defensa es la base, no el barril).

### Munición
- **Stacks a 100** (solo balas sueltas/bolts/flechas), **auto-stack** al recoger, **cantidad aleatoria al spawnear** por tipo.

---

## Controles

| Tecla / acción | Qué hace |
|---|---|
| Mirar el **mástil** + scroll | Invitar / Unirse / Cancelar invitación / Administrar party / Izar / Bajar bandera |
| **Enter** | Escribir en el chat (canal actual) |
| **`.`** | Cambiar canal de chat (GLOBAL ↔ ZONA) |
| **M** | Abrir/cerrar el mapa |
| **T** / **Y** | Poner marca / limpiar tus marcas |
| **ESC** → "Server Info" | Panel de info del server (General/Reglas/Score) |

---

## Configuración

Todo se configura por JSON en `<profile>/3xorVanillaOptimization/`. Cada archivo se crea solo con sus defaults al primer arranque, y los campos nuevos se auto-completan al cargar. **Los cambios se aplican reiniciando el server.** Si existía el `settings.json` monolítico viejo, se migra automáticamente.

### `storage.json`
| Parámetro | Default | Qué hace |
|---|---|---|
| `virtualizar_minutos` | `10` | Min sin interacción (cerrado) para virtualizar el contenido a disco. `0` = off |
| `auto_cerrar_minutos` | `5` | Cierra solo un barril dejado abierto. `0` = off |
| `multiplicador_comida` | `2.0` | La comida adentro dura N× más |
| `permitir_ropa_con_items` | `true` | Permite guardar mochilas/ropa con items adentro |
| `blacklist` | `[]` | Classnames que no se pueden guardar |
| `cooldown_abrir_segundos` | `3` | Anti-dupe: espera para reabrir el mismo barril |

### `vehiculos.json`
| Parámetro | Default | Qué hace |
|---|---|---|
| `vehiculos_dormir` | `true` | Activa el sueño de vehículos inactivos |
| `vehiculos_dormir_minutos` | `5` | Min de inactividad para dormir la física. `0` = off |
| `vehiculos_despertar_metros` | `30` | Radio para dormir/despertar según jugadores cerca |
| `vehiculos_excluidos` | `[]` | Vehículos que nunca duermen |
| `voltear_vehiculos` | `true` | Activa la acción "Voltear vehículo" |
| `voltear_segundos` | `40` | Duración de la acción de voltear |
| `camara.conductor_3ra_persona` | `true` | El conductor puede ir en 3ª |
| `camara.pasajeros_1ra_persona` | `true` | Pasajeros forzados a 1ª |
| `inventario.ver_ambos_dentro` | `true` | Ver tu inventario + el del auto a la vez |
| `dano.quitar_dano_vehiculos` | `false` | `true` = los autos no reciben daño |

### `municion.json`
| Parámetro | Default | Qué hace |
|---|---|---|
| `stack_municion_default` | `100` | Relleno con que se auto-agregan las balas a `stack_municion` |
| `stack_municion` | auto | Mapa classname→stack máx (auto-completado con todas las balas) |
| `auto_stack` | `true` | Las balas que recogés se fusionan solas |
| `spawn_municion_min_default` / `_max_default` | `15` / `65` | Relleno del rango de cantidad al spawnear |
| `spawn_municion` | auto | Mapa classname→`{min,max}` de cantidad al spawnear |
| `municion_excluida` | 40mm + bengalas + RPG/LAW | Munición que nunca se toca |

> El **stack máximo (100)** es fijo en `config.cpp` (CfgMagazines `count`); el motor no lo deja cambiar por JSON. Para otro número: editar `config.cpp` y recompilar.

### `party.json`
**`territorio`**
| Parámetro | Default | Qué hace |
|---|---|---|
| `habilitado` | `true` | Activa TODO el sistema bandera=territorio |
| `radio_metros` | `35` | Radio de anti-construcción alrededor del mástil |
| `permitir_construir_cerca` | `false` | `true` = ajenos pueden construir cerca |
| `whitelist_construible` | minas/claymore/plástico | Lo que SÍ se puede poner en territorio ajeno |
| `blacklist_construible` | `[]` | Lo que nunca se puede poner |
| `despawn_mastil_sin_miembros` | `true` | Despawnea el mástil si el party se vacía |

**`grupo`**
| Parámetro | Default | Qué hace |
|---|---|---|
| `habilitado` | `true` | Master on/off del sistema de party |
| `max_miembros` | `8` | Máximo de miembros |
| `tecla_menu` | `"P"` | Tecla del menú party (config) |
| `auto_kick_dias` | `0` | Días sin login para auto-kick. `0` = off |
| `mostrar_posicion_miembros` | `true` | Compartir posición (HUD/nameplates/mapa) |
| `mostrar_hud` | `true` | Barra de vida arriba-izq |
| `mostrar_nameplates` | `true` | Nombre 3D sobre la cabeza |
| `mostrar_distancia_miembros` | `true` | Distancia en HUD/nameplate |
| `mostrar_propio` | `false` | Incluirte a vos en HUD/nameplates |
| `permitir_marker_equipo` | `true` | Marcas con T / Y |

**`bandera`**
| Parámetro | Default | Qué hace |
|---|---|---|
| `ajenos_pueden_bajar` | `true` | Ajenos pueden bajar tu bandera |
| `bajada_bloquea_respawn` | `true` | Bandera abajo bloquea respawn en base |
| `bandera_blanca` | `false` | Cuelga bandera blanca al reclamar (protección) |
| `bandera_blanca_minutos` | `10080` | Protección en minutos reales (10080 = 7 días) |
| `bandera_blanca_cambiar_a` | `"Flag_DayZ"` | Bandera a colgar al expirar (`""` = solo destrabar) |

**`respawn_base`**
| Parámetro | Default | Qué hace |
|---|---|---|
| `habilitado` | `true` | Activa el respawn en base/mástil |
| `cooldown_segundos` | `3600` | Cooldown del respawn en base |
| `permitir_spawn_mastil_vip` | `true` | Los VIP pueden spawnear en el mástil |
| `permitir_spawn_mastil_no_vip` | `false` | Los no-VIP pueden spawnear en el mástil |

**`proteccion`** (anti-raid)
| Parámetro | Default | Qué hace |
|---|---|---|
| `bloquear_desmantelar_ajeno` | `true` | Ajenos no desmantelan en territorio ajeno |
| `log_robo_contenedor` | `true` | Loguea robo de items en territorio ajeno |
| `log_desconexion_base_ajena` | `true` | Loguea deslogueo en base ajena |
| `sacar_de_base_ajena_al_reconectar` | `true` | Teletransporta al borde al reconectar en base ajena |
| `log_dias_retener` | `7` | Días que se guardan los `raidlog`. `0` = no borrar |

### `spawns.json`
| Parámetro | Default | Qué hace |
|---|---|---|
| `habilitado` | `true` | Activa la pantalla de selección de spawn |
| `dar_cuchillo_al_spawnear` | `true` | Da un cuchillo al personaje nuevo (test) |
| `puntos[]` | ejemplo | Cada punto: `nombre`, `x`, `y`(0=al suelo), `z`, `cooldown_segundos`, `distancia_random` (radio aleatorio) |

### `mapa.json`
| Parámetro | Default | Qué hace |
|---|---|---|
| `abrir_con_m` | `true` | Abrir el mapa con M sin el ItemMap físico |
| `mostrar_mi_posicion` | `true` | Ver tu posición |
| `mostrar_miembros_party` | `true` | Ver a los miembros del party |

### `items.json`
| Parámetro | Default | Qué hace |
|---|---|---|
| `mostrar_durabilidad` | `true` | Barra de durabilidad en el tooltip |
| `mostrar_rareza` | `true` | Pastilla de rareza en el tooltip |
| `rareza_usar_tabla` | `false` | `true` = usar `rareza_tabla`; `false` = heurística |
| `rareza_tabla` | ejemplos | Mapa classname→tier (comun/poco_comun/raro/epico/legendario) |

### `vip.json`
| Parámetro | Default | Qué hace |
|---|---|---|
| `vips[]` | tu steamid (ejemplo) | Cada VIP: `steamid`, `fecha_ingreso` ("AAAA-MM-DD", vacío = se sella con hoy), `usos_por_mes` (0 = usar default global) |
| `equip_habilitado` | `true` | Activa el perk "Spawn en base + Equipamiento" |
| `equip_usos_por_mes` | `7` | Default global de usos por ciclo (si la entrada tiene 0) |
| `equip_loadout` | ejemplo | `pantalon`, `camisa`, `zapato`, `bolso` (vacío = no tocar) + `items_extra[]` (al cargo de camisa/pantalón) |

### `killfeed.json`
| Parámetro | Default | Qué hace |
|---|---|---|
| `habilitado` | `true` | Master on/off del killfeed |
| `duracion_segundos` | `6` | Cuánto dura cada línea |
| `max_lineas` | `5` | Máximo simultáneo |
| `mostrar_suicidios` | `true` | Mostrar "se ha suicidado" |

### `serverinfo.json`
| Parámetro | Default | Qué hace |
|---|---|---|
| `habilitado` | `true` | Muestra el botón "Server Info" en ESC |
| `titulo` | "Información del Server" | Título del panel |
| `tab_general` / `general_titulo` / `general_lineas[]` | on | Tab General (texto editable) |
| `tab_reglas` / `reglas_titulo` / `reglas_lineas[]` | on | Tab Reglas (texto editable) |
| `tab_score` / `score_titulo` | on | Tab Score (ranking) |

### `chat.json`
| Parámetro | Default | Qué hace |
|---|---|---|
| `habilitado` | `true` | Master on/off del chat custom |
| `radio_zona_metros` | `50` | Alcance del canal ZONA (proximity) |
| `duracion_segundos` | `25` | Cuánto dura cada línea |
| `max_lineas` | `9` | Máximo simultáneo |
| `cooldown_segundos` | `2` | Anti-spam: mínimo entre mensajes. `0` = sin límite |
| `bloquear_repetidos` | `true` | Anti-spam: bloquear el mismo mensaje seguido |

---

## Archivos que crea el mod (en `<profile>/3xorVanillaOptimization/`)

**Config (editables):** `storage.json` · `vehiculos.json` · `municion.json` · `party.json` · `spawns.json` · `mapa.json` · `items.json` · `vip.json` · `killfeed.json` · `serverinfo.json` · `chat.json`

**Datos (los maneja el server, no editar a mano salvo que sepas):**
- `groups/<id>.json` — grupos/party persistidos
- `storage/<id>.json` — contenido virtualizado de barriles
- `raidlog/raid_AAAA-MM-DD.txt` — logs forenses anti-raid (auto-purga)
- `stats.json` — ranking (kills/deaths/suicidios)
- `vip_state.json` — usos de equipamiento por VIP/ciclo

---

## types.xml (barril como loot)

Recomendado: spawnear la versión **empaquetada**; la desplegada en `nominal=0`.

```xml
<type name="Exor_Barrel_500_Packed">
    <nominal>4</nominal> <lifetime>14400</lifetime> <restock>1800</restock>
    <min>2</min> <quantmin>-1</quantmin> <quantmax>-1</quantmax> <cost>100</cost>
    <flags count_in_cargo="0" count_in_hoarder="0" count_in_map="1" count_in_player="0" crafted="0" deloot="0"/>
    <category name="containers"/> <usage name="Military"/>
</type>
<type name="Exor_Barrel_500">
    <nominal>0</nominal> <lifetime>3888000</lifetime> <restock>0</restock>
    <min>0</min> <quantmin>-1</quantmin> <quantmax>-1</quantmax> <cost>100</cost>
    <flags count_in_cargo="0" count_in_hoarder="0" count_in_map="1" count_in_player="0" crafted="0" deloot="0"/>
    <category name="containers"/>
</type>
```

> **Tip vanilla:** vehículos/items spawneados por admin/VPP que no estén en `types.xml` los limpia el CE a los ~45 s. Si tu server hace eso, agregá cada classname con `nominal=0` y `lifetime=3888000`.

---

## Estructura del repo

```
├── src/ExorStorage/          # Fuente del PBO (prefix: ExorStorage)
│   ├── config.cpp            # CfgPatches / CfgMods / CfgVehicles / CfgMagazines
│   ├── data/                 # Texturas .paa (generadas por el build)
│   ├── gui/                  # Layouts de UI (.layout)
│   └── scripts/
│       ├── 3_Game/           # Config (todos los JSON), RPC, constantes
│       ├── 4_World/          # Server/world: entidades, acciones, party, anti-raid, chat, stats, VIP...
│       └── 5_Mission/        # Cliente: HUD, menús, nameplates, killfeed, chat, server info...
├── assets/textures/          # PNG fuente de las texturas
├── tools/                    # build.ps1, gen_textures.py, pack_pbo.py
├── mod/mod.cpp               # Metadata para el launcher/Workshop
├── keys/                     # 3xorVO.bikey (pública) — la privada NO está en el repo
└── docs/                     # Plan y documentación de diseño
```

## Build

Requisitos: Windows, Python 3 con Pillow, [DayZ Tools](https://store.steampowered.com/app/830640/DayZ_Tools/) (para `ImageToPAA` y la firma).

```powershell
.\tools\build.ps1                # texturas + PBO -> dist\@3xor_Vanilla_Optimization
.\tools\build.ps1 -SkipTextures  # solo re-empaqueta + firma el PBO
```

## Instalación

**Servidor:**
1. Copiar `@3xor_Vanilla_Optimization` a la raíz del server.
2. Agregar `-mod=@3xor_Vanilla_Optimization` a los parámetros de arranque.
3. Copiar `keys/3xorVO.bikey` a la carpeta `keys/` del server (si no, BattlEye kickea).
4. (Opcional) Agregar los barriles a `types.xml` para que spawneen como loot.

**Cliente:** suscribirse al mod en Steam Workshop — el launcher lo carga solo.

## Firma

El PBO se firma en el build con `keys/3xorVO.biprivatekey` (privada, **no** está en el repo). La `.bikey` pública viaja con el mod en `keys/`.
