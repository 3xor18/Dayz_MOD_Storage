// ============================================================================
// 3xor_Vanilla_Optimization - Manager central (SOLO server)
// Tick cada 30s: auto-cierre de barriles, virtualizacion de contenido y
// cobertura de vehiculos inactivos.
// ============================================================================
class ExorVO_Manager
{
	static ref ExorVO_Manager s_Instance;

	ref array<Exor_Barrel_Base> m_Barrels;
	ref array<CarScript> m_Vehicles;
	// Mapea el objeto estatico visual (auto sin fisica) -> su cobertura
	ref map<Object, Exor_VehicleCover> m_CoverByStatic;
	// Coberturas vivas por id (para detectar JSONs huerfanos al arrancar)
	ref map<string, Exor_VehicleCover> m_CoverById;

	void ExorVO_Manager()
	{
		m_Barrels = new array<Exor_Barrel_Base>;
		m_Vehicles = new array<CarScript>;
		m_CoverByStatic = new map<Object, Exor_VehicleCover>;
		m_CoverById = new map<string, Exor_VehicleCover>;
	}

	static ExorVO_Manager Get()
	{
		if (!s_Instance)
		{
			s_Instance = new ExorVO_Manager();
		}
		return s_Instance;
	}

	static void Start()
	{
		if (!GetGame().IsServer())
			return;
		ExorVO_Serializer.EnsureDirs();
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Get().Tick, ExorStorageConstants.TICK_MS, true);
		// Self-heal: a los 2 min (con la persistencia ya cargada) recrear las
		// coberturas cuyos JSONs quedaron huerfanos (ej. limpiadas por el CE)
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Get().HealOrphanCovers, 120000, false);
		Print(string.Format("%1 Manager iniciado (tick cada %2 ms)", ExorStorageConstants.LOG, ExorStorageConstants.TICK_MS));
	}

	// ------------------------- self-heal de coberturas -------------------------
	void HealOrphanCovers()
	{
		string fileName;
		FileAttr attr;
		array<string> files = new array<string>;
		FindFileHandle handle = FindFile(string.Format("%1\\*.json", ExorStorageConstants.VEHICLES_DIR), fileName, attr, FindFileFlags.ALL);
		if (handle)
		{
			if (fileName != "")
				files.Insert(fileName);
			while (FindNextFile(handle, fileName, attr))
			{
				if (fileName != "")
					files.Insert(fileName);
			}
			CloseFindFile(handle);
		}

		int healed = 0;
		int i;
		for (i = 0; i < files.Count(); i++)
		{
			string id = files.Get(i);
			id.Replace(".json", "");
			if (m_CoverById.Contains(id))
				continue;

			// JSON sin cobertura en el mundo: recrearla donde estaba el vehiculo
			string path = string.Format("%1\\%2.json", ExorStorageConstants.VEHICLES_DIR, id);
			ExorVO_VehicleFile f = new ExorVO_VehicleFile();
			JsonFileLoader<ExorVO_VehicleFile>.JsonLoadFile(path, f);
			if (f.type == "")
				continue;

			vector pos = Vector(f.pos_x, f.pos_y, f.pos_z);
			Exor_VehicleCover cover = Exor_VehicleCover.Cast(GetGame().CreateObjectEx("Exor_VehicleCover", pos, ECE_KEEPHEIGHT));
			if (!cover)
				continue;
			cover.SetPosition(pos);
			cover.SetOrientation(Vector(f.ori_x, f.ori_y, f.ori_z));
			cover.ExorSetup(id, f.type);
			healed++;
		}

		if (healed > 0)
		{
			Print(string.Format("%1 Self-heal: %2 coberturas de vehiculo recreadas desde JSON", ExorStorageConstants.LOG, healed));
		}
	}

	// ------------------------- registro -------------------------
	static void RegisterBarrel(Exor_Barrel_Base barrel)
	{
		if (Get().m_Barrels.Find(barrel) == -1)
		{
			Get().m_Barrels.Insert(barrel);
		}
	}

	static void UnregisterBarrel(Exor_Barrel_Base barrel)
	{
		int idx = Get().m_Barrels.Find(barrel);
		if (idx != -1)
		{
			Get().m_Barrels.Remove(idx);
		}
	}

	static void RegisterVehicle(CarScript car)
	{
		if (Get().m_Vehicles.Find(car) == -1)
		{
			Get().m_Vehicles.Insert(car);
		}
	}

	static void RegisterStaticCover(Object staticObj, Exor_VehicleCover cover)
	{
		Get().m_CoverByStatic.Set(staticObj, cover);
	}

	static void RegisterCoverId(string id, Exor_VehicleCover cover)
	{
		if (id != "")
		{
			Get().m_CoverById.Set(id, cover);
		}
	}

	static void UnregisterCoverId(string id)
	{
		if (id != "")
		{
			Get().m_CoverById.Remove(id);
		}
	}

	static void UnregisterStaticCover(Object staticObj)
	{
		if (staticObj)
		{
			Get().m_CoverByStatic.Remove(staticObj);
		}
	}

	// Para la accion "Quitar la cobertura": el target puede ser la red camo
	// (el item Exor_VehicleCover) o el auto estatico visual
	static Exor_VehicleCover GetCoverForObject(Object obj)
	{
		if (!obj)
			return null;
		Exor_VehicleCover direct = Exor_VehicleCover.Cast(obj);
		if (direct)
			return direct;
		Exor_VehicleCover byStatic;
		if (Get().m_CoverByStatic.Find(obj, byStatic))
			return byStatic;
		return null;
	}

	// ------------------------- tick -------------------------
	void Tick()
	{
		ExorStorageSettings settings = GetExorStorageSettings();
		int now = GetGame().GetTime();
		int i;

		// --- Barriles: auto-cierre y virtualizacion ---
		for (i = m_Barrels.Count() - 1; i >= 0; i--)
		{
			Exor_Barrel_Base barrel = m_Barrels.Get(i);
			if (!barrel)
			{
				m_Barrels.Remove(i);
				continue;
			}
			barrel.ExorTick(now, settings);
		}

		// --- Vehiculos: cobertura por inactividad ---
		if (!settings.vehiculos_cubrir)
			return;
		if (settings.vehiculos_cubrir_minutos <= 0)
			return;

		int vehMs = settings.vehiculos_cubrir_minutos * 60000;
		for (i = m_Vehicles.Count() - 1; i >= 0; i--)
		{
			CarScript car = m_Vehicles.Get(i);
			if (!car)
			{
				m_Vehicles.Remove(i);
				continue;
			}
			if (car.ExorIsActive())
			{
				car.ExorMarkActive(now);
				continue;
			}
			if (now - car.ExorGetLastActive() < vehMs)
				continue;
			if (settings.vehiculos_excluidos.Find(car.GetType()) != -1)
				continue;
			if (IsPlayerNear(car.GetPosition(), settings.vehiculos_radio_jugador))
				continue;

			ExorVO_VehicleCoverSystem.CoverVehicle(car);
			m_Vehicles.Remove(i);
		}
	}

	static bool IsPlayerNear(vector pos, float radius)
	{
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			Man p = players.Get(i);
			if (p && vector.Distance(p.GetPosition(), pos) < radius)
				return true;
		}
		return false;
	}
}
