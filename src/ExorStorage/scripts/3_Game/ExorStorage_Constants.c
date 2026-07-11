// ============================================================================
// 3xor_Vanilla_Optimization - Constantes del mod
// ============================================================================
class ExorStorageConstants
{
	static const string MOD_NAME = "3xor_Vanilla_Optimization";
	static const string MOD_VERSION = "2.7.4";
	static const string LOG = "[3xorVO]";
	// DEBUG temporal del ciclo de vida del barril (setear/levantar/abrir/cerrar/item
	// in-out/virtualizar/restaurar/load/save/shutdown). Poner en false (o borrar las
	// llamadas ExorDbg) cuando se termine de diagnosticar el dupe del piso.
	static const bool DEBUG_BARRELS = false;

	// Config en el profile del server (se crea sola con defaults al primer arranque)
	static const string CONFIG_DIR = "$profile:3xorVanillaOptimization";
	// settings.json monolitico viejo: solo se usa como ORIGEN de migracion
	static const string CONFIG_PATH = "$profile:3xorVanillaOptimization\\settings.json";
	static const string CONFIG_MIGRATED = "$profile:3xorVanillaOptimization\\settings.json.migrated";

	// Config modular: 1 JSON por modulo (Fase A)
	static const string CFG_STORAGE   = "$profile:3xorVanillaOptimization\\storage.json";
	static const string CFG_VEHICULOS = "$profile:3xorVanillaOptimization\\vehiculos.json";
	static const string CFG_MUNICION  = "$profile:3xorVanillaOptimization\\municion.json";
	static const string CFG_PARTY     = "$profile:3xorVanillaOptimization\\party.json";
	static const string CFG_SPAWNS    = "$profile:3xorVanillaOptimization\\spawns.json";
	static const string CFG_MAPA      = "$profile:3xorVanillaOptimization\\mapa.json";
	static const string CFG_ITEMS     = "$profile:3xorVanillaOptimization\\items.json";
	static const string CFG_VIP       = "$profile:3xorVanillaOptimization\\vip.json";
	static const string CFG_KILLFEED  = "$profile:3xorVanillaOptimization\\killfeed.json";
	static const string CFG_SERVERINFO = "$profile:3xorVanillaOptimization\\serverinfo.json";
	static const string CFG_CHAT       = "$profile:3xorVanillaOptimization\\chat.json";
	static const string CFG_REPARACION = "$profile:3xorVanillaOptimization\\reparacion.json";
	static const string CFG_BODYCADAVER = "$profile:3xorVanillaOptimization\\bodycadaver.json";
	static const string CFG_AUTORUN   = "$profile:3xorVanillaOptimization\\autorun.json";
	static const string CFG_ANTICHEAT = "$profile:3xorVanillaOptimization\\anticheat.json";
	static const string CFG_KOTH      = "$profile:3xorVanillaOptimization\\koth.json";
	static const string CFG_NOBUILD   = "$profile:3xorVanillaOptimization\\nobuild.json";
	static const string CFG_COFRE     = "$profile:3xorVanillaOptimization\\cofre.json";

	// Datos del sistema party (Fase B+): grupos persistidos
	static const string GROUPS_DIR = "$profile:3xorVanillaOptimization\\groups";

	// Stats persistentes para el Score del panel de server info
	static const string STATS_FILE = "$profile:3xorVanillaOptimization\\stats.json";

	// Marcas personales del mapa (PIN) — guardadas en el CLIENTE ($profile del cliente),
	// privadas de cada jugador, no van al server. Las maneja ExorMapPins (5_Mission).
	static const string MAP_PINS_FILE = "$profile:3xorVanillaOptimization\\my_map_pins.json";

	// Estado persistente de VIP (fecha de ingreso + usos de equipamiento por ciclo)
	static const string VIP_STATE_FILE = "$profile:3xorVanillaOptimization\\vip_state.json";

	// Datos de virtualizacion (contenido de barriles)
	static const string STORAGE_DIR = "$profile:3xorVanillaOptimization\\storage";

	// Datos de virtualizacion del contenido de bolsas de cadaver
	static const string BODYBAG_DIR = "$profile:3xorVanillaOptimization\\bodybags";

	// Log de auditoria del server (1 archivo por dia: audit_YYYY-MM-DD.txt, auto-purga)
	static const string AUDITLOG_DIR = "$profile:3xorVanillaOptimization\\ServerAuditLog";

	// Ledger anti-farmeo de kills: por par killer->victima (steamid), timestamps de los
	// kills recientes. Persiste para sobrevivir los reinicios cada 4h (si no, la ventana
	// se resetearia en cada arranque). Lo maneja ExorKillFarm (4_World).
	static const string KILLFARM_FILE = "$profile:3xorVanillaOptimization\\killfarm.json";

	// Duracion de las acciones (segundos)
	static const float PACK_SECONDS = 3;
	static const float DEPLOY_SECONDS = 3;

	// Cada cuanto corre el chequeo de DORMIR vehiculos (ms). Lento porque hace
	// chequeos de distancia a jugadores (mas caro).
	static const int TICK_MS = 30000;
	// Cada cuanto se chequea si hay que despertar vehiculos (ms)
	static const int WAKE_TICK_MS = 5000;
	// Tick RAPIDO solo para barriles/bodybags (auto-cierre 10s, virtualizar 30s). Es
	// barato (solo compara timestamps), por eso puede correr seguido.
	static const int BARREL_TICK_MS = 5000;
	// Maximo de barriles que se virtualizan por tick (anti-pico: si muchos quedan
	// idle a la vez, ej. tras un raid, se reparten en varios ticks).
	static const int MAX_VIRT_PER_TICK = 15;
	// Maximo de snapshots EN VIVO (escritura del JSON a disco) por tick. El guardado
	// es I/O sincrona en el hilo del juego; si muchos barriles activos cambian a la vez
	// (raid), encadenar las escrituras genera hitch -> se reparten en varios ticks. El
	// que no entra escribe el proximo tick (el flag dirty persiste, no se pierde nada).
	static const int MAX_SNAPSHOT_PER_TICK = 12;
	// Maximo de barriles que RECONCILIAN (limpian stale + scan del piso) por tick. El
	// reconcile es lo CARO del arranque tras un crash (GetObjectsAtPosition 12m por barril
	// que quedo sin virtualizar). Repartirlo evita un hitch al cargar bases con muchos
	// barriles. Los untouched reconcilian de a poco; el que un player abra reconcilia ya.
	static const int MAX_RECONCILE_PER_TICK = 5;
}
