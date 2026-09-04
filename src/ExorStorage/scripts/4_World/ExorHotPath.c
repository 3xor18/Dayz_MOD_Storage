// ============================================================================
// 3xor_Vanilla_Optimization - CAMINO CALIENTE: caches compartidos (SOLO server)
// ============================================================================
// Todo lo de este archivo existe por una sola razon: hay codigo del mod que corre
// POR ITEM, POR JUGADOR o POR FRAME, y con 70 jugadores y miles de contenedores el
// costo por llamada deja de ser despreciable. La regla que se aplica es siempre la
// misma: LO QUE NO CAMBIA DENTRO DE UN TICK SE CALCULA UNA VEZ POR TICK.
//
//   ExorHotFlags   banderas de config leidas una vez (evita 3-4 derefs por llamada
//                  en hooks que corren miles de veces por segundo)
//   ExorLootWindow cache por MINUTO de "estamos en horario de raid" (evita parsear
//                  strings de horario en cada apertura de mueble y en cada tick)
//   ExorAliveCache posiciones de los jugadores VIVOS, una vez por tick, compartidas
//                  por todos los chequeos de proximidad
//   ExorMath       distancias AL CUADRADO (sin raiz cuadrada)
// ============================================================================

// ----------------------------------------------------------------------------
//  DISTANCIAS SIN RAIZ
// ----------------------------------------------------------------------------
// Comparar distancias no necesita la raiz cuadrada: si a^2 <= r^2 entonces a <= r.
// vector.Distance hace un sqrt por llamada y estos chequeos corren por entidad y por
// jugador en cada tick. Comparar los cuadrados da EXACTAMENTE el mismo resultado.
class ExorMath
{
	// distancia al cuadrado en el plano XZ (ignora altura, que es lo que quiere casi
	// todo el mod: un mueble en el 2do piso sigue estando "en" la base)
	static float Dist2DSq(vector a, vector b)
	{
		float dx = a[0] - b[0];
		float dz = a[2] - b[2];
		return (dx * dx) + (dz * dz);
	}

	// distancia al cuadrado en 3D
	static float Dist3DSq(vector a, vector b)
	{
		float dx = a[0] - b[0];
		float dy = a[1] - b[1];
		float dz = a[2] - b[2];
		return (dx * dx) + (dy * dy) + (dz * dz);
	}
}

// ----------------------------------------------------------------------------
//  BANDERAS DE CONFIG PRECALCULADAS
// ----------------------------------------------------------------------------
// PROBLEMA: hooks como ItemBase.EEItemLocationChanged corren en CADA movimiento de
// inventario de CADA item del mundo (incluidos los miles que genera restaurar un
// contenedor). Hacer ahi GetExorConfig().bodycadaver.forense_registrar_looteadores
// son 3 llamadas + 2 derefs por item. Multiplicado por decenas de miles deja de ser
// gratis, y ademas el valor NO cambia entre recargas de config.
// SOLUCION: se leen UNA vez y quedan en un bool estatico. Refresh() se llama al
// arrancar y cada vez que la config se recarga.
class ExorHotFlags
{
	static bool s_Ready;

	static bool s_ForenseLooteadores;	// bodycadaver.forense_registrar_looteadores
	static bool s_LogRoboContenedor;	// party.proteccion.log_robo_contenedor
	static bool s_LogAbrirBarril;		// party.proteccion.log_abrir_barril_ajeno
	static bool s_TerritorioOn;			// party.territorio.habilitado
	static bool s_SoloMiembrosLotean;	// storage.solo_miembros_lotean_muebles
	static bool s_AutoStackMunicion;	// municion.auto_stack

	static void Refresh()
	{
		ExorConfig c = GetExorConfig();
		if (!c)
			return;
		if (c.bodycadaver)
			s_ForenseLooteadores = c.bodycadaver.forense_registrar_looteadores;
		if (c.party && c.party.proteccion)
		{
			s_LogRoboContenedor = c.party.proteccion.log_robo_contenedor;
			s_LogAbrirBarril = c.party.proteccion.log_abrir_barril_ajeno;
		}
		if (c.party && c.party.territorio)
			s_TerritorioOn = c.party.territorio.habilitado;
		if (c.storage)
			s_SoloMiembrosLotean = c.storage.solo_miembros_lotean_muebles;
		if (c.municion)
			s_AutoStackMunicion = c.municion.auto_stack;
		s_Ready = true;
	}

	// Lectura segura: si todavia no se refresco (orden de arranque raro), se refresca
	// sola. Despues es un simple acceso a un bool estatico.
	static bool ForenseLooteadores()
	{
		if (!s_Ready)
			Refresh();
		return s_ForenseLooteadores;
	}

	static bool LogRoboContenedor()
	{
		if (!s_Ready)
			Refresh();
		return s_LogRoboContenedor;
	}

	static bool LogAbrirBarril()
	{
		if (!s_Ready)
			Refresh();
		return s_LogAbrirBarril;
	}

	static bool TerritorioOn()
	{
		if (!s_Ready)
			Refresh();
		return s_TerritorioOn;
	}

	static bool SoloMiembrosLotean()
	{
		if (!s_Ready)
			Refresh();
		return s_SoloMiembrosLotean;
	}

	static bool AutoStackMunicion()
	{
		if (!s_Ready)
			Refresh();
		return s_AutoStackMunicion;
	}
}

// ----------------------------------------------------------------------------
//  VENTANA DE LOOTEO LIBRE (RAID) - CACHE POR MINUTO
// ----------------------------------------------------------------------------
// ExorMuebleRules.IsLootFreeNow() parsea los strings de horario ("22:00" -> minutos)
// y compara dias en CADA llamada, y se llama al abrir cada mueble, en cada tick del
// manager y en cada pase del self-heal. El resultado solo puede cambiar cuando cambia
// el MINUTO del reloj -> se calcula como mucho una vez por minuto.
class ExorLootWindow
{
	static int  s_MinCalculado = -1;	// minuto absoluto de la ultima evaluacion
	static bool s_Valor;

	static bool IsRaidNow()
	{
		int m = ExorTimeUtil.NowMinutes();
		if (m == s_MinCalculado)
			return s_Valor;
		s_MinCalculado = m;
		s_Valor = ExorMuebleRules.CalcularLootFree();
		return s_Valor;
	}

	// fuerza el recalculo (tras recargar la config de horarios)
	static void Invalidar()
	{
		s_MinCalculado = -1;
	}
}

// ----------------------------------------------------------------------------
//  POSICIONES DE JUGADORES VIVOS - UNA VEZ POR TICK
// ----------------------------------------------------------------------------
// PROBLEMA: cada chequeo de "hay alguien cerca?" recorria la lista de jugadores
// haciendo PlayerBase.Cast + IsAlive() + vector.Distance (con raiz). Eso corre por
// contenedor abierto y por vehiculo, en cada tick: con 70 jugadores y cientos de
// entidades son cientos de miles de operaciones por minuto, casi todas repetidas.
// SOLUCION: el tick del manager llena esto UNA vez y todos comparten el array ya
// filtrado (solo vivos) y comparan distancias AL CUADRADO.
class ExorAliveCache
{
	static ref array<vector> s_Pos;
	static int s_Stamp;		// GetGame().GetTime() del ultimo llenado

	// Llena el cache para ESTE tick. Devuelve el array (nunca null).
	static array<vector> Rebuild(array<Man> players)
	{
		if (!s_Pos)
			s_Pos = new array<vector>;
		s_Pos.Clear();
		s_Stamp = GetGame().GetTime();
		if (!players)
			return s_Pos;
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase p = PlayerBase.Cast(players.Get(i));
			if (p && p.IsAlive())
				s_Pos.Insert(p.GetPosition());
		}
		return s_Pos;
	}

	static array<vector> Get()
	{
		if (!s_Pos)
			s_Pos = new array<vector>;
		return s_Pos;
	}

	// Garantiza que el cache no sea mas viejo que 'maxAgeMs'. Lo usan los llamadores que NO
	// vienen del tick del manager (ej. KOTH, a 1 Hz): si el cache esta fresco lo reusan gratis,
	// y si no, lo rearman ellos. Asi nadie trabaja con posiciones viejas y nadie recalcula al
	// pedo lo que otro ya calculo en este mismo instante.
	static void EnsureFresh(int maxAgeMs)
	{
		if (s_Pos && s_Stamp > 0 && (GetGame().GetTime() - s_Stamp) <= maxAgeMs)
			return;
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		Rebuild(players);
	}

	// hay algun jugador VIVO dentro de 'radius' de 'pos'? (comparacion al cuadrado)
	static bool AnyNear(vector pos, float radius)
	{
		return AnyNearList(Get(), pos, radius);
	}

	static bool AnyNearList(array<vector> posiciones, vector pos, float radius)
	{
		if (!posiciones)
			return false;
		float r2 = radius * radius;
		int i;
		for (i = 0; i < posiciones.Count(); i++)
		{
			if (ExorMath.Dist3DSq(posiciones.Get(i), pos) < r2)
				return true;
		}
		return false;
	}
}

// ----------------------------------------------------------------------------
//  "EN QUE TERRITORIO AJENO ESTOY PARADO?" - CACHE POR JUGADOR
// ----------------------------------------------------------------------------
// PROBLEMA: la respuesta se pedia una vez POR ITEM que entraba al inventario
// (ItemBase.OnInventoryEnter -> ExorAntiRaid.OnPickupInEnemyTerritory). Vaciar un
// locker son cientos de items en pocos segundos, y cada uno pagaba:
//     FindByPlayer  (O(clanes x miembros) con comparacion de strings)
//   + barrido de TODOS los mastiles con una raiz cuadrada cada uno.
// Y la respuesta es la MISMA para todos esos items: el jugador no se movio.
//
// SOLUCION: memoizacion por jugador con tres condiciones de caducidad, todas baratas:
//   1) se movio mas de EXOR_TERR_MOVE_M metros,
//   2) pasaron mas de EXOR_TERR_TTL_MS milisegundos,
//   3) cambio la version (se puso/saco un mastil, o cambio el roster de un clan).
// Cualquiera de las tres fuerza el recalculo. El estado vive en el propio PlayerBase
// (no hay map ni lookup global) y el puntero al mastil se anula solo si la entidad
// se borra, asi que no puede quedar colgado.
class ExorTerritoryProbe
{
	static const float EXOR_TERR_MOVE_M = 3.0;		// moverse menos que esto no cambia la respuesta
	static const int   EXOR_TERR_TTL_MS = 3000;		// red de seguridad por si algo cambia sin bump

	// version combinada: mastiles + roster. Se bumpea sola desde los dos lados.
	static int s_Version = 1;
	static void Bump() { s_Version = s_Version + 1; }
	static int Version() { return s_Version + ExorRosterVersion.Get(); }

	// Id del clan AJENO en cuyo territorio esta parado el jugador ("" si ninguno).
	// Antes devolvia la ENTIDAD mastil y la cacheaba en el PlayerBase; guardar un puntero a
	// una entidad que el motor puede borrar en cualquier momento era una fuente de punteros
	// colgados, y ademas ningun consumidor necesitaba la entidad. Ahora es un string.
	static string GrupoAjenoDe(PlayerBase p)
	{
		if (!p || !ExorHotFlags.TerritorioOn())
			return "";

		int now = GetGame().GetTime();
		vector pos = p.GetPosition();
		int ver = Version();

		if (p.ExorTerrCacheValido(now, pos, ver, EXOR_TERR_TTL_MS, EXOR_TERR_MOVE_M * EXOR_TERR_MOVE_M))
			return p.ExorTerrCacheGid();

		// MISS: resolver de verdad. El id del grupo del jugador se resuelve una vez y se
		// pasa ya hecho al indice de territorios (ver ExorTerritoryIndex).
		string gid = "";
		ExorGroup g = ExorGroupManager.Get().FindByPlayer(ExorGroupManager.SteamId(p));
		if (g)
			gid = g.id;
		string ajeno = ExorTerritoryManager.Get().FindEnemyTerritoryAtEx(gid, pos);
		p.ExorTerrCacheSet(now, pos, ver, ajeno);
		return ajeno;
	}
}

// ----------------------------------------------------------------------------
//  SCHEDULER 1 Hz UNIFICADO
// ----------------------------------------------------------------------------
// PROBLEMA: habia TRES timers independientes a 1 Hz (party en vivo, KOTH y cofre).
// Cada uno se despierta por su cuenta, cada uno pide su propia lista de jugadores y
// su propia hora del reloj, y cada uno es una entrada mas en el CallQueue del motor.
// El trabajo util es el mismo; lo repetido es el andamiaje.
//
// SOLUCION: un solo latido, con lo compartido resuelto UNA vez y pasado a los
// suscriptores. Ademas deja un unico lugar donde medir y donde ordenar: si mañana hay
// que priorizar o saltear un subsistema bajo carga, se hace aca y no en tres archivos.
//
// Es deliberadamente una lista fija de llamadas y no un registro dinamico de
// suscriptores: son tres, se conocen en tiempo de compilacion, y una indireccion por
// puntero de funcion no compra nada y esconde el orden de ejecucion.
class ExorTick1Hz
{
	static bool s_Arrancado;

	// hora local compartida por todos los suscriptores de este latido
	static int s_MinOfDay;
	static int s_Weekday;
	static int s_DayKey;

	static void Start()
	{
		if (!GetGame().IsServer() || s_Arrancado)
			return;
		s_Arrancado = true;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorTick1Hz.Latido, 1000, true);
	}

	// Lista de jugadores del latido, pedida UNA vez por segundo y compartida.
	// Antes cada suscriptor que necesitaba jugadores hacia su propio GetPlayers() con su
	// propio array nuevo -y el KOTH lo hacia una vez POR EVENTO ACTIVO, no una por tick-.
	// Es el mismo patron que ya usa el tick de contenedores.
	static ref array<Man> s_Players;
	static int s_PlayersMs;

	// Devuelve la lista del latido. Si alguien la pide fuera del latido (o antes del
	// primero), se rearma sola: nadie trabaja con datos viejos y nadie recalcula al pedo.
	static array<Man> Jugadores()
	{
		if (!s_Players)
			s_Players = new array<Man>;
		if (s_PlayersMs == 0 || GetGame().GetTime() - s_PlayersMs > 1000)
			Refrescar();
		return s_Players;
	}

	static void Refrescar()
	{
		if (!s_Players)
			s_Players = new array<Man>;
		s_Players.Clear();
		GetGame().GetPlayers(s_Players);
		s_PlayersMs = GetGame().GetTime();
	}

	static void Latido()
	{
		if (!GetGame().IsServer())
			return;

		// Hora local calculada UNA vez para los tres (antes cada uno hacia su NowLocal,
		// con sus llamadas al reloj del sistema y su calculo de dia de la semana).
		ExorCfgStorage st = GetExorConfig().storage;
		int offset = 0;
		if (st)
			offset = st.offset_horas;
		ExorCofre.NowLocal(offset, s_MinOfDay, s_Weekday, s_DayKey);
		Refrescar();	// una sola lista de jugadores para los tres suscriptores

		ExorPartyLive.Get().TickTimed();
		ExorKoth.Get().TickTimed();
		ExorCofre.Get().TickTimed();
		ExorGodPack.PushEsp(s_Players);	// ESP del duenio (no-op si esta apagado)
	}
}

// ----------------------------------------------------------------------------
//  EL APAGADO ANTERIOR FUE LIMPIO?
// ----------------------------------------------------------------------------
// PARA QUE: al abrir por primera vez tras un arranque, cada contenedor barre el piso
// a su alrededor (GetObjectsAtPosition de 12-15 m + parseo de su JSON) buscando los
// items que DayZ tira por "invalid location" al cargar contenido anidado. Medido en
// local: 83 ms UN SOLO barril. Con cientos de contenedores abriendose en los primeros
// minutos de un raid, eso es un impuesto enorme.
//
// LA CLAVE: ese derrame es un fenomeno del apagado SUCIO. Si el server cerro bien,
// OnMissionFinish corrio VirtualizeAll y TODOS los contenedores quedaron guardados con
// el cargo vacio -> el motor no tenia nada anidado que tirar al piso y no hay NADA que
// barrer. O sea que en el caso normal el barrido es trabajo puro al pedo.
//
// COMO SE SABE: se escribe un marcador al terminar VirtualizeAll y se borra al arrancar.
// Si el marcador estaba -> el apagado anterior fue limpio -> no se barre.
// Si no estaba (crash, kill del host, corte de luz) -> se barre como siempre.
// Ante la duda se BARRE: equivocarse por barrer de mas cuesta unos ms; equivocarse por
// no barrer deja items duplicados en el piso.
class ExorApagadoLimpio
{
	static bool s_Consultado;
	static bool s_FueLimpio;

	// Marcador INVERTIDO: dice "hay una sesion EN CURSO". Se crea al arrancar y se borra al
	// cerrar bien. Si al arrancar todavia esta, la sesion anterior no llego a borrarlo -> se
	// murio mal.
	//
	// Por que al reves y no "marcar el apagado limpio": escribir un archivo DURANTE el
	// apagado no es confiable (se probo y no quedaba escrito), mientras que escribirlo en el
	// arranque es un camino que el mod ya usa sin problemas (boot_repair.txt). Y sobre todo,
	// asi el modo de falla es el correcto: si el borrado del cierre falla, el proximo arranque
	// cree que fue sucio y BARRE de mas -unos ms-, en vez de creer que fue limpio y dejar
	// items duplicados en el piso.
	static string PathFor()
	{
		return ExorStorageConstants.CONFIG_DIR + "\\sesion_en_curso.txt";
	}

	// Lo llama OnInit: resuelve como fue el apagado anterior y deja la marca de ESTA sesion.
	static void Init()
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		s_Consultado = true;
		s_FueLimpio = !FileExist(PathFor());
		if (s_FueLimpio)
			Print(string.Format("%1 el apagado anterior fue LIMPIO -> los contenedores no barren el piso en su 1ra apertura", ExorStorageConstants.LOG));
		else
			Print(string.Format("%1 el apagado anterior NO fue limpio (crash o kill del host) -> los contenedores barren el piso en su 1ra apertura", ExorStorageConstants.LOG));

		if (!FileExist(ExorStorageConstants.CONFIG_DIR))
			MakeDirectory(ExorStorageConstants.CONFIG_DIR);
		FileHandle fh = OpenFile(PathFor(), FileMode.WRITE);
		if (fh != 0)
		{
			FPrintln(fh, "1");
			CloseFile(fh);
		}
	}

	// Lo llama OnMissionFinish DESPUES de virtualizar todo: el cierre llego hasta el final.
	static void Cerrar()
	{
		if (FileExist(PathFor()))
			DeleteFile(PathFor());
	}

	// true = el apagado anterior fue limpio (no hace falta barrer el piso). Si por lo que sea
	// no se llego a llamar a Init(), devuelve false = barrer (el lado seguro).
	static bool FueLimpio()
	{
		if (!s_Consultado)
			return false;
		return s_FueLimpio;
	}
}

// ----------------------------------------------------------------------------
//  LIMITES DEL TERRENO (compatibilidad con CUALQUIER mapa)
// ----------------------------------------------------------------------------
// El codigo del mod no tiene nada atado a un mapa concreto: el visor usa el MapWidget
// vanilla con coordenadas de mundo y todo lo demas trabaja en metros. Lo que SI es
// especifico del mapa son las COORDENADAS que el admin pone en los JSON: puntos de
// spawn, zonas de KOTH, lugares de cofre y zonas de no-construccion.
//
// Al cambiar de mapa esas coordenadas quedan viejas, y varias caen FUERA del terreno
// (Livonia llega a 12800 m; un mapa de 8x8 km termina en 8192). Una zona de cofre o de
// no-construccion fuera del mapa es inofensiva, pero un PUNTO DE SPAWN fuera del mapa
// tira al jugador al vacio.
//
// Esto lee el tamaño real del terreno cargado (CfgWorlds <mundo> mapSize) y da un
// chequeo barato para descartar, con un log claro, cualquier coordenada imposible.
// Falla del lado seguro: si no se puede leer el tamaño, se asume el mapa mas grande
// conocido y no se descarta nada (mejor una zona rara que borrarle la config al admin).
class ExorMapBounds
{
	static bool  s_Listo;
	static float s_Size;
	static string s_Mundo;

	static float Size()
	{
		if (s_Listo)
			return s_Size;
		s_Listo = true;
		GetGame().GetWorldName(s_Mundo);
		string w = s_Mundo;
		w.ToLower();

		// 1) valor puesto a mano en mapa.json: manda sobre todo lo demas. Es la salida
		// deterministica para un terreno cuyo config no declare el tamaño de forma estandar.
		ExorCfgMapa cm = GetExorConfig().mapa;
		if (cm && cm.tamano_mapa_metros > 0)
		{
			s_Size = cm.tamano_mapa_metros;
			Print(string.Format("%1 MAPA: '%2', %3 x %3 m (tamano_mapa_metros de mapa.json)", ExorStorageConstants.LOG, s_Mundo, s_Size));
			return s_Size;
		}

		// 2) deteccion automatica: los terrenos no declaran el tamaño todos igual, asi que se
		// prueban las claves conocidas en orden.
		s_Size = GetGame().ConfigGetFloat("CfgWorlds " + w + " mapSize");
		if (s_Size <= 0)
			s_Size = GetGame().ConfigGetFloat("CfgWorlds " + w + " worldSize");
		if (s_Size <= 0)
			s_Size = GetGame().ConfigGetFloat("CfgWorlds DayZ_" + w + " mapSize");
		if (s_Size <= 0)
			s_Size = GetGame().ConfigGetFloat("CfgWorlds " + s_Mundo + " mapSize");

		if (s_Size <= 0)
		{
			// 3) no se pudo: se asume el mapa mas grande conocido, o sea NO se descarta
			// ninguna coordenada. Perder el chequeo es inofensivo; descartar de mas le
			// borraria al admin puntos de spawn validos.
			s_Size = 15360;
			Print(string.Format("%1 MAPA: no se pudo leer el tamaño de '%2' -> se asume %3 m y NO se descarta ninguna coordenada. Si cambiaste de mapa, poné tamano_mapa_metros en mapa.json.", ExorStorageConstants.LOG, s_Mundo, s_Size));
		}
		else
		{
			Print(string.Format("%1 MAPA: '%2', %3 x %3 m (detectado del config del mundo)", ExorStorageConstants.LOG, s_Mundo, s_Size));
		}
		return s_Size;
	}

	static string Mundo()
	{
		Size();
		return s_Mundo;
	}

	// La coordenada cae dentro del terreno? 'margen' = metros de borde a respetar.
	static bool Dentro(float x, float z, float margen = 0)
	{
		float s = Size();
		if (x < margen || z < margen)
			return false;
		if (x > (s - margen) || z > (s - margen))
			return false;
		return true;
	}

	// Igual pero avisando en el log de que config viene la coordenada mala. Devuelve true
	// si la coordenada SIRVE. Se llama al cargar cada config, no en caliente.
	static bool Valida(string origen, string etiqueta, float x, float z, float margen = 0)
	{
		if (Dentro(x, z, margen))
			return true;
		Print(string.Format("%1 MAPA: %2 '%3' en <%4, %5> queda FUERA del terreno '%6' (%7 m) -> se ignora. Actualiza esa coordenada para este mapa.",
			ExorStorageConstants.LOG, origen, etiqueta, x, z, Mundo(), Size()));
		return false;
	}
}

// ----------------------------------------------------------------------------
//  MANTENIMIENTO: purga de estado por jugador
// ----------------------------------------------------------------------------
// Varios subsistemas guardan estado en mapas indexados por steamid (cooldown de chat,
// cooldown de spawn, ledger anti-farmeo) o por id de grupo (ultimo payload del party).
// Ninguno se limpiaba: crecian con cada jugador o clan distinto que hubiera pasado por
// el server y solo se vaciaban al reiniciar. No es una fuga dramatica -decenas de MB
// entre reinicios de 4 h- pero es crecimiento SIN TECHO, y la limitante conocida de
// este server es justamente la memoria.
//
// Todas esas entradas guardan un timestamp, asi que se pueden caducar por tiempo sin
// romper ningun cooldown: lo que quedo fuera de cualquier ventana util ya no lo va a
// consultar nadie. Una pasada cada EXOR_PURGA_MS alcanza de sobra.
class ExorHousekeeping
{
	static const int EXOR_PURGA_MS = 600000;	// cada 10 min
	static const int EXOR_TTL_MS   = 3600000;	// se tira lo que no se toca hace 1 h
	static int s_UltimaMs;

	// La llama el tick lento del manager. Sale enseguida si no toca todavia.
	static void Tick(int now)
	{
		if (!GetGame().IsServer())
			return;
		if (s_UltimaMs != 0 && now - s_UltimaMs < EXOR_PURGA_MS)
			return;
		s_UltimaMs = now;

		ExorChatServer.PurgarViejos(now, EXOR_TTL_MS);
		ExorSpawn.PurgarViejos(now, EXOR_TTL_MS);
		ExorKillFarm.PurgarViejos();
		ExorPartyLive.Get().PurgarViejos();
		// Renovar el reloj del CE de los mastiles: sin esto el motor los borra solo cuando
		// se les agota el lifetime, que es lo que hacia "desaparecer" las bases a los dias.
		ExorTerritoryManager.Get().RefrescarVidaMastiles();
	}
}
