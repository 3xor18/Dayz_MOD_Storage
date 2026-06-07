// ============================================================================
// 3xorStorage - Settings configurables por JSON ($profile:3xorStorage\settings.json)
// Se crea solo con defaults al primer arranque del server.
// Los campos marcados (Fase 2/3) ya existen en el JSON pero se activan en esas fases.
// ============================================================================
class ExorStorageSettings
{
	int version = 1;

	// --- Fase 2: virtualizacion ---
	// Minutos sin interaccion para que el contenido se virtualice (0 = desactivado)
	int virtualizar_minutos = 30;

	// --- Fase 2: comida ---
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

	// --- Fase 3: stack de municion ---
	// Stack por defecto para balas sueltas / bolts / flechas (0 = vanilla, sin cambio)
	int stack_municion_default = 100;
	// Override por tipo de municion: classname -> stack maximo
	ref map<string, int> stack_municion;

	void ExorStorageSettings()
	{
		blacklist = new TStringArray;
		stack_municion = new map<string, int>;
	}

	void SetDefaults()
	{
		version = 1;
		virtualizar_minutos = 30;
		multiplicador_comida = 2.0;
		permitir_ropa_con_items = true;
		cooldown_abrir_segundos = 5;
		stack_municion_default = 100;

		blacklist.Clear();
		// Ejemplos tipicos (items que pierden datos al serializar):
		// blacklist.Insert("Paper");

		stack_municion.Clear();
		// Ejemplo de override por tipo:
		// stack_municion.Set("Ammo_762x39", 60);
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
