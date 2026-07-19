// ============================================================================
// 3xor_Vanilla_Optimization - Configuracion modular (Fase A)
// Un JSON por modulo en $profile:3xorVanillaOptimization\:
//   storage.json  vehiculos.json  municion.json  party.json  spawns.json  mapa.json
// Cada archivo se crea solo con defaults al arrancar; los campos nuevos se
// auto-completan re-guardando tras cargar. Si existe el settings.json viejo
// (monolitico) se migra automaticamente la primera vez.
// Acceso global: GetExorConfig().storage / .vehiculos / .municion / .party / ...
// ============================================================================

// Rango aleatorio [min, max] de cantidad al spawnear una pila de municion
class ExorMunicionSpawnRango
{
	int min = 15;
	int max = 65;
}

// ----------------------------------------------------------------------------
// storage.json
// ----------------------------------------------------------------------------
class ExorCfgStorage
{
	int version = 1;
	// Virtualizacion ON: tras un rato sin tocar, el barril saca su loot del mundo (a disco)
	// para ALIVIAR al server (corazon del mod) Y quedar a prueba de reinicios/crash (el JSON
	// es el respaldo). Al abrir, restaura cada item en su lugar (ver ExorVO_Serializer).
	// Timing en SEGUNDOS (recomendado: cerrar 10s, virtualizar 30s).
	// El auto-cierre NO mide desde el ultimo movimiento, sino la CERCANIA del jugador:
	// mientras haya alguien a menos de cerrar_distancia_metros, el barril queda abierto;
	// recien se cierra auto_cerrar_segundos DESPUES de que el jugador se aleja. Asi nunca
	// se cierra mientras lo estas usando (aunque te quedes ordenando tu inventario).
	int auto_cerrar_segundos = 5;        // se cierra a los Xs de que NO haya nadie cerca (0 = off)
	int virtualizar_segundos = 5;        // virtualiza a los Xs de cerrado (0 = off) -> total ~10s
	float cerrar_distancia_metros = 10.0; // radio para considerar "hay un jugador cerca" (aleja 10m = se va)
	float multiplicador_comida = 2.0;
	bool permitir_ropa_con_items = true;
	ref TStringArray blacklist;
	int cooldown_abrir_segundos = 3;  // 0 = sin cooldown
	// NEVERA: dias que tarda en descargarse una bateria de coche LLENA puesta en el
	// refrigerador. Con bateria cargada la comida se conserva; al agotarse se pudre.
	// 0 = la bateria no se descarga nunca.
	float nevera_bateria_dias = 3.0;

	void ExorCfgStorage()
	{
		blacklist = new TStringArray;
	}

	void SetDefaults()
	{
		version = 1;
		auto_cerrar_segundos = 5;
		virtualizar_segundos = 5;
		cerrar_distancia_metros = 10.0;
		multiplicador_comida = 2.0;
		permitir_ropa_con_items = true;
		cooldown_abrir_segundos = 3;
		nevera_bateria_dias = 3.0;
		blacklist.Clear();
	}
}

// ----------------------------------------------------------------------------
// vehiculos.json (sueno + voltear/carflip + camara/inventario/dano)
// ----------------------------------------------------------------------------
class ExorCfgVehCamara
{
	bool conductor_3ra_persona = true;  // el conductor puede ir en 3ra
	bool pasajeros_1ra_persona = true;  // los NO-conductores van forzados a 1ra
}

class ExorCfgVehInventario
{
	bool ver_ambos_dentro = true;       // ver tu inventario + el del auto a la vez
}

class ExorCfgVehDano
{
	bool quitar_dano_vehiculos = false;        // true = los autos no reciben dano
}

class ExorCfgVehiculos
{
	int version = 1;
	bool vehiculos_dormir = true;
	int vehiculos_dormir_minutos = 5;     // 0 = off
	int vehiculos_despertar_metros = 30;
	ref TStringArray vehiculos_excluidos;
	bool voltear_vehiculos = true;
	int voltear_segundos = 40;
	bool inv_items_anidados = true;       // EXTRA: permitir ropa/contenedores CON items adentro en el baul (como los barriles). El cargo de 600 es SIEMPRE (config), esto solo gatea lo anidado.
	ref ExorCfgVehCamara camara;
	ref ExorCfgVehInventario inventario;
	ref ExorCfgVehDano dano;

	void ExorCfgVehiculos()
	{
		vehiculos_excluidos = new TStringArray;
		camara = new ExorCfgVehCamara;
		inventario = new ExorCfgVehInventario;
		dano = new ExorCfgVehDano;
	}

	void SetDefaults()
	{
		version = 1;
		vehiculos_dormir = true;
		vehiculos_dormir_minutos = 5;
		vehiculos_despertar_metros = 30;
		voltear_vehiculos = true;
		voltear_segundos = 40;
		inv_items_anidados = true;
		vehiculos_excluidos.Clear();
		camara.conductor_3ra_persona = true;
		camara.pasajeros_1ra_persona = true;
		inventario.ver_ambos_dentro = true;
		dano.quitar_dano_vehiculos = false;
	}
}

// ----------------------------------------------------------------------------
// municion.json
// ----------------------------------------------------------------------------
class ExorCfgMunicion
{
	int version = 1;
	int stack_municion_default = 100;
	ref map<string, int> stack_municion;
	bool auto_stack = true;
	int spawn_municion_min_default = 15;
	int spawn_municion_max_default = 65;
	ref map<string, ref ExorMunicionSpawnRango> spawn_municion;
	ref TStringArray municion_excluida;

	void ExorCfgMunicion()
	{
		stack_municion = new map<string, int>;
		spawn_municion = new map<string, ref ExorMunicionSpawnRango>;
		municion_excluida = new TStringArray;
	}

	void SetDefaults()
	{
		version = 1;
		stack_municion_default = 100;
		auto_stack = true;
		spawn_municion_min_default = 15;
		spawn_municion_max_default = 65;
		stack_municion.Clear();
		spawn_municion.Clear();
		municion_excluida.Clear();
		municion_excluida.Insert("Ammo_40mm_Explosive");
		municion_excluida.Insert("Ammo_40mm_ChemGas");
		municion_excluida.Insert("Ammo_40mm_Smoke_White");
		municion_excluida.Insert("Ammo_40mm_Smoke_Red");
		municion_excluida.Insert("Ammo_40mm_Smoke_Green");
		municion_excluida.Insert("Ammo_40mm_Smoke_Black");
		municion_excluida.Insert("Ammo_Flare");
		municion_excluida.Insert("Ammo_FlareRed");
		municion_excluida.Insert("Ammo_FlareGreen");
		municion_excluida.Insert("Ammo_FlareBlue");
		municion_excluida.Insert("Ammo_RPG7_HE");
		municion_excluida.Insert("Ammo_RPG7_AP");
		municion_excluida.Insert("Ammo_LAW_HE");
		municion_excluida.Insert("Ammo_GrenadeM4");
	}
}

// ----------------------------------------------------------------------------
// party.json (territorio + grupo + bandera + respawn en base)
// El comportamiento se implementa en las fases B-F; aca solo vive la config.
// ----------------------------------------------------------------------------
class ExorCfgPartyTerritorio
{
	bool habilitado = true;                 // activar/desactivar TODO el sistema de bandera=territorio
	int radio_metros = 35;                  // anti-construccion alrededor del mastil (no-miembros)
	bool permitir_construir_cerca = false;  // ajenos pueden construir cerca (default no)
	ref TStringArray whitelist_construible; // que SI se puede poner en territorio ajeno
	ref TStringArray blacklist_construible; // que nunca se puede poner (ni el dueno)
	bool despawn_mastil_sin_miembros = true;
	int cooldown_reconstruir_mastil_minutos = 360;  // cada cuanto se puede mover/reconstruir el mastil (360 = 6h; 1440 = 1 dia; 0 = sin cooldown)

	void ExorCfgPartyTerritorio()
	{
		whitelist_construible = new TStringArray;
		blacklist_construible = new TStringArray;
	}
}

class ExorCfgPartyGrupo
{
	bool habilitado = true;                  // master: activa/desactiva TODO el sistema de party
	int max_miembros = 8;
	string tecla_menu = "P";
	int auto_kick_dias = 0;                  // 0 = off
	bool mostrar_posicion_miembros = true;   // ver donde estan (gobierna sync pos/vida: HUD + nameplates + mapa)
	bool mostrar_hud = true;                 // barra de vida arriba-izquierda
	bool mostrar_nameplates = true;          // nombre 3D sobre la cabeza
	bool mostrar_distancia_miembros = true;  // numero de distancia en HUD/nameplate
	bool mostrar_propio = false;             // incluirte a vos mismo en HUD/nameplates (false = solo amigos)
	bool permitir_marker_equipo = true;      // marcar con T / limpiar con Y
}

class ExorCfgPartyBandera
{
	bool ajenos_pueden_bajar = true;
	bool bajada_bloquea_respawn = true;
	bool bandera_blanca = false;
	int bandera_blanca_minutos = 10080;   // proteccion en MINUTOS reales (10080 = 7 dias; 1440 = 1 dia; 1 = 1 min)
	string bandera_blanca_cambiar_a = "Flag_DayZ";   // al expirar, reemplaza la blanca por esta bandera ("" = no cambiar, solo destrabar el slot)
}

class ExorCfgPartyRespawnBase
{
	bool habilitado = true;                  // respawn en tu base/mastil (configurable: false = off) - interruptor maestro
	int cooldown_segundos = 3600;            // 1 hora (configurable)
	bool permitir_spawn_mastil_vip = true;   // los VIP (vip.json) pueden spawnear en el mastil
	bool permitir_spawn_mastil_no_vip = false; // el resto (no-VIP) puede spawnear en el mastil
}

// Anti-raid / proteccion del territorio (todo server-side; el log es forense)
class ExorCfgPartyProteccion
{
	bool bloquear_desmantelar_ajeno = true;       // #2: ajenos NO pueden desmantelar muros/torres en territorio que no es suyo
	bool log_robo_contenedor = true;              // #3: loguear cuando un ajeno toma items dentro de territorio enemigo
	bool log_abrir_barril_ajeno = true;           // #5: loguear cuando un ajeno ABRE un barril 3xor dentro de territorio enemigo
	bool log_desconexion_base_ajena = true;       // #4a: loguear si un ajeno se desloguea dentro de territorio enemigo
	bool sacar_de_base_ajena_al_reconectar = true;// #4b: al reconectar dentro de territorio ajeno, teletransportar al borde
	int log_dias_retener = 7;                     // dias que se conservan los archivos de raidlog (0 = nunca borrar)
	bool aviso_clan_inactivo = true;              // #6: avisar en el raidlog si un clan no conecta a nadie hace 'aviso_tiempo_inactividad_dias'
	int aviso_tiempo_inactividad_dias = 14;       // umbral de inactividad de un clan en dias (default 14; 0 = off). Se auto-agrega (=14) a configs viejas que no lo tengan
	bool log_combat_log = true;                   // #7: loguear deslogueo dentro de una zona de combate PvP (combat-log)
	int combat_log_minutos = 8;                   // minutos reales que la zona de combate sigue viva tras el ultimo daño PvP (0 = off)
	float combat_log_radio = 150;                 // radio (m) de la zona de combate alrededor de cada participante (tirador y victima); 150 cubre PvP largo por el anclaje a ambos extremos
	bool log_farmeo_kills = true;                 // #8: loguear posible farmeo de kills (un mismo killer mata al MISMO steamid muchas veces en poco tiempo). Por steamid -> inmune al cambio de nombre.
	int farmeo_ventana_minutos = 240;             // ventana deslizante en minutos reales (240 = 4h). Persiste entre reinicios.
	int farmeo_umbral = 4;                        // a partir de cuantos kills al MISMO steamid dentro de la ventana se loguea (0 = off)
}

class ExorCfgParty
{
	int version = 1;
	ref ExorCfgPartyTerritorio territorio;
	ref ExorCfgPartyGrupo grupo;
	ref ExorCfgPartyBandera bandera;
	ref ExorCfgPartyRespawnBase respawn_base;
	ref ExorCfgPartyProteccion proteccion;

	void ExorCfgParty()
	{
		territorio = new ExorCfgPartyTerritorio;
		grupo = new ExorCfgPartyGrupo;
		bandera = new ExorCfgPartyBandera;
		respawn_base = new ExorCfgPartyRespawnBase;
		proteccion = new ExorCfgPartyProteccion;
	}

	void SetDefaults()
	{
		version = 1;
		territorio.habilitado = true;
		territorio.radio_metros = 35;
		territorio.permitir_construir_cerca = false;
		territorio.despawn_mastil_sin_miembros = true;
		territorio.cooldown_reconstruir_mastil_minutos = 360;   // 6 horas
		territorio.whitelist_construible.Clear();
		territorio.whitelist_construible.Insert("LandMineTrap");
		territorio.whitelist_construible.Insert("ClaymoreMine");
		territorio.whitelist_construible.Insert("PlasticExplosive");
		territorio.blacklist_construible.Clear();

		grupo.habilitado = true;
		grupo.max_miembros = 8;
		grupo.tecla_menu = "P";
		grupo.auto_kick_dias = 0;
		grupo.mostrar_posicion_miembros = true;
		grupo.mostrar_hud = true;
		grupo.mostrar_nameplates = true;
		grupo.mostrar_distancia_miembros = true;
		grupo.mostrar_propio = false;
		grupo.permitir_marker_equipo = true;

		bandera.ajenos_pueden_bajar = true;
		bandera.bajada_bloquea_respawn = true;
		bandera.bandera_blanca = false;
		bandera.bandera_blanca_minutos = 10080;
		bandera.bandera_blanca_cambiar_a = "Flag_DayZ";

		respawn_base.habilitado = true;
		respawn_base.cooldown_segundos = 3600;
		respawn_base.permitir_spawn_mastil_vip = true;
		respawn_base.permitir_spawn_mastil_no_vip = false;

		proteccion.bloquear_desmantelar_ajeno = true;
		proteccion.log_robo_contenedor = true;
		proteccion.log_abrir_barril_ajeno = true;
		proteccion.log_desconexion_base_ajena = true;
		proteccion.sacar_de_base_ajena_al_reconectar = true;
		proteccion.log_dias_retener = 7;
		proteccion.aviso_clan_inactivo = true;
		proteccion.aviso_tiempo_inactividad_dias = 14;
		proteccion.log_combat_log = true;
		proteccion.combat_log_minutos = 8;
		proteccion.log_farmeo_kills = true;
		proteccion.farmeo_ventana_minutos = 240;
		proteccion.farmeo_umbral = 4;
		proteccion.combat_log_radio = 150;
	}
}

// ----------------------------------------------------------------------------
// spawns.json (puntos de spawn seleccionables)
// ----------------------------------------------------------------------------
class ExorSpawnPunto
{
	string nombre = "";
	float x = 0;
	float y = 0;     // 0 = ajustar al suelo
	float z = 0;
	int cooldown_segundos = 0;
	float distancia_random = 50;  // radio (m) aleatorio alrededor del punto al spawnear
}

class ExorCfgSpawns
{
	int version = 1;
	bool habilitado = true;
	bool dar_cuchillo_al_spawnear = true;   // TEST: dar un cuchillo al personaje nuevo (suicidio facil al testear). Poner false en prod.
	bool equipar_npc_test = false;          // TEST LOCAL: equipa los NPC dummy que spawnea VPP ("player") con ropa+mochila+armas, para probar la tumba. SIEMPRE false en prod (equiparia AI de otros mods).
	ref array<ref ExorSpawnPunto> puntos;

	void ExorCfgSpawns()
	{
		puntos = new array<ref ExorSpawnPunto>;
	}

	void SetDefaults()
	{
		version = 1;
		habilitado = true;
		dar_cuchillo_al_spawnear = true;
		equipar_npc_test = false;
		puntos.Clear();
		// Punto de ejemplo (el admin define los suyos). Editar/reemplazar en spawns.json.
		ExorSpawnPunto ej = new ExorSpawnPunto();
		ej.nombre = "Ejemplo - editar en spawns.json";
		ej.x = 6000; ej.y = 0; ej.z = 2000; ej.cooldown_segundos = 300;
		ej.distancia_random = 50;
		puntos.Insert(ej);
	}
}

// ----------------------------------------------------------------------------
// mapa.json
// ----------------------------------------------------------------------------
class ExorCfgMapa
{
	int version = 1;
	bool abrir_con_m = true;           // abrir mapa con M sin el ItemMap fisico
	bool mostrar_mi_posicion = true;   // siempre ver tu posicion
	bool mostrar_miembros_party = true;// ver a los miembros del party en el mapa

	void SetDefaults()
	{
		version = 1;
		abrir_con_m = true;
		mostrar_mi_posicion = true;
		mostrar_miembros_party = true;
	}
}

// ----------------------------------------------------------------------------
// items.json (Fase G: durabilidad + rareza en el nombre del item)
// ----------------------------------------------------------------------------
class ExorCfgItems
{
	int version = 1;
	bool mostrar_durabilidad = true;
	bool mostrar_rareza = true;
	bool rareza_usar_tabla = false;            // true = usar rareza_tabla (classname->tier); false = heuristica por clase
	ref map<string, string> rareza_tabla;      // classname -> tier: comun / poco_comun / raro / epico / legendario

	void ExorCfgItems()
	{
		rareza_tabla = new map<string, string>;
	}

	void SetDefaults()
	{
		version = 1;
		mostrar_durabilidad = true;
		mostrar_rareza = true;
		rareza_usar_tabla = false;
		rareza_tabla.Clear();
		// Ejemplos (editar/ampliar en items.json; tier en minuscula).
		rareza_tabla.Set("M4A1", "epico");
		rareza_tabla.Set("AKM", "raro");
		rareza_tabla.Set("Mag_STANAG_30Rnd", "poco_comun");
		rareza_tabla.Set("Apple", "comun");
	}
}

// ----------------------------------------------------------------------------
// chat.json (chat custom con canales: global + zona/proximity)
// ----------------------------------------------------------------------------
class ExorCfgChat
{
	bool habilitado = true;          // master on/off del chat custom
	int radio_zona_metros = 50;      // alcance del canal ZONA (proximity)
	int duracion_segundos = 25;      // cuanto dura cada linea en pantalla
	int max_lineas = 9;              // maximo de lineas simultaneas EN PANTALLA (la mas vieja se va)
	int cooldown_segundos = 2;       // anti-spam: minimo entre mensajes (0 = sin limite)
	bool bloquear_repetidos = true;  // anti-spam: bloquear el MISMO mensaje seguido
	int max_caracteres_por_linea = 55; // ancho de linea (= el de antes): al pasarse, el mensaje BAJA a la siguiente linea en vez de cortarse
	int max_lineas_por_mensaje = 3;    // cuantas lineas puede ocupar UN mensaje largo (el resto se trunca con "…")

	void SetDefaults()
	{
		habilitado = true;
		radio_zona_metros = 50;
		duracion_segundos = 25;
		max_lineas = 9;
		cooldown_segundos = 2;
		bloquear_repetidos = true;
		max_caracteres_por_linea = 55;
		max_lineas_por_mensaje = 3;
	}
}

// ----------------------------------------------------------------------------
// reparacion.json
//   reparar_a_pristine = al reparar con cualquier kit, el item llega hasta
//     PRISTINE (verde) en vez de quedar topado en "gastado" (quita el cap vanilla).
//   kits_stackeables = classnames de kits/consumibles que se pueden COMBINAR
//     entre si: 2 del mismo tipo gastados se unen en 1 sumando su uso (cap al max).
//     Funciona para kits cuyo "uso" es QUANTITY (costura/limpieza); los que usan
//     condicion/health pueden no combinar (a verificar in-game).
// ----------------------------------------------------------------------------
class ExorCfgReparacion
{
	int version = 1;
	bool reparar_a_pristine = true;
	ref TStringArray kits_stackeables;

	void ExorCfgReparacion()
	{
		kits_stackeables = new TStringArray;
	}

	void SetDefaults()
	{
		version = 1;
		reparar_a_pristine = true;
		kits_stackeables = new TStringArray;
		kits_stackeables.Insert("SewingKit");
		kits_stackeables.Insert("LeatherSewingKit");
		kits_stackeables.Insert("WeaponCleaningKit");
		kits_stackeables.Insert("SharpeningStone");
		kits_stackeables.Insert("Whetstone");
		kits_stackeables.Insert("EpoxyPutty");
		kits_stackeables.Insert("DuctTape");
		kits_stackeables.Insert("TireRepairKit");
	}

	bool EsStackeable(string classname)
	{
		if (!kits_stackeables)
			return false;
		int i;
		for (i = 0; i < kits_stackeables.Count(); i++)
		{
			if (kits_stackeables.Get(i) == classname)
				return true;
		}
		return false;
	}
}

// ----------------------------------------------------------------------------
// bodycadaver.json
//   Al morir un jugador, ~delay_segundos despues el cuerpo se convierte en una
//   "bolsa de cadaver" (contenedor) con TODO su loot (ropa + cargo + arma caida).
//   Se puede cargar para transportarla (camina lento). Dura duracion_minutos y
//   sobrevive reinicio. Se VIRTUALIZA (su loot se saca del mundo) cuando no hay
//   players a menos de alejar_metros por virtualizar_minutos, y se DES-virtualiza
//   cuando un player se acerca a menos de acercar_metros.
// ----------------------------------------------------------------------------
class ExorCfgBodyCadaver
{
	int version = 1;
	bool habilitado = true;         // master on/off de TODO el modulo de la lapida
	int delay_segundos = 1;         // demora entre la muerte y la aparicion de la lapida
	int duracion_minutos = 30;      // cuanto dura la lapida (30 min). Sobrevive reinicio. Configurable en bodycadaver.json.
	// VIRTUALIZACION (baja entidades sin perder loot): la tumba saca su loot a disco cuando
	// NADIE vivo esta a <alejar_metros por virtualizar_minutos, y lo RESTAURA al ABRIRLA o
	// al acercarse un player a <acercar_metros. El restore al abrir es SINCRONO (como el
	// barril) -> el player nunca ve la tumba vacia. acercar < alejar = histeresis (banda
	// muerta) para que no virtualice/restaure en loop con alguien merodeando cerca.
	// virtualizar_minutos = 0 -> nunca virtualiza (loot siempre real toda la vida de la tumba).
	float acercar_metros = 20;      // restaurar cuando un player vivo entra a este radio (+ Open() al abrir)
	float alejar_metros = 40;       // virtualizar solo si TODOS estan mas lejos que esto (lejos = no se ve el "pop")
	int virtualizar_minutos = 2;    // minutos sin nadie cerca antes de virtualizar (0 = off)
	// FORENSE: un JSON por tumba (muerto/pos/fecha/items/looteadores) en tumbas\.
	bool forense_habilitado = true;         // escribir el JSON forense por tumba
	bool forense_registrar_looteadores = true; // registrar quien saca cada item de la tumba
	int  forense_retener_dias = 3;          // cada arranque borra los JSON de tumbas mas viejos que esto

	void SetDefaults()
	{
		version = 1;
		habilitado = true;
		delay_segundos = 1;
		duracion_minutos = 30;
		acercar_metros = 20;
		alejar_metros = 40;
		virtualizar_minutos = 2;
		forense_habilitado = true;
		forense_registrar_looteadores = true;
		forense_retener_dias = 3;
	}
}

// ----------------------------------------------------------------------------
// nobuild.json (Zonas de NO construccion definidas por el admin)
//   Define uno o mas centros (posicion x,y,z del mundo) con un radio en metros
//   dentro del cual NADIE puede construir/colocar (evita bases en zonas prohibidas:
//   military, KOTH, spawns, traders, etc.). La "whiteList" son classnames (o parte
//   del classname) que SI se permiten igual dentro de la zona (minas/trampas/
//   explosivos/fuegos artificiales), para no romper el PvP.
//   - La distancia se mide EN HORIZONTAL (x,z del mundo). 'y' = altura, se ignora.
//   - Copia la posicion tal cual te la da el VPP admintools (x, y=altura, z).
// ----------------------------------------------------------------------------
class ExorCfgVec3
{
	float x = 0;
	float y = 0;
	float z = 0;
}

class ExorCfgNoBuildZona
{
	ref ExorCfgVec3 posicion;
	float desabilitar_construccion_en_metros = 0;   // radio de bloqueo (0 = zona ignorada)

	void ExorCfgNoBuildZona()
	{
		posicion = new ExorCfgVec3();
	}
}

class ExorCfgNoBuild
{
	int version = 1;
	bool activado = false;
	ref array<string> whiteList;                       // classnames permitidos SIEMPRE (contains, case-insensitive)
	ref array<ref ExorCfgNoBuildZona> lugares_no_permitidos;

	void ExorCfgNoBuild()
	{
		whiteList = new array<string>;
		lugares_no_permitidos = new array<ref ExorCfgNoBuildZona>;
	}

	void SetDefaults()
	{
		version = 1;
		activado = false;
		whiteList = new array<string>;
		// classnames REALES del juego para lo que el user pidio permitir dentro de la zona:
		whiteList.Insert("LandMineTrap");                // mina
		whiteList.Insert("ClaymoreMine");                // claymore
		whiteList.Insert("BearTrap");                    // trampa de osos
		whiteList.Insert("TripwireTrap");                // trampa de alambre
		whiteList.Insert("ImprovisedExplosive");         // explosivo improvisado
		whiteList.Insert("FireworksLauncher");           // fuegos artificiales (incluye Anniversary_FireworksLauncher por 'contains')

		lugares_no_permitidos = new array<ref ExorCfgNoBuildZona>;
		ExorCfgNoBuildZona z = new ExorCfgNoBuildZona();
		z.posicion.x = 0;
		z.posicion.y = 0;
		z.posicion.z = 0;
		z.desabilitar_construccion_en_metros = 1200;
		lugares_no_permitidos.Insert(z);
	}
}

// ----------------------------------------------------------------------------
// koth.json (King of the Hill: eventos de captura con recompensa)
//   Cada entrada de "colores" es UN koth INDEPENDIENTE con su PROPIA config y su
//   propio ciclo (pueden estar varios activos a la vez). Adentro de cada color van
//   TODOS los tiempos/parametros (inicio, completar, avisos, despawn, bonos, etc.),
//   ademas del color de humo (amarillo/verde/morado), min de jugadores, osos,
//   coordenadas (array; se elige una al azar) e items de recompensa (% por item;
//   repetir un classname = "1 seguro + 1 con suerte"). Solo quedan globales los
//   classnames de objetos y el ajuste del pallet.
// ----------------------------------------------------------------------------
class ExorCfgKothItem
{
	string classname = "";
	int probabilidad_drop_en_porcentaje_maximo_100 = 100;
}

class ExorCfgKothCoord
{
	float x = 0;
	float y = 0;
	float z = 0;
}

// UN koth independiente. Solo lleva lo PROPIO de cada koth; el resto es global (abajo).
class ExorCfgKothColor
{
	string color = "amarillo";                 // SOLO: amarillo / verde / morado (color del humo y de la marca)
	int cantidad_minima_players_online = 1;    // solo aparece si hay >= esta cantidad de conectados
	int spawnear_osos = 0;                     // osos a spawnear en ESTE koth
	int cantidad_zombies = 0;                  // infectados a spawnear en ESTE koth
	ref TStringArray clase_zombie;             // lista de classnames de infectados (por cada zombie se elige uno al azar de la lista)
	int no_spawnear_con_player_cerca_en_metros = 30; // radio: una coordenada con un player dentro NO se usa (se prueba la siguiente)
	int segundos_para_inicio_koth_al_reinicar_server = 60;   // demora del 1er spawn de ESTE koth tras arrancar
	int segundos_spawnear_nuevo_koth_tras_completar_otro = 60; // demora hasta el siguiente ciclo de ESTE koth
	int segundos_para_completar_koth = 60;     // tiempo base para izar la bandera al 100%
	ref array<ref ExorCfgKothCoord> coordenadas;     // 1 o mas ubicaciones; se elige una LIBRE al azar cada ciclo
	ref array<ref ExorCfgKothItem> item;

	void ExorCfgKothColor()
	{
		clase_zombie = new TStringArray;
		coordenadas = new array<ref ExorCfgKothCoord>;
		item = new array<ref ExorCfgKothItem>;
	}

	void Validate()
	{
		if (cantidad_zombies < 0)
			cantidad_zombies = 0;
		if (spawnear_osos < 0)
			spawnear_osos = 0;
		if (segundos_para_completar_koth < 1)
			segundos_para_completar_koth = 1;
		if (segundos_spawnear_nuevo_koth_tras_completar_otro < 0)
			segundos_spawnear_nuevo_koth_tras_completar_otro = 0;
		if (segundos_para_inicio_koth_al_reinicar_server < 0)
			segundos_para_inicio_koth_al_reinicar_server = 0;
		if (no_spawnear_con_player_cerca_en_metros < 0)
			no_spawnear_con_player_cerca_en_metros = 0;
	}
}

class ExorCfgKoth
{
	int version = 1;
	bool activar = false;                       // MASTER on/off de TODO el sistema
	// ---- globales (compartidos por todos los koth) ----
	int metros_cercania_player_para_completar = 30;  // radio de captura (y del color del humo)
	ref array<int> segundos_aviso_antes_crear_koth;  // ej [60,120,200]: avisa "faltan X" + coords + marca
	int segundos_aviso_porcentaje_completado = 60;   // cada X seg avisa el % (0 = no avisar)
	int segundos_despawner_koth_sin_player_cerca = 120; // si NADIE cerca por este tiempo, desaparece
	int metros_para_detectar_falta_player = 500;     // radio de "no hay nadie cerca" (despawn por abandono)
	bool avisar_spawn_koth = true;                   // aviso al spawnear
	bool avisar_que_un_player_inicico_koth = true;   // aviso cuando alguien empieza a capturarlo
	int segundos_limpiar_cosas_al_completar_koth = 80; // tiempo minimo antes de limpiar pallet/fuegos/marca
	int metros_limpieza_sin_player_cerca = 10;       // no limpia si hay alguien a menos de esto (no borrar mientras lootean)
	bool colocar_marca_mapa_para_todo_el_server = true; // marca en el mapa visible por TODOS
	int bonus_por_mas_de_un_player_en_radio_en_procentaje_por_player = 10; // +X% por jugador extra en el radio
	int bonus_proximidad_mastil_menos_de_5_metros_en_procentaje_solo_cuenta_1_player = 10; // +X% si hay alguien muy cerca
	int metros_proximidad_mastil_para_bonus = 5;     // radio del bonus de proximidad
	int cantidad_maxima_players_en_bono = 3;         // tope de jugadores que suman al bonus
	string clase_mastil = "TerritoryFlag";           // objeto del mastil con bandera (la bandera se iza con el progreso)
	string clase_bandera = "Flag_White";             // tela que se cuelga en el mastil
	string clase_pallet_recompensa = "";  // OVERRIDE del pallet para TODOS. Vacio = supply crate por color (amarillo=Exor_KothCrate_1, verde=_2, morado=_3). Lo que no entra NO se spawnea.
	string clase_fuegos_artificiales = "FireworksLauncher";  // objeto de fuegos que se enciende al completar
	float altura_extra_pallet = 0;                // metros que se sube el pallet si queda enterrado
	float metros_fuegos_lejos_del_pallet = 10;    // los fuegos se encienden a esta distancia del pallet
	float pallet_yaw = 0;                         // rotacion del pallet (grados); el crate ya viene derecho en 0,0,0
	float pallet_pitch = 0;
	float pallet_roll = 0;
	ref array<ref ExorCfgKothColor> colores;

	void ExorCfgKoth()
	{
		segundos_aviso_antes_crear_koth = new array<int>;
		colores = new array<ref ExorCfgKothColor>;
	}

	void Validate()
	{
		if (metros_cercania_player_para_completar < 1)
			metros_cercania_player_para_completar = 1;
		if (cantidad_maxima_players_en_bono < 1)
			cantidad_maxima_players_en_bono = 1;
		if (bonus_por_mas_de_un_player_en_radio_en_procentaje_por_player < 0)
			bonus_por_mas_de_un_player_en_radio_en_procentaje_por_player = 0;
		if (bonus_proximidad_mastil_menos_de_5_metros_en_procentaje_solo_cuenta_1_player < 0)
			bonus_proximidad_mastil_menos_de_5_metros_en_procentaje_solo_cuenta_1_player = 0;
		if (metros_proximidad_mastil_para_bonus < 0)
			metros_proximidad_mastil_para_bonus = 0;
		if (segundos_despawner_koth_sin_player_cerca < 0)
			segundos_despawner_koth_sin_player_cerca = 0;
		if (segundos_limpiar_cosas_al_completar_koth < 0)
			segundos_limpiar_cosas_al_completar_koth = 0;
		if (metros_limpieza_sin_player_cerca < 0)
			metros_limpieza_sin_player_cerca = 0;
		if (metros_fuegos_lejos_del_pallet < 0)
			metros_fuegos_lejos_del_pallet = 0;
		if (colores)
		{
			int i;
			for (i = 0; i < colores.Count(); i++)
			{
				if (colores.Get(i))
					colores.Get(i).Validate();
			}
		}
	}

	void SetDefaults()
	{
		version = 1;
		activar = false;
		metros_cercania_player_para_completar = 30;
		segundos_aviso_antes_crear_koth = new array<int>;
		segundos_aviso_antes_crear_koth.Insert(60);
		segundos_aviso_antes_crear_koth.Insert(120);
		segundos_aviso_antes_crear_koth.Insert(200);
		segundos_aviso_porcentaje_completado = 60;
		segundos_despawner_koth_sin_player_cerca = 120;
		metros_para_detectar_falta_player = 500;
		avisar_spawn_koth = true;
		avisar_que_un_player_inicico_koth = true;
		segundos_limpiar_cosas_al_completar_koth = 80;
		metros_limpieza_sin_player_cerca = 10;
		colocar_marca_mapa_para_todo_el_server = true;
		bonus_por_mas_de_un_player_en_radio_en_procentaje_por_player = 10;
		bonus_proximidad_mastil_menos_de_5_metros_en_procentaje_solo_cuenta_1_player = 10;
		metros_proximidad_mastil_para_bonus = 5;
		cantidad_maxima_players_en_bono = 3;
		clase_mastil = "TerritoryFlag";
		clase_bandera = "Flag_White";
		clase_pallet_recompensa = "";
		clase_fuegos_artificiales = "FireworksLauncher";
		altura_extra_pallet = 0;
		metros_fuegos_lejos_del_pallet = 10;
		pallet_yaw = 0;
		pallet_pitch = 0;
		pallet_roll = 0;
		colores = new array<ref ExorCfgKothColor>;
		// 3 koth de ejemplo (amarillo/verde/morado). EDITAR coordenadas reales y poner activar=true.
		ExorCfgKothColor ka = new ExorCfgKothColor();
		ka.color = "amarillo"; ka.cantidad_minima_players_online = 1;
		ExorKothAddZombie(ka, "ZmbM_SoldierNormal");
		ExorKothAddCoord(ka, 0, 0, 0);
		ExorKothAddItem(ka, "AKM", 100);
		ExorKothAddItem(ka, "Mag_AKM_30Rnd", 100);
		ExorKothAddItem(ka, "Mag_AKM_30Rnd", 50);
		colores.Insert(ka);
		ExorCfgKothColor kv = new ExorCfgKothColor();
		kv.color = "verde"; kv.cantidad_minima_players_online = 1;
		ExorKothAddZombie(kv, "ZmbM_SoldierNormal");
		ExorKothAddCoord(kv, 0, 0, 0);
		ExorKothAddItem(kv, "M4A1", 100);
		ExorKothAddItem(kv, "Mag_STANAG_30Rnd", 100);
		ExorKothAddItem(kv, "Mag_STANAG_30Rnd", 50);
		colores.Insert(kv);
		ExorCfgKothColor km = new ExorCfgKothColor();
		km.color = "morado"; km.cantidad_minima_players_online = 1; km.spawnear_osos = 5;
		ExorKothAddZombie(km, "ZmbM_SoldierNormal");
		ExorKothAddCoord(km, 0, 0, 0);
		ExorKothAddItem(km, "FAL", 100);
		ExorKothAddItem(km, "Mag_FAL_20Rnd", 100);
		ExorKothAddItem(km, "Mag_FAL_20Rnd", 50);
		colores.Insert(km);
	}

	// helpers de ejemplo (Enforce: sin expresiones multilinea)
	void ExorKothAddItem(ExorCfgKothColor c, string cls, int prob)
	{
		ExorCfgKothItem it = new ExorCfgKothItem();
		it.classname = cls;
		it.probabilidad_drop_en_porcentaje_maximo_100 = prob;
		c.item.Insert(it);
	}

	void ExorKothAddCoord(ExorCfgKothColor c, float x, float y, float z)
	{
		ExorCfgKothCoord co = new ExorCfgKothCoord();
		co.x = x; co.y = y; co.z = z;
		c.coordenadas.Insert(co);
	}

	void ExorKothAddZombie(ExorCfgKothColor c, string cls)
	{
		c.clase_zombie.Insert(cls);
	}
}

// ----------------------------------------------------------------------------
// Sync server -> cliente: SOLO la config que el cliente necesita para mostrar
// (toggles de party/HUD/nameplates/marcas, mapa, durabilidad/rareza+tabla).
// Lo pesado (municion/storage/vehiculos) es logica de server y NO se envia.
// ----------------------------------------------------------------------------
class ExorClientCfgDTO
{
	ref ExorCfgPartyGrupo grupo;
	ref ExorCfgMapa mapa;
	ref ExorCfgItems items;
	ref ExorCfgVehCamara veh_camara;	// camara por asiento (la aplica el cliente en HandleView)
	ref ExorCfgVehInventario veh_inventario;	// ver ambos inventarios en el auto (cliente)
	// serverinfo va por SU PROPIO RPC (SERVERINFO_SYNC), no en este bundle: el texto
	// largo del panel hacia el JSON demasiado grande y el sync entero fallaba.
	ref ExorCfgReparacion reparacion;	// reparar-a-pristine + lista de kits combinables (el cliente la usa para ofrecer la accion)
	ref ExorCfgAutorun autorun;	// auto-run (correr-solo): el cliente lo aplica al jugador local
	bool permitir_construir_cerca;	// toggle de anti-construccion (el cliente lo usa en CanPlaceClient/holograma)

	void ExorClientCfgDTO()
	{
		grupo = new ExorCfgPartyGrupo;
		mapa = new ExorCfgMapa;
		items = new ExorCfgItems;
		veh_camara = new ExorCfgVehCamara;
		veh_inventario = new ExorCfgVehInventario;
		reparacion = new ExorCfgReparacion;
		autorun = new ExorCfgAutorun;
	}
}

// ----------------------------------------------------------------------------
// vip.json (lista de SteamIDs VIP). Hoy se usa para permitir spawn en el mastil;
// queda listo para colgarle mas beneficios a futuro.
// ----------------------------------------------------------------------------
// Loadout VIP (mismo para todos los VIP): ropa que REEMPLAZA la de spawn +
// items extra que se ponen en el cargo de la camisa/pantalon. Vacio = no tocar.
class ExorCfgVipLoadout
{
	string pantalon = "";
	string camisa = "";
	string zapato = "";
	string bolso = "";
	string guantes = "";
	string mascara = "";
	bool full_comida_bebida = true;   // al equipar, dejar al VIP con 100% comida (energia) y bebida (agua)
	ref TStringArray items_extra;   // comida/cuchillo/etc -> cargo de camisa o pantalon

	void ExorCfgVipLoadout()
	{
		items_extra = new TStringArray;
	}
}

// Una entrada VIP: steamid + fecha de ingreso + usos de equipamiento.
// fecha_ingreso vacia = el server la sella con HOY al arrancar. El VIP vence a los
// dias_vip (def 30) dias de fecha_ingreso (ver ExorCfgVip.IsVip). Los usos NO se
// reponen solos. RENOVAR = editar fecha_ingreso a mano (reinicia los 30 dias y
// repone los usos) y/o subir usos_por_mes. El usuario avisa cuando se le vence.
class ExorCfgVipEntry
{
	string steamid = "";
	string fecha_ingreso = "";   // "AAAA-MM-DD" (vacio = el server la sella con hoy)
	int usos_por_mes = 0;        // usos de equipamiento totales para ESTE player hasta renovar (0 = usar el default global)
}

// Lista vieja (solo steamids) para migrar al formato nuevo automaticamente.
class ExorCfgVipLegacy
{
	ref TStringArray vip_steamids;
	void ExorCfgVipLegacy() { vip_steamids = new TStringArray; }
}

class ExorCfgVip
{
	ref array<ref ExorCfgVipEntry> vips;
	bool equip_habilitado = true;        // perk: opcion "Spawn en base + Equipamiento"
	int equip_usos_por_mes = 7;          // DEFAULT global de usos de equipamiento (si la entrada del player tiene 0). NO se reponen solos: ver dias_vip.
	int dias_vip = 30;                   // dias que dura el VIP desde fecha_ingreso. Pasados, IsVip = false (ya no cuenta como VIP). Renovacion = editar fecha_ingreso a mano.
	ref ExorCfgVipLoadout equip_loadout;

	void ExorCfgVip()
	{
		vips = new array<ref ExorCfgVipEntry>;
		equip_loadout = new ExorCfgVipLoadout();
	}

	void SetDefaults()
	{
		vips = new array<ref ExorCfgVipEntry>;
		// Entrada de EJEMPLO (asi se ve como agregar otros players): steamid +
		// fecha_ingreso (vacio = se sella al arrancar) + usos_por_mes propio.
		ExorCfgVipEntry ej = new ExorCfgVipEntry();
		ej.steamid = "76561198722396813";
		ej.fecha_ingreso = "";
		ej.usos_por_mes = 20;
		vips.Insert(ej);

		equip_habilitado = true;
		equip_usos_por_mes = 7;
		dias_vip = 30;
		equip_loadout = new ExorCfgVipLoadout();
		equip_loadout.pantalon = "CargoPants_Black";
		equip_loadout.camisa = "TacticalShirt_Black";
		equip_loadout.zapato = "MilitaryBoots_Black";
		equip_loadout.bolso = "TortillaBag";
		equip_loadout.guantes = "TacticalGloves_Green";
		equip_loadout.mascara = "BalaclavaMask_Blackskull";
		equip_loadout.full_comida_bebida = true;
		equip_loadout.items_extra.Insert("CombatKnife");
		equip_loadout.items_extra.Insert("TacticalBaconCan");
	}

	// Usos de equipamiento para este player (hasta renovar): el propio de la entrada,
	// o el default global. NO se reponen solos (solo al renovar fecha_ingreso).
	int UsosPorMes(string sid)
	{
		ExorCfgVipEntry e = FindEntry(sid);
		if (e && e.usos_por_mes > 0)
			return e.usos_por_mes;
		return equip_usos_por_mes;
	}

	static string Pad2(int v)
	{
		if (v < 10)
			return "0" + v.ToString();
		return v.ToString();
	}

	ExorCfgVipEntry FindEntry(string sid)
	{
		if (!vips)
			return null;
		int i;
		for (i = 0; i < vips.Count(); i++)
		{
			if (vips.Get(i) && vips.Get(i).steamid == sid)
				return vips.Get(i);
		}
		return null;
	}

	// Dias civiles exactos desde una fecha (algoritmo days-from-civil; exacto para y>=0).
	static int CivilDays(int y, int m, int d)
	{
		int yy = y;
		if (m <= 2)
			yy = yy - 1;
		int era = yy / 400;
		int yoe = yy - era * 400;
		int mp;
		if (m > 2)
			mp = m - 3;
		else
			mp = m + 9;
		int doy = (153 * mp + 2) / 5 + d - 1;
		int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
		return era * 146097 + doe - 719468;
	}

	// Parsea "AAAA-MM-DD". true si ok (deja py/pm/pd seteados).
	static bool ParseFechaVip(string s, out int py, out int pm, out int pd)
	{
		py = 0;
		pm = 0;
		pd = 0;
		if (s.Length() < 10)
			return false;
		py = s.Substring(0, 4).ToInt();
		pm = s.Substring(5, 2).ToInt();
		pd = s.Substring(8, 2).ToInt();
		if (py <= 0 || pm <= 0 || pd <= 0)
			return false;
		return true;
	}

	// Dias transcurridos desde el ingreso de esta entrada. -1 si no tiene fecha valida
	// (aun sin sellar = recien agregado; se trata como vigente hasta que el server la sella).
	int DaysSinceIngreso(ExorCfgVipEntry e)
	{
		if (!e)
			return -1;
		int py, pm, pd;
		if (!ParseFechaVip(e.fecha_ingreso, py, pm, pd))
			return -1;
		int y, m, d;
		GetYearMonthDay(y, m, d);
		return CivilDays(y, m, d) - CivilDays(py, pm, pd);
	}

	// VIP vigente = esta en la lista Y no pasaron dias_vip dias desde fecha_ingreso.
	bool IsVip(string sid)
	{
		ExorCfgVipEntry e = FindEntry(sid);
		if (!e)
			return false;
		int days = DaysSinceIngreso(e);
		if (days < 0)
			return true;	// sin fecha aun (se sella con hoy al arrancar) -> vigente
		return days < dias_vip;	// dentro de la ventana de dias_vip (def 30) dias
	}

	// Fecha de ingreso de un VIP ("" si no esta).
	string FechaIngreso(string sid)
	{
		ExorCfgVipEntry e = FindEntry(sid);
		if (e)
			return e.fecha_ingreso;
		return "";
	}

	// Migra el formato viejo (vip_steamids[]) y sella con la fecha de hoy las
	// entradas que esten sin fecha. Devuelve true si cambio algo (para re-guardar).
	bool MigrateAndStamp()
	{
		bool changed = false;

		// 1) Migrar vip_steamids[] viejo -> vips[] (solo si vips quedo vacia)
		if (vips.Count() == 0 && FileExist(ExorStorageConstants.CFG_VIP))
		{
			ExorCfgVipLegacy old = new ExorCfgVipLegacy();
			JsonFileLoader<ExorCfgVipLegacy>.JsonLoadFile(ExorStorageConstants.CFG_VIP, old);
			if (old.vip_steamids && old.vip_steamids.Count() > 0)
			{
				int k;
				for (k = 0; k < old.vip_steamids.Count(); k++)
				{
					ExorCfgVipEntry e = new ExorCfgVipEntry();
					e.steamid = old.vip_steamids.Get(k);
					e.fecha_ingreso = "";
					vips.Insert(e);
					changed = true;
				}
			}
		}

		// 2) Sellar con hoy las entradas sin fecha
		int y, m, d;
		GetYearMonthDay(y, m, d);
		string hoy = string.Format("%1-%2-%3", y, Pad2(m), Pad2(d));
		int i;
		for (i = 0; i < vips.Count(); i++)
		{
			ExorCfgVipEntry en = vips.Get(i);
			if (en && en.steamid != "" && en.fecha_ingreso == "")
			{
				en.fecha_ingreso = hoy;
				changed = true;
			}
		}
		return changed;
	}

	// true si hay al menos una pieza/item configurado en el loadout
	bool TieneLoadout()
	{
		if (!equip_loadout)
			return false;
		if (equip_loadout.pantalon != "") return true;
		if (equip_loadout.camisa != "") return true;
		if (equip_loadout.zapato != "") return true;
		if (equip_loadout.bolso != "") return true;
		if (equip_loadout.guantes != "") return true;
		if (equip_loadout.mascara != "") return true;
		if (equip_loadout.items_extra && equip_loadout.items_extra.Count() > 0) return true;
		return false;
	}
}

// ----------------------------------------------------------------------------
// killfeed.json (mensajes de muerte PvP / suicidio arriba a la derecha)
// ----------------------------------------------------------------------------
class ExorCfgKillfeed
{
	bool habilitado = true;            // master on/off del killfeed
	int duracion_segundos = 6;         // cuanto dura cada linea en pantalla
	int max_lineas = 5;                // maximo de lineas simultaneas (la mas vieja se va)
	bool mostrar_suicidios = true;     // mostrar la linea "se ha suicidado"
	ref array<string> killboard_excluidos;  // SteamID64 que NO suman ni figuran en el killboard/ranking (admins/owner). Editar en killfeed.json

	void ExorCfgKillfeed()
	{
		killboard_excluidos = new array<string>;
	}

	void SetDefaults()
	{
		habilitado = true;
		duracion_segundos = 6;
		max_lineas = 5;
		mostrar_suicidios = true;
		killboard_excluidos.Clear();
		killboard_excluidos.Insert("76561198722396813");   // owner: no figura en el ranking
	}

	bool EsExcluido(string sid)
	{
		if (!killboard_excluidos || sid == "")
			return false;
		return killboard_excluidos.Find(sid) != -1;
	}
}

// ----------------------------------------------------------------------------
// serverinfo.json (panel "Información del server" desde ESC: tabs General/Reglas/Score)
// El texto de General y Reglas lo escribe el admin aca. Se sincroniza al cliente.
// ----------------------------------------------------------------------------
class ExorCfgServerInfo
{
	bool habilitado = true;            // muestra el boton "Server Info" en el menu de ESC
	string titulo = "Información del Server";
	bool tab_general = true;
	string general_titulo = "General";
	ref TStringArray general_lineas;
	bool tab_reglas = true;
	string reglas_titulo = "Reglas";
	ref TStringArray reglas_lineas;
	bool tab_score = true;
	string score_titulo = "Score";
	string discord_texto = "Discord";              // texto del boton grande en la tab General
	string discord_url = "https://www.google.com"; // a donde lleva el boton al hacer click (por ahora Google)

	void ExorCfgServerInfo()
	{
		general_lineas = new TStringArray;
		reglas_lineas = new TStringArray;
	}

	void SetDefaults()
	{
		habilitado = true;
		titulo = "Información del Server";
		tab_general = true;
		general_titulo = "General";
		discord_texto = "Discord";
		discord_url = "https://www.google.com";
		general_lineas = new TStringArray;
		general_lineas.Insert("Bienvenido al servidor.");
		general_lineas.Insert("");
		general_lineas.Insert("== TERRITORIO Y PARTY ==");
		general_lineas.Insert("Para armar tu base creá un kit de mástil con palos y cuerda y colocalo: se construirá el mástil y reclamarás el territorio.");
		general_lineas.Insert("Al reclamarlo se coloca una bandera blanca que dura 7 días; pasados esos días se reemplaza automáticamente por otra (o podrás reemplazarla vos).");
		general_lineas.Insert("Para sumar a alguien a tu territorio, invitalo mirando el mástil y que él acepte mirando ese mismo mástil. Para sacarlo, también desde el mástil ('Administrar party').");
		general_lineas.Insert("Los teams que no se conecten durante 3 semanas perderán su base: el servidor lleva un log que avisa cuando un team entero queda inactivo.");
		general_lineas.Insert("");
		general_lineas.Insert("== REGLAS ANTI-RAID ==");
		general_lineas.Insert("No se permite saltar muros ni entrar a bases ajenas glitcheando o usando scripts. Hay logs para detectarlo y se aplicará ban permanente.");
		general_lineas.Insert("No se permite lootear cosas en bases que no son tuyas. Hay logs; si ocurre en horario de no-raid, el ban será permanente.");
		general_lineas.Insert("");
		general_lineas.Insert("== COMBAT-LOG ==");
		general_lineas.Insert("El combat-log no está permitido: si en tu zona hay PvP (aunque no dispares vos) y te desconectás antes de los 8 minutos, serás baneado. Hay logs que lo registran.");
		general_lineas.Insert("");
		general_lineas.Insert("== ANTI-CHEAT ==");
		general_lineas.Insert("Quienes maten a través de estructuras o hagan prefire serán detectados por los logs: se les pedirá un scan y, de confirmarse, se aplicará posible ban permanente.");
		tab_reglas = true;
		reglas_titulo = "Reglas";
		reglas_lineas = new TStringArray;
		reglas_lineas.Insert("1. No insultar.");
		reglas_lineas.Insert("2. Editá las reglas en serverinfo.json (reglas_lineas).");
		tab_score = true;
		score_titulo = "Score";
	}
}

// ----------------------------------------------------------------------------
// autorun.json (correr-solo / auto-run). CLIENT-side: el cliente fuerza el
// movimiento del jugador local. Se sincroniza para que el admin lo pueda
// cambiar server-wide.
// ----------------------------------------------------------------------------
class ExorCfgAutorun
{
	int version = 1;
	bool habilitado = true;       // permitir el auto-run
	int tecla = KeyCode.KC_Z;     // tecla para activar/desactivar (default Z). Otros: KC_X, KC_C, KC_V, KC_R...
	int velocidad = 3;            // 1=caminar  2=trotar  3=sprint (corre fuerte; baja a trote cuando se queda sin stamina)
	bool parar_con_movimiento = true; // tocar cualquier tecla de movimiento (W/A/S/D) cancela el auto-run

	void SetDefaults()
	{
		version = 1;
		habilitado = true;
		tecla = KeyCode.KC_Z;
		velocidad = 3;
		parar_con_movimiento = true;
	}
}

// ----------------------------------------------------------------------------
// anticheat.json (SOLO server, heuristico). Mide GEOMETRIA y RESULTADOS con los
// eventos del motor; NO lee memoria/inputs del cliente (eso es BattlEye). Da
// INDICIOS contados (BAJO/MEDIO/ALTO) para que el admin revise a mano, NUNCA un
// baneo automatico (hay falsos positivos por lag/desync/peeker's advantage).
// Todo va al log de auditoria (ServerAuditLog\audit_YYYY-MM-DD.txt) en vivo.
//
//   Feature 1 (detector_kill): se evalua en CADA kill PvP, sobre TODOS (menos
//     exentos). Por kill chequea: linea-de-vision bloqueada (wallhack), angulo
//     del arma vs la victima (mato sin apuntar = aimbot / "mirando al cielo"),
//     y distancia sospechosa por arma. Cuenta cuantas senales saltan -> nivel.
//
//   Feature 2 (watchlist): muestreo ~1 Hz SOLO sobre los SteamIDs vigilados
//     (pocos). Por cada vigilado vs los demas: si apunta DIRECTO (arma en alto)
//     a un jugador OCULTO tras pared = ESP/prefire; si corre DERECHO hacia un
//     oculto = ESP. El raycast (caro) solo se lanza si el angulo barato ya salto.
//
//   exentos[]: a estos NO se les aplica NADA de anti-cheat (admins/confianza).
//     El Score/stats se les sigue sumando igual (eso no es anti-cheat).
// ----------------------------------------------------------------------------
class ExorCfgAnticheat
{
	int version = 1;
	bool habilitado = true;                 // master on/off de TODO el anti-cheat
	bool solo_watchlist = true;             // true = SOLO se evalua a los SteamIDs de watchlist[] (kill incluido); false = el detector por kill corre sobre TODOS

	// ---- Feature 1: detector por kill (sobre todos menos exentos) ----
	bool detector_kill = true;
	bool kill_check_los = true;             // linea-de-vision bloqueada al matar (wallhack) - FIABLE
	bool kill_check_angulo = false;         // OFF por defecto: el server NO expone una direccion de apuntado fiable
	                                        // (el modelo del arma no esta pose-ado al aim real server-side) -> daba falsos
	                                        // en kills limpios. El aimbot se detecta MEJOR por estadistica (T3 accuracy/HS).
	bool kill_check_distancia = true;       // kill a distancia sospechosa para el arma usada
	float kill_angulo_grados = 120;         // umbral alto (si se reactiva, solo marca matar a alguien CLARAMENTE no-de-frente)
	int kill_min_senales = 1;               // minimo de senales coincidentes para loguear (1 = cualquier indicio)
	float dist_sospechosa_default = 400;    // umbral (m) generico por arma (0 = no chequear distancia)
	ref map<string, float> dist_sospechosa_por_arma; // classname de arma -> umbral propio (ej pistolas mas bajo)

	// ---- Feature 2: watchlist (seguimiento dirigido de sospechosos) ----
	bool watchlist_activa = true;
	bool watch_check_mira = true;           // apunta (arma en alto) a un jugador oculto = ESP/prefire
	bool watch_check_aproximacion = true;   // corre derecho hacia un jugador oculto = ESP
	bool watch_check_velocidad = true;      // velocidad imposible / salto de posicion = speedhack/teleport
	bool watch_check_bajo_tierra = true;    // jugador por DEBAJO del terreno = noclip/glitch bajo mapa
	bool watch_check_godmode = true;        // recibe impactos reales seguidos y la vida no baja = god mode
	bool watch_check_spinbot = true;        // el arma/mira gira muchisimo entre ticks de forma sostenida = spinbot
	bool watch_check_seguimiento = true;    // SIGUE con la mira a un jugador que NO deberia ver (lejos u oculto) varios ticks = ESP
	// ---- tracking de punteria por engagement (solo watchlist): mide acc/HS/rango por tiroteo ----
	bool watch_check_punteria = true;       // registrar tiroteos a jugadores (disparos/impactos/zona/dist) para detectar aimbot
	float aim_track_angulo = 5;             // tolerancia (grados) para decidir a que jugador "apunta" un disparo
	float aim_track_dist_max = 1000;        // alcance maximo (m) para considerar que apunta a un jugador
	int aim_engagement_timeout_ms = 4000;   // sin dispararle por este tiempo -> se cierra el engagement
	int aim_min_shots_log = 3;              // minimo de disparos apuntados para loguear el engagement (o >=1 impacto/kill)
	int aim_min_shots_resumen = 30;         // disparos apuntados minimos en una vida para evaluar PUNTERIA_SOSPECHOSA
	float aim_acc_sospechosa = 0.70;        // acc global >= esto en una vida (con muestra) = sospechoso (MEDIO)
	float aim_acc_alta = 0.90;              // acc global >= esto = casi seguro aimbot (ALTO). Ningun humano pega 90%+ sostenido
	float aim_acc_lejos_sospechosa = 0.60;  // acc a >150m >= esto = sospechoso
	float aim_hs_sospechosa = 0.35;         // % de headshots >= esto = corroborante (SECUNDARIO: el aimbot deja elegir la zona del cuerpo, asi que HS es bypasseable; la ACCURACY es la señal que no se puede esconder)
	float watch_angulo_grados = 8;          // tolerancia de "apunta/corre/sigue DIRECTO" al objetivo
	float watch_dist_min = 25;              // ignorar objetivos a menos de esto (ruido a corta)
	float watch_velocidad_max = 12;         // m/s por encima de lo cual = sospechoso (sprint normal ~6-7; a pie. Vehiculos se saltean)
	float teleport_velocidad_max = 35;      // m/s por encima de lo cual NO es speedhack sino un salto/desync/teleport -> se descarta (no se loguea)
	int vel_min_streak = 2;                 // ticks SEGUIDOS por encima de watch_velocidad_max para loguear (un pico de 1 tick = lag, se ignora)
	int grace_ms = 5000;                    // ventana de gracia (ms) tras teleport/respawn/conexion: se saltean los chequeos de velocidad/godmode/bajo-tierra
	float watch_bajo_tierra_metros = 2.5;   // metros por debajo de la superficie para marcar noclip
	int godmode_hits = 4;                   // impactos reales SEGUIDOS sin que baje la vida para marcar god mode (mas alto = menos falsos)
	float spinbot_grados = 120;             // giro de la mira por tick por encima de lo cual cuenta como "giro imposible"
	int spinbot_ticks = 3;                  // ticks SEGUIDOS de giro imposible para marcar spinbot
	float watch_track_dist_max = 1500;      // alcance maximo del seguimiento (m). A 1500m el cliente ni renderiza al otro
	float watch_lejos_metros = 800;         // con LOS clara, mas alla de esto = no deberia verlo a simple vista (cuenta para el seguimiento)
	int watch_track_ticks = 4;              // ticks SEGUIDOS rastreando a un objetivo OCULTO cercano (pudo ojearlo) para marcar ESP
	int watch_track_ticks_lejos = 2;        // ticks SEGUIDOS para un objetivo MUY LEJOS (>watch_lejos_metros, imposible de ver): basta menos, es mas delatante
	int watch_log_cooldown_seg = 15;        // no repetir el mismo aviso del vigilado antes de esto
	ref TStringArray watchlist;             // SteamIDs a vigilar (el admin los agrega; pocos)

	// ---- exentos: NO se les aplica anti-cheat (stats SI) ----
	ref TStringArray exentos;

	void ExorCfgAnticheat()
	{
		dist_sospechosa_por_arma = new map<string, float>;
		watchlist = new TStringArray;
		exentos = new TStringArray;
	}

	void SetDefaults()
	{
		version = 1;
		habilitado = true;
		solo_watchlist = true;

		detector_kill = true;
		kill_check_los = true;
		kill_check_angulo = false;
		kill_check_distancia = true;
		kill_angulo_grados = 120;
		kill_min_senales = 1;
		dist_sospechosa_default = 400;
		dist_sospechosa_por_arma.Clear();
		// Ejemplos (editar/ampliar en anticheat.json). Umbral bajo = un kill mas
		// lejos que esto con esa arma se marca como sospechoso.
		dist_sospechosa_por_arma.Set("FNX45", 90);
		dist_sospechosa_por_arma.Set("Mlock-91", 90);
		dist_sospechosa_por_arma.Set("Deagle", 110);
		dist_sospechosa_por_arma.Set("MP5k", 150);
		dist_sospechosa_por_arma.Set("Saiga", 90);

		watchlist_activa = true;
		watch_check_mira = true;
		watch_check_aproximacion = true;
		watch_check_velocidad = true;
		watch_check_bajo_tierra = true;
		watch_check_godmode = true;
		watch_check_spinbot = true;
		watch_check_seguimiento = true;
		watch_check_punteria = true;
		aim_track_angulo = 5;
		aim_track_dist_max = 1000;
		aim_engagement_timeout_ms = 4000;
		aim_min_shots_log = 3;
		aim_min_shots_resumen = 30;
		aim_acc_sospechosa = 0.70;
		aim_acc_alta = 0.90;
		aim_acc_lejos_sospechosa = 0.60;
		aim_hs_sospechosa = 0.35;
		watch_angulo_grados = 8;
		watch_dist_min = 25;
		watch_velocidad_max = 12;
		teleport_velocidad_max = 35;
		vel_min_streak = 2;
		grace_ms = 5000;
		watch_bajo_tierra_metros = 2.5;
		godmode_hits = 4;
		spinbot_grados = 120;
		spinbot_ticks = 3;
		watch_track_dist_max = 1500;
		watch_lejos_metros = 800;
		watch_track_ticks = 4;
		watch_track_ticks_lejos = 2;
		watch_log_cooldown_seg = 15;
		watchlist.Clear();   // vacia: el admin agrega los SteamIDs a vigilar

		exentos.Clear();     // vacia: agregar admins/confianza para saltearles el anti-cheat
	}

	// Umbral de distancia para un arma: el propio del classname, o el default.
	// Devuelve 0 si no hay que chequear (default 0).
	float DistThreshold(string cls)
	{
		if (dist_sospechosa_por_arma && cls != "" && dist_sospechosa_por_arma.Contains(cls))
			return dist_sospechosa_por_arma.Get(cls);
		return dist_sospechosa_default;
	}

	bool EsExento(string sid)
	{
		if (!exentos || sid == "")
			return false;
		return exentos.Find(sid) != -1;
	}

	bool EsVigilado(string sid)
	{
		if (!watchlist || sid == "")
			return false;
		return watchlist.Find(sid) != -1;
	}
}

// ============================================================================
// Manager de configuracion
// ============================================================================
// ============================================================================
//  COFRE - cofres de recompensa que se abren en zonas por horario.
//  El player suelta una CAJA CERRADA (item del mod) en el piso de una zona activa;
//  tras tiempo_abrir_un_cofre_minutos se convierte en un COFRE ABIERTO (1000 slots)
//  relleno con UN bundle aleatorio de la tabla de loot de su color (cofres[]).
// ============================================================================
class ExorCfgCofreDia
{
	string dia = "lunes";        // lunes/martes/miercoles/jueves/viernes/sabado/domingo
	string hora_inicio = "07:00";
	string hora_fin = "18:00";
	bool activado = true;
}

class ExorCfgCofrePos
{
	float x = 0;
	float y = 0;   // 0 = auto (altura de la superficie)
	float z = 0;
}

// UNA zona: donde se pueden abrir cofres, con su ventana por dia.
class ExorCfgCofreLugar
{
	bool activo = true;
	ref ExorCfgCofrePos posicion;
	int rango_efecto = 5;        // radio (m): la caja tiene que soltarse dentro de esto
	ref array<ref ExorCfgCofreDia> dias;

	void ExorCfgCofreLugar()
	{
		posicion = new ExorCfgCofrePos;
		dias = new array<ref ExorCfgCofreDia>;
	}
}

// UN color de cofre + su tabla de loot. items: cada clave ("1","2",...) es un BUNDLE
// (lista de classnames). Al abrir se elige UN bundle al azar y se spawnea completo.
class ExorCfgCofreDef
{
	string color = "azul";
	ref map<string, ref TStringArray> items;

	void ExorCfgCofreDef()
	{
		items = new map<string, ref TStringArray>;
	}
}

// UN objeto de la estructura del evento (se spawnea en pos_zona + offset, con rotacion).
// Tambien se usa para las LUCES (roadflares): classname se ignora (siempre Exor_CofreLight),
// dx/dy/dz = offset respecto al PISO de la zona, yaw/pitch/roll = orientacion (roll 90 = parada).
class ExorCfgCofreObj
{
	string classname = "";
	float dx = 0;   // offset respecto al centro de la zona (metros)
	float dy = 0;
	float dz = 0;
	float yaw = 0;  // rotacion (grados)
	float pitch = 0;
	float roll = 0;
}

class ExorCfgCofre
{
	int version = 1;
	bool activado = true;                               // MASTER on/off del modulo
	// El reloj del host puede estar en UTC (ver diagnostico NWD): ajusta las horas del
	// horario. Ej: host en UTC y server en UTC-3 -> offset_horas = -3.
	int offset_horas = -4;
	int tiempo_abrir_un_cofre_minutos = 10;            // minutos en zona activa para abrir la caja
	bool colocar_marca_mapa = true;                    // marca en el mapa mientras la zona esta abierta
	ref array<int> aviso_antes_de_spawnear;            // minutos antes de abrir la ventana: avisa "en X min..."
	bool aviso_al_spawnwar = true;                     // aviso al abrirse la ventana [nombre del JSON, sic]
	int rango_detectar_payer_para_aviso_metros = 150;  // radio para contar players en zona [sic]
	bool aviso_de_players_dentro_del_rango_efecto = true;
	int minutos_aviso_players_en_zona = 5;             // cada cuantos min avisa "hay N players en la zona"
	// Al abrir la ventana aparece una ESTRUCTURA (uno o mas objetos con offset) rodeada de
	// bengalas; al cerrar la ventana la estructura y las bengalas se despawnean.
	bool evento_estructura = true;
	ref array<ref ExorCfgCofreObj> estructura_objetos;	// objetos de la estructura (pos = zona + offset)
	float altura_estructura = 0.5;                      // sube toda la estructura sobre el piso (para que no queden patas enterradas)
	// roadflares encendidas del evento: cada una con posicion (offset respecto al PISO de la
	// zona) y orientacion (roll 90 = parada/vertical). Default: 1 bajo la mesa + 1 parada sobre el drill.
	ref array<ref ExorCfgCofreObj> luces;
	// Slots de la mesa: en vez de tirar la caja al piso, se coloca sobre la mesa (hasta N).
	int mesa_max_cajas = 3;                             // cuantas cajas caben en la mesa
	float mesa_altura_slots = 0.45;                     // altura (m) de los slots sobre la base de la mesa (donde se apoyan las cajas)
	float mesa_espaciado_slots = 0.9;                   // separacion (m) entre cajas a lo largo de la mesa
	// Si al cerrar el evento hay una caja (cerrada o abierta CON loot) en la mesa, se mantiene
	// TODO (mesa, luz, cajas) esta cantidad de minutos de GRACIA antes de despawnear (para que
	// los players no pierdan cajas). Si no hay cajas con contenido, se despawnea al toque.
	int minutos_despawn_cofres_tras_evento = 30;
	// (Los cofres ABIERTOS y VACIOS se despawnean con el scan de la mesa cada minuto.)
	ref array<ref ExorCfgCofreLugar> lugares;
	ref array<ref ExorCfgCofreDef> cofres;

	void ExorCfgCofre()
	{
		aviso_antes_de_spawnear = new array<int>;
		estructura_objetos = new array<ref ExorCfgCofreObj>;
		luces = new array<ref ExorCfgCofreObj>;
		lugares = new array<ref ExorCfgCofreLugar>;
		cofres = new array<ref ExorCfgCofreDef>;
	}

	void Validate()
	{
		if (tiempo_abrir_un_cofre_minutos < 0)
			tiempo_abrir_un_cofre_minutos = 0;
		if (rango_detectar_payer_para_aviso_metros < 0)
			rango_detectar_payer_para_aviso_metros = 0;
		if (minutos_aviso_players_en_zona < 1)
			minutos_aviso_players_en_zona = 1;
		if (lugares)
		{
			int i;
			for (i = 0; i < lugares.Count(); i++)
			{
				ExorCfgCofreLugar l = lugares.Get(i);
				if (l && l.rango_efecto < 1)
					l.rango_efecto = 1;
			}
		}
	}

	void SetDefaults()
	{
		version = 1;
		activado = true;
		offset_horas = -4;
		tiempo_abrir_un_cofre_minutos = 10;
		colocar_marca_mapa = true;
		aviso_antes_de_spawnear = new array<int>;
		aviso_antes_de_spawnear.Insert(15);
		aviso_antes_de_spawnear.Insert(10);
		aviso_antes_de_spawnear.Insert(5);
		aviso_al_spawnwar = true;
		rango_detectar_payer_para_aviso_metros = 150;
		aviso_de_players_dentro_del_rango_efecto = true;
		minutos_aviso_players_en_zona = 5;
		evento_estructura = true;
		// estructura = mesa + drill (el drill va sobre la mesa: offset parseado del set de VPP).
		estructura_objetos = new array<ref ExorCfgCofreObj>;
		ExorCofreAddObj("vbldr_table_umakart", 0, 0, 0, 0);
		ExorCofreAddObj("StaticObj_Furniture_Drill", 0, 0.8, -1.4, 0);
		altura_estructura = 0.5;
		luces = new array<ref ExorCfgCofreObj>;
		ExorCofreAddLuz(0, 0.15, 0, 0, 0, 0);      // roadflare tirada bajo la mesa (piso + 0.15)
		ExorCofreAddLuz(0, 2.0, -1.4, 0, 0, 90);   // roadflare PARADA (roll 90) sobre el drill
		mesa_max_cajas = 3;
		mesa_altura_slots = 0.45;
		mesa_espaciado_slots = 0.9;
		minutos_despawn_cofres_tras_evento = 30;
		lugares = new array<ref ExorCfgCofreLugar>;
		// 2 zonas de ejemplo (0,0,0 = EDITAR). Ventana 07:00-18:00 todos los dias.
		lugares.Insert(ExorCofreDefaultLugar(644.0, 486.357, 1149.0));
		lugares.Insert(ExorCofreDefaultLugar(3965.58, 238.16, 10120.7));
		// 3 cofres de ejemplo (azul/verde/rojo) con classnames REALES de dayz para testear.
		cofres = new array<ref ExorCfgCofreDef>;

		// AZUL: bundle 1 = set M4 (+ ghillie); bundle 2 = set AKM (+ 2 ammo + ghillie)
		ExorCfgCofreDef az = new ExorCfgCofreDef();
		az.color = "azul";
		TStringArray az1 = new TStringArray;
		az1.Insert("M4_Suppressor"); az1.Insert("M4_T3NRDSOptic"); az1.Insert("M4_CQBBttstck_Black");
		az1.Insert("M4_MPHndgrd_Black"); az1.Insert("M4A1"); az1.Insert("Mag_STANAG_60Rnd");
		az1.Insert("Mag_STANAG_60Rnd"); az1.Insert("Ammo_556x45"); az1.Insert("Ammo_556x45");
		az1.Insert("GhillieAtt_Mossy");
		az.items.Set("1", az1);
		TStringArray az2 = new TStringArray;
		az2.Insert("AKM"); az2.Insert("ak_suppressor"); az2.Insert("ak_woodbttstck_black");
		az2.Insert("ak_railhndgrd_black"); az2.Insert("pso1optic"); az2.Insert("Mag_AKM_Drum75Rnd");
		az2.Insert("Mag_AKM_Drum75Rnd"); az2.Insert("Ammo_762x39"); az2.Insert("Ammo_762x39");
		az2.Insert("GhillieAtt_Mossy");
		az.items.Set("2", az2);
		cofres.Insert(az);

		// VERDE: bundle 1 = set SV98 (+ ammo); bundle 2 = set M14
		ExorCfgCofreDef vd = new ExorCfgCofreDef();
		vd.color = "verde";
		TStringArray vd1 = new TStringArray;
		vd1.Insert("SV98"); vd1.Insert("Mag_SV98_10Rnd"); vd1.Insert("Mag_SV98_10Rnd");
		vd1.Insert("MK4Optic_Green"); vd1.Insert("GhillieAtt_Woodland"); vd1.Insert("Ammo_762x54");
		vd.items.Set("1", vd1);
		TStringArray vd2 = new TStringArray;
		vd2.Insert("M14"); vd2.Insert("Mag_M14_20Rnd"); vd2.Insert("Mag_M14_20Rnd");
		vd2.Insert("MK4Optic_Black"); vd2.Insert("Ammo_308Win"); vd2.Insert("Ammo_308Win");
		vd2.Insert("GhillieAtt_Tan");
		vd.items.Set("2", vd2);
		cofres.Insert(vd);

		// ROJO: bundle 1 = granadas/40mm/M79; bundle 2 = explosivos
		ExorCfgCofreDef rj = new ExorCfgCofreDef();
		rj.color = "rojo";
		TStringArray rj1 = new TStringArray;
		rj1.Insert("M67Grenade:20"); rj1.Insert("Grenade_ChemGas:5"); rj1.Insert("Ammo_40mm_ChemGas:5");
		rj1.Insert("Ammo_40mm_Explosive:5"); rj1.Insert("M79");
		rj.items.Set("1", rj1);
		TStringArray rj2 = new TStringArray;
		rj2.Insert("ImprovisedExplosive:3"); rj2.Insert("Plastic_Explosive:6"); rj2.Insert("RemoteDetonator:3");
		rj.items.Set("2", rj2);
		cofres.Insert(rj);
	}

	// helpers (Enforce: sin expresiones multilinea)
	ExorCfgCofreLugar ExorCofreDefaultLugar(float x, float y, float z)
	{
		ExorCfgCofreLugar l = new ExorCfgCofreLugar();
		l.activo = true;
		l.posicion.x = x;
		l.posicion.y = y;
		l.posicion.z = z;
		l.rango_efecto = 5;
		ExorCofreAddDia(l, "lunes");
		ExorCofreAddDia(l, "martes");
		ExorCofreAddDia(l, "miercoles");
		ExorCofreAddDia(l, "jueves");
		ExorCofreAddDia(l, "viernes");
		ExorCofreAddDia(l, "sabado");
		ExorCofreAddDia(l, "domingo");
		return l;
	}

	void ExorCofreAddDia(ExorCfgCofreLugar l, string dia)
	{
		ExorCfgCofreDia d = new ExorCfgCofreDia();
		d.dia = dia;
		d.hora_inicio = "00:00";
		d.hora_fin = "23:59";
		d.activado = true;
		l.dias.Insert(d);
	}

	void ExorCofreAddBundle(ExorCfgCofreDef def, string key, string a, string b)
	{
		TStringArray arr = new TStringArray;
		arr.Insert(a);
		arr.Insert(b);
		def.items.Set(key, arr);
	}

	void ExorCofreAddObj(string cls, float dx, float dy, float dz, float yaw)
	{
		ExorCfgCofreObj o = new ExorCfgCofreObj();
		o.classname = cls;
		o.dx = dx; o.dy = dy; o.dz = dz; o.yaw = yaw;
		estructura_objetos.Insert(o);
	}

	void ExorCofreAddLuz(float dx, float dy, float dz, float yaw, float pitch, float roll)
	{
		ExorCfgCofreObj o = new ExorCfgCofreObj();
		o.dx = dx; o.dy = dy; o.dz = dz; o.yaw = yaw; o.pitch = pitch; o.roll = roll;
		luces.Insert(o);
	}
}

class ExorConfig
{
	ref ExorCfgStorage storage;
	ref ExorCfgVehiculos vehiculos;
	ref ExorCfgMunicion municion;
	ref ExorCfgParty party;
	ref ExorCfgSpawns spawns;
	ref ExorCfgMapa mapa;
	ref ExorCfgItems items;
	ref ExorCfgVip vip;
	ref ExorCfgKillfeed killfeed;
	ref ExorCfgServerInfo serverinfo;
	ref ExorCfgChat chat;
	ref ExorCfgReparacion reparacion;
	ref ExorCfgBodyCadaver bodycadaver;
	ref ExorCfgAutorun autorun;
	ref ExorCfgAnticheat anticheat;
	ref ExorCfgKoth koth;
	ref ExorCfgNoBuild nobuild;
	ref ExorCfgCofre cofre;
	bool m_Synced;	// cliente: true cuando ya recibio la config del server

	void ExorConfig()
	{
		storage = new ExorCfgStorage;
		vehiculos = new ExorCfgVehiculos;
		municion = new ExorCfgMunicion;
		party = new ExorCfgParty;
		spawns = new ExorCfgSpawns;
		mapa = new ExorCfgMapa;
		items = new ExorCfgItems;
		vip = new ExorCfgVip;
		killfeed = new ExorCfgKillfeed;
		serverinfo = new ExorCfgServerInfo;
		chat = new ExorCfgChat;
		reparacion = new ExorCfgReparacion;
		bodycadaver = new ExorCfgBodyCadaver;
		autorun = new ExorCfgAutorun;
		anticheat = new ExorCfgAnticheat;
		koth = new ExorCfgKoth;
		nobuild = new ExorCfgNoBuild;
		cofre = new ExorCfgCofre;
	}

	// SERVER: serializa la config relevante al cliente a JSON
	string BuildClientJson()
	{
		ExorClientCfgDTO d = new ExorClientCfgDTO();
		d.grupo = party.grupo;	// reusar los objetos vivos solo para escribir
		d.mapa = mapa;
		d.items = items;
		d.veh_camara = vehiculos.camara;
		d.veh_inventario = vehiculos.inventario;
		d.reparacion = reparacion;
		d.autorun = autorun;
		d.permitir_construir_cerca = party.territorio.permitir_construir_cerca;
		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(d, false, data);
		return data;
	}

	// CLIENTE: aplica la config recibida del server sobre el singleton
	static void ApplyClientJson(string json)
	{
		ExorClientCfgDTO d = new ExorClientCfgDTO();
		JsonSerializer js = new JsonSerializer();
		string err;
		if (!js.ReadFromString(d, json, err))
			return;
		ExorConfig c = GetExorConfig();
		if (d.grupo)
			c.party.grupo = d.grupo;
		if (d.mapa)
			c.mapa = d.mapa;
		if (d.items)
			c.items = d.items;
		if (d.veh_camara)
			c.vehiculos.camara = d.veh_camara;
		if (d.veh_inventario)
			c.vehiculos.inventario = d.veh_inventario;
		if (d.reparacion)
			c.reparacion = d.reparacion;
		if (d.autorun)
			c.autorun = d.autorun;
		c.party.territorio.permitir_construir_cerca = d.permitir_construir_cerca;
		c.m_Synced = true;
	}

	// SERVER: serializa SOLO el serverinfo (panel) para su RPC propio. Se manda aparte
	// del bundle grande porque el texto del panel puede ser largo y el JSON combinado
	// se pasaba del limite, haciendo fallar TODO el sync.
	string BuildServerInfoJson()
	{
		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(serverinfo, false, data);
		return data;
	}

	// CLIENTE: aplica el serverinfo recibido por su RPC propio.
	static void ApplyServerInfoJson(string json)
	{
		ExorCfgServerInfo si = new ExorCfgServerInfo();
		JsonSerializer js = new JsonSerializer();
		string err;
		if (!js.ReadFromString(si, json, err))
			return;
		GetExorConfig().serverinfo = si;
	}

	static ref ExorConfig Load()
	{
		ExorConfig c = new ExorConfig();

		if (!FileExist(ExorStorageConstants.CONFIG_DIR))
			MakeDirectory(ExorStorageConstants.CONFIG_DIR);

		// Migracion del settings.json monolitico viejo (una sola vez)
		c.MigrateLegacyIfNeeded();

		c.LoadStorage();
		c.LoadVehiculos();
		c.LoadMunicion();
		c.LoadParty();
		c.LoadSpawns();
		c.LoadMapa();
		c.LoadItems();
		c.LoadVip();
		c.LoadKillfeed();
		c.LoadServerInfo();
		c.LoadChat();
		c.LoadReparacion();
		c.LoadBodyCadaver();
		c.LoadAutorun();
		c.LoadAnticheat();
		c.LoadKoth();
		c.LoadNoBuild();
		c.LoadCofre();

		return c;
	}

	// ---- carga/creacion por modulo (cargar + re-guardar para completar campos nuevos) ----
	void LoadStorage()
	{
		if (FileExist(ExorStorageConstants.CFG_STORAGE))
			JsonFileLoader<ExorCfgStorage>.JsonLoadFile(ExorStorageConstants.CFG_STORAGE, storage);
		else
			storage.SetDefaults();
		JsonFileLoader<ExorCfgStorage>.JsonSaveFile(ExorStorageConstants.CFG_STORAGE, storage);
	}

	void LoadVehiculos()
	{
		if (FileExist(ExorStorageConstants.CFG_VEHICULOS))
			JsonFileLoader<ExorCfgVehiculos>.JsonLoadFile(ExorStorageConstants.CFG_VEHICULOS, vehiculos);
		else
			vehiculos.SetDefaults();
		JsonFileLoader<ExorCfgVehiculos>.JsonSaveFile(ExorStorageConstants.CFG_VEHICULOS, vehiculos);
	}

	void LoadMunicion()
	{
		if (FileExist(ExorStorageConstants.CFG_MUNICION))
			JsonFileLoader<ExorCfgMunicion>.JsonLoadFile(ExorStorageConstants.CFG_MUNICION, municion);
		else
			municion.SetDefaults();
		JsonFileLoader<ExorCfgMunicion>.JsonSaveFile(ExorStorageConstants.CFG_MUNICION, municion);
	}

	void LoadParty()
	{
		if (FileExist(ExorStorageConstants.CFG_PARTY))
			JsonFileLoader<ExorCfgParty>.JsonLoadFile(ExorStorageConstants.CFG_PARTY, party);
		else
			party.SetDefaults();
		JsonFileLoader<ExorCfgParty>.JsonSaveFile(ExorStorageConstants.CFG_PARTY, party);
	}

	void LoadSpawns()
	{
		if (FileExist(ExorStorageConstants.CFG_SPAWNS))
			JsonFileLoader<ExorCfgSpawns>.JsonLoadFile(ExorStorageConstants.CFG_SPAWNS, spawns);
		else
			spawns.SetDefaults();
		JsonFileLoader<ExorCfgSpawns>.JsonSaveFile(ExorStorageConstants.CFG_SPAWNS, spawns);
	}

	void LoadMapa()
	{
		if (FileExist(ExorStorageConstants.CFG_MAPA))
			JsonFileLoader<ExorCfgMapa>.JsonLoadFile(ExorStorageConstants.CFG_MAPA, mapa);
		else
			mapa.SetDefaults();
		JsonFileLoader<ExorCfgMapa>.JsonSaveFile(ExorStorageConstants.CFG_MAPA, mapa);
	}

	void LoadAutorun()
	{
		if (FileExist(ExorStorageConstants.CFG_AUTORUN))
			JsonFileLoader<ExorCfgAutorun>.JsonLoadFile(ExorStorageConstants.CFG_AUTORUN, autorun);
		else
			autorun.SetDefaults();
		JsonFileLoader<ExorCfgAutorun>.JsonSaveFile(ExorStorageConstants.CFG_AUTORUN, autorun);
	}

	void LoadAnticheat()
	{
		if (FileExist(ExorStorageConstants.CFG_ANTICHEAT))
			JsonFileLoader<ExorCfgAnticheat>.JsonLoadFile(ExorStorageConstants.CFG_ANTICHEAT, anticheat);
		else
			anticheat.SetDefaults();
		JsonFileLoader<ExorCfgAnticheat>.JsonSaveFile(ExorStorageConstants.CFG_ANTICHEAT, anticheat);
	}

	void LoadKoth()
	{
		// Si ya existe, cargarlo y NO re-guardarlo: asi el archivo del admin queda EXACTO
		// como lo edito (no se reformatea ni se pisa en cada arranque). Solo se crea/guarda
		// la primera vez (con defaults). Mismo criterio que serverinfo.json.
		if (FileExist(ExorStorageConstants.CFG_KOTH))
		{
			JsonFileLoader<ExorCfgKoth>.JsonLoadFile(ExorStorageConstants.CFG_KOTH, koth);
			koth.Validate();	// clampa en memoria (no toca el archivo)
		}
		else
		{
			koth.SetDefaults();
			koth.Validate();
			JsonFileLoader<ExorCfgKoth>.JsonSaveFile(ExorStorageConstants.CFG_KOTH, koth);
		}
	}

	void LoadItems()
	{
		if (FileExist(ExorStorageConstants.CFG_ITEMS))
			JsonFileLoader<ExorCfgItems>.JsonLoadFile(ExorStorageConstants.CFG_ITEMS, items);
		else
			items.SetDefaults();
		JsonFileLoader<ExorCfgItems>.JsonSaveFile(ExorStorageConstants.CFG_ITEMS, items);
	}

	void LoadVip()
	{
		if (FileExist(ExorStorageConstants.CFG_VIP))
			JsonFileLoader<ExorCfgVip>.JsonLoadFile(ExorStorageConstants.CFG_VIP, vip);
		else
			vip.SetDefaults();
		// Migra vip_steamids[] viejo y sella la fecha de ingreso (hoy) de los nuevos.
		vip.MigrateAndStamp();
		JsonFileLoader<ExorCfgVip>.JsonSaveFile(ExorStorageConstants.CFG_VIP, vip);
	}

	void LoadKillfeed()
	{
		if (FileExist(ExorStorageConstants.CFG_KILLFEED))
			JsonFileLoader<ExorCfgKillfeed>.JsonLoadFile(ExorStorageConstants.CFG_KILLFEED, killfeed);
		else
			killfeed.SetDefaults();
		JsonFileLoader<ExorCfgKillfeed>.JsonSaveFile(ExorStorageConstants.CFG_KILLFEED, killfeed);
	}

	void LoadServerInfo()
	{
		// El contenido lo escribe el admin a mano (texto de tabs). NO re-guardar si ya
		// existe: un re-guardado en cada arranque le pisaba el texto si el JSON tenia
		// cualquier error sutil (coma de mas, comillas tipograficas, tilde mal codificada)
		// porque el parser falla en silencio y deja el objeto vacio. Solo se crea 1 vez.
		if (FileExist(ExorStorageConstants.CFG_SERVERINFO))
		{
			JsonFileLoader<ExorCfgServerInfo>.JsonLoadFile(ExorStorageConstants.CFG_SERVERINFO, serverinfo);
		}
		else
		{
			serverinfo.SetDefaults();
			JsonFileLoader<ExorCfgServerInfo>.JsonSaveFile(ExorStorageConstants.CFG_SERVERINFO, serverinfo);
		}
	}

	void LoadChat()
	{
		if (FileExist(ExorStorageConstants.CFG_CHAT))
			JsonFileLoader<ExorCfgChat>.JsonLoadFile(ExorStorageConstants.CFG_CHAT, chat);
		else
			chat.SetDefaults();
		JsonFileLoader<ExorCfgChat>.JsonSaveFile(ExorStorageConstants.CFG_CHAT, chat);
	}

	void LoadReparacion()
	{
		if (FileExist(ExorStorageConstants.CFG_REPARACION))
			JsonFileLoader<ExorCfgReparacion>.JsonLoadFile(ExorStorageConstants.CFG_REPARACION, reparacion);
		else
			reparacion.SetDefaults();
		JsonFileLoader<ExorCfgReparacion>.JsonSaveFile(ExorStorageConstants.CFG_REPARACION, reparacion);
	}

	void LoadBodyCadaver()
	{
		if (FileExist(ExorStorageConstants.CFG_BODYCADAVER))
			JsonFileLoader<ExorCfgBodyCadaver>.JsonLoadFile(ExorStorageConstants.CFG_BODYCADAVER, bodycadaver);
		else
			bodycadaver.SetDefaults();
		JsonFileLoader<ExorCfgBodyCadaver>.JsonSaveFile(ExorStorageConstants.CFG_BODYCADAVER, bodycadaver);
	}

	void LoadNoBuild()
	{
		if (FileExist(ExorStorageConstants.CFG_NOBUILD))
			JsonFileLoader<ExorCfgNoBuild>.JsonLoadFile(ExorStorageConstants.CFG_NOBUILD, nobuild);
		else
			nobuild.SetDefaults();
		JsonFileLoader<ExorCfgNoBuild>.JsonSaveFile(ExorStorageConstants.CFG_NOBUILD, nobuild);
	}

	void LoadCofre()
	{
		// Igual que KOTH: si ya existe, cargar y NO re-guardar (el archivo del admin queda
		// EXACTO como lo edito, no se reformatea). Solo se crea la 1ra vez con defaults.
		if (FileExist(ExorStorageConstants.CFG_COFRE))
		{
			JsonFileLoader<ExorCfgCofre>.JsonLoadFile(ExorStorageConstants.CFG_COFRE, cofre);
			cofre.Validate();
		}
		else
		{
			cofre.SetDefaults();
			cofre.Validate();
			JsonFileLoader<ExorCfgCofre>.JsonSaveFile(ExorStorageConstants.CFG_COFRE, cofre);
		}
	}

	// ---- migracion del settings.json monolitico viejo ----
	void MigrateLegacyIfNeeded()
	{
		// Solo migrar si existe el viejo Y todavia no se hizo el split
		if (!FileExist(ExorStorageConstants.CONFIG_PATH))
			return;
		if (FileExist(ExorStorageConstants.CFG_STORAGE))
			return;	// ya hay config nueva: no pisar

		ExorStorageSettings old = new ExorStorageSettings();
		JsonFileLoader<ExorStorageSettings>.JsonLoadFile(ExorStorageConstants.CONFIG_PATH, old);

		// storage (el settings.json viejo estaba en MINUTOS -> pasar a segundos)
		storage.virtualizar_segundos = old.virtualizar_minutos * 60;
		storage.auto_cerrar_segundos = old.auto_cerrar_minutos * 60;
		storage.multiplicador_comida = old.multiplicador_comida;
		storage.permitir_ropa_con_items = old.permitir_ropa_con_items;
		storage.cooldown_abrir_segundos = old.cooldown_abrir_segundos;
		storage.blacklist.Clear();
		if (old.blacklist)
		{
			int bi;
			for (bi = 0; bi < old.blacklist.Count(); bi++)
				storage.blacklist.Insert(old.blacklist.Get(bi));
		}

		// vehiculos
		vehiculos.vehiculos_dormir = old.vehiculos_dormir;
		vehiculos.vehiculos_dormir_minutos = old.vehiculos_dormir_minutos;
		vehiculos.vehiculos_despertar_metros = old.vehiculos_despertar_metros;
		vehiculos.voltear_vehiculos = old.voltear_vehiculos;
		vehiculos.voltear_segundos = old.voltear_segundos;
		vehiculos.vehiculos_excluidos.Clear();
		if (old.vehiculos_excluidos)
		{
			int vi;
			for (vi = 0; vi < old.vehiculos_excluidos.Count(); vi++)
				vehiculos.vehiculos_excluidos.Insert(old.vehiculos_excluidos.Get(vi));
		}

		// municion
		municion.stack_municion_default = old.stack_municion_default;
		municion.auto_stack = old.auto_stack;
		municion.spawn_municion_min_default = old.spawn_municion_min_default;
		municion.spawn_municion_max_default = old.spawn_municion_max_default;
		municion.stack_municion.Clear();
		if (old.stack_municion)
		{
			int si;
			for (si = 0; si < old.stack_municion.Count(); si++)
				municion.stack_municion.Set(old.stack_municion.GetKey(si), old.stack_municion.GetElement(si));
		}
		municion.spawn_municion.Clear();
		if (old.spawn_municion)
		{
			int spi;
			for (spi = 0; spi < old.spawn_municion.Count(); spi++)
				municion.spawn_municion.Set(old.spawn_municion.GetKey(spi), old.spawn_municion.GetElement(spi));
		}
		municion.municion_excluida.Clear();
		if (old.municion_excluida)
		{
			int mi;
			for (mi = 0; mi < old.municion_excluida.Count(); mi++)
				municion.municion_excluida.Insert(old.municion_excluida.Get(mi));
		}

		// Guardar los 3 modulos migrados (party/spawns/mapa se crean con defaults en sus Load)
		JsonFileLoader<ExorCfgStorage>.JsonSaveFile(ExorStorageConstants.CFG_STORAGE, storage);
		JsonFileLoader<ExorCfgVehiculos>.JsonSaveFile(ExorStorageConstants.CFG_VEHICULOS, vehiculos);
		JsonFileLoader<ExorCfgMunicion>.JsonSaveFile(ExorStorageConstants.CFG_MUNICION, municion);

		// Respaldar (re-escribiendo el DTO ya cargado) y sacar de en medio el viejo
		JsonFileLoader<ExorStorageSettings>.JsonSaveFile(ExorStorageConstants.CONFIG_MIGRATED, old);
		DeleteFile(ExorStorageConstants.CONFIG_PATH);

		Print(string.Format("%1 settings.json migrado a config modular (respaldo: settings.json.migrated)", ExorStorageConstants.LOG));
	}
}

// Singleton global
static ref ExorConfig g_ExorConfig;

static ExorConfig GetExorConfig()
{
	if (!g_ExorConfig)
	{
		g_ExorConfig = ExorConfig.Load();
	}
	return g_ExorConfig;
}
