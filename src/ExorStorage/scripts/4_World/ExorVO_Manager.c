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

	// Backfill del registro de barriles (self-heal): cuantos por tick como MUCHO, y si ya
	// termino. Bajo a proposito: es trabajo de una sola vez y no compite con el juego.
	static const int EXOR_REG_BACKFILL_PER_TICK = 3;
	bool m_RegBackfillDone = false;

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
		// CANARY de neveras: si llegamos a arrancar, la carga de la persistencia TERMINO OK
		// (ninguna nevera crasheo) -> borrar el canary (si no, la ultima nevera cargada quedaria
		// marcada y se descartaria de gusto el proximo arranque). Diferido 60s: para cuando el
		// CE storage ya termino de restaurar los dynamic (la carga real ocurre tras el OnInit).
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorFridgeCanary.Clear, 60000, false);

		// SELF-HEAL de muebles despawneados/descartados: 1 pasada DIFERIDA 90s tras arrancar.
		// Recrea (vacios) los muebles del registro que no estan vivos -> incluye la nevera que el
		// canary descarto por corrupta. Ver ExorMuebleRegistry. NO causaba el OOM (era la nevera).
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Get().HealTick, 90000, false);
		Print(string.Format("%1 Manager iniciado (tick %2 ms, barrel-tick %3 ms, wake-tick %4 ms)", ExorStorageConstants.LOG, ExorStorageConstants.TICK_MS, ExorStorageConstants.BARREL_TICK_MS, ExorStorageConstants.WAKE_TICK_MS));
	}

	// ------------------------- registro -------------------------
	// SETS PARA EL ALTA (el recorrido sigue usando los arrays).
	// El alta hacia un Find() lineal antes de insertar, y la llama CADA entidad al crearse:
	// en el arranque, con N contenedores, eso es O(N^2). Con 700 barriles mas cientos de
	// muebles son millones de comparaciones metidas justo en la carga del mundo, que es el
	// momento mas cargado del server.
	// Los arrays se conservan porque el tick necesita RECORRER en orden y con cursor; el set
	// paralelo es solo el indice de pertenencia, que es lo unico que hacia el Find.
	static ref set<Exor_Barrel_Base> s_BarrelSet;
	static ref set<Exor_OpenableStorage> s_OpenableSet;
	static ref set<Exor_BodyBag> s_BagSet;

	static void RegisterBarrel(Exor_Barrel_Base barrel)
	{
		if (!barrel)
			return;
		if (!s_BarrelSet)
			s_BarrelSet = new set<Exor_Barrel_Base>();
		if (s_BarrelSet.Find(barrel) >= 0)
			return;
		s_BarrelSet.Insert(barrel);
		Get().m_Barrels.Insert(barrel);
	}

	static void UnregisterBarrel(Exor_Barrel_Base barrel)
	{
		if (s_BarrelSet)
			s_BarrelSet.RemoveItem(barrel);
		int idx = Get().m_Barrels.Find(barrel);
		if (idx != -1)
			Get().m_Barrels.Remove(idx);
	}

	// --- muebles abribles (nevera, etc.): mismo trato que los barriles ---
	static void RegisterOpenable(Exor_OpenableStorage f)
	{
		if (!f)
			return;
		if (!s_OpenableSet)
			s_OpenableSet = new set<Exor_OpenableStorage>();
		if (s_OpenableSet.Find(f) >= 0)
			return;
		s_OpenableSet.Insert(f);
		Get().m_Openables.Insert(f);
	}

	static void UnregisterOpenable(Exor_OpenableStorage f)
	{
		if (s_OpenableSet)
			s_OpenableSet.RemoveItem(f);
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

	// RE-SYNC del candado al CONECTAR (o tras reinicio): los sets del cliente (s_Member/s_Access)
	// arrancan vacios cada sesion. Recorremos los autos con candado y a cada uno del que el jugador
	// es MIEMBRO le avisamos (CAR_MEMBER) + si ya estaba desbloqueado le devolvemos el acceso al baul.
	// Sin esto, tras reiniciar el server el dueño ve su auto "como si no fuese suyo".
	static void ExorSyncCarLocksForPlayer(PlayerBase player)
	{
		if (!GetGame().IsServer() || !player)
			return;
		ExorVO_Manager m = Get();
		if (!m || !m.m_Vehicles)
			return;
		string sid = ExorGroupManager.SteamId(player);
		if (sid == "")
			return;
		int i;
		for (i = 0; i < m.m_Vehicles.Count(); i++)
		{
			CarScript car = m.m_Vehicles.Get(i);
			if (!car || !car.ExorCarHasLock())
				continue;
			if (car.ExorCarIsMemberOfLockGroup(sid))
			{
				player.ExorSendCarMember(car);
				if (car.ExorCarIsUnlockedBy(sid))
					player.ExorSendCarAccessGrant(car);
			}
		}
	}

	static void RegisterBodyBag(Exor_BodyBag bag)
	{
		if (!bag)
			return;
		if (!s_BagSet)
			s_BagSet = new set<Exor_BodyBag>();
		if (s_BagSet.Find(bag) >= 0)
			return;
		s_BagSet.Insert(bag);
		Get().m_BodyBags.Insert(bag);
	}

	static void UnregisterBodyBag(Exor_BodyBag bag)
	{
		if (s_BagSet)
			s_BagSet.RemoveItem(bag);
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
		// Cierre llegado hasta el final: todos los contenedores quedaron con el cargo vacio,
		// asi que el proximo arranque no tiene derrame de "invalid location" que barrer.
		ExorApagadoLimpio.Cerrar();
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
		// Posiciones de los VIVOS, calculadas UNA vez para todo el tick. Las consumen los
		// chequeos de proximidad de barriles, muebles, bolsas y vehiculos. Ver ExorAliveCache.
		array<vector> alivePos = ExorAliveCache.Rebuild(players);

		// --- PRESUPUESTO COMPARTIDO Y ADAPTATIVO ---
		// Un solo pool para barriles + muebles (antes tenian uno cada uno = el doble de
		// trabajo maximo por tick). El factor lo mueve el peor frame observado: si el server
		// sufre, el cupo se corta a la mitad; si esta holgado, se devuelve de a poco.
		AdaptBudgetFactor();
		int budget = BudgetVirtualizar();
		int pendientes = 0;		// contenedores con contenido real esperando virtualizar (para el proximo tick)
		int reconcileBudget = ScaleBudget(ExorStorageConstants.MAX_RECONCILE_PER_TICK);
		int snapBudget = ScaleBudget(ExorStorageConstants.MAX_SNAPSHOT_PER_TICK);

		// PAUSA DE VIRTUALIZACION EN RAID: en las ventanas de looteo libre (= horario de raid)
		// no auto-virtualizamos barriles ni muebles. Los reales se quedan reales (abrir =
		// instantaneo, sin restaurar) y no metemos ops de virtualizar/restaurar en el pico.
		// El snapshot (crash-safety) y el reconcile siguen; abrir/restaurar bajo demanda anda
		// igual; nada desaparece. Se calcula 1 vez por tick, no por barril.
		// PAUSA DE VIRTUALIZACION EN RAID: sigue siendo configurable, pero ahora tiene un
		// LIMITE. La idea original era ahorrarse el costo de virtualizar en el pico; el efecto
		// real era peor: con la pausa puesta, cada contenedor que alguien abre se queda REAL
		// con sus cientos de items hasta que termina la ventana, y con 40 lockers por base eso
		// son decenas de miles de entidades vivas que el motor simula y replica a todos los
		// clientes cercanos. El costo de virtualizar es un pico acotado; el de no hacerlo
		// crece sin techo. Por eso la pausa se levanta sola cuando la cola pasa el umbral.
		bool pauseVirt = false;
		if (cfg.storage.pausar_virt_en_raid && ExorMuebleRules.IsLootFreeNow())
		{
			pauseVirt = m_PendientesPrev < EXOR_PAUSA_MAX_PENDIENTES;
			if (!pauseVirt && m_PendientesPrev > 0)
				Print(string.Format("%1 RAID: %2 contenedores esperando virtualizar -> se levanta la pausa (acumular entidades reales lagea mas que virtualizar)", ExorStorageConstants.LOG, m_PendientesPrev));
		}

		// ANTI-DUPE: faltando pocos minutos para un reinicio PROGRAMADO, cerrar y virtualizar
		// todo a la fuerza. Asi el server se apaga con los cargos vacios y al cargar no se
		// cumple "justReconciled && ExorCargoCount() > 0" -> la ruta del restore duplicado
		// no se puede disparar. No cubre las caidas del host (no avisan); para eso esta el
		// GUARD de ExorDoRestore, que es el que cierra el agujero al 100%.
		// Costo real: casi todo ya esta virtualizado (se auto-virtualiza a los pocos segundos
		// de cerrarse), asi que este pase solo toca el punado que alguien esta usando.
		// Ignora pauseVirt a proposito: si el reinicio cae en horario de raid, igual hay que
		// dejar todo virtualizado.
		if (ExorStorageBootLock.CercaDeReinicio(cfg.storage))
		{
			int forzados = 0;
			int fb;
			for (fb = m_Barrels.Count() - 1; fb >= 0; fb--)
			{
				Exor_Barrel_Base bPre = m_Barrels.Get(fb);
				if (bPre && bPre.ExorForzarVirtualizar())
					forzados++;
			}
			for (fb = m_Openables.Count() - 1; fb >= 0; fb--)
			{
				Exor_OpenableStorage oPre = m_Openables.Get(fb);
				if (oPre && oPre.ExorForzarVirtualizar())
					forzados++;
			}
			if (forzados > 0)
				Print(string.Format("%1 PRE-REINICIO: %2 contenedor(es) cerrados y virtualizados a la fuerza (anti-dupe)", ExorStorageConstants.LOG, forzados));
		}

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

		// BACKFILL del registro de barriles (self-heal). Los barriles no tenian registro, asi
		// que una cuarentena de persistencia se llevo 38 puestos sin forma de recrearlos (su
		// JSON guarda el contenido pero NO la posicion).
		// PERF, que es lo que importa con 700 barriles y 55 players en raid:
		//   - NO se hace en la carga (serian ~700 escrituras de golpe en el arranque).
		//   - Va con TOPE POR TICK y sale del MISMO presupuesto adaptativo que todo lo demas:
		//     si el server esta sufriendo, el cupo se achica solo y esto se frena.
		//   - Por barril el costo normal es un FileExist (stat, microsegundos). Solo escribe
		//     el JSON el barril que todavia NO tiene registro -> es trabajo que se hace UNA
		//     vez en la vida del barril y despues nunca mas.
		int backfillBudget = 0;
		if (!m_RegBackfillDone)
			backfillBudget = ScaleBudget(EXOR_REG_BACKFILL_PER_TICK);
		int backfillHechos = 0;
		int backfillVistos = 0;

		for (i = m_Barrels.Count() - 1; i >= 0; i--)
		{
			Exor_Barrel_Base barrel = m_Barrels.Get(i);
			if (!barrel)
			{
				m_Barrels.Remove(i);
				continue;
			}

			// Mantener el registro al dia si el barril paso del piso a un auto (o al reves).
			// Solo compara un puntero; toca disco unicamente en el instante del cambio.
			barrel.ExorRegSyncParented();

			if (backfillBudget > 0)
			{
				backfillVistos++;
				// los atados (slot de auto / dentro de un cargo) NO se registran: los maneja
				// el JSON del auto. RegisterBarrel igual lo re-chequea.
				if (!barrel.GetHierarchyParent() && !ExorMuebleRegistry.TieneRegistro(barrel.ExorGetID()))
				{
					ExorMuebleRegistry.RegisterBarrel(barrel);
					backfillBudget--;
					backfillHechos++;
				}
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
			// cola: contenido real sin virtualizar (realimenta el cupo del proximo tick)
			if (!barrel.ExorIsVirtualized() && barrel.ExorCargoCount() > 0)
				pendientes++;
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
		// Backfill terminado: se recorrieron TODOS los barriles con cupo de sobra y ninguno
		// necesito registro -> a partir de aca ni siquiera se hace el FileExist (costo cero
		// en operacion normal). Un barril nuevo se registra al colocarlo, no aca.
		if (!m_RegBackfillDone && backfillHechos == 0 && backfillBudget > 0 && backfillVistos >= m_Barrels.Count())
		{
			m_RegBackfillDone = true;
			Print(string.Format("%1 Registro de barriles al dia (%2 barriles) -> backfill apagado", ExorStorageConstants.LOG, m_Barrels.Count()));
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
				if (!fur.ExorIsVirtualized() && fur.ExorCargoCount() > 0)
					pendientes++;
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

		// --- Bolsas de cadaver: TTL (todas) + proximidad (con cupo y cursor) ---
		// PASE 1 - TTL: barato (resta de enteros) y no se puede diferir sin que las tumbas
		// duren de mas, asi que corre para TODAS las bolsas en TODOS los ticks. De paso limpia
		// los huecos nulos del array.
		for (i = m_BodyBags.Count() - 1; i >= 0; i--)
		{
			Exor_BodyBag bag = m_BodyBags.Get(i);
			if (!bag)
			{
				m_BodyBags.Remove(i);
				continue;
			}
			bag.ExorBagTTLTick();	// puede borrar la bolsa (expirada)
		}

		// PASE 2 - PROXIMIDAD (lo caro): mismo trato que barriles y muebles, cupo + cursor
		// rotativo. Antes esto recorria las 250 bolsas por tick escaneando a los 50 players
		// cada una = 656ms de pico. Ver MAX_BAGS_PER_TICK.
		// Las posiciones de los players VIVOS se calculan UNA vez por tick y se pasan ya
		// resueltas: antes cada bolsa hacia su propio PlayerBase.Cast + IsAlive por cada player.
		int bagCount = m_BodyBags.Count();
		if (bagCount > 0)
		{
			int bagBudget = ScaleBudget(ExorStorageConstants.MAX_BAGS_PER_TICK);
			int bagStart = m_BagCursor % bagCount;
			int done = 0;
			for (int bi = 0; bi < bagCount && done < bagBudget; bi++)
			{
				Exor_BodyBag bag2 = m_BodyBags.Get((bagStart + bi) % bagCount);
				if (!bag2)
					continue;	// el pase 1 ya limpia los nulos; aca solo se saltea
				bag2.ExorBagProximityTick(now, alivePos);
				done++;
			}
			m_BagCursor = (bagStart + done) % bagCount;
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
		// Realimentacion del controlador de cupo: lo medido en ESTE tick dimensiona el del
		// proximo (ver BudgetVirtualizar).
		m_PendientesPrev = pendientes;

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
	// A partir de cuantos contenedores en cola se ignora la pausa de raid. 25 contenedores
	// reales es del orden de 10.000 entidades: mucho antes de eso hay que estar drenando.
	static const int EXOR_PAUSA_MAX_PENDIENTES = 25;

	int m_FurCursor = 0;	// cursor rotativo de muebles (anti-starvation sin cupo dedicado)
	int m_BagCursor = 0;	// cursor rotativo de bolsas de cadaver (el pase caro va con cupo)

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

	// ------------------------- presupuesto que sigue a la COLA -------------------------
	// Contenedores que quedaron con contenido REAL esperando virtualizarse, medido en el tick
	// ANTERIOR. Es el termino de realimentacion del controlador: el cupo base esta pensado
	// para el regimen normal (unos pocos contenedores en uso), pero en un raid se abren
	// decenas a la vez y con un cupo fijo la cola no se drena nunca -> las entidades reales se
	// acumulan y ESO es lo que lagea, no el costo de virtualizar.
	//
	// La regla: se apunta a vaciar la cola en EXOR_DRENAJE_TICKS ticks. El cupo sube solo
	// cuando hay atraso y vuelve al piso cuando no lo hay, siempre acotado por el techo duro
	// (para que un pico no se convierta en un pico peor) y siempre multiplicado despues por el
	// factor adaptativo, que sigue siendo el que manda si el server esta sufriendo.
	int m_PendientesPrev;
	static const int EXOR_DRENAJE_TICKS = 6;		// ~30 s a 5 s por tick
	static const int EXOR_VIRT_TECHO_DURO = 90;		// tope absoluto de cupo por tick

	int BudgetVirtualizar()
	{
		int base_ = ExorStorageConstants.MAX_VIRT_PER_TICK;
		int necesario = m_PendientesPrev / EXOR_DRENAJE_TICKS;
		if (necesario > base_)
			base_ = necesario;
		if (base_ > EXOR_VIRT_TECHO_DURO)
			base_ = EXOR_VIRT_TECHO_DURO;
		return ScaleBudget(base_);
	}

	// ------------------------- tick lento (30s): vehiculos -------------------------
	void Tick()
	{
		// Mantenimiento: purga del estado por jugador que crecia sin techo (chat, spawn,
		// anti-farmeo, party). Se auto-throttlea a 1 pasada cada 10 min. Ver ExorHousekeeping.
		ExorHousekeeping.Tick(GetGame().GetTime());

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

	// SELF-HEAL: recrea (vacios) los muebles del registro que el motor despawneo / el canary
	// descarto. 1 pasada diferida al arrancar. El grueso esta en ExorMuebleRegistry.HealScan.
	// Cada cuanto se repite el self-heal despues del pase inicial. Antes corria SOLO al
	// arrancar: un mueble que el motor despawneaba en medio de la sesion no volvia hasta el
	// proximo reinicio (hasta 4 horas sin su locker). Con el scan ya optimizado (los vivos se
	// descartan por el nombre del archivo, sin abrir un solo JSON) repetirlo sale casi gratis.
	static const int EXOR_HEAL_PERIOD_MS = 1800000;	// 30 min
	// Si un pase se saltea (raid / server exigido) NO se espera media hora: se reintenta
	// pronto, asi el mueble del player vuelve apenas se despeja el momento pico.
	static const int EXOR_HEAL_RETRY_MS = 300000;	// 5 min
	bool m_HealFirstDone = false;

	void HealTick()
	{
		if (!GetGame().IsServer())
			return;

		// EL PRIMER PASE (90s tras arrancar) CORRE SIEMPRE, sin guards. Es el que devuelve lo
		// que se perdio en el reinicio y todavia no hay jugadores a los que molestar. Ademas
		// justo ahi el factor adaptativo esta bajo POR el arranque mismo (se midio 0.25), asi
		// que aplicarle el guard lo saltaba siempre: el pase importante nunca corria.
		if (!m_HealFirstDone)
		{
			m_HealFirstDone = true;
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(HealTick, EXOR_HEAL_PERIOD_MS, false);
			ExorMuebleRegistry.HealScan();
			return;
		}

		// PROTECCION 1 - EN RAID NO. En las ventanas de looteo libre (= horario de raid) es
		// cuando hay mas gente junta y mas presion; es exactamente el momento en que NO se
		// quiere meter un barrido de disco. Se salta y se reintenta en 30 min. Mismo criterio
		// que la pausa de virtualizacion.
		if (GetExorConfig().storage.pausar_virt_en_raid && ExorMuebleRules.IsLootFreeNow())
		{
			Print(string.Format("%1 SELF-HEAL: pase periodico SALTEADO (horario de raid) -> se reintenta en 5 min", ExorStorageConstants.LOG));
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(HealTick, EXOR_HEAL_RETRY_MS, false);
			return;
		}

		// PROTECCION 2 - SI EL SERVER SUFRE, TAMPOCO. El mismo factor adaptativo que recorta
		// los cupos del tick: si viene castigado, este pase se pospone.
		if (s_BudgetFactor < 0.9)
		{
			Print(string.Format("%1 SELF-HEAL: pase periodico SALTEADO (server exigido, factor %2) -> se reintenta en 5 min", ExorStorageConstants.LOG, s_BudgetFactor));
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(HealTick, EXOR_HEAL_RETRY_MS, false);
			return;
		}

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(HealTick, EXOR_HEAL_PERIOD_MS, false);
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
	// Cualquier jugador (vivo o no) dentro del radio. Distancia AL CUADRADO: comparar
	// a^2 < r^2 es identico a a < r y ahorra una raiz por jugador por entidad por tick.
	static bool IsPlayerNearList(array<Man> players, vector pos, float radius)
	{
		if (!players)
			return false;
		float r2 = radius * radius;
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			Man p = players.Get(i);
			if (p && ExorMath.Dist3DSq(p.GetPosition(), pos) < r2)
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

	// Igual que los lockers pero para AUTOS: al expulsar al que puso el candado, se lo saca
	// para que el clan no quede afuera de su propio auto.
	static void ClearCarLocksBySetter(string sid)
	{
		if (sid == "")
			return;
		ExorVO_Manager m = Get();
		if (!m || !m.m_Vehicles)
			return;
		int i;
		int n = 0;
		for (i = 0; i < m.m_Vehicles.Count(); i++)
		{
			CarScript car = m.m_Vehicles.Get(i);
			if (car && car.ExorCarHasLock() && car.ExorCarGetSetter() == sid)
			{
				car.ExorCarClearLock();
				n++;
			}
		}
		if (n > 0)
			Print(string.Format("%1 candado limpiado de %2 auto(s) al expulsar a %3", ExorStorageConstants.LOG, n, sid));
	}

	// Como IsPlayerNear pero solo cuenta players VIVOS (un cadaver tambien es un Man).
	// Version sin lista: para llamadores fuera del tick del manager. Refresca el cache de
	// posiciones si esta viejo (1 s de tolerancia) y consulta.
	static bool IsAlivePlayerNear(vector pos, float radius)
	{
		ExorAliveCache.EnsureFresh(1000);
		return ExorAliveCache.AnyNear(pos, radius);
	}

	// Usa el cache de posiciones de VIVOS que el tick llena una sola vez (ExorAliveCache):
	// antes esto hacia PlayerBase.Cast + IsAlive() + una raiz cuadrada POR JUGADOR, en cada
	// llamada, y se llama por contenedor abierto y por vehiculo en cada tick.
	// El parametro 'players' se conserva por compatibilidad de firma pero ya no se recorre.
	static bool IsAlivePlayerNearList(array<Man> players, vector pos, float radius)
	{
		return ExorAliveCache.AnyNear(pos, radius);
	}
}
