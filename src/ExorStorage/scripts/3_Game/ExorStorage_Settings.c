// ============================================================================
// 3xorStorage - Settings configurables por JSON ($profile:3xorStorage\settings.json)
// Se crea solo con defaults al primer arranque del server.
// IMPORTANTE: los campos marcados (Fase 2/3) ya existen en el JSON pero se
// activan cuando se implemente esa fase. Ver README.md para la doc completa.
// ============================================================================
class ExorStorageSettings
{
	int version = 1;

	// --- Fase 2: virtualizacion ---
	// Minutos sin interaccion (cerrado) para virtualizar el contenido (0 = desactivado)
	int virtualizar_minutos = 30;
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
	int cooldown_abrir_segundos = 5;

	// --- Fase 3: municion (SOLO balas sueltas/bolts/flechas; ver municion_excluida) ---
	// Stack maximo por defecto (0 = vanilla, sin cambio)
	int stack_municion_default = 100;
	// Override de stack maximo por tipo: classname -> stack
	ref map<string, int> stack_municion;
	// Fusionar automaticamente las pilas al recogerlas
	bool auto_stack = true;
	// Cantidad al spawnear como loot (0 = no tocar, usa el % vanilla de types.xml)
	int spawn_municion_default = 0;
	// Override de cantidad de spawn por tipo: classname -> cantidad
	ref map<string, int> spawn_municion;
	// Municion que NUNCA se toca (granadas 40mm, bengalas, etc.)
	ref TStringArray municion_excluida;

	void ExorStorageSettings()
	{
		blacklist = new TStringArray;
		stack_municion = new map<string, int>;
		spawn_municion = new map<string, int>;
		municion_excluida = new TStringArray;
	}

	void SetDefaults()
	{
		version = 1;
		virtualizar_minutos = 30;
		auto_cerrar_minutos = 5;
		multiplicador_comida = 2.0;
		permitir_ropa_con_items = true;
		cooldown_abrir_segundos = 5;
		stack_municion_default = 100;
		auto_stack = true;
		spawn_municion_default = 0;

		blacklist.Clear();

		stack_municion.Clear();
		// Ejemplo de override por tipo:
		// stack_municion.Set("Ammo_762x39", 60);

		spawn_municion.Clear();
		// Ejemplo: que la 7.62x39 spawnee siempre con 75 balas:
		// spawn_municion.Set("Ammo_762x39", 75);

		municion_excluida.Clear();
		municion_excluida.Insert("Ammo_40mm_Explosive");
		municion_excluida.Insert("Ammo_40mm_Smoke_White");
		municion_excluida.Insert("Ammo_40mm_Smoke_Red");
		municion_excluida.Insert("Ammo_40mm_Smoke_Green");
		municion_excluida.Insert("Ammo_Flare");
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
