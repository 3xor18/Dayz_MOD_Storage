// ============================================================================
// 3xorStorage - Settings configurables por JSON ($profile:3xorStorage\settings.json)
// Se crea solo con defaults al primer arranque del server.
// IMPORTANTE: los campos marcados (Fase 2/3) ya existen en el JSON pero se
// activan cuando se implemente esa fase. Ver README.md para la doc completa.
// ============================================================================
// Rango aleatorio [min, max] de cantidad al spawnear una pila de municion
class ExorMunicionSpawnRango
{
	int min = 15;
	int max = 65;
}

class ExorStorageSettings
{
	int version = 1;

	// --- Fase 2: virtualizacion ---
	// Minutos sin interaccion (cerrado) para virtualizar el contenido (0 = desactivado)
	int virtualizar_minutos = 10;
	// Minutos para que un barril dejado abierto se cierre solo (0 = desactivado)
	int auto_cerrar_minutos = 5;
	// Multiplicador de duracion de la comida dentro del barril (2.0 = dura el doble)
	float multiplicador_comida = 2.0;

	// --- Fase 3: reglas de guardado ---
	// Permitir guardar mochilas/ropa CON items adentro (solo en barriles 3xor)
	bool permitir_ropa_con_items = true;
	// Items que NO se pueden guardar en los barriles 3xor (classnames)
	ref TStringArray blacklist;

	// --- Fase 3: anti-dupe ---
	// Segundos de espera entre abrir/cerrar el mismo barril (0 = sin cooldown)
	int cooldown_abrir_segundos = 3;

	// --- Fase 3: municion (SOLO balas sueltas/bolts/flechas; ver municion_excluida) ---
	// stack_municion: UNA entrada por cada bala del juego -> stack maximo.
	// En la Fase 3 la lista se AUTO-COMPLETA al arrancar con todas las municiones
	// detectadas (vanilla + mods) usando stack_municion_default como relleno.
	// Despues editas el numero de la que quieras.
	int stack_municion_default = 100;
	ref map<string, int> stack_municion;
	// Fusionar automaticamente las pilas al recogerlas
	bool auto_stack = true;
	// spawn_municion: UNA entrada por bala -> rango {min, max}; la cantidad al
	// spawnear como loot sale aleatoria entre min y max. Tambien se auto-completa
	// (relleno: spawn_municion_min/max_default).
	int spawn_municion_min_default = 15;
	int spawn_municion_max_default = 65;
	ref map<string, ref ExorMunicionSpawnRango> spawn_municion;
	// Municion que NUNCA se toca (granadas 40mm, bengalas, etc.)
	ref TStringArray municion_excluida;

	// --- Fase 2.5: sueno automatico de vehiculos ---
	// Dormir la fisica de vehiculos inactivos (mismo ahorro, invisible al jugador)
	bool vehiculos_dormir = true;
	// Minutos de inactividad (motor apagado, sin ocupantes) para dormirse
	int vehiculos_dormir_minutos = 5;
	// Radio en metros: el vehiculo solo duerme si no hay jugadores mas cerca
	// que esto, y DESPIERTA solo cuando alguien entra a este radio
	int vehiculos_despertar_metros = 150;
	// Vehiculos que nunca se duermen (classnames)
	ref TStringArray vehiculos_excluidos;

	// --- Voltear vehiculos ---
	// Permitir voltear vehiculos volcados/de costado con una accion
	bool voltear_vehiculos = true;
	// Segundos que tarda la accion de voltear (anti-abuso en combate)
	int voltear_segundos = 40;

	void ExorStorageSettings()
	{
		blacklist = new TStringArray;
		stack_municion = new map<string, int>;
		spawn_municion = new map<string, ref ExorMunicionSpawnRango>;
		municion_excluida = new TStringArray;
		vehiculos_excluidos = new TStringArray;
	}

	void SetDefaults()
	{
		version = 1;
		virtualizar_minutos = 10;
		auto_cerrar_minutos = 5;
		multiplicador_comida = 2.0;
		permitir_ropa_con_items = true;
		cooldown_abrir_segundos = 3;
		stack_municion_default = 100;
		auto_stack = true;
		spawn_municion_min_default = 15;
		spawn_municion_max_default = 65;

		blacklist.Clear();

		// Las listas por bala se auto-completan en la Fase 3 al arrancar el server
		// con todas las municiones detectadas (vanilla + mods).
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

		vehiculos_dormir = true;
		vehiculos_dormir_minutos = 5;
		vehiculos_despertar_metros = 150;
		vehiculos_excluidos.Clear();

		voltear_vehiculos = true;
		voltear_segundos = 40;
	}

	static ref ExorStorageSettings Load()
	{
		ExorStorageSettings settings = new ExorStorageSettings();

		if (!FileExist(ExorStorageConstants.CONFIG_DIR))
		{
			MakeDirectory(ExorStorageConstants.CONFIG_DIR);
		}

		if (FileExist(ExorStorageConstants.CONFIG_PATH))
		{
			JsonFileLoader<ExorStorageSettings>.JsonLoadFile(ExorStorageConstants.CONFIG_PATH, settings);
			// Re-guardar para completar campos nuevos si el JSON es de una version vieja
			JsonFileLoader<ExorStorageSettings>.JsonSaveFile(ExorStorageConstants.CONFIG_PATH, settings);
			Print("[3xorStorage] settings.json cargado desde el profile");
		}
		else
		{
			settings.SetDefaults();
			JsonFileLoader<ExorStorageSettings>.JsonSaveFile(ExorStorageConstants.CONFIG_PATH, settings);
			Print("[3xorStorage] settings.json no existia: creado con defaults");
		}

		return settings;
	}
}

// Singleton global (solo lo usa el server)
static ref ExorStorageSettings g_ExorStorageSettings;

static ExorStorageSettings GetExorStorageSettings()
{
	if (!g_ExorStorageSettings)
	{
		g_ExorStorageSettings = ExorStorageSettings.Load();
	}
	return g_ExorStorageSettings;
}
