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
	ref array<Exor_BodyBag> m_BodyBags;

	void ExorVO_Manager()
	{
		m_Barrels = new array<Exor_Barrel_Base>;
		m_Vehicles = new array<CarScript>;
		m_BodyBags = new array<Exor_BodyBag>;
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
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Get().BarrelTick, ExorStorageConstants.BARREL_TICK_MS, true);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Get().WakeTick, ExorStorageConstants.WAKE_TICK_MS, true);
		Print(string.Format("%1 Manager iniciado (tick %2 ms, barrel-tick %3 ms, wake-tick %4 ms)", ExorStorageConstants.LOG, ExorStorageConstants.TICK_MS, ExorStorageConstants.BARREL_TICK_MS, ExorStorageConstants.WAKE_TICK_MS));
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

	// DEBUG: cuantos barriles estan ABIERTOS ahora mismo (para diagnosticar el bug de
	// "abro varios y se rompe la grilla"). Tambien suma el total de items reales en todos.
	static int CountOpenBarrels(out int totalCargo)
	{
		ExorVO_Manager m = Get();
		int n = 0;
		totalCargo = 0;
		int i;
		for (i = 0; i < m.m_Barrels.Count(); i++)
		{
			Exor_Barrel_Base b = m.m_Barrels.Get(i);
			if (b && b.IsOpen())
			{
				n++;
				totalCargo += b.ExorCargoCount();
			}
		}
		return n;
	}

	static void RegisterVehicle(CarScript car)
	{
		if (Get().m_Vehicles.Find(car) == -1)
		{
			Get().m_Vehicles.Insert(car);
		}
	}

	static void RegisterBodyBag(Exor_BodyBag bag)
	{
		if (Get().m_BodyBags.Find(bag) == -1)
			Get().m_BodyBags.Insert(bag);
	}

	static void UnregisterBodyBag(Exor_BodyBag bag)
	{
		int idx = Get().m_BodyBags.Find(bag);
		if (idx != -1)
			Get().m_BodyBags.Remove(idx);
	}

	// Virtualiza TODOS los barriles y bodybags con contenido. Se llama al APAGAR el
	// server (OnMissionFinish): asi su contenido anidado pasa al JSON ANTES de que el
	// engine guarde la persistencia -> al reiniciar no quedan items reales en
	// bag-in-barril que el motor tire al piso por "invalid location" (anidado profundo).
	static void VirtualizeAll()
	{
		ExorVO_Manager m = Get();
		int virt = 0;
		int i;
		for (i = 0; i < m.m_Barrels.Count(); i++)
		{
			Exor_Barrel_Base b = m.m_Barrels.Get(i);
			if (b && !b.ExorIsVirtualized() && b.ExorCargoCount() > 0)
			{
				b.ExorVirtualize();
				virt++;
			}
		}
		int j;
		for (j = 0; j < m.m_BodyBags.Count(); j++)
		{
			Exor_BodyBag bag = m.m_BodyBags.Get(j);
			if (bag && !bag.ExorIsVirtualized() && bag.ExorCargoCount() > 0)
			{
				bag.ExorVirtualize();
				virt++;
			}
		}
		Print(string.Format("%1 VirtualizeAll (apagado): %2 contenedores virtualizados a disco", ExorStorageConstants.LOG, virt));
	}

	// ------------------------- tick RAPIDO (5s): barriles + bodybags -------------------------
	void BarrelTick()
	{
		ExorConfig cfg = GetExorConfig();
		int now = GetGame().GetTime();
		int i;

		// --- Barriles: reconcile de arranque (THROTTLE) + auto-cierre + virtualizacion (THROTTLE) + snapshot (THROTTLE) ---
		int budget = ExorStorageConstants.MAX_VIRT_PER_TICK;
		int reconcileBudget = ExorStorageConstants.MAX_RECONCILE_PER_TICK;
		int snapBudget = ExorStorageConstants.MAX_SNAPSHOT_PER_TICK;
		for (i = m_Barrels.Count() - 1; i >= 0; i--)
		{
			Exor_Barrel_Base barrel = m_Barrels.Get(i);
			if (!barrel)
			{
				m_Barrels.Remove(i);
				continue;
			}
			// RECONCILE caro (scan del piso tras crash): repartirlo. Si no hay cupo este
			// tick, el barril espera al proximo (sigue sin tickear hasta reconciliar). Si
			// un player lo abre antes, ExorRestoreIfNeeded lo reconcilia en el acto.
			if (barrel.ExorNeedsReconcile())
			{
				if (reconcileBudget <= 0)
					continue;
				barrel.ExorReconcileNow();
				reconcileBudget--;
			}
			// allowVirtualize=budget>0 y allowSnapshot=snapBudget>0: si se acabo el cupo de
			// este tick, el barril se auto-cierra igual pero difiere virtualizar/snapshot al
			// proximo tick -> sin pico de CPU (virtualizar) ni de I/O a disco (snapshot).
			bool didSnap;
			if (barrel.ExorTick(now, cfg.storage, budget > 0, snapBudget > 0, didSnap))
				budget--;
			if (didSnap)
				snapBudget--;
		}

		// --- Bolsas de cadaver: TTL + virtualizar por lejania ---
		for (i = m_BodyBags.Count() - 1; i >= 0; i--)
		{
			Exor_BodyBag bag = m_BodyBags.Get(i);
			if (!bag)
			{
				m_BodyBags.Remove(i);
				continue;
			}
			bag.ExorBagTick(now);
		}
	}

	// ------------------------- tick lento (30s): vehiculos -------------------------
	void Tick()
	{
		ExorConfig cfg = GetExorConfig();
		ExorCfgVehiculos veh = cfg.vehiculos;
		int now = GetGame().GetTime();
		int i;

		// --- Vehiculos: dormir los inactivos ---
		if (!veh.vehiculos_dormir)
			return;
		if (veh.vehiculos_dormir_minutos <= 0)
			return;

		int sleepMs = veh.vehiculos_dormir_minutos * 60000;
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
			if (veh.vehiculos_excluidos.Find(car.GetType()) != -1)
				continue;
			if (IsPlayerNear(car.GetPosition(), veh.vehiculos_despertar_metros))
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
	void WakeTick()
	{
		int b;
		// Bolsas de cadaver: des-virtualizar cuando un player vivo se acerca
		for (b = m_BodyBags.Count() - 1; b >= 0; b--)
		{
			Exor_BodyBag bag = m_BodyBags.Get(b);
			if (!bag)
			{
				m_BodyBags.Remove(b);
				continue;
			}
			bag.ExorBagWake();
		}

		ExorCfgVehiculos veh = GetExorConfig().vehiculos;
		if (!veh.vehiculos_dormir)
			return;

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
			if (IsPlayerNear(car.GetPosition(), veh.vehiculos_despertar_metros))
			{
				car.ExorWake();
			}
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

	// Como IsPlayerNear pero solo cuenta players VIVOS (un cadaver tambien es un Man).
	static bool IsAlivePlayerNear(vector pos, float radius)
	{
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase p = PlayerBase.Cast(players.Get(i));
			if (p && p.IsAlive() && vector.Distance(p.GetPosition(), pos) < radius)
				return true;
		}
		return false;
	}
}
