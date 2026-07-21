// ============================================================================
// 3xor_Vanilla_Optimization - GARAGE de vehiculos virtualizados (SOLO server)
// ============================================================================
// Capa de persistencia + operaciones sobre ExorVO_Vehicle (Capture/Restore).
// Un JSON por auto en VEHICLES_DIR, con el groupId embebido -> el menu del mastil
// filtra por clan. Cada auto virtualizado guarda su id, tipo, posicion, fluidos,
// daño, piezas y cargo (ver ExorVO_VehicleFile).
//
// SEGURIDAD (perder un auto es mucho peor que perder un item):
//   - Virtualizar: capturar -> guardar JSON -> VERIFICAR que el JSON existe -> recien
//     ahi borrar el auto. Si algo falla antes del delete, el auto REAL sigue intacto.
//   - Restaurar: cargar JSON -> recrear -> VERIFICAR que el auto existe -> recien ahi
//     borrar el JSON. Si el recreate falla, el JSON queda (no se pierde el auto).
//   - Guards de virtualizacion: sin tripulantes, motor apagado, no virtualizado ya.
// ============================================================================
class ExorVehicleGarage
{
	// registro de parkings colocados (para saber cuantos hay / futuros usos)
	static ref array<EntityAI> s_Parkings;

	static void RegisterParking(EntityAI p)
	{
		if (!s_Parkings)
			s_Parkings = new array<EntityAI>;
		if (s_Parkings.Find(p) == -1)
			s_Parkings.Insert(p);
	}
	static void UnregisterParking(EntityAI p)
	{
		if (s_Parkings)
		{
			int i = s_Parkings.Find(p);
			if (i != -1)
				s_Parkings.Remove(i);
		}
	}

	// hay un PARKING colocado a <=radius de 'pos'? Si si, devuelve su posicion (out) y true.
	// Lo usa el auto-virtualizado: solo se auto-virtualizan autos dentro del radio de un parking.
	static bool NearParking(vector pos, float radius, out vector parkingPos)
	{
		if (!s_Parkings)
			return false;
		int i;
		for (i = 0; i < s_Parkings.Count(); i++)
		{
			EntityAI p = s_Parkings.Get(i);
			if (p && vector.Distance(p.GetPosition(), pos) <= radius)
			{
				parkingPos = p.GetPosition();
				return true;
			}
		}
		return false;
	}

	static void EnsureDir()
	{
		if (!FileExist(ExorStorageConstants.VEHICLES_DIR))
			MakeDirectory(ExorStorageConstants.VEHICLES_DIR);
	}

	static string PathFor(string id)
	{
		return string.Format("%1\\%2.json", ExorStorageConstants.VEHICLES_DIR, id);
	}

	// -------------------- VIRTUALIZAR --------------------
	// Saca el auto del mundo y lo guarda a disco. Devuelve "" si OK, o un motivo de error.
	static string Virtualize(CarScript car, string groupId)
	{
		if (!GetGame() || !GetGame().IsServer())
			return "solo server";
		if (!car)
			return "auto invalido";

		// GUARD 1: nadie adentro (conductor NI pasajeros). Virtualizar con alguien dentro
		// lo dejaria flotando/atascado en el cliente.
		int c;
		for (c = 0; c < car.CrewSize(); c++)
		{
			if (car.CrewMember(c))
				return "hay alguien adentro del auto";
		}
		// GUARD 2: motor apagado (si esta prendido, lo esta usando alguien recien)
		if (car.EngineIsOn())
			return "el motor esta encendido";
		// GUARD 3: no virtualizar si hay jugadores AJENOS (no del clan) en el radio del parking
		// (config parking_radio_metros, default 30m) del auto. Evita que se "roben"/escondan
		// autos de otros, o que un miembro esconda el auto con un enemigo cerca (mid-raid, scout).
		float enemyRadius = GetExorConfig().storage.parking_radio_metros;
		if (enemyRadius <= 0)
			enemyRadius = 30.0;
		if (ExorEnemyNear(car.GetPosition(), enemyRadius, groupId))
			return "hay jugadores ajenos cerca del auto";

		// CAPTURA. Devuelve null si NO es seguro (ej: barril 3xor del slot a medias de
		// virtualizar) -> abortar sin tocar el auto, se reintenta cuando el barril este listo.
		ExorVO_VehicleFile f = ExorVO_Vehicle.Capture(car);
		if (!f)
			return "el auto tiene un contenedor 3xor a medio virtualizar, reintenta en unos segundos";

		f.id = ExorVO_Serializer.GenerateId();
		f.group_id = groupId;
		f.display = car.GetDisplayName();

		// GUARDAR PRIMERO, verificar, y RECIEN borrar el auto (anti-perdida).
		EnsureDir();
		string path = PathFor(f.id);
		JsonFileLoader<ExorVO_VehicleFile>.JsonSaveFile(path, f);
		if (!FileExist(path))
			return "no se pudo escribir el archivo del auto (no se borro nada, el auto sigue)";

		// Sacarlo YA de la lista del manager: ObjectDelete puede diferir el borrado real hasta
		// el fin del frame, y el menu re-escanea NearbyReal (que itera m_Vehicles) justo despues
		// -> sin esto el auto recien virtualizado seguiria apareciendo en "Sin Almacenar" (en los
		// DOS paneles) hasta reabrir el menu. (EEDelete tambien lo saca, pero mas tarde.)
		ExorVO_Manager vo = ExorVO_Manager.Get();
		if (vo && vo.m_Vehicles)
		{
			int vidx = vo.m_Vehicles.Find(car);
			if (vidx != -1)
				vo.m_Vehicles.Remove(vidx);
		}

		GetGame().ObjectDelete(car);
		Print(string.Format("%1 GARAGE: auto '%2' virtualizado id=%3 grupo=%4", ExorStorageConstants.LOG, f.display, f.id, groupId));
		return "";
	}

	// -------------------- SPAWNEAR (des-virtualizar) --------------------
	// Recrea el auto desde su JSON y borra el JSON. Devuelve "" si OK, o un motivo de error.
	static string Spawn(string id, string groupId)
	{
		if (!GetGame() || !GetGame().IsServer())
			return "solo server";
		string path = PathFor(id);
		if (!FileExist(path))
			return "ese auto ya no existe";

		ExorVO_VehicleFile f = new ExorVO_VehicleFile();
		JsonFileLoader<ExorVO_VehicleFile>.JsonLoadFile(path, f);

		// dueño: solo el clan que lo guardo puede sacarlo
		if (groupId != "" && f.group_id != "" && f.group_id != groupId)
			return "ese auto es de otro clan";

		CarScript car = ExorVO_Vehicle.Restore(f);
		if (!car)
			return "no se pudo recrear el auto (el archivo NO se borro, reintenta)";

		// recien ahora que el auto EXISTE, borrar el JSON (anti-dupe)
		DeleteFile(path);
		Print(string.Format("%1 GARAGE: auto '%2' recreado id=%3", ExorStorageConstants.LOG, f.display, id));
		return "";
	}

	// -------------------- LISTAR los virtualizados de un clan --------------------
	// Devuelve pares "id|display" para el menu. Recorre los JSON del dir filtrando por grupo.
	static void ListForGroup(string groupId, out array<string> outIds, out array<string> outNames)
	{
		outIds = new array<string>;
		outNames = new array<string>;
		if (!FileExist(ExorStorageConstants.VEHICLES_DIR))
			return;
		string pattern = string.Format("%1\\*.json", ExorStorageConstants.VEHICLES_DIR);
		string fileName;
		FileAttr attr;
		FindFileHandle h = FindFile(pattern, fileName, attr, FindFileFlags.ALL);
		if (!h)
			return;
		bool more = true;
		while (more)
		{
			if (fileName != "")
			{
				string path = string.Format("%1\\%2", ExorStorageConstants.VEHICLES_DIR, fileName);
				ExorVO_VehicleFile f = new ExorVO_VehicleFile();
				JsonFileLoader<ExorVO_VehicleFile>.JsonLoadFile(path, f);
				if (f.id != "" && (groupId == "" || f.group_id == groupId))
				{
					outIds.Insert(f.id);
					outNames.Insert(f.display);
				}
			}
			more = FindNextFile(h, fileName, attr);
		}
		CloseFindFile(h);
	}

	// hay algun jugador AJENO (que NO sea del clan 'groupId') vivo a <=radius de 'pos'?
	// El propio operador y sus compañeros de clan NO cuentan (comparten groupId). Los STAFF
	// del array de bypass (admins) TAMPOCO cuentan como ajenos -> su presencia no bloquea el
	// virtualizado (pueden estar administrando la base sin trabar la accion).
	static bool ExorEnemyNear(vector pos, float radius, string groupId)
	{
		ExorCfgStorage s = GetExorConfig().storage;
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase pb = PlayerBase.Cast(players.Get(i));
			if (!pb || !pb.IsAlive())
				continue;
			if (vector.Distance(pb.GetPosition(), pos) > radius)
				continue;
			// mismo clan (o sin clan definido) -> no es un ajeno
			if (groupId != "" && pb.ExorGetGroupId() == groupId)
				continue;
			// STAFF (bypass) -> no cuenta como ajeno
			if (s && s.bypass_lootear_steamids && s.bypass_lootear_steamids.Find(ExorGroupManager.SteamId(pb)) != -1)
				continue;
			return true;	// jugador ajeno cerca
		}
		return false;
	}

	// -------------------- autos REALES en el radio del mastil (para virtualizar) --------------------
	// Del registro del manager, los que estan a <=radius de 'center', sin tripulantes.
	static void NearbyReal(vector center, float radius, out array<CarScript> outCars)
	{
		outCars = new array<CarScript>;
		ExorVO_Manager vo = ExorVO_Manager.Get();
		if (!vo || !vo.m_Vehicles)
			return;
		int i;
		for (i = 0; i < vo.m_Vehicles.Count(); i++)
		{
			CarScript car = vo.m_Vehicles.Get(i);
			if (!car)
				continue;
			if (vector.Distance(car.GetPosition(), center) > radius)
				continue;
			outCars.Insert(car);
		}
	}
}
