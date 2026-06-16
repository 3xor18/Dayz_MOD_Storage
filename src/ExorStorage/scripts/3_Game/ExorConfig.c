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
	int virtualizar_minutos = 10;     // 0 = off
	int auto_cerrar_minutos = 5;      // 0 = off
	float multiplicador_comida = 2.0;
	bool permitir_ropa_con_items = true;
	ref TStringArray blacklist;
	int cooldown_abrir_segundos = 3;  // 0 = sin cooldown

	void ExorCfgStorage()
	{
		blacklist = new TStringArray;
	}

	void SetDefaults()
	{
		version = 1;
		virtualizar_minutos = 10;
		auto_cerrar_minutos = 5;
		multiplicador_comida = 2.0;
		permitir_ropa_con_items = true;
		cooldown_abrir_segundos = 3;
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
	bool quitar_dano_vehiculos = false; // true = los autos no reciben dano
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
	bool log_desconexion_base_ajena = true;       // #4a: loguear si un ajeno se desloguea dentro de territorio enemigo
	bool sacar_de_base_ajena_al_reconectar = true;// #4b: al reconectar dentro de territorio ajeno, teletransportar al borde
	int log_dias_retener = 7;                     // dias que se conservan los archivos de raidlog (0 = nunca borrar)
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
		proteccion.log_desconexion_base_ajena = true;
		proteccion.sacar_de_base_ajena_al_reconectar = true;
		proteccion.log_dias_retener = 7;
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
	int max_lineas = 9;              // maximo de lineas simultaneas (la mas vieja se va)
	int cooldown_segundos = 2;       // anti-spam: minimo entre mensajes (0 = sin limite)
	bool bloquear_repetidos = true;  // anti-spam: bloquear el MISMO mensaje seguido

	void SetDefaults()
	{
		habilitado = true;
		radio_zona_metros = 50;
		duracion_segundos = 25;
		max_lineas = 9;
		cooldown_segundos = 2;
		bloquear_repetidos = true;
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
	ref ExorCfgServerInfo serverinfo;	// panel de server info (texto de tabs)
	ref ExorCfgReparacion reparacion;	// reparar-a-pristine + lista de kits combinables (el cliente la usa para ofrecer la accion)

	void ExorClientCfgDTO()
	{
		grupo = new ExorCfgPartyGrupo;
		mapa = new ExorCfgMapa;
		items = new ExorCfgItems;
		veh_camara = new ExorCfgVehCamara;
		veh_inventario = new ExorCfgVehInventario;
		serverinfo = new ExorCfgServerInfo;
		reparacion = new ExorCfgReparacion;
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
		equip_loadout.bolso = "AssaultBag_Black";
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

	void SetDefaults()
	{
		habilitado = true;
		duracion_segundos = 6;
		max_lineas = 5;
		mostrar_suicidios = true;
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
		general_lineas = new TStringArray;
		general_lineas.Insert("Bienvenido al server.");
		general_lineas.Insert("Editá este texto en serverinfo.json (general_lineas).");
		tab_reglas = true;
		reglas_titulo = "Reglas";
		reglas_lineas = new TStringArray;
		reglas_lineas.Insert("1. No insultar.");
		reglas_lineas.Insert("2. Editá las reglas en serverinfo.json (reglas_lineas).");
		tab_score = true;
		score_titulo = "Score";
	}
}

// ============================================================================
// Manager de configuracion
// ============================================================================
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
		d.serverinfo = serverinfo;
		d.reparacion = reparacion;
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
		if (d.serverinfo)
			c.serverinfo = d.serverinfo;
		if (d.reparacion)
			c.reparacion = d.reparacion;
		c.m_Synced = true;
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
		if (FileExist(ExorStorageConstants.CFG_SERVERINFO))
			JsonFileLoader<ExorCfgServerInfo>.JsonLoadFile(ExorStorageConstants.CFG_SERVERINFO, serverinfo);
		else
			serverinfo.SetDefaults();
		JsonFileLoader<ExorCfgServerInfo>.JsonSaveFile(ExorStorageConstants.CFG_SERVERINFO, serverinfo);
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

		// storage
		storage.virtualizar_minutos = old.virtualizar_minutos;
		storage.auto_cerrar_minutos = old.auto_cerrar_minutos;
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
