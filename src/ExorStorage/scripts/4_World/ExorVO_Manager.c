// ============================================================================
// 3xor_Vanilla_Optimization - Manager central (SOLO server)
// Tick cada 30s: auto-cierre de barriles, virtualizacion de contenido y
// sueno de vehiculos inactivos.
// WakeTick cada 5s: despierta vehiculos dormidos cuando un jugador se acerca.
// ============================================================================
class ExorVO_Manager
{
	static ref ExorVO_Manager s_Instance;

	ref array<Exor_Barrel_Base> m_Barrels;
	ref array<CarScript> m_Vehicles;

	void ExorVO_Manager()
	{
		m_Barrels = new array<Exor_Barrel_Base>;
		m_Vehicles = new array<CarScript>;
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
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Get().WakeTick, ExorStorageConstants.WAKE_TICK_MS, true);
		Print(string.Format("%1 Manager iniciado (tick %2 ms, wake-tick %3 ms)", ExorStorageConstants.LOG, ExorStorageConstants.TICK_MS, ExorStorageConstants.WAKE_TICK_MS));
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

	// ------------------------- tick lento (30s) -------------------------
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

		// --- Vehiculos: dormir los inactivos ---
		if (!settings.vehiculos_dormir)
			return;
		if (settings.vehiculos_dormir_minutos <= 0)
			return;

		int sleepMs = settings.vehiculos_dormir_minutos * 60000;
		int dormidos = 0;
		int totalDormidos = 0;
		for (i = m_Vehicles.Count() - 1; i >= 0; i--)
		{
			CarScript car = m_Vehicles.Get(i);
			if (!car)
			{
				m_Vehicles.Remove(i);
				continue;
			}
			if (car.ExorIsSleeping())
			{
				totalDormidos++;
				continue;
			}
			if (car.IsRuined())
				continue;
			if (car.ExorIsActive())
			{
				car.ExorMarkActive(now);
				continue;
			}
			if (now - car.ExorGetLastActive() < sleepMs)
				continue;
			if (settings.vehiculos_excluidos.Find(car.GetType()) != -1)
				continue;
			if (IsPlayerNear(car.GetPosition(), settings.vehiculos_despertar_metros))
				continue;

			car.ExorSleep();
			dormidos++;
			totalDormidos++;
		}

		if (dormidos > 0)
		{
			Print(string.Format("%1 Vehiculos dormidos: +%2 (total %3 de %4)", ExorStorageConstants.LOG, dormidos, totalDormidos, m_Vehicles.Count()));
		}
	}

	// ------------------------- tick rapido (5s): despertar -------------------------
	protected int m_WakeRuns = 0;

	void WakeTick()
	{
		ExorStorageSettings settings = GetExorStorageSettings();
		if (!settings.vehiculos_dormir)
			return;

		m_WakeRuns++;
		int dormidos = 0;
		int i;
		for (i = m_Vehicles.Count() - 1; i >= 0; i--)
		{
			CarScript car = m_Vehicles.Get(i);
			if (!car)
			{
				m_Vehicles.Remove(i);
				continue;
			}
			if (!car.ExorIsSleeping())
				continue;
			dormidos++;
			if (IsPlayerNear(car.GetPosition(), settings.vehiculos_despertar_metros))
			{
				car.ExorWake();
				Print(string.Format("%1 Vehiculo %2 despierto (jugador cerca)", ExorStorageConstants.LOG, car.GetType()));
			}
		}

		// Diagnostico: las primeras 3 corridas y despues 1 vez por minuto
		if (m_WakeRuns <= 3 || m_WakeRuns % 12 == 0)
		{
			array<Man> players = new array<Man>;
			GetGame().GetPlayers(players);
			string extra = "sin dormidos";
			for (i = 0; i < m_Vehicles.Count(); i++)
			{
				CarScript primero = m_Vehicles.Get(i);
				if (primero && primero.ExorIsSleeping())
				{
					vector carPos = primero.GetPosition();
					float minDist = -1;
					int j;
					for (j = 0; j < players.Count(); j++)
					{
						Man pj = players.Get(j);
						if (!pj)
							continue;
						float d = vector.Distance(pj.GetPosition(), carPos);
						if (minDist < 0 || d < minDist)
							minDist = d;
					}
					extra = string.Format("dormido[0]=%1 pos=%2 distMin=%3", primero.GetType(), carPos.ToString(), minDist);
					break;
				}
			}
			Print(string.Format("%1 WakeTick #%2: %3 veh (%4 dormidos) radio=%5 jugadores=%6 | %7", ExorStorageConstants.LOG, m_WakeRuns, m_Vehicles.Count(), dormidos, settings.vehiculos_despertar_metros, players.Count(), extra));
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
