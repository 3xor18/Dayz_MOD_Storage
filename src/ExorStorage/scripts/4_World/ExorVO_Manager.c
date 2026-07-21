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
	ref array<Exor_OpenableStorage> m_Openables;	// muebles abribles (nevera, etc.)
	ref array<CarScript> m_Vehicles;
	ref array<Exor_BodyBag> m_BodyBags;

	void ExorVO_Manager()
	{
		m_Barrels = new array<Exor_Barrel_Base>;
		m_Openables = new array<Exor_OpenableStorage>;
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
		// SELF-HEAL de muebles despawneados por el motor: 1 SOLA pasada DIFERIDA 90s tras arrancar
		// (esperar a que cargue toda la persistencia -> no recrear duplicados de los que iban a
		// cargar). El despawn pasa en el ARRANQUE (clip con pared / limpieza de CE), asi que con la
		// pasada de arranque alcanza -> no hace falta un scan periodico (ahorra recursos).
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Get().HealTick, 90000, false);
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

	// --- muebles abribles (nevera, etc.): mismo trato que los barriles ---
	static void RegisterOpenable(Exor_OpenableStorage f)
	{
		if (Get().m_Openables.Find(f) == -1)
			Get().m_Openables.Insert(f);
	}

	static void UnregisterOpenable(Exor_OpenableStorage f)
	{
		int idx = Get().m_Openables.Find(f);
		if (idx != -1)
			Get().m_Openables.Remove(idx);
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
	// true durante el apagado del server (OnMissionFinish -> VirtualizeAll). Lo mira el log de
	// MUEBLE-REMOVIDO para NO loguear las bajas normales del shutdown (solo las de sesion viva).
	static bool s_ShuttingDown;

	static void VirtualizeAll()
	{
		s_ShuttingDown = true;
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
		// muebles abribles (nevera, etc.): igual que los barriles al apagar
		for (i = 0; i < m.m_Openables.Count(); i++)
		{
			Exor_OpenableStorage fur = m.m_Openables.Get(i);
			if (fur && !fur.ExorIsVirtualized() && fur.ExorCargoCount() > 0)
			{
				fur.ExorVirtualize();
				virt++;
			}
		}
		// BOLSAS DE CADAVER: antes se saltaban aca, asumiendo que su cargo era "top-level" y
		// que la persistencia normal alcanzaba. NO alcanza: el loot de una tumba incluye ROPA
		// CON ITEMS ADENTRO (anidado), que es exactamente lo que DayZ tira al piso al recargar.
		// Reproducido en el test local del 20-jul: 2 tumbas con loot 12/12, reinicio, y al
		// volver estaban VACIAS con el loot desparramado en el piso e inagarrable.
		// (El motivo historico de la exclusion -"virtualizar perdia el loot"- era el bug viejo,
		// ya resuelto en v2.8.0: hoy las tumbas virtualizan por distancia todo el tiempo sin
		// perder nada, asi que hacerlo tambien al apagar es el mismo camino ya probado.)
		for (i = 0; i < m.m_BodyBags.Count(); i++)
		{
			Exor_BodyBag bag = m.m_BodyBags.Get(i);
			// ExorContentCount (no CargoCount): en la tumba la mayor parte del loot vive en
			// los SLOTS DE EQUIPO, no en el cargo; contar solo el cargo daria 0.
			if (bag && !bag.ExorIsVirtualized() && bag.ExorContentCount() > 0)
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

		// lista de players obtenida UNA vez para este tick (la reusan todos los barriles en
		// vez de llamar GetPlayers() por cada barril abierto). De paso, cachear el conteo de
		// conectados para POP_REQ (el contador del mapa) sin re-escanear por request.
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		s_PopCount = players.Count();

		// --- PRESUPUESTO COMPARTIDO Y ADAPTATIVO ---
		// Un solo pool para barriles + muebles (antes tenian uno cada uno = el doble de
		// trabajo maximo por tick). El factor lo mueve el peor frame observado: si el server
		// sufre, el cupo se corta a la mitad; si esta holgado, se devuelve de a poco.
		AdaptBudgetFactor();
		int budget = ScaleBudget(ExorStorageConstants.MAX_VIRT_PER_TICK);
		int reconcileBudget = ScaleBudget(ExorStorageConstants.MAX_RECONCILE_PER_TICK);
		int snapBudget = ScaleBudget(ExorStorageConstants.MAX_SNAPSHOT_PER_TICK);

		// PAUSA DE VIRTUALIZACION EN RAID: en las ventanas de looteo libre (= horario de raid)
		// no auto-virtualizamos barriles ni muebles. Los reales se quedan reales (abrir =
		// instantaneo, sin restaurar) y no metemos ops de virtualizar/restaurar en el pico.
		// El snapshot (crash-safety) y el reconcile siguen; abrir/restaurar bajo demanda anda
		// igual; nada desaparece. Se calcula 1 vez por tick, no por barril.
		bool pauseVirt = cfg.storage.pausar_virt_en_raid && ExorMuebleRules.IsLootFreeNow();

		// RESERVA PARA MUEBLES: el bloque de barriles corre primero y, si se comiera todo el
		// cupo, los muebles no reconciliarian hasta que terminaran los ~657 barriles (y hasta
		// entonces NO auto-cierran ni virtualizan). Se le guarda la mitad del cupo de
		// reconcile al bloque de muebles. Si no hay muebles el reservado se devuelve abajo.
		int furReserve = 0;
		if (m_Openables.Count() > 0 && reconcileBudget > 1)
		{
			furReserve = reconcileBudget / 2;
			reconcileBudget = reconcileBudget - furReserve;
		}

		// Instrumentacion: cuanto tarda cada bloque de este tick. Se loguea SOLO si el tick
		// se pasa de TICK_WARN_MS -> en operacion normal no escribe nada.
		int tStart = GetGame().GetTime();
		int nBarrelWork = 0;
		int nFurWork = 0;

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
					continue;	// sin cupo: espera al proximo tick (sigue sin tickear)
				// solo descuenta si REALMENTE reconcilio (los vacios se saldan gratis y
				// siguen de largo al tick normal en vez de perder el turno)
				if (barrel.ExorReconcileNow())
				{
					reconcileBudget--;
					nBarrelWork++;
				}
			}
			// allowVirtualize=budget>0 y allowSnapshot=snapBudget>0: si se acabo el cupo de
			// este tick, el barril se auto-cierra igual pero difiere virtualizar/snapshot al
			// proximo tick -> sin pico de CPU (virtualizar) ni de I/O a disco (snapshot).
			bool didSnap;
			if (barrel.ExorTick(now, cfg.storage, budget > 0 && !pauseVirt, snapBudget > 0, players, didSnap))
			{
				budget--;
				nBarrelWork++;
			}
			if (didSnap)
			{
				snapBudget--;
				nBarrelWork++;
			}
		}
		int tBarrels = GetGame().GetTime() - tStart;

		// --- Muebles abribles (nevera, locker, guncab, etc.): MISMA logica que el barril y
		// COMPARTIENDO el pool (lo que sobro del bloque de barriles).
		//
		// Antes tenian presupuesto PROPIO para que los 634 barriles no los starvearan. El
		// efecto colateral fue duplicar el techo de trabajo por tick (15+15 virt, 12+12
		// snapshot, 3+3 reconcile) justo cuando se desplegaron los muebles -> mas pico de
		// CPU/IO por tick con alta poblacion. Ahora el pool es unico y el anti-starvation
		// se resuelve con el CURSOR ROTATIVO de abajo: cada tick los muebles empiezan a
		// recorrerse desde donde quedo el anterior, asi que ninguno se queda sin turno
		// aunque el cupo se haya agotado varias veces seguidas. Escala igual con 3 muebles
		// que con 300.
		// pase 1: limpiar referencias muertas (barato, sin tocar presupuesto)
		for (i = m_Openables.Count() - 1; i >= 0; i--)
		{
			if (!m_Openables.Get(i))
				m_Openables.Remove(i);
		}

		// pase 2: recorrer DESDE EL CURSOR, dando la vuelta. El que no alcanzo cupo este
		// tick queda primero en el proximo -> reparto justo sin presupuesto dedicado.
		// devolver la reserva: lo que sobro del bloque de barriles + lo reservado para muebles
		reconcileBudget = reconcileBudget + furReserve;

		int furCount = m_Openables.Count();
		if (furCount > 0)
		{
			int furStart = m_FurCursor % furCount;
			for (int k = 0; k < furCount; k++)
			{
				Exor_OpenableStorage fur = m_Openables.Get((furStart + k) % furCount);
				if (!fur)
					continue;
				// FAST-SKIP: si el mueble esta idle (cerrado+virtualizado+limpio, sin periodica
				// propia) no hay NADA que hacer -> saltarlo sin entrar a ExorTick/PeriodicTick.
				// Con cientos de muebles casi todos estan idle -> el tick deja de recorrerlos de
				// verdad (era el nuevo cuello: 47ms por 1-2 ops sobre 92 muebles).
				if (fur.ExorIsIdle())
					continue;
				if (fur.ExorNeedsReconcile())
				{
					if (reconcileBudget <= 0)
						continue;
					if (fur.ExorReconcileNow())
					{
						reconcileBudget--;
						nFurWork++;
					}
				}
				bool didSnapF;
				// PESO: una op de mueble descuenta MUEBLE_BUDGET_WEIGHT del cupo (no 1 como el
				// barril) porque cuesta ~15x mas -> evita apilar varios muebles caros en un tick.
				if (fur.ExorTick(now, cfg.storage, budget > 0 && !pauseVirt, snapBudget > 0, players, didSnapF))
				{
					budget -= ExorStorageConstants.MUEBLE_BUDGET_WEIGHT;
					nFurWork++;
				}
				if (didSnapF)
				{
					snapBudget -= ExorStorageConstants.MUEBLE_BUDGET_WEIGHT;
					nFurWork++;
				}
				// logica periodica de la subclase (ej: bateria de la nevera). Centralizado
				// aca en vez de un timer por-nevera -> escala a muchas neveras sin cientos
				// de timers. La nevera throttlea internamente (cada ~60s).
				fur.ExorPeriodicTick(now);
			}
			// avanzar el cursor para que el proximo tick arranque en otro punto
			m_FurCursor = (furStart + 1) % furCount;
		}
		int tFurniture = GetGame().GetTime() - tStart - tBarrels;

		// --- Bolsas de cadaver: TTL + virtualizar/restaurar por distancia (reusa 'players') ---
		for (i = m_BodyBags.Count() - 1; i >= 0; i--)
		{
			Exor_BodyBag bag = m_BodyBags.Get(i);
			if (!bag)
			{
				m_BodyBags.Remove(i);
				continue;
			}
			bag.ExorBagTick(now, players);
		}

		// --- Cerrar sesiones de saqueo inactivas (escribe la linea agrupada al audit) ---
		ExorRoboBuffer.Tick();

		// --- Volcar el forense de tumbas (1 escritura por tumba, no por item looteado) ---
		ExorTumbaForense.FlushPending();

		// --- Volcar el audit bufferizado a disco ---
		// El unico llamador periodico de Flush() era el tick 1Hz del anti-cheat; al sacarlo
		// quedo SOLO el flush de OnMissionFinish, o sea el audit se escribia recien al apagar
		// y un crash se llevaba puesto todo el log del ciclo. Ahora se vuelca cada 5s
		// (barato: sale enseguida si la cola esta vacia, y abre el archivo UNA vez por flush).
		ExorRaidLog.Flush();

		// --- INSTRUMENTACION: solo habla cuando el tick DUELE ---
		// Sin esto solo se ve el FPS global (duele, pero no donde). Con esto, cuando el tick
		// se pasa del umbral queda en el RPT que bloque lo causo y con cuanta carga.
		// grep "3xorVO TICK-LENTO" <RPT>
		int tTotal = GetGame().GetTime() - tStart;
		if (tTotal >= ExorStorageConstants.TICK_WARN_MS)
		{
			// OJO: string.Format acepta como MUCHO 9 parametros (%1..%9). Por eso va en dos
			// lineas en vez de una sola larga.
			int tBags = tTotal - tBarrels - tFurniture;
			Print(string.Format("%1 TICK-LENTO total=%2ms | barriles=%3ms/%4ops de %5 | muebles=%6ms/%7ops de %8",
				ExorStorageConstants.LOG,
				tTotal, tBarrels, nBarrelWork, m_Barrels.Count(),
				tFurniture, nFurWork, m_Openables.Count()));
			Print(string.Format("%1 TICK-LENTO (cont) bolsas=%2ms de %3 | players=%4 | cupo=%5%%",
				ExorStorageConstants.LOG,
				tBags, m_BodyBags.Count(),
				s_PopCount, Math.Round(m_BudgetFactor * 100)));
		}
	}

	// ------------------------- presupuesto adaptativo -------------------------
	// Factor actual del cupo (1.0 = techo completo). Lo mueve el peor frame observado por
	// ExorPerfMonitor entre ticks: el server se auto-regula en vez de depender de que
	// alguien tunee constantes a mano para cada nivel de poblacion.
	float m_BudgetFactor = 1.0;
	// espejo estatico del factor: lo leen las entidades en su tick (ej. el debounce del
	// snapshot) sin tener que recibirlo por parametro en toda la cadena
	static float s_BudgetFactor = 1.0;

	// ------------------------- serializacion de RESTORES -------------------------
	// Restaurar un contenedor es lo mas caro que hace el mod y NO pasaba por ningun
	// presupuesto: lo dispara el jugador al ABRIR, y corre entero en UN frame (crea y
	// reubica cada item, recursivo por attachments y cargo anidado). Un locker lleno son
	// facilmente 1500-2500 entidades creadas de golpe.
	// Con 50-60 jugadores varias aperturas caen en el mismo frame y se suman -> hitch.
	// Esto no parte el restore individual (partirlo a mitad es riesgoso para el loot):
	// solo garantiza que no arranquen DOS en el mismo frame. El inventario del mueble esta
	// bloqueado hasta que termina, asi que esperar unos ms es invisible para el jugador.
	static int s_LastRestoreMs;
	static const int RESTORE_SPACING_MS = 250;	// separacion minima entre restores

	// true si se puede restaurar YA. Si no, el llamador debe re-agendar.
	static bool CanRestoreNow()
	{
		int now = GetGame().GetTime();
		if (now - s_LastRestoreMs < RESTORE_SPACING_MS)
			return false;
		s_LastRestoreMs = now;
		return true;
	}
	int m_FurCursor = 0;	// cursor rotativo de muebles (anti-starvation sin cupo dedicado)

	void AdaptBudgetFactor()
	{
		float worst = ExorPerfMonitor.ConsumeWorstMs();
		if (worst <= 0)
			return;	// todavia sin muestra (arranque): dejar el factor como esta

		if (worst >= ExorStorageConstants.ADAPT_FRAME_BAD_MS)
		{
			// el server viene sufriendo -> cortar rapido y dejar respirar al frame
			m_BudgetFactor = m_BudgetFactor * ExorStorageConstants.ADAPT_STEP_DOWN;
			if (m_BudgetFactor < ExorStorageConstants.ADAPT_MIN_FACTOR)
				m_BudgetFactor = ExorStorageConstants.ADAPT_MIN_FACTOR;
		}
		else if (worst <= ExorStorageConstants.ADAPT_FRAME_OK_MS)
		{
			// holgado -> devolver cupo DE A POCO (subir de golpe hace oscilar el frame)
			m_BudgetFactor = m_BudgetFactor + ExorStorageConstants.ADAPT_STEP_UP;
			if (m_BudgetFactor > 1.0)
				m_BudgetFactor = 1.0;
		}
		// zona muerta entre OK y BAD: no tocar nada (histeresis, evita el ping-pong)
		s_BudgetFactor = m_BudgetFactor;
	}

	// Escala un cupo por el factor actual, con piso de 1: aunque el server este muy
	// cargado SIEMPRE se hace al menos una operacion por tick -> nunca se estanca la cola.
	int ScaleBudget(int techo)
	{
		int v = (int)Math.Round(techo * m_BudgetFactor);
		if (v < 1)
			v = 1;
		return v;
	}

	// ------------------------- tick lento (30s): vehiculos -------------------------
	void Tick()
	{
		ExorConfig cfg = GetExorConfig();
		ExorCfgVehiculos veh = cfg.vehiculos;
		int now = GetGame().GetTime();
		int i;

		// volcar stats pendientes a disco (1 sola escritura cada 30s, no por cada kill)
		ExorStats.Get().FlushIfDirty();
		// idem el ledger anti-farmeo: solo se persistia en OnMissionFinish, asi que un crash
		// borraba la ventana de 4h y el contador de farmeo arrancaba de cero.
		ExorKillFarm.FlushIfDirty();
		ExorBodyBagGuard.Purgar();	// soltar las marcas de tumba vencidas

		// --- Vehiculos: dormir los inactivos ---
		if (!veh.vehiculos_dormir)
			return;
		if (veh.vehiculos_dormir_minutos <= 0)
			return;

		// players obtenidos UNA vez para todos los chequeos de distancia de este tick
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);

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
			if (IsPlayerNearList(players, car.GetPosition(), veh.vehiculos_despertar_metros))
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

	// SELF-HEAL: recrea los muebles que el motor despawneo (registro vs vivos). 1 vez al arrancar
	// (diferido) + cada 6h. El grueso del trabajo esta en ExorMuebleRegistry.HealScan (barato).
	void HealTick()
	{
		if (!GetGame().IsServer())
			return;
		ExorMuebleRegistry.HealScan();
	}

	// ------------------------- tick rapido (5s): despertar + auto-virtualizar -------------------------
	void WakeTick()
	{
		// Las bolsas de cadaver ya NO se tocan aca: su virtualizar/restaurar por distancia
		// corre en el BarrelTick (5s, que ya tiene la lista de players) + Open() al abrir.
		ExorCfgVehiculos veh = GetExorConfig().vehiculos;
		ExorCfgStorage st = GetExorConfig().storage;
		bool autoVirt = st.parking_auto_virtualizar;
		if (!veh.vehiculos_dormir && !autoVirt)
			return;

		// players obtenidos UNA vez para todos los chequeos de distancia de este tick
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		int now = GetGame().GetTime();

		int i;
		for (i = m_Vehicles.Count() - 1; i >= 0; i--)
		{
			CarScript car = m_Vehicles.Get(i);
			if (!car)
			{
				m_Vehicles.Remove(i);
				continue;
			}
			// AUTO-VIRTUALIZAR: auto idle dentro del radio de un parking, sin jugador por el
			// umbral -> se guarda solo (sale de la red). Si lo hizo, el auto se borro -> continue.
			if (autoVirt && ExorTryAutoVirtualize(car, players, now, st))
				continue;
			// DESPERTAR dormidos cuando un jugador entra al radio
			if (veh.vehiculos_dormir && car.ExorIsSleeping())
			{
				if (IsPlayerNearList(players, car.GetPosition(), veh.vehiculos_despertar_metros))
					car.ExorWake();
			}
		}
	}

	// Auto-virtualiza un auto si: NO esta en uso, esta dentro del radio de un parking (base),
	// no hubo jugador cerca por 'parking_auto_minutos', y el parking esta en un territorio (para
	// taggear el auto con ese grupo y poder recuperarlo del menu). Devuelve TRUE solo si lo
	// VIRTUALIZO (el auto quedo borrado -> el caller debe hacer 'continue'). Los guards de robo/
	// tripulantes/motor los aplica ExorVehicleGarage.Virtualize.
	bool ExorTryAutoVirtualize(CarScript car, array<Man> players, int now, ExorCfgStorage st)
	{
		if (car.IsRuined())
			return false;
		// en uso (motor prendido o alguien adentro) -> resetear timer, no virtualizar
		if (car.ExorIsActive())
		{
			car.ExorSetLastNear(now);
			return false;
		}
		// debe estar dentro del radio de un PARKING (si no, es un auto suelto: no se recuperaria)
		vector parkingPos;
		if (!ExorVehicleGarage.NearParking(car.GetPosition(), st.parking_radio_metros, parkingPos))
			return false;
		// jugador cerca del auto -> resetear timer (sigue "en uso reciente")
		if (IsPlayerNearList(players, car.GetPosition(), st.parking_radio_metros))
		{
			car.ExorSetLastNear(now);
			return false;
		}
		// todavia no paso el umbral de inactividad
		if (now - car.ExorGetLastNear() < st.parking_auto_minutos * 60000)
			return false;
		// grupo dueño del territorio del parking (para recuperar el auto desde el menu de ESE clan)
		string group = ExorTerritoryManager.Get().GroupAtPos(parkingPos);
		if (group == "")
			return false;	// parking fuera de territorio -> no se podria recuperar -> no virtualizar
		string reason = ExorVehicleGarage.Virtualize(car, group);
		if (reason == "")
		{
			Print(string.Format("%1 auto-virtualizado (idle cerca de un parking, grupo %2)", ExorStorageConstants.LOG, group));
			return true;
		}
		return false;	// un guard lo bloqueo (enemigo cerca, etc.) -> se reintenta el proximo tick
	}

	// Contador de jugadores conectados, CACHEADO (lo refresca BarrelTick reusando la lista
	// que ya obtiene). POP_REQ responde este valor -> sin GetPlayers() por request.
	static int s_PopCount;

	static bool IsPlayerNear(vector pos, float radius)
	{
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		return IsPlayerNearList(players, pos, radius);
	}

	// Version que reusa una lista ya obtenida (para no llamar GetPlayers por cada entidad).
	static bool IsPlayerNearList(array<Man> players, vector pos, float radius)
	{
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			Man p = players.Get(i);
			if (p && vector.Distance(p.GetPosition(), pos) < radius)
				return true;
		}
		return false;
	}

	// Limpia la clave de los LOCKERS cuya clave puso 'sid'. Lo llama el kick del party: si
	// expulsan al miembro que puso la clave (ej: dejo de conectarse y no la compartio), su
	// locker queda SIN clave para que el resto del clan no quede afuera.
	static void ClearLockerKeysBySetter(string sid)
	{
		if (sid == "")
			return;
		ExorVO_Manager m = Get();
		if (!m || !m.m_Openables)
			return;
		int i;
		int n = 0;
		for (i = 0; i < m.m_Openables.Count(); i++)
		{
			Exor_OpenableStorage fur = m.m_Openables.Get(i);
			if (fur && fur.ExorHasCodeLock() && fur.ExorHasKey() && fur.ExorGetKeySetterSid() == sid)
			{
				fur.ExorClearKey();
				n++;
			}
		}
		if (n > 0)
			Print(string.Format("%1 clave limpiada de %2 locker(s) al expulsar a %3", ExorStorageConstants.LOG, n, sid));
	}

	// Como IsPlayerNear pero solo cuenta players VIVOS (un cadaver tambien es un Man).
	static bool IsAlivePlayerNear(vector pos, float radius)
	{
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		return IsAlivePlayerNearList(players, pos, radius);
	}

	static bool IsAlivePlayerNearList(array<Man> players, vector pos, float radius)
	{
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
