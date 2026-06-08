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
	static const string CONFIG_PATH = "$profile:3xorVanillaOptimization\\settings.json";

	// Datos de virtualizacion (contenido de barriles)
	static const string STORAGE_DIR = "$profile:3xorVanillaOptimization\\storage";

	// Duracion de las acciones (segundos)
	static const float PACK_SECONDS = 3;
	static const float DEPLOY_SECONDS = 3;

	// Cada cuanto corre el chequeo de virtualizacion/auto-cierre/dormir (ms)
	static const int TICK_MS = 30000;
	// Cada cuanto se chequea si hay que despertar vehiculos (ms)
	static const int WAKE_TICK_MS = 5000;
}
