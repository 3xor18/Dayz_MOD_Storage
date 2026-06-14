// ============================================================================
// 3xor_Vanilla_Optimization - settings.json MONOLITICO (LEGACY)
// Esta clase ya NO se usa en runtime: la config se parti en modulos
// (ver ExorConfig.c). Se conserva SOLO como DTO para leer un settings.json
// viejo y migrarlo a los nuevos JSON por modulo (ExorConfig.MigrateLegacyIfNeeded).
// No agregar campos nuevos aca: van en el modulo correspondiente de ExorConfig.c.
// ============================================================================
class ExorStorageSettings
{
	int version = 1;

	// storage
	int virtualizar_minutos = 10;
	int auto_cerrar_minutos = 5;
	float multiplicador_comida = 2.0;
	bool permitir_ropa_con_items = true;
	ref TStringArray blacklist;
	int cooldown_abrir_segundos = 3;

	// municion
	int stack_municion_default = 100;
	ref map<string, int> stack_municion;
	bool auto_stack = true;
	int spawn_municion_min_default = 15;
	int spawn_municion_max_default = 65;
	ref map<string, ref ExorMunicionSpawnRango> spawn_municion;
	ref TStringArray municion_excluida;

	// vehiculos
	bool vehiculos_dormir = true;
	int vehiculos_dormir_minutos = 5;
	int vehiculos_despertar_metros = 30;
	ref TStringArray vehiculos_excluidos;
	bool voltear_vehiculos = true;
	int voltear_segundos = 40;

	void ExorStorageSettings()
	{
		blacklist = new TStringArray;
		stack_municion = new map<string, int>;
		spawn_municion = new map<string, ref ExorMunicionSpawnRango>;
		municion_excluida = new TStringArray;
		vehiculos_excluidos = new TStringArray;
	}
}
