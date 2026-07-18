// ============================================================================
// 3xor_Vanilla_Optimization - MUEBLE ABRIBLE + VIRTUALIZABLE (base reutilizable)
// ----------------------------------------------------------------------------
// Base para muebles de guardado (nevera, y los proximos muebles). Combina:
//   1) ABRIR/CERRAR LIMPIO (estilo MMG Container_Base): estado propio net-sync
//      (m_IsOpened) + Lock/UnlockInventory + animacion de puerta ligada a ESE
//      estado. Nunca se desincroniza la accion Abrir/Cerrar de la puerta/inventario.
//   2) VIRTUALIZACION (portada del barril 3xor, battle-tested): el contenido vive
//      en un JSON (la VERDAD); al cerrarse + alejarse el jugador + pasar el tiempo
//      configurado, los items reales se SACAN del mundo (aliviana el server, crash-safe)
//      y se RECREAN al abrir. Reusa la MISMA config que el barril (cfg.storage:
//      cerrar_distancia_metros / auto_cerrar_segundos / virtualizar_segundos).
//
// Las subclases (ej Exor_Fridge) solo definen: filtro de cargo (ExorCanStore),
// tipo empacado (ExorGetPackedType) y extras (bateria, etc.). NO tocan la
// virtualizacion ni el abrir/cerrar.
// ============================================================================

class Exor_OpenableStorage : Container_Base
{
	// ---- estado ABIERTO/CERRADO ----
	protected bool m_IsOpened;			// net-sync: puerta abierta / inventario accesible
	protected bool m_DoorAnimApplied;	// ultimo estado aplicado a la animacion
	protected bool m_DoorAnimInit;

	// ---- virtualizacion (portado del barril) ----
	protected string m_ExorID;			// ID persistente (liga el mueble con su JSON)
	protected int  m_ExorLastInteractMs;
	protected int  m_ExorLastCloseMs;
	protected bool m_ExorVirtualizedSync;	// net-sync: tiene contenido guardado en disco
	protected bool m_ExorVirt;			// los items reales estan SACADOS del mundo (en el JSON)
	protected bool m_ExorSnapDirty;		// hubo cambios de cargo sin volcar al JSON
	protected bool m_ExorLoadDone;		// ya se reconcilio JSON vs persistencia tras cargar
	protected bool m_ExorRestoring;		// recreando items DESDE el JSON (no marcar dirty)
	protected bool m_ExorFloorCleaned;	// ya se limpio el piso (drops de DayZ) tras el arranque

	void Exor_OpenableStorage()
	{
		RegisterNetSyncVariableBool("m_IsOpened");
		RegisterNetSyncVariableBool("m_ExorVirtualizedSync");
	}

	// ---- HOOKS que las subclases pueden override ----
	// nombre del source de animacion de la puerta (en model.cfg + AnimationSources)
	string ExorGetDoorAnimSource() { return "Lid"; }
	// filtro de cargo de la subclase (ej: solo comida). Default: acepta todo.
	bool ExorCanStore(EntityAI item) { return true; }
	// tipo del item empacado (para re-empaquetar). "" = no empaquetable.
	string ExorGetPackedType() { return ""; }

	void ExorDbg(string ev)
	{
		if (!ExorStorageConstants.DEBUG_BARRELS)
			return;
		Print(string.Format("%1[DBG-mueble] %2 | id=%3 pos=%4 cargo=%5 open=%6 virt=%7", ExorStorageConstants.LOG, ev, m_ExorID, GetPosition(), ExorCargoCount(), m_IsOpened, m_ExorVirt));
	}

	// ======================= INIT / PERSISTENCIA =======================
	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			SetAllowDamage(false);		// indestructible: el loot nunca se pierde
			m_ExorLastInteractMs = GetGame().GetTime();
			m_ExorLastCloseMs = 0;
			m_IsOpened = false;			// arranca CERRADA
			if (GetInventory())
				GetInventory().LockInventory(HIDE_INV_FROM_SCRIPT);
			SetSynchDirty();
			ExorVO_Manager.RegisterOpenable(this);
			ExorDbg("EEInit");
		}
		ExorUpdateDoorAnim();
	}

	override void EEDelete(EntityAI parent)
	{
		if (GetGame().IsServer())
			ExorVO_Manager.UnregisterOpenable(this);
		super.EEDelete(parent);
	}

	override void OnStoreSave(ParamsWriteContext ctx)
	{
		super.OnStoreSave(ctx);
		ctx.Write(m_ExorID);
	}

	override bool OnStoreLoad(ParamsReadContext ctx, int version)
	{
		if (!super.OnStoreLoad(ctx, version))
			return false;
		string id;
		if (!ctx.Read(id))
			return false;
		m_ExorID = id;
		return true;
	}

	override void AfterStoreLoad()
	{
		super.AfterStoreLoad();
		if (GetGame().IsServer())
		{
			// carga CERRADA + inventario bloqueado; el reconcile (JSON manda) corre en el 1er tick.
			m_IsOpened = false;
			if (GetInventory())
				GetInventory().LockInventory(HIDE_INV_FROM_SCRIPT);
			m_ExorVirtualizedSync = ExorHasContent();
			SetSynchDirty();
		}
		ExorUpdateDoorAnim();
	}

	// ======================= ABRIR / CERRAR =======================
	override void Open()
	{
		m_IsOpened = true;
		SetSynchDirty();
		if (GetInventory())
			GetInventory().UnlockInventory(HIDE_INV_FROM_SCRIPT);	// abierta = accesible
		if (GetGame().IsServer())
		{
			ExorRestoreIfNeeded();		// recrear items del JSON si estaba virtualizado
			m_ExorLastInteractMs = GetGame().GetTime();
		}
		ExorUpdateDoorAnim();
	}

	override void Close()
	{
		m_IsOpened = false;
		SetSynchDirty();
		if (GetInventory())
			GetInventory().LockInventory(HIDE_INV_FROM_SCRIPT);
		if (GetGame().IsServer())
		{
			int now = GetGame().GetTime();
			m_ExorLastInteractMs = now;
			m_ExorLastCloseMs = now;
		}
		ExorUpdateDoorAnim();
	}

	override bool IsOpen()
	{
		return m_IsOpened;
	}

	override void OnVariablesSynchronized()
	{
		super.OnVariablesSynchronized();
		ExorUpdateDoorAnim();
	}

	// Anima la puerta segun el estado. Solo (re)aplica cuando CAMBIA (evita re-animar
	// en cada sync). El source lo da ExorGetDoorAnimSource() (default "Lid").
	void ExorUpdateDoorAnim()
	{
		bool open = IsOpen();
		if (m_DoorAnimInit && open == m_DoorAnimApplied)
			return;
		m_DoorAnimInit = true;
		m_DoorAnimApplied = open;
		float phase = 0.0;
		if (open)
			phase = 1.0;
		SetAnimationPhase(ExorGetDoorAnimSource(), phase);
	}

	// ======================= FILTRO / REGLAS DE CARGO =======================
	override bool CanReceiveItemIntoCargo(EntityAI item)
	{
		if (m_ExorRestoring)	// restaurando del JSON -> permitir siempre (nunca perder loot)
			return super.CanReceiveItemIntoCargo(item);
		if (!IsOpen())			// cerrado -> no acepta
			return false;
		if (item && !ExorCanStore(item))	// filtro de la subclase
			return false;
		return super.CanReceiveItemIntoCargo(item);
	}

	override bool CanReleaseCargo(EntityAI cargo)
	{
		if (m_ExorRestoring)
			return true;
		return IsOpen();
	}

	// El mueble desplegado NO se levanta a la mano ni entra en otro contenedor si tiene
	// contenido (se re-empaca vacio con la accion). Vacio se podria, pero por diseno de
	// mueble lo bloqueamos siempre (se mueve re-empacando).
	override bool CanPutInCargo(EntityAI parent) { return false; }
	override bool CanPutIntoHands(EntityAI parent) { return false; }

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

	override bool IsHeavyBehaviour() { return true; }
	override bool IsTwoHandedBehaviour() { return true; }

	// ======================= ACCIONES =======================
	override void SetActions()
	{
		super.SetActions();
		AddAction(ExorActionOpenCloseFridge);
		// El empaque va en el DESTORNILLADOR (ExorFridge_Screwdriver.c): con el
		// destornillador en la mano y mirando el mueble aparece "Empaquetar".
	}

	// ======================= EMPAQUE =======================
	// Empaquetable si esta CERRADO, sano, sin comida adentro y sin contenido
	// virtualizado (empaquetar eso lo perderia). La BATERIA (attachment) NO bloquea:
	// se suelta al piso al empaquetar (ver ExorActionPackFridge).
	bool ExorCanBePacked()
	{
		if (IsOpen())
			return false;
		if (IsDamageDestroyed())
			return false;
		if (m_ExorVirtualizedSync)	// contenido virtualizado en disco -> no perderlo
			return false;
		if (GetInventory())
		{
			CargoBase cargo = GetInventory().GetCargo();
			if (cargo && cargo.GetItemCount() > 0)	// comida real adentro
				return false;
		}
		return true;
	}

	// ======================= VIRTUALIZACION (portado del barril) =======================
	string ExorGetID()
	{
		if (m_ExorID == "")
			m_ExorID = ExorVO_Serializer.GenerateId();
		return m_ExorID;
	}

	string ExorGetStoragePath()
	{
		return string.Format("%1\\%2.json", ExorStorageConstants.STORAGE_DIR, ExorGetID());
	}

	bool ExorIsVirtualized() { return m_ExorVirt; }
	bool ExorHasContent()    { return FileExist(ExorGetStoragePath()); }

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

	// el manager pregunta esto para repartir el reconcile de arranque (scan caro)
	bool ExorNeedsReconcile() { return !m_ExorLoadDone; }

	void ExorReconcileNow()
	{
		if (m_ExorLoadDone)
			return;
		m_ExorLoadDone = true;
		ExorReconcileOnLoad();
	}

	// Vuelca el cargo ACTUAL al JSON (los items SIGUEN en el mundo). Guardado "en vivo".
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
		if (f.items.Count() == 0)
		{
			if (FileExist(ExorGetStoragePath()))
				DeleteFile(ExorGetStoragePath());
		}
		else
		{
			JsonFileLoader<ExorVO_ContainerFile>.JsonSaveFile(ExorGetStoragePath(), f);
		}
		m_ExorSnapDirty = false;
	}

	// Saca los items reales del mundo (quedan en el JSON, la verdad permanente).
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

		// solo reescribir si el player cambio algo (dirty); si no, el JSON ya es la verdad.
		if (m_ExorSnapDirty)
			ExorWriteSnapshot();

		if (toDelete.Count() == 0)
			return;

		m_ExorRestoring = true;	// los EECargoOut del borrado no marcan dirty
		for (i = 0; i < toDelete.Count(); i++)
			GetGame().ObjectDelete(toDelete.Get(i));
		m_ExorRestoring = false;

		m_ExorVirt = true;
		m_ExorVirtualizedSync = true;
		SetSynchDirty();
		ExorDbg(string.Format("virtualizado (%1 items al JSON)", toDelete.Count()));
	}

	// Se llama al ABRIR: recrea los items reales desde el JSON (o migra un mueble viejo).
	void ExorRestoreIfNeeded()
	{
		bool justReconciled = !m_ExorLoadDone;
		ExorReconcileNow();

		if (m_ExorVirt)
		{
			// si abren ANTES del tick de reconcile, el borrado del cargo stale es DIFERIDO
			// (fin de frame) -> diferir el restore un instante para que se liberen los slots.
			if (justReconciled && ExorCargoCount() > 0)
			{
				GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorDoRestore, 300, false);
				return;
			}
			ExorDoRestore();
			return;
		}

		// LEGACY: mueble con items reales y sin JSON -> crear el JSON al abrir.
		if (!ExorHasContent() && ExorCargoCount() > 0)
			ExorWriteSnapshot();
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

		// anti-dupe: SOLO en la 1ra apertura poco despues del arranque, limpiar lo que DayZ
		// tiro al piso por "invalid location" al cargar (contenido anidado). Ventana de 5 min.
		int floorCleanWindowMs = 300000;
		if (!m_ExorFloorCleaned)
		{
			m_ExorFloorCleaned = true;
			if (GetGame().GetTime() < floorCleanWindowMs)
				ExorCleanDroppedNearby();
		}

		vector hidden = GetPosition();
		m_ExorRestoring = true;
		ExorVO_Serializer.RestoreItemsBigFirst(f.items, this, hidden);
		m_ExorRestoring = false;

		m_ExorVirt = false;
		m_ExorVirtualizedSync = false;
		SetSynchDirty();

		// hook para la subclase (ej: la nevera pone la comida fria si tiene bateria, o
		// la envejece por el tiempo que estuvo virtualizada sin bateria).
		ExorOnItemsRestored(f);
		ExorDbg(string.Format("restaurado (%1/%2 items real/esperado)", ExorCargoCount(), f.items.Count()));
	}

	// hook: la subclase ajusta los items recien recreados. Corre en SERVER tras el restore.
	// 'f' trae los datos del JSON (incl. metadata que la subclase haya guardado).
	void ExorOnItemsRestored(ExorVO_ContainerFile f) {}

	// RECONCILIAR AL CARGAR: el JSON manda. Limpia el cargo stale del engine + los drops
	// del piso, y deja el mueble virtualizado (se restaura del JSON al abrir).
	void ExorReconcileOnLoad()
	{
		if (!ExorHasContent())
			return;

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

		int dropped = 0;
		if (cleared > 0)
		{
			dropped = ExorCleanDroppedNearby();
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorCleanDroppedRetry, 8000, false);
		}

		m_ExorVirt = true;
		m_ExorSnapDirty = false;
		m_ExorVirtualizedSync = true;
		SetSynchDirty();
	}

	void ExorCleanDroppedRetry()
	{
		ExorCleanDroppedNearby();
	}

	// extrae los classnames del JSON leyendo el TEXTO crudo (busca cada "type": "X")
	TStringArray ExorTypesFromJsonText(string path)
	{
		TStringArray result = new TStringArray;
		FileHandle fh = OpenFile(path, FileMode.READ);
		if (fh == 0)
			return result;
		string line;
		while (FGets(fh, line) >= 0)
		{
			int kp = line.IndexOf("\"type\"");
			if (kp < 0)
				continue;
			string rest = line.Substring(kp + 6, line.Length() - (kp + 6));
			int q1 = rest.IndexOf("\"");
			if (q1 < 0)
				continue;
			string rest2 = rest.Substring(q1 + 1, rest.Length() - (q1 + 1));
			int q2 = rest2.IndexOf("\"");
			if (q2 < 0)
				continue;
			string val = rest2.Substring(0, q2);
			if (val != "" && result.Find(val) < 0)
				result.Insert(val);
		}
		CloseFile(fh);
		return result;
	}

	// borra items SUELTOS cerca (plano XZ) cuyo tipo esta en el JSON = drops de "invalid
	// location". Conservador: solo items sin parent, del tipo guardado, cerca en horizontal.
	int ExorCleanDroppedNearby(float scanRadius = 15.0, float maxHoriz = 10.0)
	{
		string path = ExorGetStoragePath();
		if (!FileExist(path))
			return 0;
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
			if (e.GetHierarchyParent())
				continue;
			if (!e.IsInherited(ItemBase))
				continue;
			if (types.Find(e.GetType()) < 0)
				continue;
			vector d = e.GetPosition() - bpos;
			d[1] = 0;
			if (d.Length() > maxHoriz)
				continue;
			GetGame().ObjectDelete(e);
			removed++;
		}
		return removed;
	}

	// ======================= TICK (auto-cierre + virtualizar) =======================
	// Lo llama el manager (con presupuesto por tick). Devuelve true si virtualizo.
	bool ExorTick(int now, ExorCfgStorage settings, bool allowVirtualize, bool allowSnapshot, array<Man> players, out bool didSnapshot)
	{
		didSnapshot = false;
		if (!m_ExorLoadDone)	// espera su turno de reconcile
			return false;

		int cerrarMs = settings.auto_cerrar_segundos * 1000;
		int virtMs = settings.virtualizar_segundos * 1000;

		if (IsOpen())
		{
			// mientras haya alguien cerca, sigue "en uso" (no auto-cierra)
			if (ExorVO_Manager.IsAlivePlayerNearList(players, GetPosition(), settings.cerrar_distancia_metros))
				m_ExorLastInteractMs = now;
			if (m_ExorSnapDirty && !m_ExorVirt && allowSnapshot)
			{
				ExorWriteSnapshot();
				didSnapshot = true;
			}
			if (cerrarMs > 0 && now - m_ExorLastInteractMs > cerrarMs)
				Close();
			return false;
		}

		// cerrado
		if (m_ExorSnapDirty && !m_ExorVirt && allowSnapshot)
		{
			ExorWriteSnapshot();
			didSnapshot = true;
		}
		if (!allowVirtualize)
			return false;
		if (virtMs <= 0)
			return false;
		if (ExorIsVirtualized())
			return false;
		if (now - m_ExorLastInteractMs < virtMs)
			return false;
		if (ExorCargoCount() == 0)
			return false;
		if (!ExorCanVirtualizeNow())	// la subclase puede vetar (ej: nevera sin bateria con comida)
			return false;

		ExorVirtualize();
		return true;
	}

	// hook: la subclase decide si PUEDE virtualizar ahora. Default: siempre.
	// La nevera lo usa para NO virtualizar comida perecedera cuando NO tiene bateria
	// (asi se pudre real con el motor vanilla, sin bookkeeping de tiempo virtualizado).
	bool ExorCanVirtualizeNow() { return true; }

	// ======================= dirty tracking del cargo =======================
	override void EECargoIn(EntityAI item)
	{
		super.EECargoIn(item);
		if (GetGame() && GetGame().IsServer() && !m_ExorRestoring)
		{
			m_ExorLastInteractMs = GetGame().GetTime();
			m_ExorSnapDirty = true;
		}
	}

	override void EECargoOut(EntityAI item)
	{
		super.EECargoOut(item);
		if (GetGame() && GetGame().IsServer() && !m_ExorRestoring)
		{
			m_ExorLastInteractMs = GetGame().GetTime();
			m_ExorSnapDirty = true;
		}
	}
}
