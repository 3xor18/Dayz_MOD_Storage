// ============================================================================
// 3xor_Vanilla_Optimization - Constantes del mod
// ============================================================================
class ExorStorageConstants
{
	static const string MOD_NAME = "3xor_Vanilla_Optimization";
	static const string MOD_VERSION = "0.2.0";
	static const string LOG = "[3xorVO]";

	// Config en el profile del server (se crea sola con defaults al primer arranque)
	static const string CONFIG_DIR = "$profile:3xorVanillaOptimization";
	static const string CONFIG_PATH = "$profile:3xorVanillaOptimization\\settings.json";

	// Datos de virtualizacion (contenido de barriles y vehiculos cubiertos)
	static const string STORAGE_DIR = "$profile:3xorVanillaOptimization\\storage";
	static const string VEHICLES_DIR = "$profile:3xorVanillaOptimization\\vehicles";

	// Duracion de las acciones (segundos)
	static const float PACK_SECONDS = 3;
	static const float DEPLOY_SECONDS = 3;
	static const float UNCOVER_SECONDS = 4;

	// Cada cuanto corre el chequeo de virtualizacion/auto-cierre/vehiculos (ms)
	static const int TICK_MS = 30000;
}
