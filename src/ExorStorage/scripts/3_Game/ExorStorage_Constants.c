// ============================================================================
// 3xor_Vanilla_Optimization - Constantes del mod
// ============================================================================
class ExorStorageConstants
{
	static const string MOD_NAME = "3xor_Vanilla_Optimization";
	static const string MOD_VERSION = "1.0.0";
	static const string LOG = "[3xorVO]";

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

	// Datos del sistema party (Fase B+): grupos persistidos
	static const string GROUPS_DIR = "$profile:3xorVanillaOptimization\\groups";

	// Stats persistentes para el Score del panel de server info
	static const string STATS_FILE = "$profile:3xorVanillaOptimization\\stats.json";

	// Estado persistente de VIP (fecha de ingreso + usos de equipamiento por ciclo)
	static const string VIP_STATE_FILE = "$profile:3xorVanillaOptimization\\vip_state.json";

	// Datos de virtualizacion (contenido de barriles)
	static const string STORAGE_DIR = "$profile:3xorVanillaOptimization\\storage";

	// Log forense anti-raid (1 archivo por dia: raid_YYYY-MM-DD.txt, auto-purga)
	static const string RAIDLOG_DIR = "$profile:3xorVanillaOptimization\\raidlog";

	// Duracion de las acciones (segundos)
	static const float PACK_SECONDS = 3;
	static const float DEPLOY_SECONDS = 3;

	// Cada cuanto corre el chequeo de virtualizacion/auto-cierre/dormir (ms)
	static const int TICK_MS = 30000;
	// Cada cuanto se chequea si hay que despertar vehiculos (ms)
	static const int WAKE_TICK_MS = 5000;
}
