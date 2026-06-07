// ============================================================================
// 3xorStorage - Constantes del mod
// ============================================================================
class ExorStorageConstants
{
	static const string MOD_NAME = "3xorStorage";
	static const string MOD_VERSION = "0.1.0";

	// Config en el profile del server (se crea sola con defaults al primer arranque)
	static const string CONFIG_DIR = "$profile:3xorStorage";
	static const string CONFIG_PATH = "$profile:3xorStorage\\settings.json";

	// Carpeta de datos de virtualizacion (Fase 2)
	static const string STORAGE_DIR = "$profile:3xorStorage\\storage";

	// Duracion de las acciones (segundos)
	static const float PACK_SECONDS = 3;
	static const float DEPLOY_SECONDS = 3;
}
