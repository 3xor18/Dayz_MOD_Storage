// ============================================================================
// 3xor_Vanilla_Optimization - Marcas personales del mapa (PIN)
// El jugador abre el mapa, hace clic donde quiere y guarda una marca con nombre
// y categoria (base enemiga / loot / otro). Son PRIVADAS: se guardan en el
// $profile del CLIENTE (my_map_pins.json), no van al server (cero carga con 50
// players) y solo las ve quien las puso. Persisten entre relogueos/reinicios.
// ============================================================================

class ExorMapPin
{
	float x;
	float y;
	float z;
	string label;
	int cat;	// 0 = base enemiga (rojo), 1 = loot (verde), 2 = otro (blanco)
}

class ExorMapPinsFile
{
	ref array<ref ExorMapPin> pins;
	void ExorMapPinsFile() { pins = new array<ref ExorMapPin>; }
}

// Manager estatico (cliente). Carga perezosa + autoguardado en cada cambio.
class ExorMapPins
{
	static ref ExorMapPinsFile s_File;
	static const int MAX_PINS = 80;	// tope de seguridad por jugador

	static void EnsureLoaded()
	{
		if (s_File)
			return;
		s_File = new ExorMapPinsFile();
		if (FileExist(ExorStorageConstants.MAP_PINS_FILE))
			JsonFileLoader<ExorMapPinsFile>.JsonLoadFile(ExorStorageConstants.MAP_PINS_FILE, s_File);
		if (!s_File.pins)
			s_File.pins = new array<ref ExorMapPin>;
	}

	static array<ref ExorMapPin> Get()
	{
		EnsureLoaded();
		return s_File.pins;
	}

	static void Save()
	{
		if (!s_File)
			return;
		// el dir puede no existir en el cliente (solo el server lo crea); asegurarlo
		if (!FileExist(ExorStorageConstants.CONFIG_DIR))
			MakeDirectory(ExorStorageConstants.CONFIG_DIR);
		JsonFileLoader<ExorMapPinsFile>.JsonSaveFile(ExorStorageConstants.MAP_PINS_FILE, s_File);
	}

	static void Add(vector pos, string label, int cat)
	{
		EnsureLoaded();
		if (s_File.pins.Count() >= MAX_PINS)
			return;
		if (label == "")
			label = "Marca";
		ExorMapPin p = new ExorMapPin();
		p.x = pos[0];
		p.y = pos[1];
		p.z = pos[2];
		p.label = label;
		p.cat = cat;
		s_File.pins.Insert(p);
		Save();
	}

	static void RemoveAt(int idx)
	{
		EnsureLoaded();
		if (idx >= 0 && idx < s_File.pins.Count())
		{
			s_File.pins.Remove(idx);
			Save();
		}
	}

	// color ARGB por categoria (para el AddUserMark del mapa)
	static int CatColor(int cat)
	{
		if (cat == 0)
			return ARGB(255, 235, 45, 45);	// base enemiga = rojo
		if (cat == 1)
			return ARGB(255, 60, 220, 60);	// loot = verde
		return ARGB(255, 240, 240, 240);	// otro = blanco
	}
}

// Handler de clics del MapWidget. Se engancha con SetHandler (igual que el
// MapHandler vanilla) — NO rompe el paneo/zoom (eso es nativo del engine), solo
// agrega el callback de clic. Reenvia el clic (en coords de pantalla) al menu.
class ExorMapClickHandler : ScriptedWidgetEventHandler
{
	protected ExorMapMenu m_Menu;

	void ExorMapClickHandler(ExorMapMenu menu)
	{
		m_Menu = menu;
	}

	// OnMouseButtonDown (no OnClick): el clic simple sobre el MapWidget lo consume el
	// paneo nativo y OnClick no siempre llega; el button-down si. Devolvemos true solo
	// si lo usamos (colocando/borrando) para que NO panee en ese caso.
	override bool OnMouseButtonDown(Widget w, int x, int y, int button)
	{
		if (m_Menu && m_Menu.OnMapClick(x, y, button))
			return true;
		return false;
	}
}
