# 3xor_Vanilla_Optimization

Mod **todo-en-uno** para DayZ (cliente + servidor), **100% standalone**. Combina sistemas de **comunidad/PvP** (party, territorio, anti-raid, spawns, chat, killfeed, VIP) con **optimización del servidor** (barriles virtualizados, vehículos que se duermen, stacks de munición) — menos entidades vivas = menos lag.

> Antes llamado "3xorStorage"; se renombró porque dejó de ser solo storage.

## Dependencias

**NINGUNA.** El mod **no requiere ningún otro mod** de Steam Workshop. Solo usa addons **vanilla** de DayZ:

```
requiredAddons[] = { DZ_Data, DZ_Scripts, DZ_Gear_Containers, DZ_Weapons_Ammunition,
                     DZ_Gear_Camping, DZ_Characters_Backpacks, DZ_Characters };
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
- **No desmantelar** estructuras (muros, puertas, ventanas, torres, etc.) en territorio ajeno si no sos del party — cubre construcciones **vanilla y BaseBuildingPlus (BBP)** (la cobertura BBP se activa sola si BBP está cargado; en un server vanilla no afecta nada).
- **No construir** (deployables, kits, **campos de plantación**) en territorio ajeno — bloqueado en cliente y reforzado server-side.
- **Logs forenses** (`ServerAuditLog/audit_AAAA-MM-DD.txt`, auto-purga): robo de items, **apertura de barriles 3xor**, deslogueo y expulsión en base ajena, con steamid + nombre + pos + hora.
- **Anti-combat-log**: al reconectar dentro de territorio ajeno, te teletransporta al borde.

### Anti-cheat (heurístico, server-side)
Mide geometría y resultados con los eventos del motor para dar **indicios** (NUNCA banea solo: hay falsos positivos por lag/desync). Por defecto **solo evalúa a los SteamIDs de `watchlist`** (`solo_watchlist`). Todo se escribe en vivo al mismo log de auditoría (`ServerAuditLog/audit_*.txt`).
- **Por kill** (`SOSPECHA_KILL`): mató a través de pared (línea de visión bloqueada), apuntando lejos de la víctima (aimbot / "mirando al cielo"), o a distancia imposible para el arma. Cuenta señales → nivel BAJO/MEDIO/ALTO.
- **Watchlist** (solo los vigilados, muestreo ~1 Hz): apunta a un oculto con arma en alto = ESP/prefire (`WATCH_MIRA`); corre derecho hacia un oculto (`WATCH_APROX`); **sigue con la mira a alguien que no debería ver** (lejos hasta 1500 m u oculto) varios ticks = ESP (`WATCH_SEGUIMIENTO`); velocidad imposible/teleport (`WATCH_VELOCIDAD`); por debajo del terreno/noclip (`WATCH_BAJO_TIERRA`); recibe impactos reales seguidos sin perder vida = god mode (`WATCH_GODMODE`); giro de mira imposible sostenido = spinbot (`WATCH_SPINBOT`).
- **`exentos`**: SteamIDs a los que NO se les aplica nada de anti-cheat (admins/confianza); el ranking/stats se les suma igual.

### Spawns
- Pantalla de selección al morir / primer login, con puntos configurables + "Mi base".
- **Cooldown por punto** y por base, con cuenta regresiva en vivo (gris + rojo cuando no disponible).
- Respawn en base (cooldown configurable, requiere bandera arriba).

### VIP
- **Spawn en base + Equipamiento**: reemplaza la ropa por un loadout (pantalón/camisa/zapato/bolso + items extra) y gasta 1 uso.
- **Vencimiento a los 30 días** (`dias_vip`) desde la `fecha_ingreso`: pasados, deja de contar como VIP automáticamente.
- **Los usos NO se reponen solos.** La única forma de renovar es **editar a mano** en `vip.json` la `fecha_ingreso` (reinicia los 30 días y repone los usos) y/o subir `usos_por_mes`. Pensado para que el jugador avise cuando se le venció y vos controles.
- **Distancia en marcas** (ver HUD): solo los VIP ven la distancia a las marcas del party.

### HUD / UI
- HUD de party (barra de vida + nombre + distancia), **nameplates 3D**, **mapa** (M) con tu posición + la de tu grupo, **marcas** (T pone / Y limpia). Las marcas muestran la **distancia en verde** solo a los jugadores **VIP**.
- **Killfeed** (arriba-derecha) PvP/suicidio.
- **Panel de info del server** (ESC → "Server Info"): tabs General / Reglas (texto editable) y **Score** (ranking kills/deaths/suicidios/distancia).
- **Tooltip de items**: pastilla de rareza por tier + barra de durabilidad.

### Chat
- **2 canales**: GLOBAL (a todos) y ZONA (proximity, radio configurable). Tecla **`.`** cambia el canal.
- Panel abajo-izquierda (nombre azul + mensaje blanco), líneas con duración configurable. **Reemplaza el chat vanilla.**
- **Anti-spam**: cooldown entre mensajes + bloqueo de mensaje repetido.

### Vehículos
- **Conductor en 3ª persona** (forzada — funciona incluso en servers de solo-1ª-persona) / pasajeros forzados a 1ª; ver tu inventario + el del auto; quitar daño (toggle).
- **Sueño automático**: autos inactivos X min dejan de simular física (gran ahorro) y despiertan solos cuando alguien se acerca.
- **Voltear vehículo** (acción con hold, anti-abuso).
- **Inventario ampliado**: el baúl de **todos los autos vanilla** (Olga 24/OffroadHatchback, Sedan/CivilianSedan, Gunter 2/Hatchback_02, Sedan_02, ADA 4x4/Offroad_02, M3S/Truck_01_Covered) pasa a **600 slots**, y permite guardar **ropa/contenedores con items adentro** (como los barriles). El tamaño de cargo es de build (no se togglea por JSON); lo anidado sí (`inv_items_anidados`).

### Storage (barriles 3xor)
- `Exor_Barrel_500` (500 slots) + su versión **empaquetable** (caja transportable).
- **Virtualización** del contenido a disco (menos entidades), **auto-cierre**, **anti-dupe**, comida que dura más, mochilas/ropa con items adentro.
- **Indestructible y no lockeable** (la defensa es la base, no el barril).

### Munición
- **Stacks a 100** (solo balas sueltas/bolts/flechas), **auto-stack** al recoger, **cantidad aleatoria al spawnear** por tipo.

### Reparación y kits
- **Reparar a pristine**: al reparar con cualquier kit, el ítem llega hasta **pristine** (verde) en vez de toparse en "gastado".
- **Combinar kits gastados**: 2 kits del mismo tipo (costura, limpieza de armas, piedra de afilar, etc.) se **unen sumando su uso** (2 al 50% → 1 al 100%). Lista configurable.

### Bolsa de cadáver (lápida)
- Al morir, ~1s después el cuerpo se convierte en una **lápida** (sin colisión, no se mueve) con **todo el loadout** (ropa/chaleco/mochila/bolsillos **+ las dos armas**) — se lootea como un cadáver. Las armas conservan su cargador, mira y supresor.
- **Persiste** (configurable, default 2h) y **sobrevive el reinicio** del server.
- El loot queda **siempre como entidades reales** durante toda la vida de la lápida: se ve y se lootea sin depender de proximidad (nada se "pierde" ni desaparece).

### Zonas de no-construcción
- Definí uno o más **centros (coordenadas x,y,z) con un radio en metros** donde **nadie puede construir/colocar nada**: bases, deployables, torres, etc. Ideal para proteger military, KOTH, spawns, traders.
- **Whitelist** de classnames que **sí** se permiten dentro de la zona (minas/trampas/explosivos/fuegos artificiales), para no romper el PvP.
- Cubre construcción **vanilla, BuildEverywhere y BaseBuildingPlus (BBP)**. Funciona con o sin BBP cargado. Solo afecta colocaciones **nuevas** (las bases existentes nunca se tocan).

### Vanilla tweaks
- **Ghillies al slot del brazalete**: los ghillie vanilla pasan al slot Armband, liberando la espalda (podés llevar **bolso + ghillie** a la vez).

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
| `inv_items_anidados` | `true` | Permite guardar ropa/contenedores con items adentro en el baúl (el cargo de 600 es siempre, de build) |
| `camara.conductor_3ra_persona` | `true` | El conductor va en 3ª (forzada, incluso en servers de 1ª persona) |
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
| `bloquear_desmantelar_ajeno` | `true` | Ajenos no desmantelan en territorio ajeno (vanilla + BBP) |
| `log_robo_contenedor` | `true` | Loguea robo de items en territorio ajeno |
| `log_abrir_barril_ajeno` | `true` | Loguea apertura de barriles 3xor en territorio ajeno |
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
| `dias_vip` | `30` | Días que dura el VIP desde `fecha_ingreso`. Pasados, deja de contar como VIP |
| `equip_habilitado` | `true` | Activa el perk "Spawn en base + Equipamiento" |
| `equip_usos_por_mes` | `7` | Default global de usos de equipamiento (si la entrada tiene 0). **No se reponen solos** |
| `equip_loadout` | ejemplo | `pantalon`, `camisa`, `zapato`, `bolso` (vacío = no tocar) + `items_extra[]` (al cargo de camisa/pantalón) |

> **Renovar un VIP:** editar su `fecha_ingreso` (reinicia los 30 días **y** repone los usos) y/o subir `usos_por_mes`. Los usos no se reponen automáticamente.

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

### `reparacion.json`
| Parámetro | Default | Qué hace |
|---|---|---|
| `reparar_a_pristine` | `true` | Al reparar con cualquier kit, el ítem llega hasta pristine (verde) |
| `kits_stackeables` | costura/cuero/limpieza/piedra/whetstone/epoxy/duct tape/tire kit | Classnames de kits que se pueden combinar entre sí sumando su uso |

### `bodycadaver.json`
| Parámetro | Default | Qué hace |
|---|---|---|
| `habilitado` | `true` | Master on/off de la bolsa de cadáver (lápida) |
| `delay_segundos` | `1` | Demora entre la muerte y la aparición de la lápida |
| `duracion_minutos` | `120` | Cuánto dura la lápida (2h). Sobrevive reinicio |
| `acercar_metros` / `alejar_metros` / `virtualizar_minutos` | — | **Obsoletos**: la lápida ya no virtualiza (el loot queda siempre real). Se ignoran |

### `nobuild.json`
Zonas donde no se puede construir. Los cambios se aplican **reiniciando el server**.
| Parámetro | Default | Qué hace |
|---|---|---|
| `activado` | `false` | Master on/off de todas las zonas |
| `whiteList` | (minas/trampas/explosivos/fuegos) | Classnames permitidos **siempre**, aun dentro de una zona (match por *contains*, sin distinguir mayúsculas) |
| `lugares_no_permitidos` | `[]` | Lista de zonas. Cada una: `posicion` `{x,y,z}` (coords del mundo; pegá las del VPP admintools, `y`=altura) + `desabilitar_construccion_en_metros` (radio horizontal) |

### `anticheat.json`
Todo server-side. Por defecto solo evalúa a los SteamIDs de `watchlist`; los de `exentos` se saltean por completo.
| Parámetro | Default | Qué hace |
|---|---|---|
| `habilitado` | `true` | Master on/off de todo el anti-cheat |
| `solo_watchlist` | `true` | `true` = solo evalúa a los de `watchlist` (kill incluido); `false` = el detector por kill corre sobre todos |
| `detector_kill` | `true` | Activa el análisis en cada kill PvP |
| `kill_check_los` / `kill_check_angulo` / `kill_check_distancia` | `true` | Señales del kill: LOS bloqueada / ángulo arma→víctima / distancia por arma |
| `kill_angulo_grados` | `30` | Ángulo arma→víctima por encima del cual = "mató sin apuntar" |
| `kill_min_senales` | `1` | Mínimo de señales coincidentes para loguear el kill |
| `dist_sospechosa_default` | `400` | Umbral genérico de distancia por arma (m); `0` = no chequear |
| `dist_sospechosa_por_arma` | (ejemplos) | `classname → metros` (ej. pistolas más bajo) |
| `watchlist_activa` | `true` | Activa el muestreo ~1 Hz de la watchlist |
| `watch_check_mira` / `_aproximacion` / `_seguimiento` | `true` | ESP: apunta a oculto / corre a oculto / sigue a lejano-u-oculto |
| `watch_check_velocidad` / `_bajo_tierra` / `_godmode` / `_spinbot` | `true` | speedhack-teleport / noclip / god mode / spinbot |
| `watch_angulo_grados` | `8` | Tolerancia de "apunta/corre/sigue directo" al objetivo |
| `watch_dist_min` | `25` | Ignorar objetivos a menos de esto (ruido a corta) |
| `watch_velocidad_max` | `12` | m/s a pie por encima de lo cual = sospechoso (vehículos exentos) |
| `watch_bajo_tierra_metros` | `2.5` | Metros bajo la superficie para marcar noclip |
| `godmode_hits` | `4` | Impactos reales seguidos sin perder vida para marcar god mode |
| `spinbot_grados` / `spinbot_ticks` | `120` / `3` | Giro de mira por tick y ticks seguidos para marcar spinbot |
| `watch_track_dist_max` | `1500` | Alcance máximo del seguimiento (m) |
| `watch_lejos_metros` | `800` | Con LOS clara, más allá de esto = no debería verlo (cuenta para el seguimiento) |
| `watch_track_ticks` | `4` | Ticks seguidos rastreando a un oculto/lejano para marcar ESP |
| `watch_log_cooldown_seg` | `15` | No repetir el mismo aviso del vigilado antes de esto |
| `watchlist` | `[]` | SteamIDs a vigilar (vacío = no evalúa a nadie) |
| `exentos` | `[]` | SteamIDs a los que NO se les aplica anti-cheat (stats sí) |

---

## Archivos que crea el mod (en `<profile>/3xorVanillaOptimization/`)

**Config (editables):** `storage.json` · `vehiculos.json` · `municion.json` · `party.json` · `spawns.json` · `mapa.json` · `items.json` · `vip.json` · `killfeed.json` · `serverinfo.json` · `chat.json` · `reparacion.json` · `bodycadaver.json` · `nobuild.json` · `anticheat.json` · `koth.json`

**Datos (los maneja el server, no editar a mano salvo que sepas):**
- `groups/<id>.json` — grupos/party persistidos
- `storage/<id>.json` — contenido virtualizado de barriles
- `ServerAuditLog/audit_AAAA-MM-DD.txt` — logs forenses anti-raid + anti-cheat (auto-purga)
- `stats.json` — ranking (kills/deaths/suicidios)
- `vip_state.json` — usos de equipamiento por VIP

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

> **Tip vanilla:** vehículos/items spawneados por herramientas de admin que no estén en `types.xml` los limpia el CE a los ~45 s. Si tu server hace eso, agregá cada classname con `nominal=0` y `lifetime=3888000`.

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
