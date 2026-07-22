// ============================================================================
// 3xor_Vanilla_Optimization - Barril 3xor
// Fase 1: empaquetar/desplegar, indestructible, no lockeable
// Fase 2: virtualizacion del contenido + auto-cierre
// Fase 3: cooldown anti-dupe de apertura + blacklist + ropa con items
// ============================================================================

class Exor_Barrel_Base : Barrel_ColorBase
{
	// ID persistente del barril (liga el barril con su JSON de contenido)
	protected string m_ExorID;
	// Timestamps de runtime (ms de uptime del server)
	protected int m_ExorLastInteractMs;
	protected int m_ExorLastCloseMs;
	// Sincronizado al cliente: el barril tiene contenido virtualizado en disco
	// (el cliente no puede chequear el archivo; sin esto mostraria "Empaquetar")
	protected bool m_ExorVirtualizedSync;
	// NUEVO MODELO (idea del user): el JSON es la VERDAD permanente del contenido,
	// se actualiza en VIVO en cada cambio de cargo (throttle por tick) y se borra solo
	// al levantar el barril. Estado runtime:
	// Ultimo estado conocido "estoy ATADO a algo" (slot de barril de un auto, cargo, manos).
	// Sirve para mantener el registro del self-heal al dia SIN tocar disco todos los ticks:
	// solo se escribe/borra cuando el estado CAMBIA. Ver ExorRegSyncParented.
	protected bool m_ExorRegParented;
	// ticks que faltan para dar de alta al barril que acaba de volver al piso. Se difiere
	// porque justo al soltarlo su posicion todavia no se asento (puede estar en la del player
	// o del auto) y guardariamos una posicion mala. Un par de ticks y ya esta quieto.
	protected int m_ExorRegPendingTicks;
	// Ultima posicion con la que quedo escrito el registro. Sirve para detectar que el barril
	// SE MOVIO y refrescar el registro, sin tener que leer el JSON de todos los barriles en
	// cada arranque. Ver ExorRegSyncParented.
	protected vector m_ExorRegLastPos;
	protected bool m_ExorRegPosInit;
	protected bool m_ExorVirt;        // los items reales estan SACADOS del mundo (estan en el JSON)
	protected bool m_ExorSnapDirty;   // hubo cambios de cargo sin volcar todavia al JSON
	protected bool m_ExorLoadDone;    // ya se reconcilio JSON vs persistencia tras cargar
	protected bool m_ExorRestoring;   // estamos recreando items DESDE el JSON (no marcar dirty)
	protected bool m_ExorFloorCleaned;// ya se limpio el piso (drops de DayZ) tras este arranque

	void Exor_Barrel_Base()
	{
		RegisterNetSyncVariableBool("m_ExorVirtualizedSync");
	}

	// ------------------------- DEBUG temporal (sacar al terminar) -------------------------
	// Loguea el evento + el estado completo del barril. NO genera ID (usa m_ExorID crudo)
	// para no asignar un ID random antes de que OnStoreLoad lea el guardado.
	void ExorDbg(string ev)
	{
		if (!ExorStorageConstants.DEBUG_BARRELS)
			return;
		bool jsonExists = false;
		if (m_ExorID != "")
			jsonExists = FileExist(string.Format("%1\\%2.json", ExorStorageConstants.STORAGE_DIR, m_ExorID));
		// string.Format en Enforce acepta MAX 9 params -> armar el estado en sub-formats.
		string st1 = string.Format("cargo=%1 open=%2 virt=%3 json=%4", ExorCargoCount(), IsOpen(), m_ExorVirt, jsonExists);
		string st2 = string.Format("loadDone=%1 floorCleaned=%2 dirty=%3", m_ExorLoadDone, m_ExorFloorCleaned, m_ExorSnapDirty);
		Print(string.Format("%1[DBG] %2 | id=%3 pos=%4 %5 %6", ExorStorageConstants.LOG, ev, m_ExorID, GetPosition(), st1, st2));
	}

	// ------------------------- init / persistencia -------------------------
	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			SetAllowDamage(false);	// indestructible: el loot nunca se pierde
			m_ExorLastInteractMs = GetGame().GetTime();
			m_ExorLastCloseMs = 0;
			ExorVO_Manager.RegisterBarrel(this);
			ExorDbg("EEInit (barril creado/spawneado en el mundo)");
		}
	}

	override void EEDelete(EntityAI parent)
	{
		if (GetGame().IsServer())
		{
			ExorDbg("EEDelete (barril sacado del mundo: levantado, borrado o descargado)");
			ExorVO_Manager.UnregisterBarrel(this);
		}
		super.EEDelete(parent);
	}

	// true si el barril esta ATTACHED a un slot de barril de un vehiculo/objeto (no en el piso
	// ni en un cargo suelto). Se usa para abrirlo solo en el slot (asi acepta items ahi) y no
	// en el cargo del vehiculo (transporte). No se toca EEParentedTo para no romper la colocacion.
	// InventoryLocation REUTILIZABLE (una sola instancia para TODOS los barriles). Antes
	// esta funcion hacia `new InventoryLocation()` en cada llamada, y el manager la llama
	// para los 600+ barriles cada tick (5s) -> 600+ allocaciones/tick = basura que dispara
	// pausas de GC (los "peorFrame" parejos ~99ms). El script es mono-hilo y la llamada es
	// secuencial (1 barril a la vez en el tick), asi que compartir el objeto es seguro:
	// GetCurrentInventoryLocation lo sobreescribe entero en cada uso.
	static ref InventoryLocation s_ExorSlotLoc;

	bool ExorIsInVehicleSlot()
	{
		// FAST-PATH: un barril tirado en el piso o en una base NO tiene padre en la jerarquia.
		// Solo los que estan ENGANCHADOS a algo (slot de vehiculo, cargo de una carpa, etc.)
		// tienen padre. La MAYORIA de los 600+ barriles estan sueltos -> cortamos aca con un
		// accesor barato y NOS AHORRAMOS la consulta de inventario para casi todos.
		if (!GetHierarchyParent())
			return false;
		if (!GetInventory())
			return false;
		if (!s_ExorSlotLoc)
			s_ExorSlotLoc = new InventoryLocation();
		if (!GetInventory().GetCurrentInventoryLocation(s_ExorSlotLoc))
			return false;
		return s_ExorSlotLoc.GetType() == InventoryLocationType.ATTACHMENT;
	}

	override void OnStoreSave(ParamsWriteContext ctx)
	{
		super.OnStoreSave(ctx);
		ctx.Write(m_ExorID);
		// CLAVE para el dupe: muestra el estado del cargo JUSTO cuando la persistencia
		// guarda. Si cargo>0 aca = el barril se guarda con items reales -> al cargar DayZ
		// los anidados caen al piso. Si virt=true/cargo=0 = guardado limpio (sin drop).
		ExorDbg("OnStoreSave (persistencia guardando este barril)");
	}

	override bool OnStoreLoad(ParamsReadContext ctx, int version)
	{
		if (!super.OnStoreLoad(ctx, version))
			return false;
		string id;
		if (!ctx.Read(id))
			return false;
		m_ExorID = id;
		ExorDbg("OnStoreLoad (persistencia cargando este barril; ID leido del disco)");
		return true;
	}

	override void AfterStoreLoad()
	{
		super.AfterStoreLoad();
		// Tras reinicio, avisar al cliente si este barril tiene contenido guardado (JSON).
		// El estado fino (virtualizado real) lo fija ExorReconcileOnLoad en el 1er tick.
		if (GetGame().IsServer())
		{
			m_ExorVirtualizedSync = ExorHasContent();
			SetSynchDirty();
			ExorDbg("AfterStoreLoad (barril ya cargado; cargo=lo que trajo el engine)");
		}
	}

	string ExorGetID()
	{
		if (m_ExorID == "")
		{
			m_ExorID = ExorVO_Serializer.GenerateId();
		}
		return m_ExorID;
	}

	string ExorGetStoragePath()
	{
		return string.Format("%1\\%2.json", ExorStorageConstants.STORAGE_DIR, ExorGetID());
	}

	// ------------------------- registro del self-heal: atado vs en el piso -------------------------
	// PROBLEMA (lo cazo el user): no alcanza con "no registrar los que estan atados". Un barril
	// que estaba EN EL PISO ya quedo registrado; si despues alguien lo mete al slot de barril de
	// un auto y ese auto se virtualiza, el barril se guarda DENTRO del JSON del auto y su entidad
	// se borra -> el self-heal veria el registro viejo, no lo veria vivo, y lo recrearia en la
	// posicion vieja del piso = DOS barriles con el mismo contenido.
	//
	// SOLUCION: seguir el estado. Al ATARSE se da de baja del registro; al volver AL PISO se da
	// de alta con su posicion nueva. Lo maneja el mod solo: no hay que editar ni llevar nada.
	//
	// PERF: corre en el tick del barril, pero solo compara un puntero (GetHierarchyParent) contra
	// el ultimo estado conocido. Toca disco UNICAMENTE en el instante del cambio -> con 700
	// barriles quietos el costo es 700 comparaciones cada 5s y CERO I/O.
	// cuanto se tiene que haber movido un barril para volver a escribir su registro. 1.5m es
	// mas que cualquier reasentado del motor y menos que una reubicacion real.
	static const float EXOR_REG_MOVE_M = 1.5;

	void ExorRegSyncParented()
	{
		if (!GetGame().IsServer())
			return;
		bool atadoAhora = (GetHierarchyParent() != null);

		if (atadoAhora == m_ExorRegParented)
		{
			// sin cambio de estado. Solo queda resolver un alta diferida pendiente.
			if (m_ExorRegPendingTicks > 0)
			{
				m_ExorRegPendingTicks--;
				if (m_ExorRegPendingTicks == 0 && !atadoAhora)
				{
					ExorMuebleRegistry.RegisterBarrel(this);	// ya quieto: guardar su posicion real
					m_ExorRegLastPos = GetPosition();
					m_ExorRegPosInit = true;
				}
				return;
			}

			// SE MOVIO? (el motor lo corrio, quedo reasentado, etc.). Los MUEBLES resuelven
			// esto reescribiendo su registro en CADA arranque; con 700 barriles eso serian
			// 700 escrituras de golpe al arrancar, justo lo que se quiere evitar. Aca se
			// compara contra la posicion con la que quedo escrito -en memoria, sin tocar
			// disco- y se reescribe SOLO si de verdad se movio. Ademas capta movimientos
			// DURANTE la sesion, que el metodo de los muebles no ve hasta el proximo reinicio.
			if (!atadoAhora)
			{
				if (!m_ExorRegPosInit)
				{
					// primer tick: tomar la posicion actual como referencia SIN escribir nada
					// (el registro ya la tiene; si faltara, lo cubre el backfill).
					m_ExorRegLastPos = GetPosition();
					m_ExorRegPosInit = true;
				}
				else if (vector.Distance(m_ExorRegLastPos, GetPosition()) > EXOR_REG_MOVE_M)
				{
					ExorMuebleRegistry.RegisterBarrel(this);
					m_ExorRegLastPos = GetPosition();
					ExorDbg("registro refrescado: el barril se movio");
				}
			}
			return;		// sin cambios: no se toca el disco
		}

		m_ExorRegParented = atadoAhora;
		if (atadoAhora)
		{
			m_ExorRegPendingTicks = 0;						// se lo llevaron: cancelar el alta pendiente
			ExorMuebleRegistry.Unregister(ExorGetID());		// paso a estar adentro de algo -> fuera del registro
		}
		else
		{
			m_ExorRegPendingTicks = 2;						// volvio al piso: dar de alta en 2 ticks (~10s)
			m_ExorRegPosInit = false;						// la referencia de posicion se toma al dar de alta
		}
	}

	// Re-liga el id al recrear el barril desde el registro (self-heal): con el id viejo,
	// ExorRestoreIfNeeded encuentra su JSON y le devuelve TODO el contenido. Solo lo llama
	// ExorMuebleRegistry.ExorRecreate. Ver [[ExorMuebleRegistry]].
	void ExorSetIDForHeal(string id)
	{
		if (id != "")
			m_ExorID = id;
	}

	// virtualizado = items reales sacados del mundo (estan en el JSON). Estado runtime.
	bool ExorIsVirtualized()
	{
		return m_ExorVirt;
	}

	// el barril tiene contenido guardado (JSON en disco), este virtualizado o no
	bool ExorHasContent()
	{
		return FileExist(ExorGetStoragePath());
	}

	// Vuelca el cargo ACTUAL del barril al JSON (los items SIGUEN en el mundo). Es el
	// "guardado en vivo": el JSON queda siempre al dia con item+posicion+anidado.
	void ExorWriteSnapshot()
	{
		GameInventory inv = GetInventory();
		if (!inv)
			return;
		CargoBase cargo = inv.GetCargo();
		if (!cargo)
			return;

		ExorVO_ContainerFile f = new ExorVO_ContainerFile();
		f.id = ExorGetID();
		f.owner_type = GetType();
		int i;
		for (i = 0; i < cargo.GetItemCount(); i++)
		{
			EntityAI it = cargo.GetItem(i);
			if (it)
				f.items.Insert(ExorVO_Serializer.CaptureItem(it));
		}
		// si quedo vacio, borrar el JSON (barril sin contenido = no hay nada que guardar)
		if (f.items.Count() == 0)
		{
			if (FileExist(ExorGetStoragePath()))
				DeleteFile(ExorGetStoragePath());
			ExorDbg("ExorWriteSnapshot: cargo VACIO -> JSON borrado");
		}
		else
		{
			JsonFileLoader<ExorVO_ContainerFile>.JsonSaveFile(ExorGetStoragePath(), f);
			ExorDbg(string.Format("ExorWriteSnapshot: JSON actualizado en vivo con %1 items top-level", f.items.Count()));
		}
		m_ExorSnapDirty = false;
	}

	// ------------------------- abrir / cerrar -------------------------
	override void Open()
	{
		if (GetGame().IsServer())
		{
			ExorCfgStorage settings = GetExorConfig().storage;
			int now = GetGame().GetTime();
			int cdMs = settings.cooldown_abrir_segundos * 1000;
			if (cdMs > 0 && m_ExorLastCloseMs > 0 && now - m_ExorLastCloseMs < cdMs)
			{
				// Anti-dupe: cooldown de reapertura activo
				ExorDbg("Open BLOQUEADO (cooldown de reapertura anti-dupe)");
				return;
			}
			ExorDbg("Open (player abriendo la tapa; va a restaurar si esta virtualizado)");
		}

		// IMPORTANTE: abrir ANTES de restaurar — un barril cerrado rechaza
		// items en su cargo (regla vanilla) y todo caeria al piso
		super.Open();

		if (GetGame().IsServer())
		{
			ExorRestoreIfNeeded();
			m_ExorLastInteractMs = GetGame().GetTime();
			if (ExorStorageConstants.DEBUG_BARRELS)
			{
				int totalCargo;
				int openNow = ExorVO_Manager.CountOpenBarrels(totalCargo);
				Print(string.Format("%1[DBG] Open FIN | id=%2 | barriles ABIERTOS ahora=%3 | items reales totales en abiertos=%4", ExorStorageConstants.LOG, m_ExorID, openNow, totalCargo));
			}
		}
	}

	override void Close()
	{
		super.Close();
		if (GetGame().IsServer())
		{
			int now = GetGame().GetTime();
			m_ExorLastInteractMs = now;
			m_ExorLastCloseMs = now;
			ExorDbg("Close (tapa cerrada; auto-cierre o manual)");
		}
	}

	// ------------------------- virtualizacion (Fase 2) -------------------------
	// Llamado por el manager cada tick
	// Devuelve true si virtualizo en este tick (el manager usa eso para el throttle).
	// allowVirtualize=false -> el barril igual se auto-cierra pero NO virtualiza este tick.
	// El barril todavia no reconcilio su estado tras la carga del mundo. El manager usa
	// esto para repartir los reconciles caros (scan del piso) en varios ticks (anti-pico).
	bool ExorNeedsReconcile()
	{
		return !m_ExorLoadDone;
	}

	// Reconciliar AL CARGAR (1ra vez): el JSON es la VERDAD. Si DayZ cargo items reales
	// (barril no-virtualizado al guardar) o tiro anidados al piso, se limpia todo y el
	// barril queda virtualizado -> se restaura del JSON al abrir. Lo llama el manager
	// (con throttle) o ExorRestoreIfNeeded si un player abre antes de su turno.
	// Devuelve true si hizo trabajo REAL. Ver la nota en ExorStorage_Openable.ExorReconcileNow:
	// un contenedor sin JSON no debe consumir cupo de reconcile del tick.
	bool ExorReconcileNow()
	{
		if (m_ExorLoadDone)
			return false;
		ExorDbg("ExorReconcileNow (1ra reconciliacion tras cargar; JSON manda)");
		m_ExorLoadDone = true;
		if (!ExorHasContent())
			return false;	// barril vacio/legacy: nada que reconciliar, no gastar presupuesto
		ExorReconcileOnLoad();
		return true;
	}

	// allowSnapshot/didSnapshot: el guardado en vivo del JSON escribe a DISCO de forma
	// SINCRONA en el hilo del juego. Para no encadenar muchas escrituras en un mismo tick
	// (hitch en raids con muchos barriles activos a la vez), el manager reparte un cupo de
	// snapshots por tick (MAX_SNAPSHOT_PER_TICK); el barril que no entra escribe el proximo
	// tick (el flag dirty persiste). didSnapshot avisa al manager para descontar el cupo.
	bool ExorTick(int now, ExorCfgStorage settings, bool allowVirtualize, bool allowSnapshot, array<Man> players, out bool didSnapshot)
	{
		didSnapshot = false;
		// El reconcile lo dispara el manager (con presupuesto por tick) o la apertura.
		// Si todavia no reconcilio, no hacer nada este tick (espera su turno).
		if (!m_ExorLoadDone)
			return false;

		// Barril ATTACHED a un SLOT de barril de un vehiculo (no en cargo suelto): mantenerlo
		// ABIERTO para que acepte items (un barril cerrado rechaza el cargo -regla vanilla- y en
		// el vehiculo no hay accion de "abrir"), y NO auto-cerrarlo/virtualizarlo. Si esta en el
		// CARGO del vehiculo (no en un slot), se deja cerrado y NO acepta items (transporte).
		if (ExorIsInVehicleSlot())
		{
			if (!IsOpen())
			{
				m_ExorLastCloseMs = 0;	// sin cooldown de reapertura estando en el slot
				Open();
			}
			return false;
		}

		// Umbrales en ms (config en SEGUNDOS). Recomendado: cerrar 10s, virtualizar 30s.
		int cerrarMs = settings.auto_cerrar_segundos * 1000;
		int virtMs = settings.virtualizar_segundos * 1000;

		// Auto-cierre por CERCANIA del jugador: mientras haya alguien a menos de
		// cerrar_distancia_metros, el barril sigue ABIERTO (lo esta usando, aunque solo
		// ordene su inventario sin mover items del barril). Se cierra recien cerrarMs
		// DESPUES de que el ultimo jugador se aleja. (EECargoIn/Out tambien resetean.)
		if (IsOpen())
		{
			if (ExorVO_Manager.IsAlivePlayerNearList(players, GetPosition(), settings.cerrar_distancia_metros))
				m_ExorLastInteractMs = now;
			// GUARDADO EN VIVO (con cupo del manager): si hubo cambios de cargo, volcar el
			// contenido al JSON. SOLO si NO esta virtualizado (un virtualizado tiene cargo
			// vacio a proposito y su JSON es la verdad -> jamas pisarlo con un vacio).
			if (m_ExorSnapDirty && !m_ExorVirt && allowSnapshot)
			{
				ExorWriteSnapshot();
				didSnapshot = true;
			}
			if (cerrarMs > 0 && now - m_ExorLastInteractMs > cerrarMs)
			{
				ExorDbg("ExorTick -> auto-cierre (nadie cerca por el umbral)");
				Close();
				ExorDbg("auto-cerrado (nadie cerca)");	// rutina: ~483/8h -> a debug
			}
			return false;
		}

		// cerrado: si quedo algo sin volcar, volcarlo (con cupo) antes de virtualizar. Solo si
		// tiene items reales (no virtualizado) -> nunca pisar el JSON con un cargo vacio.
		if (m_ExorSnapDirty && !m_ExorVirt && allowSnapshot)
		{
			ExorWriteSnapshot();
			didSnapshot = true;
		}

		if (!allowVirtualize)	// THROTTLE: sin cupo este tick -> virtualiza en el proximo
			return false;

		// Virtualizacion: cerrado, con contenido, sin interaccion por el umbral. Al
		// virtualizar se escribe el JSON (respaldo) y se sacan los items del mundo ->
		// el barril queda crash-safe (cualquier reinicio restaura del JSON) y aliviana el server.
		if (virtMs <= 0)
			return false;
		if (ExorIsVirtualized())
			return false;
		if (now - m_ExorLastInteractMs < virtMs)
			return false;
		if (ExorCargoCount() == 0)
			return false;

		ExorVirtualize();
		return true;
	}

	// reinicia el timer de inactividad cada vez que entra/sale un item del barril (asi el
	// auto-cierre de 10s mide actividad REAL, no el tiempo desde que se abrio).
	// (Nombres vanilla correctos: EECargoIn/EECargoOut, 1 solo parametro.)
	override void EECargoIn(EntityAI item)
	{
		super.EECargoIn(item);
		if (GetGame() && GetGame().IsServer())
		{
			string it = "?";
			if (item)
				it = item.GetType();
			if (m_ExorRestoring)
				ExorDbg("EECargoIn (RESTAURANDO, no marca dirty) item=" + it);
			else
			{
				m_ExorLastInteractMs = GetGame().GetTime();
				m_ExorSnapDirty = true;	// cambio REAL del player -> volcar al JSON (throttle en el tick)
				ExorDbg("EECargoIn (player METIO item) item=" + it);
			}
		}
	}

	override void EECargoOut(EntityAI item)
	{
		super.EECargoOut(item);
		if (GetGame() && GetGame().IsServer())
		{
			string it = "?";
			if (item)
				it = item.GetType();
			if (m_ExorRestoring)
				ExorDbg("EECargoOut (RESTAURANDO/virtualizando, no marca dirty) item=" + it);
			else
			{
				m_ExorLastInteractMs = GetGame().GetTime();
				m_ExorSnapDirty = true;
				ExorDbg("EECargoOut (player SACO item) item=" + it);
			}
		}
	}

	int ExorCargoCount()
	{
		GameInventory inv = GetInventory();
		if (!inv)
			return 0;
		CargoBase cargo = inv.GetCargo();
		if (!cargo)
			return 0;
		return cargo.GetItemCount();
	}

	// Saca los items reales del mundo. El JSON ya esta al dia (guardado en vivo), pero
	// lo reescribimos por las dudas antes de borrar (crash-safe). El JSON NO se borra:
	// es la verdad permanente del barril hasta que lo levanten.
	void ExorVirtualize()
	{
		GameInventory inv = GetInventory();
		if (!inv)
			return;
		CargoBase cargo = inv.GetCargo();
		if (!cargo)
			return;

		array<EntityAI> toDelete = new array<EntityAI>;
		int i;
		for (i = 0; i < cargo.GetItemCount(); i++)
		{
			EntityAI it = cargo.GetItem(i);
			if (it)
				toDelete.Insert(it);
		}

		// SOLO reescribir el JSON si el PLAYER cambio algo (dirty). Si no cambio nada (recien
		// restaurado), el JSON YA es la verdad -> NO reescribir. ASI un restore donde un item
		// no entro (cayo afuera) NUNCA achica el JSON -> el loot nunca se pierde, se reintenta.
		if (m_ExorSnapDirty)
			ExorWriteSnapshot();

		if (toDelete.Count() == 0)
			return;

		ExorDbg(string.Format("ExorVirtualize INICIO: sacando %1 items del mundo al JSON", toDelete.Count()));
		// guard: los EECargoOut del borrado NO son cambios del player -> no marcar dirty.
		m_ExorRestoring = true;
		for (i = 0; i < toDelete.Count(); i++)
			GetGame().ObjectDelete(toDelete.Get(i));
		m_ExorRestoring = false;

		m_ExorVirt = true;
		m_ExorVirtualizedSync = true;
		SetSynchDirty();
		ExorDbg(string.Format("virtualizado: %1 items sacados del mundo", toDelete.Count()));	// rutina: ~1240/8h -> a debug
		ExorDbg("ExorVirtualize FIN (barril virtualizado, cargo vacio en el mundo)");
	}

	// Se llama al ABRIR. Recrea los items reales desde el JSON, o migra un barril viejo.
	// Reintento del restore cuando el turno estaba tomado por otro contenedor.
	void ExorRestoreRetry()
	{
		if (!m_ExorVirt)
			return;	// ya lo restauro otro camino
		if (!ExorVO_Manager.CanRestoreNow())
		{
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorRestoreRetry, ExorVO_Manager.RESTORE_SPACING_MS, false);
			return;
		}
		ExorDoRestore();
	}

	void ExorRestoreIfNeeded()
	{
		// si abren ANTES de su turno de reconcile (throttle del manager), forzar la
		// reconciliacion ahora (si no, m_ExorVirt seguiria false y el barril se veria
		// vacio aunque el JSON tenga el loot).
		bool justReconciled = !m_ExorLoadDone;	// capturar ANTES (ExorReconcileNow lo pone true)
		ExorReconcileNow();

		// virtualizado -> recrear los items desde el JSON (que QUEDA en disco)
		if (m_ExorVirt)
		{
			// BUG "se ve 1/N item + se lagea" (bases grandes): si el player abre ANTES del
			// tick de reconcile, ExorReconcileNow ACABA de borrar el cargo stale del engine
			// con GetGame().ObjectDelete() -> que en DayZ es DIFERIDO (corre al fin del frame).
			// El cargo TODAVIA tiene esos items ocupando las casillas, asi que restaurar en el
			// MISMO frame falla: los items del JSON no entran (casilla ocupada) y queda ~1/N.
			// -> diferir el restore un instante para que el motor libere los slots primero.
			// (El restore por un tick ya-reconciliado entra aca con el cargo vacio y va directo.)
			if (justReconciled && ExorCargoCount() > 0)
			{
				ExorDbg("ExorRestoreIfNeeded: cargo stale pendiente de borrar (delete diferido) -> restore DIFERIDO 300ms");
				GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorDoRestore, 300, false);
				return;
			}
			// SERIALIZAR: no arrancar dos restores en el mismo frame (cada uno crea y
			// reubica cientos de entidades). Ver ExorVO_Manager.CanRestoreNow.
			if (!ExorVO_Manager.CanRestoreNow())
			{
				GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorRestoreRetry, ExorVO_Manager.RESTORE_SPACING_MS, false);
				return;
			}
			ExorDbg("ExorRestoreIfNeeded: esta virtualizado -> restaurar del JSON");
			ExorDoRestore();
			return;
		}

		// LEGACY: barril desplegado ANTES de esta feature -> tiene items reales y NO json.
		// Al abrirlo, crear el JSON con su contenido actual (lo migra al nuevo modelo).
		if (!ExorHasContent() && ExorCargoCount() > 0)
		{
			ExorWriteSnapshot();
			Print(string.Format("%1 Barril %2: migrado al abrir (JSON creado con %3 items)", ExorStorageConstants.LOG, ExorGetID(), ExorCargoCount()));
			ExorDbg("ExorRestoreIfNeeded: barril LEGACY migrado a JSON al abrir");
		}
		else
			ExorDbg("ExorRestoreIfNeeded: nada que hacer (no virt, sin contenido o ya con items reales)");
	}

	void ExorDoRestore()
	{
		string path = ExorGetStoragePath();
		if (!FileExist(path))
		{
			m_ExorVirt = false;
			return;
		}

		ExorVO_ContainerFile f = new ExorVO_ContainerFile();
		JsonFileLoader<ExorVO_ContainerFile>.JsonLoadFile(path, f);

		// ANTI-DUPE: SOLO en la 1ra apertura tras el arranque, limpiar lo que DayZ tiro al piso
		// por "invalid location" al cargar (contenido de mochilas anidadas). En el reconcile del
		// 1er tick esos items todavia "colgaban" de la mochila (no sueltos) y la limpieza los
		// erraba; al abrir ya estan asentados -> aca SI se borran, antes de recrearlos del JSON.
		// Se hace UNA sola vez (flag): los drops son un fenomeno del CARGADO, no de cada apertura
		// -> asi nunca borramos un bolso/loot que el player deje tirado al lado a proposito.
		// El spill de "invalid location" es un fenomeno del ARRANQUE del server (DayZ
		// tira los anidados al cargar el mundo). Por eso esta limpieza SOLO corre si la
		// 1ra apertura ocurre poco despues del arranque. Si el barril se abre por 1ra vez
		// horas despues (durante un PvP en un town), NO se barre el piso: antes borrabamos
		// cualquier item suelto de tipo coincidente a 10m -> se comia un arma que un player
		// acababa de tirar al lado (bug: VSS despawneado en PvP). GetGame().GetTime() = ms
		// desde el arranque de la mision; los spills reales ya se limpiaron en el reconcile
		// de carga + su reintento a los 8s, muy dentro de esta ventana.
		int floorCleanWindowMs = 300000;	// 5 min tras el arranque
		if (!m_ExorFloorCleaned)
		{
			m_ExorFloorCleaned = true;
			if (GetGame().GetTime() < floorCleanWindowMs)
			{
				int dropped = ExorCleanDroppedNearby();
				if (dropped > 0)
					Print(string.Format("%1 Barril %2: %3 items del piso (invalid location de DayZ) borrados antes de restaurar", ExorStorageConstants.LOG, ExorGetID(), dropped));
			}
		}

		// armar EN LA POSICION DEL BARRIL (NO bajo tierra). Crear los items a -1000m hacia
		// que el motor los limpie por "out-of-bounds" ANTES de que entren al cargo -> se perdia
		// la mayoria del loot (quedaba ~1 item). Se arman en el barril y se mueven al cargo en
		// el mismo frame (igual que la version que funcionaba). Puede haber un parpadeo minimo;
		// es preferible a perder el contenido.
		vector hidden = GetPosition();
		// m_ExorRestoring: los EECargoIn de la restauracion NO marcan dirty -> el JSON no se
		// reescribe por el restore (solo por cambios REALES del player despues).
		ExorDbg(string.Format("ExorDoRestore INICIO: recreando %1 items del JSON al barril", f.items.Count()));
		// GUARD: podar de la data guardada lo imposible de restaurar (classname fantasma,
		// cargador que no calza en su arma) -> no crear entidades a medio armar que despues
		// se persisten y tumban el arranque. Ver ExorVO_Serializer.Sanitize.
		int podados = ExorVO_Serializer.Sanitize(f.items, "", false);
		if (podados > 0)
			Print(string.Format("%1 GUARD: barril %2 -> %3 item(s) corruptos descartados antes de restaurar", ExorStorageConstants.LOG, ExorGetID(), podados));
		m_ExorRestoring = true;
		ExorVO_Serializer.RestoreItemsBigFirst(f.items, this, hidden);
		m_ExorRestoring = false;

		// el JSON NO se borra (es la verdad permanente). m_ExorVirt=false evita re-restaurar.
		m_ExorVirt = false;
		m_ExorVirtualizedSync = false;
		SetSynchDirty();
		// LOG SOLO SI HAY DISCREPANCIA. La restauracion correcta era ~1248 lineas por sesion
		// de 8h (I/O sincrona en el hilo del juego, por evento de rutina). Lo que importa
		// forensemente es cuando el cargo real NO coincide con el JSON = se perdio loot;
		// ese caso sigue gritando en el RPT. grep "3xorVO PERDIDA"
		int realCount = ExorCargoCount();
		int espCount = f.items.Count();
		if (realCount != espCount)
			Print(string.Format("%1 PERDIDA Barril %2 restaurado INCOMPLETO: %3/%4 items (real/esperado)", ExorStorageConstants.LOG, ExorGetID(), realCount, espCount));
		ExorDbg(string.Format("ExorDoRestore FIN: cargo real ahora = %1 (esperado %2)", realCount, espCount));
	}

	// RECONCILIAR AL CARGAR: el JSON manda. Si DayZ cargo items reales (barril no
	// virtualizado al guardar) y/o tiro anidados al piso, se limpia TODO y el barril
	// queda virtualizado -> se restaura del JSON (intacto, posiciones exactas) al abrir.
	void ExorReconcileOnLoad()
	{
		if (!ExorHasContent())	// sin JSON -> barril viejo/vacio, no hay nada que reconciliar
		{
			ExorDbg("ExorReconcileOnLoad: SIN JSON (barril vacio/legacy) -> no reconcilia");
			return;
		}
		ExorDbg("ExorReconcileOnLoad INICIO (tiene JSON; el cargo del engine se considera stale)");

		// 1) borrar lo que DayZ haya cargado en el cargo (stale; el JSON es la verdad)
		int cleared = 0;
		GameInventory inv = GetInventory();
		if (inv)
		{
			CargoBase cargo = inv.GetCargo();
			if (cargo)
			{
				array<EntityAI> stale = new array<EntityAI>;
				int s;
				for (s = 0; s < cargo.GetItemCount(); s++)
				{
					EntityAI se = cargo.GetItem(s);
					if (se)
						stale.Insert(se);
				}
				cleared = stale.Count();
				for (s = 0; s < cleared; s++)
					GetGame().ObjectDelete(stale.Get(s));
			}
		}

		// 2) limpiar los items que el engine tiro al PISO (anidados que no pudo recolocar)
		//    pegados al barril y cuyo tipo esta en el JSON -> si no, al restaurar dupearian.
		//    Solo si habia items reales (barril no-virtualizado al guardar); un barril
		//    virtualizado limpio cargo vacio y nunca tira nada al piso -> no hace falta escanear.
		int dropped = 0;
		if (cleared > 0)
		{
			dropped = ExorCleanDroppedNearby();
			// reintento diferido: algunos drops tardan en asentarse tras la carga del mundo.
			// SOLO si el 1er barrido encontro algo: si el piso estaba limpio, repetir el
			// scan de 15m + re-parseo del JSON por cada barril es trabajo puro de mas.
			if (dropped > 0)
				GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorCleanDroppedRetry, 8000, false);
		}

		m_ExorVirt = true;	// queda virtualizado; se restaura al abrir
		m_ExorSnapDirty = false;	// el borrado de stale marco "sucio" de forma espuria -> limpiar
		m_ExorVirtualizedSync = true;
		SetSynchDirty();
		if (cleared > 0 || dropped > 0)
			Print(string.Format("%1 Barril %2: reconciliado al cargar (cargo stale=%3, piso=%4 borrados; el JSON manda)", ExorStorageConstants.LOG, ExorGetID(), cleared, dropped));
	}

	// Reintento DIFERIDO de la limpieza del piso. DayZ tira los anidados al piso DURANTE
	// la carga del mundo; a veces tardan en "asentarse" como objetos sueltos -> se reintenta
	// unos segundos despues del reconcile para barrer los que el 1er pase no agarro.
	void ExorCleanDroppedRetry()
	{
		int dropped = ExorCleanDroppedNearby();
		if (dropped > 0)
			Print(string.Format("%1 Barril %2: %3 items del piso borrados (reintento diferido)", ExorStorageConstants.LOG, ExorGetID(), dropped));
	}

	// Borra items SUELTOS en el piso cerca del barril cuyo tipo esta en el JSON = lo que
	// DayZ tiro por "invalid location" al cargar (contenido anidado de mochilas). Conservador
	// para no tocar loot legitimo: solo items sin parent, de un tipo que el barril guarda, y
	// CERCA en HORIZONTAL. La distancia se mide en el plano XZ (ignora la altura) porque el
	// barril suele estar ELEVADO sobre un piso de base y los drops caen al terreno varios
	// metros ABAJO -> con un radio esferico chico (2m) quedaban afuera y no se borraban (bug
	// del dupe). scanRadius grande para que la esfera ALCANCE el suelo aunque este elevado;
	// maxHoriz chico para no borrar loot que un player dejo tirado lejos a proposito.
	// Extrae TODOS los classnames del JSON leyendo el TEXTO crudo (busca cada `"type": "X"`).
	// Robusto: NO depende de la deserializacion de JsonFileLoader+CollectTypes, que cargaba
	// solo el 1er tipo (los items anidados quedaban medio rotos en ese load) -> el scan no
	// matcheaba nada. Asi juntamos cada tipo top-level Y anidado (latas en mochilas, etc.).
	TStringArray ExorTypesFromJsonText(string path)
	{
		TStringArray result = new TStringArray;
		// dedup con map en vez de result.Find() lineal: ver la nota en ExorStorage_Openable.
		map<string, bool> seen = new map<string, bool>;
		FileHandle fh = OpenFile(path, FileMode.READ);
		if (fh == 0)
			return result;
		string line;
		while (FGets(fh, line) >= 0)
		{
			int kp = line.IndexOf("\"type\"");
			if (kp < 0)
				continue;
			string rest = line.Substring(kp + 6, line.Length() - (kp + 6));	// despues de "type"
			int q1 = rest.IndexOf("\"");
			if (q1 < 0)
				continue;
			string rest2 = rest.Substring(q1 + 1, rest.Length() - (q1 + 1));	// despues de la 1ra comilla
			int q2 = rest2.IndexOf("\"");
			if (q2 < 0)
				continue;
			string val = rest2.Substring(0, q2);
			if (val != "" && !seen.Contains(val))
			{
				seen.Set(val, true);
				result.Insert(val);
			}
		}
		CloseFile(fh);
		return result;
	}

	int ExorCleanDroppedNearby(float scanRadius = 15.0, float maxHoriz = 10.0)
	{
		string path = ExorGetStoragePath();
		if (!FileExist(path))
			return 0;
		// tipos desde el TEXTO crudo del JSON (la deserializacion solo traia el 1er tipo).
		TStringArray types = ExorTypesFromJsonText(path);
		if (types.Count() == 0)
			return 0;

		vector bpos = GetPosition();
		array<Object> nearby = new array<Object>;
		array<CargoBase> proxy = new array<CargoBase>;
		GetGame().GetObjectsAtPosition(bpos, scanRadius, nearby, proxy);

		int removed = 0;
		int i;
		for (i = 0; i < nearby.Count(); i++)
		{
			EntityAI e = EntityAI.Cast(nearby.Get(i));
			if (!e || e == this)
				continue;
			if (e.GetHierarchyParent())	// solo items SUELTOS (no dentro de un inventario)
				continue;
			if (!e.IsInherited(ItemBase))
				continue;
			if (types.Find(e.GetType()) < 0)	// solo tipos que este barril guarda
				continue;
			// distancia HORIZONTAL (plano XZ): ignora la altura -> agarra los drops aunque
			// el barril este elevado sobre un piso de base (caen al terreno mas abajo).
			vector d = e.GetPosition() - bpos;
			d[1] = 0;
			if (d.Length() > maxHoriz)
				continue;
			GetGame().ObjectDelete(e);
			removed++;
		}
		return removed;
	}

	// ------------------------- reglas de guardado (Fase 3) -------------------------
	override bool CanReceiveItemIntoCargo(EntityAI item)
	{
		if (GetGame().IsServer() && item)
		{
			if (GetExorConfig().storage.blacklist.Find(item.GetType()) != -1)
				return false;
		}
		return super.CanReceiveItemIntoCargo(item);
	}

	// No se puede TOMAR EN LA MANO un barril CON contenido (igual que empaquetar): hay que
	// vaciarlo primero. Un barril VACIO si se puede tomar/mover/empaquetar normalmente.
	// Evita robar/mover un barril lleno y los casos raros de mover loot virtualizado.
	override bool CanPutIntoHands(EntityAI parent)
	{
		// m_ExorVirtualizedSync esta sincronizado -> el cliente tambien oculta la accion.
		if (m_ExorVirtualizedSync)
			return false;
		// red de seguridad en server: cualquier contenido guardado (JSON) bloquea, este
		// virtualizado o no (la ventana breve cerrado-con-items-reales).
		if (GetGame().IsServer() && ExorHasContent())
			return false;
		return super.CanPutIntoHands(parent);
	}

	// No lockeable: bloquea CodeLock / candados de cualquier mod
	override bool CanReceiveAttachment(EntityAI attachment, int slotId)
	{
		if (attachment)
		{
			string type = attachment.GetType();
			type.ToLower();
			if (type.Contains("codelock") || type.Contains("combinationlock") || type.Contains("padlock") || type.Contains("lock"))
				return false;
		}
		return super.CanReceiveAttachment(attachment, slotId);
	}

	// ------------------------- empaquetado (Fase 1) -------------------------
	// Solo si esta cerrado, sano, sin items adentro Y SIN contenido virtualizado.
	// El liquido (agua de lluvia) NO bloquea: se descarta al empaquetar.
	bool ExorCanBePacked()
	{
		if (IsOpen())
			return false;
		if (IsRuined())
			return false;
		// CRITICO: un barril virtualizado parece vacio pero su loot esta en
		// disco; empaquetarlo perderia el contenido. La variable sincronizada
		// hace que el CLIENTE tampoco muestre la accion
		if (m_ExorVirtualizedSync)
			return false;
		// el server bloquea si hay CUALQUIER contenido guardado (JSON), este virtualizado
		// o no -> empaquetar perderia ese loot
		if (GetGame().IsServer() && ExorHasContent())
			return false;
		if (GetInventory())
		{
			if (GetInventory().AttachmentCount() > 0)
				return false;
			CargoBase cargo = GetInventory().GetCargo();
			if (cargo && cargo.GetItemCount() > 0)
				return false;
		}
		return true;
	}

	string ExorGetPackedType()
	{
		return "";
	}
}

class Exor_Barrel_500 : Exor_Barrel_Base
{
	override string ExorGetPackedType()
	{
		return "Exor_Barrel_500_Packed";
	}
}

// ---------------------------------------------------------------------------
// Barril empaquetado (item transportable, sin cargo). Se despliega con accion.
// ---------------------------------------------------------------------------
class Exor_Barrel_Packed_Base : ItemBase
{
	// La caja tambien es indestructible (consistente con el barril)
	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			SetAllowDamage(false);
		}
	}

	override void SetActions()
	{
		super.SetActions();
		AddAction(ExorActionDeployBarrel);
	}

	string ExorGetDeployedType()
	{
		return "";
	}
}

class Exor_Barrel_500_Packed : Exor_Barrel_Packed_Base
{
	override string ExorGetDeployedType()
	{
		return "Exor_Barrel_500";
	}
}
