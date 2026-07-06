# 3xor_Vanilla_Optimization

Mod **todo-en-uno** para DayZ (cliente + servidor), **100% standalone**. Combina sistemas de **comunidad/PvP** (party, territorio, anti-raid, spawns, chat, killfeed, VIP, KOTH, evento Cofre) con **optimización del servidor** (barriles virtualizados, vehículos que se duermen, stacks de munición) — menos entidades vivas = menos lag.

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
- **Self-heal del mástil**: si un crash corrompe la persistencia y el mástil queda "fantasma" (el grupo sobrevive pero el poste no), se recrea solo al reconectar un miembro, en la misma posición y con la bandera guardada.

### Anti-raid (gateado por el radio del mástil ajeno)
- **No desmantelar** estructuras (muros, puertas, ventanas, torres, etc.) en territorio ajeno si no sos del party — cubre construcciones **vanilla y BaseBuildingPlus (BBP)** (la cobertura BBP se activa sola si BBP está cargado; en un server vanilla no afecta nada).
- **No construir** (deployables, kits, **campos de plantación**) en territorio ajeno — bloqueado en cliente y reforzado server-side.
- **Logs forenses** (`ServerAuditLog/audit_AAAA-MM-DD.txt`, auto-purga): robo de items, **apertura de barriles 3xor**, deslogueo y expulsión en base ajena, **combat-log** en PvP, **farmeo de kills** y **aviso de clan inactivo**, con steamid + nombre + pos + hora.
- **Anti-combat-log**: al reconectar dentro de territorio ajeno, te teletransporta al borde.

### Anti-cheat (heurístico, server-side)
Mide geometría y resultados con los eventos del motor para dar **indicios** (NUNCA banea solo: hay falsos positivos por lag/desync). Por defecto **solo evalúa a los SteamIDs de `watchlist`** (`solo_watchlist`). Todo se escribe en vivo al mismo log de auditoría (`ServerAuditLog/audit_*.txt`).
- **Por kill** (`SOSPECHA_KILL`): mató a través de pared (línea de visión bloqueada) o a distancia imposible para el arma. Cuenta señales → nivel BAJO/MEDIO/ALTO.
- **Watchlist** (solo los vigilados, muestreo ~1 Hz): apunta a un oculto con arma en alto = ESP/prefire; corre derecho hacia un oculto; **sigue con la mira a alguien que no debería ver** (lejos hasta 1500 m u oculto); velocidad imposible/teleport; por debajo del terreno/noclip; god mode; spinbot; **puntería** (accuracy/headshots por tiroteo) = aimbot.
- **`exentos`**: SteamIDs a los que NO se les aplica nada de anti-cheat (admins/confianza); el ranking/stats se les suma igual.

### KOTH (King of the Hill)
- Eventos de captura por color (amarillo/verde/morado), **independientes** y simultáneos, cada uno con su propia config y ciclo.
- Humo del color, bandera que sube con el progreso, **bonos por jugadores** en el radio y por proximidad al mástil, avisos + marca en el mapa, **recompensa** en supply crate con loot por %, zombies y osos.

### Evento Cofre
- Zonas con **horario por día** donde se pueden abrir cofres. El player lleva una **caja cerrada** (item del mod, 3 colores) a la mesa del evento y la suelta: se coloca sola sobre la mesa y tras unos minutos se abre con un **loot aleatorio por color**.
- Estructura del evento (mesa + drill + luz de roadflare), avisos killfeed y marca en el mapa. Gracia post-evento para no perder cajas.

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

### Auto-run (correr-solo)
- Tecla configurable (default **Z**) para que el personaje corra solo; se cancela al tocar movimiento.

### Vehículos
- **Conductor en 3ª persona** (forzada — funciona incluso en servers de solo-1ª-persona) / pasajeros forzados a 1ª; ver tu inventario + el del auto; quitar daño (toggle).
- **Sueño automático**: autos inactivos X min dejan de simular física (gran ahorro) y despiertan solos cuando alguien se acerca.
- **Voltear vehículo** (acción con hold, anti-abuso).
- **Inventario ampliado**: el baúl de **todos los autos vanilla** (Olga 24/OffroadHatchback, Sedan/CivilianSedan, Gunter 2/Hatchback_02, Sedan_02, ADA 4x4/Offroad_02, M3S/Truck_01_Covered) pasa a **600 slots**, y permite guardar **ropa/contenedores con items adentro** (como los barriles). El tamaño de cargo es de build (no se togglea por JSON); lo anidado sí (`inv_items_anidados`).

### Storage (barriles 3xor)
- `Exor_Barrel_500` (500 slots) + su versión **empaquetable** (caja transportable).
- **Virtualización** del contenido a disco (menos entidades), **auto-cierre**, **anti-dupe**, comida que dura más, mochilas/ropa con items adentro.
- **Indestructible y no lockeable** (la defensa es la base, no el barril). Un barril en un **slot de barril de vehículo** acepta items; en el cargo suelto del auto, no.

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
| **Z** | Auto-run (correr-solo); tecla configurable |
| **ESC** → "Server Info" | Panel de info del server (General/Reglas/Score) |

---

## Configuración — referencia módulo por módulo

Todo se configura por JSON en `<profile>/3xorVanillaOptimization/`. **Cada archivo se crea solo con sus defaults al primer arranque**; los campos nuevos se auto-completan al cargar (excepto `koth.json` y `cofre.json`, que son *no-resave*: si ya existen se respetan EXACTOS y no se sobreescribe lo que editaste). **Los cambios se aplican reiniciando el server.** Si existía el `settings.json` monolítico viejo, se migra automáticamente.

Columna **Valores** = qué puede tomar cada campo. `bool` = `true`/`false`. `int`/`float` = número (los rangos indicados son recomendados, no límites duros salvo que se aclare). `string` = texto entre comillas (classname del juego, tecla, etc.). `[…]` = lista.

### `storage.json` — barriles 3xor
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `version` | `1` | int | Versión interna del archivo. No tocar. |
| `auto_cerrar_segundos` | `10` | int seg (`0`=off) | Segundos tras alejarse el jugador para que el barril se cierre solo. Mide cercanía, no el último movimiento: no se cierra mientras alguien está cerca usándolo. |
| `virtualizar_segundos` | `10` | int seg (`0`=off) | Segundos ya cerrado tras los que saca su loot del mundo a disco (menos entidades = menos lag; a prueba de reinicios). Al abrir restaura todo. |
| `cerrar_distancia_metros` | `5.0` | float m | Radio para considerar "hay un jugador usando el barril" (mantiene abierto mientras alguien está dentro de este radio). |
| `multiplicador_comida` | `2.0` | float ≥1 | La comida guardada dentro dura N× más. |
| `permitir_ropa_con_items` | `true` | bool | Permite guardar mochilas/ropa con items adentro. |
| `blacklist` | `[]` | [string] classnames | Classnames que NO se pueden guardar en el barril. |
| `cooldown_abrir_segundos` | `3` | int seg (`0`=sin cooldown) | Anti-dupe: espera mínima para reabrir el mismo barril. |

### `vehiculos.json` — sueño + cámara + inventario + daño
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `vehiculos_dormir` | `true` | bool | Activa el sueño de vehículos inactivos (dejan de simular física). |
| `vehiculos_dormir_minutos` | `5` | int min (`0`=off) | Minutos de inactividad sin jugadores cerca antes de dormir. |
| `vehiculos_despertar_metros` | `30` | int m | Radio de jugadores para dormir/despertar el vehículo. |
| `vehiculos_excluidos` | `[]` | [string] classnames | Vehículos que nunca duermen. |
| `voltear_vehiculos` | `true` | bool | Activa la acción "Voltear vehículo". |
| `voltear_segundos` | `40` | int seg | Duración del hold para voltear (anti-abuso). |
| `inv_items_anidados` | `true` | bool | Permite guardar ropa/contenedores CON items adentro en el baúl. El cargo de 600 slots es de build (no se togglea). |
| `camara.conductor_3ra_persona` | `true` | bool | El conductor va en 3ª persona (forzada, incluso en servers de solo-1ª). |
| `camara.pasajeros_1ra_persona` | `true` | bool | Los no-conductores van forzados a 1ª persona. |
| `inventario.ver_ambos_dentro` | `true` | bool | Ver tu inventario + el del auto a la vez. |
| `dano.quitar_dano_vehiculos` | `false` | bool | `true` = los autos no reciben daño. |

### `municion.json` — stacks y cantidad al spawnear
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `stack_municion_default` | `100` | int | Relleno con el que se auto-completa `stack_municion` para cada bala. Ojo: el stack máx real (100) es de `config.cpp`, el motor no lo cambia por JSON. |
| `stack_municion` | auto | map classname→int | Stack máximo por classname (auto-completado con todas las balas). |
| `auto_stack` | `true` | bool | Las balas sueltas que recogés se fusionan solas. |
| `spawn_municion_min_default` | `15` | int | Relleno del mínimo de cantidad al spawnear una pila. |
| `spawn_municion_max_default` | `65` | int | Relleno del máximo de cantidad al spawnear una pila. |
| `spawn_municion` | auto | map classname→`{min,max}` | Rango de cantidad al spawnear por tipo de munición. |
| `municion_excluida` | 40mm + bengalas + RPG/LAW/M4 | [string] classnames | Munición que nunca se toca (ni stack ni cantidad). |

### `party.json`
**`territorio`**
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `habilitado` | `true` | bool | Activa TODO el sistema bandera=territorio. |
| `radio_metros` | `35` | int m | Radio de anti-construcción alrededor del mástil (no-miembros). |
| `permitir_construir_cerca` | `false` | bool | `true` = ajenos pueden construir cerca. |
| `whitelist_construible` | minas/claymore/plástico | [string] classnames | Lo que SÍ se puede poner en territorio ajeno (aunque esté bloqueado). |
| `blacklist_construible` | `[]` | [string] classnames | Lo que nunca se puede poner (ni el dueño). |
| `despawn_mastil_sin_miembros` | `true` | bool | Despawnea el mástil si el party se queda sin miembros. |

**`grupo`**
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `habilitado` | `true` | bool | Master on/off del sistema de party. |
| `max_miembros` | `8` | int | Máximo de miembros por grupo. |
| `tecla_menu` | `"P"` | string tecla | Tecla del menú party. |
| `auto_kick_dias` | `0` | int días (`0`=off) | Días sin login para auto-expulsar a un miembro. |
| `mostrar_posicion_miembros` | `true` | bool | Compartir posición/vida (gobierna HUD + nameplates + mapa). |
| `mostrar_hud` | `true` | bool | Barra de vida arriba-izquierda. |
| `mostrar_nameplates` | `true` | bool | Nombre 3D sobre la cabeza de los miembros. |
| `mostrar_distancia_miembros` | `true` | bool | Número de distancia en HUD/nameplate. |
| `mostrar_propio` | `false` | bool | Incluirte a vos mismo en HUD/nameplates (`false` = solo amigos). |
| `permitir_marker_equipo` | `true` | bool | Marcas de equipo con T (poner) / Y (limpiar). |

**`bandera`**
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `ajenos_pueden_bajar` | `true` | bool | Si ajenos pueden bajar tu bandera. |
| `bajada_bloquea_respawn` | `true` | bool | Con la bandera abajo se bloquea el respawn en base. |
| `bandera_blanca` | `false` | bool | Cuelga bandera blanca de protección al reclamar. |
| `bandera_blanca_minutos` | `10080` | int min | Protección en minutos reales (10080 = 7 días; 1440 = 1 día). |
| `bandera_blanca_cambiar_a` | `"Flag_DayZ"` | string classname (`""`=no cambiar) | Bandera que reemplaza a la blanca al expirar (`""` = solo destrabar el slot). |

**`respawn_base`**
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `habilitado` | `true` | bool | Interruptor maestro del respawn en tu base/mástil. |
| `cooldown_segundos` | `3600` | int seg | Cooldown entre respawns en base (3600 = 1h). |
| `permitir_spawn_mastil_vip` | `true` | bool | Los VIP (`vip.json`) pueden spawnear en el mástil. |
| `permitir_spawn_mastil_no_vip` | `false` | bool | El resto (no-VIP) puede spawnear en el mástil. |

**`proteccion`** (anti-raid, todo server-side; el log es forense)
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `bloquear_desmantelar_ajeno` | `true` | bool | Ajenos no desmantelan muros/torres en territorio ajeno (vanilla + BBP). |
| `log_robo_contenedor` | `true` | bool | Loguea cuando un ajeno toma items dentro de territorio enemigo. |
| `log_abrir_barril_ajeno` | `true` | bool | Loguea cuando un ajeno abre un barril 3xor en territorio enemigo. |
| `log_desconexion_base_ajena` | `true` | bool | Loguea si un ajeno se desloguea dentro de territorio enemigo. |
| `sacar_de_base_ajena_al_reconectar` | `true` | bool | Al reconectar dentro de territorio ajeno, teletransporta al borde. |
| `log_dias_retener` | `7` | int días (`0`=nunca borrar) | Días que se conservan los archivos de raidlog. |
| `aviso_clan_inactivo` | `true` | bool | Avisa en el raidlog cuando un clan entero queda inactivo. |
| `inactividad_dias` | `21` | int días (`0`=off) | Umbral de inactividad de un clan (21 = 3 semanas). |
| `log_combat_log` | `true` | bool | Loguea deslogueo dentro de una zona de combate PvP (combat-log). |
| `combat_log_minutos` | `8` | int min (`0`=off) | Minutos que la zona de combate sigue viva tras el último daño PvP. |
| `combat_log_radio` | `150` | float m | Radio de la zona de combate alrededor de cada participante (tirador y víctima). |
| `log_farmeo_kills` | `true` | bool | Loguea posible farmeo (un killer mata al MISMO steamid muchas veces). |
| `farmeo_ventana_minutos` | `240` | int min | Ventana deslizante para contar los kills repetidos (240 = 4h). Persiste entre reinicios. |
| `farmeo_umbral` | `4` | int (`0`=off) | A partir de cuántos kills al mismo steamid en la ventana se loguea. |

### `spawns.json` — puntos de spawn seleccionables
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `habilitado` | `true` | bool | Activa la pantalla de selección de spawn. |
| `dar_cuchillo_al_spawnear` | `true` | bool | **TEST**: da un cuchillo al personaje nuevo (suicidio fácil al probar). Poner `false` en prod. |
| `equipar_npc_test` | `false` | bool | **TEST LOCAL**: equipa los NPC dummy del VPP para probar la tumba. Siempre `false` en prod. |
| `puntos[]` | 1 ejemplo | lista | Cada punto: `nombre` (string), `x`/`z` (float mundo), `y` (float; `0`=al suelo), `cooldown_segundos` (int), `distancia_random` (float m; radio aleatorio alrededor del punto). |

### `mapa.json`
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `abrir_con_m` | `true` | bool | Abrir el mapa con M sin necesitar el ItemMap físico. |
| `mostrar_mi_posicion` | `true` | bool | Ver siempre tu posición en el mapa. |
| `mostrar_miembros_party` | `true` | bool | Ver a los miembros del party en el mapa. |

### `items.json` — tooltip (durabilidad + rareza)
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `mostrar_durabilidad` | `true` | bool | Barra de durabilidad en el tooltip del item. |
| `mostrar_rareza` | `true` | bool | Pastilla de rareza (tier) en el tooltip. |
| `rareza_usar_tabla` | `false` | bool | `true` = usar `rareza_tabla`; `false` = heurística automática por clase. |
| `rareza_tabla` | ejemplos | map classname→tier | Tier por item: `comun` / `poco_comun` / `raro` / `epico` / `legendario` (en minúscula). |

### `vip.json`
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `vips[]` | 1 ejemplo | lista | Cada VIP: `steamid` (string SteamID64), `fecha_ingreso` (`"AAAA-MM-DD"`; vacío = se sella con hoy al arrancar), `usos_por_mes` (int; `0` = usar el default global). |
| `equip_habilitado` | `true` | bool | Activa el perk "Spawn en base + Equipamiento". |
| `equip_usos_por_mes` | `7` | int | Default global de usos de equipamiento (si la entrada del player tiene 0). **No se reponen solos.** |
| `dias_vip` | `30` | int días | Días que dura el VIP desde `fecha_ingreso`. Pasados, deja de contar como VIP. |
| `equip_loadout` | ejemplo | objeto | `pantalon`/`camisa`/`zapato`/`bolso`/`guantes`/`mascara` (string classname, `""`=no tocar) + `full_comida_bebida` (bool; deja al VIP con 100% comida/bebida) + `items_extra[]` (classnames al cargo de camisa/pantalón). |

> **Renovar un VIP:** editar su `fecha_ingreso` (reinicia los `dias_vip` **y** repone los usos) y/o subir `usos_por_mes`. Los usos no se reponen automáticamente.

### `killfeed.json`
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `habilitado` | `true` | bool | Master on/off del killfeed (arriba-derecha). |
| `duracion_segundos` | `6` | int seg | Cuánto dura cada línea en pantalla. |
| `max_lineas` | `5` | int | Máximo de líneas simultáneas (la más vieja se va). |
| `mostrar_suicidios` | `true` | bool | Mostrar la línea "se ha suicidado". |
| `killboard_excluidos` | owner steamid | [string] SteamID64 | Quiénes NO suman ni figuran en el ranking (admins/owner). |

### `serverinfo.json` — panel de info (ESC)
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `habilitado` | `true` | bool | Muestra el botón "Server Info" en el menú de ESC. |
| `titulo` | "Información del Server" | string | Título del panel. |
| `tab_general` | `true` | bool | Muestra la tab General. |
| `general_titulo` | "General" | string | Título de la tab General. |
| `general_lineas` | texto ejemplo | [string] | Líneas de texto de la tab General (editable). |
| `tab_reglas` | `true` | bool | Muestra la tab Reglas. |
| `reglas_titulo` | "Reglas" | string | Título de la tab Reglas. |
| `reglas_lineas` | texto ejemplo | [string] | Líneas de texto de la tab Reglas (editable). |
| `tab_score` | `true` | bool | Muestra la tab Score (ranking). |
| `score_titulo` | "Score" | string | Título de la tab Score. |
| `discord_texto` | "Discord" | string | Texto del botón grande de la tab General. |
| `discord_url` | google (placeholder) | string URL | A dónde lleva el botón al hacer click. |

### `chat.json` — chat custom con canales
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `habilitado` | `true` | bool | Master on/off del chat custom (reemplaza el vanilla). |
| `radio_zona_metros` | `50` | int m | Alcance del canal ZONA (proximity). |
| `duracion_segundos` | `25` | int seg | Cuánto dura cada línea en pantalla. |
| `max_lineas` | `9` | int | Máximo de líneas simultáneas (la más vieja se va). |
| `cooldown_segundos` | `2` | int seg (`0`=sin límite) | Anti-spam: mínimo entre mensajes. |
| `bloquear_repetidos` | `true` | bool | Anti-spam: bloquear el mismo mensaje seguido. |

### `reparacion.json`
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `reparar_a_pristine` | `true` | bool | Al reparar con cualquier kit, el ítem llega hasta pristine (verde) en vez de topar en "gastado". |
| `kits_stackeables` | costura/cuero/limpieza/piedra/whetstone/epoxy/duct tape/tire kit | [string] classnames | Kits que se pueden combinar entre sí sumando su uso (2 gastados → 1). |

### `bodycadaver.json` — bolsa de cadáver / lápida
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `habilitado` | `true` | bool | Master on/off de todo el módulo de la lápida. |
| `delay_segundos` | `1` | int seg | Demora entre la muerte y la aparición de la lápida. |
| `duracion_minutos` | `120` | int min | Cuánto dura la lápida (2h). Sobrevive reinicio. |
| `acercar_metros` / `alejar_metros` / `virtualizar_minutos` | — | — | **Obsoletos**: las tumbas ya no virtualizan (el loot queda siempre real). El código los ignora. |

### `nobuild.json` — zonas de no-construcción
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `activado` | `false` | bool | Master on/off de todas las zonas. |
| `whiteList` | minas/trampas/explosivos/fuegos | [string] classnames | Classnames permitidos SIEMPRE, aun dentro de una zona (match por *contains*, sin distinguir mayúsculas). |
| `lugares_no_permitidos[]` | 1 ejemplo (radio 1200) | lista | Cada zona: `posicion` `{x,y,z}` (coords del mundo del VPP admintools; `y`=altura, se ignora) + `desabilitar_construccion_en_metros` (float radio horizontal; `0` = zona ignorada). |

### `autorun.json` — correr-solo (client-side)
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `habilitado` | `true` | bool | Permite el auto-run. |
| `tecla` | `Z` (`KC_Z`) | KeyCode | Tecla para activar/desactivar. Ej: `KC_Z`, `KC_X`, `KC_C`, `KC_V`, `KC_R`… |
| `velocidad` | `3` | `1`/`2`/`3` | 1 = caminar, 2 = trotar, 3 = sprint (baja a trote sin stamina). |
| `parar_con_movimiento` | `true` | bool | Tocar W/A/S/D cancela el auto-run. |

### `anticheat.json` — heurístico, solo server
Por defecto solo evalúa a los SteamIDs de `watchlist` (`solo_watchlist=true`); los de `exentos` se saltean por completo. Todo va al log de auditoría; NUNCA banea solo.
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `habilitado` | `true` | bool | Master on/off de todo el anti-cheat. |
| `solo_watchlist` | `true` | bool | `true` = solo evalúa a los de `watchlist` (kill incluido); `false` = el detector por kill corre sobre TODOS. |
| `detector_kill` | `true` | bool | Activa el análisis geométrico en cada kill PvP. |
| `kill_check_los` | `true` | bool | Señal: línea de visión bloqueada al matar (wallhack). La más fiable. |
| `kill_check_angulo` | `false` | bool | Señal ángulo arma→víctima. **Off** por defecto (el server no expone un aim fiable → falsos positivos). |
| `kill_check_distancia` | `true` | bool | Señal: kill a distancia sospechosa para el arma usada. |
| `kill_angulo_grados` | `120` | float grados | Umbral del chequeo de ángulo (si se reactiva). |
| `kill_min_senales` | `1` | int | Mínimo de señales coincidentes para loguear el kill. |
| `dist_sospechosa_default` | `400` | float m (`0`=no chequear) | Umbral genérico de distancia por arma. |
| `dist_sospechosa_por_arma` | ejemplos | map classname→float m | Umbral propio por arma (pistolas más bajo). |
| `watchlist_activa` | `true` | bool | Activa el muestreo ~1 Hz de la watchlist. |
| `watch_check_mira` | `true` | bool | Apunta (arma en alto) a un oculto = ESP/prefire. |
| `watch_check_aproximacion` | `true` | bool | Corre derecho hacia un oculto = ESP. |
| `watch_check_velocidad` | `true` | bool | Velocidad imposible / salto de posición = speedhack/teleport. |
| `watch_check_bajo_tierra` | `true` | bool | Jugador por debajo del terreno = noclip. |
| `watch_check_godmode` | `true` | bool | Recibe impactos reales seguidos y la vida no baja = god mode. |
| `watch_check_spinbot` | `true` | bool | La mira gira muchísimo entre ticks de forma sostenida = spinbot. |
| `watch_check_seguimiento` | `true` | bool | Sigue con la mira a alguien que no debería ver (lejos/oculto) = ESP. |
| `watch_check_punteria` | `true` | bool | Registra tiroteos (acc/HS/rango) para detectar aimbot por estadística. |
| `aim_track_angulo` | `5` | float grados | Tolerancia para decidir a qué jugador "apunta" un disparo. |
| `aim_track_dist_max` | `1000` | float m | Alcance máximo para considerar que apunta a un jugador. |
| `aim_engagement_timeout_ms` | `4000` | int ms | Sin dispararle por este tiempo → se cierra el engagement. |
| `aim_min_shots_log` | `3` | int | Mínimo de disparos apuntados para loguear el engagement. |
| `aim_min_shots_resumen` | `30` | int | Disparos mínimos en una vida para evaluar PUNTERIA_SOSPECHOSA. |
| `aim_acc_sospechosa` | `0.70` | float 0–1 | Accuracy global ≥ esto en una vida = sospechoso (MEDIO). |
| `aim_acc_alta` | `0.90` | float 0–1 | Accuracy ≥ esto = casi seguro aimbot (ALTO). |
| `aim_acc_lejos_sospechosa` | `0.60` | float 0–1 | Accuracy a >150m ≥ esto = sospechoso. |
| `aim_hs_sospechosa` | `0.35` | float 0–1 | % de headshots ≥ esto = corroborante (secundario). |
| `watch_angulo_grados` | `8` | float grados | Tolerancia de "apunta/corre/sigue directo" al objetivo. |
| `watch_dist_min` | `25` | float m | Ignorar objetivos a menos de esto (ruido a corta). |
| `watch_velocidad_max` | `12` | float m/s | A pie, por encima de esto = sospechoso (vehículos exentos). |
| `teleport_velocidad_max` | `35` | float m/s | Por encima de esto = salto/desync/teleport → se descarta (no se loguea). |
| `vel_min_streak` | `2` | int ticks | Ticks seguidos sobre `watch_velocidad_max` para loguear (1 pico = lag). |
| `grace_ms` | `5000` | int ms | Gracia tras teleport/respawn/conexión: se saltean vel/godmode/bajo-tierra. |
| `watch_bajo_tierra_metros` | `2.5` | float m | Metros bajo la superficie para marcar noclip. |
| `godmode_hits` | `4` | int | Impactos reales seguidos sin perder vida para marcar god mode. |
| `spinbot_grados` | `120` | float grados | Giro de mira por tick por encima del cual cuenta como imposible. |
| `spinbot_ticks` | `3` | int ticks | Ticks seguidos de giro imposible para marcar spinbot. |
| `watch_track_dist_max` | `1500` | float m | Alcance máximo del seguimiento (a 1500m el cliente ni renderiza al otro). |
| `watch_lejos_metros` | `800` | float m | Con LOS clara, más allá = no debería verlo a simple vista. |
| `watch_track_ticks` | `4` | int ticks | Ticks seguidos rastreando a un oculto cercano para marcar ESP. |
| `watch_track_ticks_lejos` | `2` | int ticks | Ticks seguidos para un objetivo muy lejos (basta menos, es más delatante). |
| `watch_log_cooldown_seg` | `15` | int seg | No repetir el mismo aviso del vigilado antes de esto. |
| `watchlist` | `[]` | [string] SteamID64 | SteamIDs a vigilar (vacío = no evalúa a nadie si `solo_watchlist`). |
| `exentos` | `[]` | [string] SteamID64 | SteamIDs a los que NO se les aplica anti-cheat (stats sí). |

### `koth.json` — King of the Hill
*No-resave*: si el archivo existe se respeta exacto. Empieza con `activar=false` y coordenadas en `0,0,0` de ejemplo — hay que **editar las coordenadas reales y poner `activar=true`**.

**Globales** (compartidos por todos los KOTH)
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `activar` | `false` | bool | Master on/off de TODO el sistema KOTH. |
| `metros_cercania_player_para_completar` | `30` | int m | Radio de captura (y del color del humo). |
| `segundos_aviso_antes_crear_koth` | `[60,120,200]` | [int] seg | A esos segundos antes avisa "faltan X" + coords + marca. |
| `segundos_aviso_porcentaje_completado` | `60` | int seg (`0`=no avisar) | Cada X seg avisa el % de captura. |
| `segundos_despawner_koth_sin_player_cerca` | `120` | int seg | Si nadie cerca por este tiempo, el KOTH desaparece. |
| `metros_para_detectar_falta_player` | `500` | int m | Radio de "no hay nadie cerca" para el despawn por abandono. |
| `avisar_spawn_koth` | `true` | bool | Aviso al spawnear el KOTH. |
| `avisar_que_un_player_inicico_koth` | `true` | bool | Aviso cuando alguien empieza a capturarlo. |
| `segundos_limpiar_cosas_al_completar_koth` | `80` | int seg | Tiempo mínimo antes de limpiar pallet/fuegos/marca. |
| `metros_limpieza_sin_player_cerca` | `10` | int m | No limpia si hay alguien a menos de esto (no borrar mientras lootean). |
| `colocar_marca_mapa_para_todo_el_server` | `true` | bool | Marca en el mapa visible por TODOS. |
| `bonus_por_mas_de_un_player_en_radio_en_procentaje_por_player` | `10` | int % | +X% de progreso por cada jugador extra en el radio. |
| `bonus_proximidad_mastil_menos_de_5_metros_en_procentaje_solo_cuenta_1_player` | `10` | int % | +X% si hay alguien muy cerca del mástil. |
| `metros_proximidad_mastil_para_bonus` | `5` | int m | Radio del bonus de proximidad. |
| `cantidad_maxima_players_en_bono` | `3` | int | Tope de jugadores que suman al bonus. |
| `clase_mastil` | `"TerritoryFlag"` | string classname | Objeto del mástil (la bandera se iza con el progreso). |
| `clase_bandera` | `"Flag_White"` | string classname | Tela que se cuelga en el mástil. |
| `clase_pallet_recompensa` | `""` | string (`""`=auto) | Override del pallet para todos. Vacío = supply crate por color (amarillo=Exor_KothCrate_1, verde=_2, morado=_3). |
| `clase_fuegos_artificiales` | `"FireworksLauncher"` | string classname | Objeto de fuegos que se enciende al completar. |
| `altura_extra_pallet` | `0` | float m | Metros que se sube el pallet si queda enterrado. |
| `metros_fuegos_lejos_del_pallet` | `10` | float m | Distancia a la que se encienden los fuegos del pallet. |
| `pallet_yaw` / `pallet_pitch` / `pallet_roll` | `0` | float grados | Rotación del pallet (el crate ya viene derecho en 0,0,0). |
| `colores[]` | 3 ejemplos | lista | Un KOTH independiente por entrada (ver abajo). |

**Cada entrada de `colores[]`** (un KOTH independiente)
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `color` | `"amarillo"` | `amarillo`/`verde`/`morado` | Color del humo y de la marca del mapa. |
| `cantidad_minima_players_online` | `1` | int | Solo aparece si hay ≥ esta cantidad de conectados. |
| `spawnear_osos` | `0` | int | Osos a spawnear en este KOTH. |
| `cantidad_zombies` | `0` | int | Infectados a spawnear en este KOTH. |
| `clase_zombie` | `[]` | [string] classnames | Lista de tipos de infectado (por cada zombie se elige uno al azar). |
| `no_spawnear_con_player_cerca_en_metros` | `30` | int m | Una coordenada con un player dentro no se usa (prueba la siguiente). |
| `segundos_para_inicio_koth_al_reinicar_server` | `60` | int seg | Demora del 1er spawn de este KOTH tras arrancar. |
| `segundos_spawnear_nuevo_koth_tras_completar_otro` | `60` | int seg | Demora hasta el siguiente ciclo de este KOTH. |
| `segundos_para_completar_koth` | `60` | int seg | Tiempo base para izar la bandera al 100%. |
| `coordenadas[]` | `{0,0,0}` | [{x,y,z}] float | 1 o más ubicaciones; se elige una libre al azar cada ciclo. |
| `item[]` | ejemplos | [{classname, prob}] | Recompensa: `classname` + `probabilidad_drop_en_porcentaje_maximo_100` (0–100). Repetir un classname = "1 seguro + 1 con suerte". |

### `cofre.json` — evento Cofre
*No-resave*: si el archivo existe se respeta exacto y no se sobreescribe lo que edites. Se crea con **2 zonas de coordenadas de ejemplo** (de un mapa de test) — en tu server hay que **poner las coordenadas reales**.

**Globales**
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `activado` | `true` | bool | Master on/off del módulo Cofre. |
| `offset_horas` | `-4` | int horas | Ajuste del reloj del host vs tu zona horaria (host UTC + jugás UTC-4 → `-4`). |
| `tiempo_abrir_un_cofre_minutos` | `10` | int min | Minutos en la mesa (zona activa) antes de que la caja se abra sola. |
| `colocar_marca_mapa` | `true` | bool | Marca en el mapa mientras la zona está abierta. |
| `aviso_antes_de_spawnear` | `[15,10,5]` | [int] min | A esos minutos antes avisa "en X min podrán abrir cofres". |
| `aviso_al_spawnwar` | `true` | bool | Aviso killfeed al abrirse la ventana. |
| `rango_detectar_payer_para_aviso_metros` | `150` | int m | Radio para contar players "en la zona" para el aviso. |
| `aviso_de_players_dentro_del_rango_efecto` | `true` | bool | Activa el aviso "hay N players en la zona". |
| `minutos_aviso_players_en_zona` | `5` | int min (≥1) | Cada cuántos minutos repite ese aviso. |
| `evento_estructura` | `true` | bool | Spawnea la estructura (mesa + drill + luz) al abrir la zona. |
| `estructura_objetos[]` | mesa + drill | lista | Objetos de la estructura: `classname` + offset `dx/dy/dz` (m) + `yaw` (grados). |
| `altura_estructura` | `0.5` | float m | Sube toda la estructura sobre el piso (que no queden patas enterradas). |
| `luces[]` | 2 roadflares | lista | Luces del evento: offset `dx/dy/dz` (m) + `yaw/pitch/roll` (grados; roll 90 = parada). El `classname` se ignora (siempre roadflare). |
| `mesa_max_cajas` | `3` | int | Cuántas cajas caben en la mesa a la vez. |
| `mesa_altura_slots` | `0.45` | float m | Altura de los slots sobre la base de la mesa (donde se apoyan las cajas). |
| `mesa_espaciado_slots` | `0.9` | float m | Separación entre cajas a lo largo de la mesa. |
| `minutos_despawn_cofres_tras_evento` | `30` | int min | Gracia al cerrar el evento si quedan cajas con contenido, antes de despawnear todo. |
| `lugares[]` | 2 ejemplos | lista | Zonas del evento (ver abajo). |
| `cofres[]` | azul/verde/rojo | lista | Tablas de loot por color (ver abajo). |

**Cada entrada de `lugares[]`** (una zona)
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `activo` | `true` | bool | Si la zona está en uso. |
| `posicion` | `{x,y,z}` | float | Centro de la mesa. Pegá las coords del VPP (`y`=altura; funciona en pisos elevados). |
| `rango_efecto` | `5` | int m (≥1) | Radio dentro del cual hay que soltar la caja. |
| `dias[]` | 24h todos los días | lista | Por día: `dia` (lunes…domingo), `hora_inicio`/`hora_fin` (`"HH:MM"`), `activado` (bool). |

**Cada entrada de `cofres[]`** (un color)
| Parámetro | Default | Valores | Descripción |
|---|---|---|---|
| `color` | azul/verde/rojo | string | Color de la caja (define qué tabla usa cada caja). |
| `items` | 2 bundles | map `"1"/"2"/…`→[classnames] | Cada clave es un BUNDLE (lista de classnames). Al abrir se elige UN bundle al azar y se spawnea completo. Sintaxis `classname:cantidad` para poner varias unidades (ej. `"M67Grenade:20"`). |

---

## Archivos que crea el mod (en `<profile>/3xorVanillaOptimization/`)

**Config (editables):** `storage.json` · `vehiculos.json` · `municion.json` · `party.json` · `spawns.json` · `mapa.json` · `items.json` · `vip.json` · `killfeed.json` · `serverinfo.json` · `chat.json` · `reparacion.json` · `bodycadaver.json` · `nobuild.json` · `autorun.json` · `anticheat.json` · `koth.json` · `cofre.json`

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
