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
	protected int  m_ExorDirtySinceMs;	// uptime ms del PRIMER cambio sucio sin volcar (debounce)
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
	// virtualizar TAMBIEN los attachments (ej armas en slots del mueble, ropa en el
	// locker). Default FALSE (barril/nevera: la nevera NO virtualiza su bateria).
	bool ExorVirtualizeAttachments() { return false; }

	void ExorDbg(string ev)
	{
		if (!ExorStorageConstants.DEBUG_BARRELS)
			return;
		Print(string.Format("%1[DBG-mueble] %2 | id=%3 pos=%4 cargo=%5 open=%6 virt=%7", ExorStorageConstants.LOG, ev, m_ExorID, GetPosition(), ExorCargoCount(), m_IsOpened, m_ExorVirt));
	}

	// Marca que hay cambios sin volcar. Solo la PRIMERA marca de un periodo sucio sella el
	// timestamp: asi el debounce mide desde el primer cambio, no desde el ultimo, y un
	// jugador moviendo items sin parar igual termina guardando cada SNAP_DEBOUNCE_MS.
	void ExorMarkSnapDirty()
	{
		if (!m_ExorSnapDirty)
			m_ExorDirtySinceMs = GetGame().GetTime();
		m_ExorSnapDirty = true;
	}

	// Debounce del guardado en vivo, ADAPTATIVO segun como venga el server.
	//
	// El trade-off: espaciar el guardado ahorra I/O, pero agranda la ventana en la que un
	// crash pierde cambios. Y la perdida es REAL, no teorica: ExorReconcileOnLoad borra el
	// cargo que no este en el JSON ("el JSON manda"), asi que lo que no se alcanzo a volcar
	// desaparece al reiniciar.
	//
	// Por eso el debounce NO es fijo:
	//   server sano (factor 1.0) -> 0 ms  = guarda como siempre, ventana de perdida minima
	//   server cargado (factor 0.25) -> el maximo = prioriza el frame
	// O sea: solo se acepta mas riesgo cuando el server realmente esta sufriendo, que es
	// cuando ahorrar I/O vale la pena. Con poblacion normal el comportamiento no cambia.
	int ExorSnapDebounceMs()
	{
		float f = ExorVO_Manager.s_BudgetFactor;
		if (f > 1.0)
			f = 1.0;
		if (f < 0)
			f = 0;
		return (int)(ExorStorageConstants.SNAP_DEBOUNCE_MS * (1.0 - f));
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
			ExorLockAttachments(true);	// las armas cargadas de persistencia arrancan bloqueadas
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
		ExorLockAttachments(false);	// las armas/prendas vuelven a ser accesibles
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
		ExorLockAttachments(true);
		if (GetGame().IsServer())
		{
			int now = GetGame().GetTime();
			m_ExorLastInteractMs = now;
			m_ExorLastCloseMs = now;
		}
		ExorUpdateDoorAnim();
	}

	// Bloquea/desbloquea el inventario de CADA attachment (las armas de los slots Exor_Gun*,
	// la ropa, etc.).
	//
	// Sin esto, cerrar el mueble ocultaba sus slots y su cargo -CanDisplayAttachmentSlot y
	// CanDisplayCargo devuelven IsOpen()- pero NO el contenido de cada arma: DayZ dibuja cada
	// item attached que tiene cargo/attachments propios como su propia fila expandible en el
	// inventario. Resultado: con el locker CERRADO seguian viendose las armas con sus miras,
	// cargadores, etc. y -lo grave- se podia interactuar con ellas, o sea lootear el mueble
	// sin abrirlo.
	// Bloquear el inventario de cada attachment corta la interaccion de raiz, sin depender de
	// que el cliente refresque bien la UI.
	void ExorLockAttachments(bool lockThem)
	{
		GameInventory inv = GetInventory();
		if (!inv)
			return;
		int i;
		for (i = 0; i < inv.AttachmentCount(); i++)
		{
			EntityAI att = inv.GetAttachmentFromIndex(i);
			if (!att || !att.GetInventory())
				continue;
			if (lockThem)
				att.GetInventory().LockInventory(HIDE_INV_FROM_SCRIPT);
			else
				att.GetInventory().UnlockInventory(HIDE_INV_FROM_SCRIPT);
		}
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
	// en cada sync). Aplica la fase via ExorApplyDoorPhase (overridable para 2+ puertas).
	void ExorUpdateDoorAnim()
	{
		bool open = IsOpen();
		if (m_DoorAnimInit && open == m_DoorAnimApplied)
			return;
		bool wasInit = m_DoorAnimInit;
		m_DoorAnimInit = true;
		m_DoorAnimApplied = open;
		float phase = 0.0;
		if (open)
			phase = 1.0;
		ExorApplyDoorPhase(phase);
		// SONIDO de abrir/cerrar: solo en toggles REALES (no en el spawn inicial) y en
		// clientes (el sonido es local; en clientes cercanos suena via OnVariablesSynchronized).
		if (wasInit && !GetGame().IsDedicatedServer())
			ExorPlayOpenCloseSound(open);
	}

	// Sound sets (vanilla). Overridable por subclase si se quiere otro sonido.
	string ExorGetOpenSoundSet()  { return "Barrel_Open_SoundSet"; }
	string ExorGetCloseSoundSet() { return "Barrel_Close_SoundSet"; }

	void ExorPlayOpenCloseSound(bool open)
	{
		string ss;
		if (open)
			ss = ExorGetOpenSoundSet();
		else
			ss = ExorGetCloseSoundSet();
		if (ss == "")
			return;
		EffectSound snd = SEffectManager.PlaySoundOnObject(ss, this);
		if (snd)
			snd.SetAutodestroy(true);
	}

	// Aplica la fase de animacion a la(s) puerta(s). Default: UNA puerta con el source de
	// ExorGetDoorAnimSource(). Un mueble de 2+ puertas lo overridea y setea cada source.
	void ExorApplyDoorPhase(float phase)
	{
		SetAnimationPhase(ExorGetDoorAnimSource(), phase);
	}

	// ======================= COLOCACION (compartida por todos los muebles) =======================
	// La llaman los items empacados en OnPlacementComplete. Crea el mueble ESTATICO
	// (create_physics=false, no se asienta/hunde) y apoya su base sobre la superficie REAL
	// bajo el punto de colocacion (terreno O piso de base) via raycast que IGNORA el
	// holograma del preview (si no, el raycast lo golpea a el). baseOffset = offset
	// origen->patas del modelo (calibrar in-game por mueble: se hunde -> subir; flota -> bajar).
	// Limpia la proyeccion server-side del holograma (_Ghost de projectionTypename, creado con
	// ECE_PLACE_ON_SURFACE = entidad real). Sin esto queda una "caja acostada" huerfana pickeable
	// al lado (nuestro OnPlacementComplete borra el packed y el hologram no limpia su proyeccion).
	// Se usa tanto al deployar OK como al BLOQUEAR (limite de muebles). 'keep' = el mueble deployado.
	static void ExorSweepGhosts(PlayerBase pb, vector pos, EntityAI keep)
	{
		if (pb)
		{
			Hologram holo = pb.GetHologramServer();
			if (holo && holo.GetProjectionEntity())
				GetGame().ObjectDelete(holo.GetProjectionEntity());
		}
		// la ref del hologram suele venir ya nula -> barrer por POSICION cualquier _Ghost cercano
		array<Object> objs = new array<Object>;
		array<CargoBase> prx = new array<CargoBase>;
		GetGame().GetObjectsAtPosition3D(pos, 4.0, objs, prx);
		foreach (Object o : objs)
		{
			if (o && o != keep && o.GetType().Contains("_Ghost"))
				GetGame().ObjectDelete(o);
		}
	}

	static EntityAI ExorDeployFurniture(Man player, string type, vector position, vector orientation, float baseOffset, float health)
	{
		if (!GetGame().IsServer())
			return null;

		PlayerBase pb = PlayerBase.Cast(player);

		// LIMITE DE MUEBLES / TERRITORIO (event-time): solo cerca del mastil + maximo por base.
		// Si no se permite, avisar en rojo y NO deployar (el packed queda en la mano, no se borra).
		string denyReason;
		if (pb && !ExorMuebleRules.CanPlaceMueble(pb, position, denyReason))
		{
			ExorMuebleRules.SendRed(pb, denyReason);
			ExorSweepGhosts(pb, position, null);	// no dejar la caja huerfana del holograma al bloquear
			return null;
		}

		Object ignoreObj = pb;
		if (pb)
		{
			Hologram holo = pb.GetHologramServer();
			if (holo && holo.GetProjectionEntity())
				ignoreObj = holo.GetProjectionEntity();
		}

		// La Y de colocacion viene del holograma (ya snapeo a la superficie donde apuntaste,
		// incluido el piso de un edificio o de una torre). La usamos como base.
		// El raycast afina SOLO en pendientes: se acepta unicamente si cae MUY cerca de esa Y.
		// Si cae lejos (terreno DEBAJO del piso del edificio -> se hundia; o piso de ARRIBA en
		// una torre -> saltaba), se IGNORA. Asi el mueble queda donde mostro el holograma.
		float surfaceY = position[1];
		vector rayStart = Vector(position[0], position[1] + 2.5, position[2]);
		vector rayEnd   = Vector(position[0], position[1] - 2.5, position[2]);
		vector hitPos, hitNorm;
		int hitComp;
		bool rayHit = DayZPhysics.RaycastRV(rayStart, rayEnd, hitPos, hitNorm, hitComp, null, null, ignoreObj, true, false);
		if (rayHit && Math.AbsFloat(hitPos[1] - position[1]) <= 0.5)
			surfaceY = hitPos[1];

		vector pos = position;
		pos[1] = surfaceY - baseOffset;

		// create_physics=false NO registra la colision hasta el primer guardado/recarga -> el
		// mueble RECIEN seteado se puede atravesar (los ya persistidos son solidos). Lo creamos
		// CON fisica (ECE_CREATEPHYSICS) para colision inmediata y luego congelamos el cuerpo
		// (ALWAYS_INACTIVE) para que NO se hunda/caiga: colisiona pero no se simula ni se mueve.
		EntityAI e = EntityAI.Cast(GetGame().CreateObjectEx(type, pos, ECE_CREATEPHYSICS));
		if (!e)
		{
			Print("[3xorStorage] ERROR: no se pudo crear " + type + " al colocar el mueble");
			return null;
		}
		e.SetPosition(pos);
		e.SetOrientation(orientation);
		dBodyDynamic(e, false);							// cuerpo ESTATICO: solido, no se simula ni se hunde
		e.SetHealth01("", "", health);
		ExorSweepGhosts(pb, pos, e);					// limpiar la proyeccion ghost (no dejar caja huerfana)
		return e;
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

	// No lockeable: bloquea CodeLock / candados de cualquier mod.
	// El motor llama esto por CADA slot candidato al arrastrar algo, y los lockers tienen 24
	// slots -> antes eran 24 ToLower() + hasta 96 barridos de substring por validacion, y la
	// UI lo repite mientras arrastras. Con un solo Contains("lock") alcanza: los otros tres
	// terminan todos en "lock", asi que eran redundantes.
	override bool CanReceiveAttachment(EntityAI attachment, int slotId)
	{
		if (attachment)
		{
			string type = attachment.GetType();
			type.ToLower();
			if (type.Contains("lock"))	// cubre codelock / combinationlock / padlock / *lock
				return false;
		}
		return super.CanReceiveAttachment(attachment, slotId);
	}

	override bool IsHeavyBehaviour() { return true; }
	override bool IsTwoHandedBehaviour() { return true; }

	// Ocultar cargo Y slots de attachment cuando el mueble esta CERRADO (solo se ven abierto).
	override bool CanDisplayCargo() { return IsOpen(); }
	override bool CanDisplayAttachmentSlot(int slot_id) { return IsOpen(); }
	override bool CanDisplayAnyAttachmentSlot() { return IsOpen(); }

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
			// attachments (armas/ropa en slots) bloquean el empaque para no perderlos
			if (ExorVirtualizeAttachments() && GetInventory().AttachmentCount() > 0)
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

	// Devuelve true si hizo trabajo REAL (scan del piso + limpieza de cargo stale). Un mueble
	// sin JSON se salda con un FileExist y devuelve false -> el manager NO le descuenta cupo.
	// Antes cada mueble vacio consumia un slot de MAX_RECONCILE_PER_TICK (3 por tick): con
	// 657 barriles + cientos de muebles, ponerse al dia tardaba entre 30 y 90 minutos, y
	// mientras tanto ningun mueble auto-cerraba ni virtualizaba.
	bool ExorReconcileNow()
	{
		if (m_ExorLoadDone)
			return false;
		m_ExorLoadDone = true;
		if (!ExorHasContent())
			return false;	// nada que reconciliar: no gastar presupuesto
		ExorReconcileOnLoad();
		return true;
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
		// attachments (ej armas en los slots del mueble, ropa en el locker) si la subclase lo pide
		if (ExorVirtualizeAttachments())
		{
			for (i = 0; i < inv.AttachmentCount(); i++)
			{
				EntityAI att = inv.GetAttachmentFromIndex(i);
				if (att)
					f.att.Insert(ExorVO_Serializer.CaptureItem(att));
			}
		}
		if (f.items.Count() == 0 && f.att.Count() == 0)
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
		// attachments (armas/ropa en slots) si la subclase lo pide
		if (ExorVirtualizeAttachments())
		{
			for (i = 0; i < inv.AttachmentCount(); i++)
			{
				EntityAI att = inv.GetAttachmentFromIndex(i);
				if (att)
					toDelete.Insert(att);
			}
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
	// Reintento del restore cuando el turno estaba tomado por otro contenedor. Se re-agenda
	// solo hasta conseguirlo; si mientras tanto dejo de estar virtualizado, no hace nada.
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
			// SERIALIZAR: si otro contenedor acaba de restaurar, esperar el turno en vez de
			// sumar dos avalanchas de creacion de entidades en el mismo frame.
			if (!ExorVO_Manager.CanRestoreNow())
			{
				GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorRestoreRetry, ExorVO_Manager.RESTORE_SPACING_MS, false);
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
		// attachments PRIMERO (armas/ropa a sus slots), luego el cargo
		if (f.att)
		{
			for (int a = 0; a < f.att.Count(); a++)
				ExorVO_Serializer.RestoreItem(f.att.Get(a), this, hidden, this, true);
		}
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
			array<EntityAI> stale = new array<EntityAI>;
			int s;
			CargoBase cargo = inv.GetCargo();
			if (cargo)
			{
				for (s = 0; s < cargo.GetItemCount(); s++)
				{
					EntityAI se = cargo.GetItem(s);
					if (se)
						stale.Insert(se);
				}
			}
			// ANTI-DUPE: si virtualizamos attachments, tambien borrar los que el engine
			// recreo nativamente al cargar (sino quedan en el mundo Y en el JSON = dupe).
			if (ExorVirtualizeAttachments())
			{
				for (s = 0; s < inv.AttachmentCount(); s++)
				{
					EntityAI ae = inv.GetAttachmentFromIndex(s);
					if (ae)
						stale.Insert(ae);
				}
			}
			cleared = stale.Count();
			for (s = 0; s < cleared; s++)
				GetGame().ObjectDelete(stale.Get(s));
		}

		int dropped = 0;
		if (cleared > 0)
		{
			dropped = ExorCleanDroppedNearby();
			// El retry a 8s existe porque el motor puede dropear items DESPUES del 1er
			// barrido. Pero solo tiene sentido si el 1ro encontro algo: si el piso estaba
			// limpio, no hay razon para pagar otro GetObjectsAtPosition(15m) + re-parseo del
			// JSON por contenedor. Con cientos de muebles eso son cientos de scans de mas.
			if (dropped > 0)
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
		// DEDUP CON MAP: antes era result.Find(val) por cada "type" del archivo, o sea una
		// busqueda LINEAL sobre los tipos ya encontrados. Con un JSON de 128 KB (~10k lineas)
		// y ~100 tipos unicos daba del orden de 10^6 comparaciones de string por llamada, y
		// esta funcion se llama hasta 3 veces por contenedor. Con el map es O(n).
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
			string rest = line.Substring(kp + 6, line.Length() - (kp + 6));
			int q1 = rest.IndexOf("\"");
			if (q1 < 0)
				continue;
			string rest2 = rest.Substring(q1 + 1, rest.Length() - (q1 + 1));
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
			// DEBOUNCE del guardado EN VIVO: mientras el mueble esta ABIERTO el jugador
			// sigue moviendo items, y cada snapshot reserializa el contenedor ENTERO (500
			// slots de cargo + los arboles de attachment) y reescribe el archivo completo:
			// hasta 128 KB de I/O sincrona, cada 5s, por mueble en uso. Se espera a que se
			// acumulen SNAP_DEBOUNCE_MS desde el primer cambio antes de volcar.
			// Loot-safe: Close() y ExorVirtualize() fuerzan el volcado igual, y el flag
			// dirty persiste, asi que nada se pierde por esperar.
			if (m_ExorSnapDirty && !m_ExorVirt && allowSnapshot && now - m_ExorDirtySinceMs >= ExorSnapDebounceMs())
			{
				ExorWriteSnapshot();
				didSnapshot = true;
			}
			if (cerrarMs > 0 && now - m_ExorLastInteractMs > cerrarMs)
				Close();
			return false;
		}

		// cerrado: ya nadie lo esta usando -> volcar sin esperar el debounce
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
		// nada que virtualizar? (cargo vacio Y -si aplica- sin attachments)
		bool hasAtt = ExorVirtualizeAttachments() && GetInventory() && GetInventory().AttachmentCount() > 0;
		if (ExorCargoCount() == 0 && !hasAtt)
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

	// hook: logica periodica de la subclase (ej: descarga de bateria de la nevera). La
	// llama el MANAGER en su tick central (no un timer por-entidad -> escala a muchas
	// neveras). 'now' = GetGame().GetTime() en ms. La subclase throttlea lo que necesite.
	void ExorPeriodicTick(int now) {}

	// ======================= dirty tracking del cargo =======================
	override void EECargoIn(EntityAI item)
	{
		super.EECargoIn(item);
		if (GetGame() && GetGame().IsServer() && !m_ExorRestoring)
		{
			m_ExorLastInteractMs = GetGame().GetTime();
			ExorMarkSnapDirty();
		}
	}

	override void EECargoOut(EntityAI item)
	{
		super.EECargoOut(item);
		if (GetGame() && GetGame().IsServer() && !m_ExorRestoring)
		{
			m_ExorLastInteractMs = GetGame().GetTime();
			ExorMarkSnapDirty();
		}
	}

	// Attachments (armas/ropa en slots): marcar dirty para que el snapshot los guarde
	// ANTES de virtualizar (sino se borrarian del mundo sin quedar en el JSON = perdida).
	override void EEItemAttached(EntityAI item, string slot_name)
	{
		super.EEItemAttached(item, slot_name);
		// Si el mueble ya esta CERRADO cuando entra el item (restore desde el JSON, carga de
		// persistencia), su inventario tiene que nacer bloqueado. Si no, esa arma quedaria
		// visible e interactuable con el mueble cerrado.
		if (!m_IsOpened && item && item.GetInventory())
			item.GetInventory().LockInventory(HIDE_INV_FROM_SCRIPT);
		if (ExorVirtualizeAttachments() && GetGame() && GetGame().IsServer() && !m_ExorRestoring)
		{
			m_ExorLastInteractMs = GetGame().GetTime();
			ExorMarkSnapDirty();
		}
	}

	override void EEItemDetached(EntityAI item, string slot_name)
	{
		super.EEItemDetached(item, slot_name);
		// SIEMPRE desbloquear al salir. El bloqueo es una propiedad de "estar guardado en un
		// mueble cerrado", no del item: si un arma se fuera del locker todavia bloqueada,
		// el jugador no podria acceder a su cargador ni a sus attachments = arma inutil.
		if (item && item.GetInventory())
			item.GetInventory().UnlockInventory(HIDE_INV_FROM_SCRIPT);
		if (ExorVirtualizeAttachments() && GetGame() && GetGame().IsServer() && !m_ExorRestoring)
		{
			m_ExorLastInteractMs = GetGame().GetTime();
			ExorMarkSnapDirty();
		}
	}
}
