# 3xor Party / Territorio / Spawns — Plan de desarrollo

Extensión del mod `3xor_Vanilla_Optimization` (mismo PBO). Sistema de party + territorio
por mástil + spawns + HUD + UI de items. **100% custom sobre vanilla DayZ, sin depender
de Expansion ni de ningún otro mod** (decisión 12-jun-2026: el mod debe correr solo).

> Realidad técnica: Expansion ya trae Groups/Territories/SpawnSelection. Acá NO se usa
> nada de eso — se reimplementa desde cero para que el mod sea independiente. Si en un
> server además corre Expansion-Groups/Territories, puede haber choque de hooks; ese caso
> queda fuera de alcance (este mod asume que es el único sistema de grupos del server).

---

## 0. Refactor de configuración (va primero, todo lo demás depende de esto)

Hoy hay un solo `$profile:3xorStorage/settings.json`. Se parte en **un JSON por módulo**
dentro de la carpeta de config:

```
$profile:3xorStorage/
├── storage.json      # virtualizar_minutos, auto_cerrar, comida, blacklist, cooldown...
├── vehiculos.json    # dormir, despertar_metros, voltear (el "autoflip"), excluidos...
├── municion.json     # stack, auto_stack, spawn_municion, excluida...
├── party.json        # (nuevo) territorio + grupos + HUD
├── spawns.json       # (nuevo) puntos de spawn
└── mapa.json         # (nuevo) mapa con M + posición propia y de miembros
```

- Cada archivo se crea solo con defaults al arrancar; campos nuevos se completan solos.
- **Migración**: si existe el viejo `settings.json`, se reparte automáticamente a los 5
  archivos y se renombra a `settings.json.migrated` (no se pierde nada).
- El loader pasa de una clase monolítica a un manager con un objeto de settings por módulo.

### `party.json` (esquema propuesto)

```jsonc
{
  "territorio": {
    "radio_metros": 50,              // anti-construcción alrededor del mástil
    "permitir_construir_cerca": false,// booleano: ajenos pueden construir cerca (default no)
    "whitelist_construible": [        // qué SÍ se puede poner en territorio ajeno
      "LandMineTrap", "ClaymoreMine", "PlasticExplosive"
    ],
    "blacklist_construible": [],      // qué nunca se puede poner (ni el dueño)
    "despawn_mastil_sin_miembros": true
  },
  "grupo": {
    "max_miembros": 8,
    "tecla_menu": "P",                   // tecla del menú de party (configurable)
    "auto_kick_dias": 0,                 // saca solo al miembro que no se loguea en X días (0 = off)
    "mostrar_distancia_miembros": true,  // distancia+nombre en pantalla
    "permitir_marker_equipo": true       // marcar con T / limpiar con Y (default true)
  },
  "bandera": {
    "ajenos_pueden_bajar": true,         // no-miembros pueden des-izar la bandera
    "bajada_bloquea_respawn": true,      // si está abajo, no podés respawnear en base (config on/off)
    "bandera_blanca": false,             // si true, arranca con bandera blanca
    "bandera_blanca_dias": 7             // días de protección; durante ese tiempo nadie cambia
                                         // la tela (sí izar/bajar). Después: pasa a bandera DayZ
                                         // y recién ahí se puede cambiar por banderas de país.
  },
  "respawn_base": {
    "habilitado": false,                 // spawn en tu mástil/base (default false)
    "cooldown_segundos": 3600            // 1 hora por default para volver a usarlo
  }
}
```

### `spawns.json` (esquema propuesto)

```jsonc
{
  "habilitado": true,
  "puntos": [
    { "nombre": "Costa Sur",  "x": 6000, "y": 0, "z": 2000, "cooldown_segundos": 0 },
    { "nombre": "Bosque NW",  "x": 4200, "y": 0, "z": 9800, "cooldown_segundos": 600 }
  ]
}
```
Al spawnear/morir, el jugador elige punto en una pantalla. Cada punto con su cooldown
propio (0 = sin cooldown). `y` se ajusta al suelo si se deja en 0.

### `mapa.json` (esquema propuesto)

```jsonc
{
  "abrir_con_m": true,             // abrir el mapa con M sin necesitar el ItemMap físico (default on, apagable)
  "mostrar_mi_posicion": true,     // siempre ver dónde estás vos (como GPS siempre activo)
  "mostrar_miembros_party": true   // ver a los miembros de tu party en el mapa al abrirlo
}
```
**Mapas**: funciona en **Chernarus, Livonia, BitterRoot y Namalsk** (y cualquier otro) porque
el render del mapa lo hace el motor según el mundo cargado (CfgWorlds) — los markers se ubican
con coordenadas reales del mundo, así que son **agnósticos al mapa** (no hay imagen por-mapa que
configurar). Los 4 listados quedan como los probados oficialmente.

### `vehiculos.json` — sección nueva de cámara/inventario/daño

Se agrega al JSON que ya tiene el carflip y el sueño de vehículos (mismo módulo):

```jsonc
{
  // ... (dormir, voltear/carflip, excluidos: lo que ya existe) ...
  "camara": {
    "conductor_3ra_persona": true,   // el conductor puede ir en 3ra persona (default on)
    "pasajeros_1ra_persona": true    // los NO-conductores van forzados a 1ra dentro del auto
  },
  "inventario": {
    "ver_ambos_dentro": true         // al abrir inventario en el auto, se ven el tuyo + el del auto
  },
  "dano": {
    "quitar_dano_vehiculos": false   // true = los autos no reciben daño (como algunos mods). Default false
  }
}
```
Todo con enable/disable; defaults: cámara conductor 3ra = ON, pasajeros 1ra = ON,
ver ambos inventarios = ON, quitar daño = OFF (los autos reciben daño normal por default).

---

## Fases

### ✅ Fase A — Config modular (base) — IMPLEMENTADA
Partir settings.json en los 6 JSON + migración + manager. Sin features nuevas todavía;
es la fundación de la que cuelgan party y spawns. **Testeable solo (no rompe nada).**
- `3_Game/ExorConfig.c` (nuevo): 6 clases de config (storage/vehiculos/municion/party/spawns/mapa)
  + manager `ExorConfig` + `GetExorConfig()` + migración del `settings.json` viejo.
- `3_Game/ExorStorage_Settings.c`: reducido a DTO legacy solo-migración.
- Consumidores migrados a `GetExorConfig().<modulo>`: Mission, Ammo, Manager, FlipVehicle,
  Clothing, Food, Barrels.
- Pendiente: smoke-test en server local (toca features ya vivas) + actualizar README/config-examples.

### ✅ Fase B — Party / Grupos (núcleo, server-authoritative) — IMPLEMENTADA
Archivos: `3_Game/ExorParty_Net.c` (RPC ids + ExorTimeUtil), `4_World/Party/ExorGroup.c`
(modelo: ExorGroup/ExorGroupMember/ExorRosterDTO/ExorPendingInvite), `4_World/Party/
ExorGroupManager.c` (manager server: crear/invitar/aceptar/rechazar/salir/kick/auto-kick +
persistencia groups/<id>.json + sync roster), `4_World/Actions/ExorActionInviteParty.c`
(apuntar a jugador → invitar, solo líder). Extensiones: PlayerBase (membresía + OnRPC +
requests cliente→server), MissionServer (Init + OnClientReadyEvent sync). El menú P
(aceptar/rechazar/salir/kick botones) es Fase E; el engine y los RPC ya están.

#### Detalle original:
- Store de grupos en memoria + persistencia `$profile:3xorStorage/groups/<id>.json`.
- Un jugador pertenece a 0 o 1 grupo. Para aceptar otro, primero sale del actual.
- RPCs: invitar → la otra persona recibe prompt → aceptar/rechazar.
- Acción **"Invitar miembro"** en el mástil (apuntando a un jugador) + límite `max_miembros`.
- **Sacar/kickear miembro** (solo el dueño/líder desde el menú P): expulsa a un miembro
  del grupo. Útil para el que no se conecta más. Opcional: auto-kick por inactividad
  (`auto_kick_dias`, 0 = off) si no se loguea en X días.
- Salir del grupo. Grupo sin miembros → se borra (y dispara el despawn del mástil, Fase C).
- Sincronización a cada cliente de su roster (nombres + estado).

### ✅ Fase C — Territorio por mástil — IMPLEMENTADA
Archivos: `4_World/Party/ExorTerritory.c` (registro server de mástiles + caché cliente +
regla CanPlace + sync), `4_World/Entities/ExorTerritoryMast.c` (mástil = TerritoryFlag,
binding a grupo, persistencia, disband↔despawn), acciones `ExorActionAssembleMastKit`
(soga+3 palos→kit), `ExorActionDeployMast` (kit→mástil+reclamar), `ExorActionRemoveMast`
(líder quita mástil→disuelve), `4_World/ExorTerritory_Items.c` (kit + Rope.SetActions +
`ItemBase.CanBePlaced` anti-construcción con whitelist/blacklist). config.cpp: `Exor_MastKit`
+ `Exor_TerritoryMast`. **VERIFICAR in-game**: ruta modelo del kit (`Flag_Kit.p3d`),
estado visual del mástil al spawnear (TerritoryFlag base-building), cobertura de CanBePlaced.

#### Detalle original:
- **Kit de mástil custom**: armar con soga + 3 palos cortos, **sin roca** (a diferencia del
  vanilla). Al setearlo, sale el mástil de una y **reclama territorio** para tu grupo.
- Si ya sos miembro de otro grupo, **no podés setear** otro mástil hasta borrar el anterior.
- Anti-construcción: hook de colocación (hologram + build) → si el punto está dentro de
  `radio_metros` de un mástil de OTRO grupo, se bloquea, salvo que el classname esté en
  `whitelist_construible` (claymore/explosivos/minas por default). `blacklist_construible`
  bloquea para todos. `permitir_construir_cerca` afloja todo si está en true.
- Despawn del mástil cuando el grupo se queda sin miembros (si `despawn_mastil_sin_miembros`).

### ✅ Fase D — Bandera — IMPLEMENTADA (parcial, ver nota)
Mástil con estado `m_ExorFlagRaised` (sincronizado) + `m_ExorClaimDay` (persistido).
Acciones `ExorActionRaiseFlag`/`ExorActionLowerFlag` (izar=solo miembros; bajar=miembros
o ajenos si `ajenos_pueden_bajar`). Ventana de bandera blanca (`ExorInWhiteFlagWindow`)
+ `CanReceiveAttachment` bloquea cambiar la tela durante la protección. El bloqueo de
respawn con bandera abajo lo consume la Fase F.
**Simplificado / VERIFICAR in-game**: el auto-swap de la tela a bandera DayZ al expirar la
ventana NO se implementó (solo se levanta la restricción para poder cambiarla); classnames
reales de las telas de bandera y el slot de attachment a confirmar.

#### Detalle original:
- Izar/bajar la bandera. No-miembros pueden bajarla si `ajenos_pueden_bajar`.
- Bandera abajo + `bajada_bloquea_respawn` → no se puede respawnear en base.
- **Bandera blanca**: durante `bandera_blanca_dias` nadie cambia la tela (sí izar/bajar);
  pasado el tiempo, cambia a bandera DayZ y se habilita cambiarla por banderas de país vanilla.

### Fase E — HUD de party + Mapa (cliente)
- Arriba a la izquierda: por cada miembro, cruz con **vida + nombre**.
- En pantalla: **distancia + nombre** de cada miembro (toggle `mostrar_distancia_miembros`).
- Requiere que el server empuje (throttled) posición+vida de los miembros a cada cliente
  (los jugadores lejanos no están replicados por defecto). Esta misma sync alimenta el mapa.
- **Marker de equipo**: tecla **T** crea una marca visible para tu grupo; **Y** la limpia
  (`permitir_marker_equipo`).
- **Mapa (`mapa.json`)**: abrir el mapa con **M** sin necesitar el ItemMap físico
  (`abrir_con_m`, default on). Al abrirlo, mostrar **tu posición** (`mostrar_mi_posicion`) y
  la de **los miembros del party** (`mostrar_miembros_party`) con sus nombres, en tiempo real.
  Markers agnósticos al mapa (coords reales del mundo) → andan en Chernarus, Livonia,
  BitterRoot, Namalsk y cualquier otro.

### Fase F — Sistema de spawn
- Pantalla de **selección de punto de spawn** al conectar/morir, leyendo `spawns.json`.
- Cooldown por punto.
- **Respawn en base**: si `respawn_base.habilitado`, opción de spawnear en tu mástil con
  `cooldown_segundos` (default 1 h). Bloqueado si la bandera está abajo (Fase D).

### Fase G — UI de items (durabilidad + rareza)
- Barra de **durabilidad** en el tooltip/inspección: 100% = pristine, 0% = ruined.
- **Rareza** del item, **derivada del tier del loot** (Tier1→Común, Tier2→Poco común,
  Tier3→Raro, Tier4→Épico), con tabla de override por classname para casos especiales.
- Independiente de todo lo demás; se puede hacer en cualquier momento.

### Fase H — Cámara / inventario / daño de vehículos (módulo vehiculos)
- **Conductor en 3ra persona** (toggle, default ON); **pasajeros forzados a 1ra persona**
  dentro del auto (toggle, default ON).
- Al abrir el inventario dentro del auto, ver y manipular **tu inventario + el del auto**
  a la vez (toggle, default ON).
- **Quitar daño a vehículos** (toggle, default OFF): los autos no reciben daño.
- Vive en `vehiculos.json` junto al carflip y el sueño de vehículos.

---

## Estado de implementación E–H (lo construido vs. lo que falta afinar in-game)

### ✅ Fase E — HUD + party en vivo — IMPLEMENTADA (con desvíos)
- `4_World/Party/ExorPartyLive.c`: sync server→cliente (cada 2s) de posición+vida de
  miembros + marcas. `5_Mission/ExorPartyHud.c` + `gui/exor_party_hud.layout`: HUD arriba
  a la izquierda con nombre + vida (% con "+") + distancia, y lista de marcas con distancia.
- Gestión por **acciones** (no menú P): `ExorActionAcceptInvite`/`DeclineInvite` (self),
  `ExorActionLeaveParty` (en mástil), `ExorActionKickMember` (apuntando), `ExorActionPlaceMarker`/
  `ExorActionClearMarkers`. **DESVÍO**: no usé keybinds P/T/Y/M porque `cfgInputActions` mal
  escrito rompe TODO el config; se entregó la funcionalidad por acciones + HUD siempre visible.
  El **menú P con tecla** y el **mapa con M mostrando posiciones** quedan como mejora in-game
  (requieren `cfgInputActions` + hook del MapMenu, a verificar en vivo). El `.layout` del HUD
  se afina con el editor GUI.

### ✅ Fase F — Spawn — IMPLEMENTADA (backend)
- `4_World/Party/ExorSpawn.c` + overrides en MissionServer (`OnClientNewEvent` primer login,
  `OnClientRespawnEvent` muerte). Respawn en base con cooldown + bloqueo si la bandera está
  abajo; puntos de `spawns.json` con cooldown por punto. **DESVÍO**: la **pantalla de selección**
  necesita UI; por ahora elige automáticamente un punto válido. Cooldowns en memoria (se
  reinician al reiniciar el server).

### ✅ Fase G — Durabilidad + rareza — IMPLEMENTADA (en el nombre)
- `4_World/ExorItemInfo.c` + `ItemBase.GetDisplayName` agregan barra de durabilidad ASCII + %
  y rareza al nombre del item (config `items.json`: `mostrar_durabilidad`/`mostrar_rareza`).
  **DESVÍO**: rareza por **heurística de clase** (el tier de types.xml no está en el item en
  runtime). Barra en el tooltip "propio" (widget) queda para el editor GUI; se eligió el nombre
  por ser compile-safe (aparece también en el menú scroll — verboso, ajustable).

### ✅ Fase H — Vehículos — IMPLEMENTADA (parcial)
- **Quitar daño** (`vehiculos.dano.quitar_dano_vehiculos`): `SetAllowDamage(false)` en CarScript. ✅
- **Ver ambos inventarios**: ya es **vanilla** (en el auto, Tab muestra tu inv + cargo del auto). ✅
- **Cámara 3ra conductor / 1ra pasajeros**: NO implementada — la API de cámara por asiento es
  engine-específica; se deja documentada para hacerla in-game sin romper compilación.

## Partes difíciles / riesgos (honestidad técnica)

- **UI es lo más lento** en DayZ vanilla scripting: menú P (grupo), pantalla de spawn,
  HUD de miembros y tooltip de durabilidad son `.layout` + lógica fina. Tiempo real acá.
- **Sincronizar posición/vida de miembros lejanos**: el server tiene que pushear data que
  normalmente no está replicada → RPC throttled (ej. cada 1-2 s) para no inflar el tráfico.
- **Hook de construcción**: cubrir TODOS los caminos de colocación (hologram, build part,
  deploy de otros mods) sin romper la construcción legítima del dueño.
- **Bandera blanca con días** y cambio de tela: manejar el timer persistente y las
  restricciones de qué bandera se puede poner en cada ventana.
- **Spawn selection**: vanilla no tiene pantalla de selección; hay que inyectarla en el
  flujo de respawn sin romper el login normal.

## Decisiones tomadas (12-jun-2026)
- Standalone, sin Expansion. Todo dentro de `3xor_Vanilla_Optimization`.
- Config partido en 1 JSON por módulo con migración automática del settings.json viejo.

## Decisiones tomadas (12-jun-2026, ronda 2)
- Tecla del menú de party: **`P`** por default, configurable en `party.json`.
- Rareza de items: **derivada del tier del loot** (Tier1-4), con override por classname.
- Pantalla de spawn: **al morir Y en el primer login**.
- **Kick de miembros** por el líder + auto-kick opcional por inactividad (`auto_kick_dias`).
- Vehículos: cámara 3ra/1ra, ver ambos inventarios, quitar daño — todo en `vehiculos.json`.
- **Mapa** (`mapa.json`): abrir con M (default on), mostrar tu posición + miembros del party,
  agnóstico al mapa (Chernarus/Livonia/BitterRoot/Namalsk). Va con el HUD en la Fase E.
