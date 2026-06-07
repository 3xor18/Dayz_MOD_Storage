// ============================================================================
// 3xor_Vanilla_Optimization - Manager central (SOLO server)
// Tick cada 30s: auto-cierre de barriles, virtualizacion de contenido,
// cobertura de vehiculos inactivos y self-heal de coberturas perdidas.
// ============================================================================
class ExorVO_Manager
{
	static ref ExorVO_Manager s_Instance;

	ref array<Exor_Barrel_Base> m_Barrels;
	ref array<CarScript> m_Vehicles;
	// Mapea el objeto estatico visual (auto sin fisica) -> su cobertura
	ref map<Object, Exor_VehicleCover> m_CoverByStatic;
	// Coberturas vivas por id
	ref map<string, Exor_VehicleCover> m_CoverById;
	// Vehiculos cubiertos (dormidos bajo tierra) por id
	ref map<string, CarScript> m_CoveredVehicleById;

	void ExorVO_Manager()
	{
		m_Barrels = new array<Exor_Barrel_Base>;
		m_Vehicles = new array<CarScript>;
		m_CoverByStatic = new map<Object, Exor_VehicleCover>;
		m_CoverById = new map<string, Exor_VehicleCover>;
		m_CoveredVehicleById = new map<string, CarScript>;
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
		Print(string.Format("%1 Manager iniciado (tick cada %2 ms)", ExorStorageConstants.LOG, ExorStorageConstants.TICK_MS));
	}

	// ------------------------- registro: barriles -------------------------
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

	// ------------------------- registro: vehiculos -------------------------
	static void RegisterVehicle(CarScript car)
	{
		if (Get().m_Vehicles.Find(car) == -1)
		{
			Get().m_Vehicles.Insert(car);
		}
	}

	static void RegisterCoveredVehicle(string id, CarScript car)
	{
		if (id != "")
		{
			Get().m_CoveredVehicleById.Set(id, car);
		}
	}

	static void UnregisterCoveredVehicle(string id)
	{
		if (id != "")
		{
			Get().m_CoveredVehicleById.Remove(id);
		}
	}

	static CarScript GetCoveredVehicle(string id)
	{
		CarScript car;
		if (Get().m_CoveredVehicleById.Find(id, car))
			return car;
		return null;
	}

	// ------------------------- registro: coberturas -------------------------
	static void RegisterStaticCover(Object staticObj, Exor_VehicleCover cover)
	{
		Get().m_CoverByStatic.Set(staticObj, cover);
	}

	static void UnregisterStaticCover(Object staticObj)
	{
		if (staticObj)
		{
			Get().m_CoverByStatic.Remove(staticObj);
		}
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

		// --- Self-heal: vehiculo cubierto cuya cobertura se perdio ---
		HealLostCovers();

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
			if (car.ExorIsCovered())
				continue;
			if (car.IsRuined())
				continue;	// chatarra no se cubre
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
		}
	}

	// Si una cobertura desaparecio (admin, bug, limpieza) pero el vehiculo
	// dormido sigue bajo tierra, recrear la cobertura en su lugar original
	void HealLostCovers()
	{
		array<string> ids = new array<string>;
		int i;
		for (i = 0; i < m_CoveredVehicleById.Count(); i++)
		{
			ids.Insert(m_CoveredVehicleById.GetKey(i));
		}

		for (i = 0; i < ids.Count(); i++)
		{
			string id = ids.Get(i);
			if (m_CoverById.Contains(id))
				continue;

			CarScript car = GetCoveredVehicle(id);
			if (!car)
			{
				m_CoveredVehicleById.Remove(id);
				continue;
			}

			vector pos = car.ExorGetOrigPos();
			Exor_VehicleCover cover = Exor_VehicleCover.Cast(GetGame().CreateObjectEx("Exor_VehicleCover", pos, ECE_KEEPHEIGHT));
			if (!cover)
				continue;
			cover.SetPosition(pos);
			cover.SetOrientation(car.GetOrientation());
			cover.ExorSetup(id, car.GetType());
			Print(string.Format("%1 Self-heal: cobertura %2 recreada para %3", ExorStorageConstants.LOG, id, car.GetType()));
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
