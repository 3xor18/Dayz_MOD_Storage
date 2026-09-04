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
	protected int  m_ExorSnapFirma;		// firma del contenido del ULTIMO volcado (ver ExorWriteSnapshot)
	// el ultimo restore dejo items afuera -> re-capturar achicaria el JSON (ver ExorVirtualize)
	protected bool m_ExorRestoreParcial;

	// ---- CLAVE (code-lock, solo lockers) ----
	protected string m_ExorLockKey;			// "" = sin clave. Persistido. SOLO server la conoce.
	protected string m_ExorKeySetterSid;	// steamid del que puso la clave (para limpiarla si lo expulsan). Persistido.
	protected ref TStringArray m_ExorUnlockedBy;	// steamids que ya metieron la clave (runtime, se
													// resetea al reiniciar el server -> vuelve a pedirla).

	void Exor_OpenableStorage()
	{
		RegisterNetSyncVariableBool("m_IsOpened");
		RegisterNetSyncVariableBool("m_ExorVirtualizedSync");
		m_ExorUnlockedBy = new TStringArray;
	}

	// ======================= CLAVE (code-lock) =======================
	// La subclase LOCKER lo pone en true -> tiene candado (accion "Poner/Cambiar clave" + pide
	// clave al abrir). Los demas muebles (nevera) devuelven false = abren normal.
	bool ExorHasCodeLock() { return false; }

	bool ExorHasKey() { return m_ExorLockKey != ""; }

	// setea/cambia la clave. Al cambiarla, se OLVIDAN los desbloqueos viejos (todos deben meter
	// la nueva), salvo el que la acaba de setear (obvio que la sabe).
	void ExorSetKey(string key, string setterSteamId)
	{
		m_ExorLockKey = key;
		m_ExorKeySetterSid = setterSteamId;	// "" al limpiar
		if (m_ExorUnlockedBy)
			m_ExorUnlockedBy.Clear();
		if (key != "" && setterSteamId != "")
			m_ExorUnlockedBy.Insert(setterSteamId);
		ExorGuardarLockState();	// la clave vive en un JSON lateral, NO en el stream del engine
	}

	// Vuelca la clave a su JSON lateral (ExorLockStore). Se escribe solo cuando la clave
	// CAMBIA -que es rarisimo-, no en cada guardado de persistencia.
	void ExorGuardarLockState()
	{
		if (!GetGame().IsServer())
			return;
		ExorLockState st = new ExorLockState();
		st.clave = m_ExorLockKey;
		st.setter = m_ExorKeySetterSid;
		ExorLockStore.Guardar(ExorGetPid(), st);
	}

	string ExorGetKeySetterSid() { return m_ExorKeySetterSid; }

	// limpia la clave (la usa el kick del party: si expulsan a quien la puso, el locker
	// queda sin clave para que el resto del clan no quede afuera).
	void ExorClearKey()
	{
		ExorSetKey("", "");
	}

	bool ExorKeyMatches(string key)
	{
		return m_ExorLockKey != "" && m_ExorLockKey == key;
	}

	// Quienes ya ingresaron la clave. Es estado DE SESION a proposito: al reiniciar, el
	// miembro la vuelve a poner una vez y listo. Antes se persistia en el stream del engine
	// con un contador delante, y un contador leido de un stream corrido hacia que el server
	// se comiera 21 GB de RAM insertando strings basura hasta que el hosting lo mataba.
	// Ningun dato de largo variable vuelve a ese stream.
	void ExorMarkUnlocked(string steamId)
	{
		if (!m_ExorUnlockedBy)
			m_ExorUnlockedBy = new TStringArray;
		if (steamId != "" && m_ExorUnlockedBy.Find(steamId) == -1)
			m_ExorUnlockedBy.Insert(steamId);
	}

	bool ExorIsUnlockedBy(string steamId)
	{
		return m_ExorUnlockedBy && m_ExorUnlockedBy.Find(steamId) != -1;
	}

	// ---- HOOKS que las subclases pueden override ----
	// nombre del source de animacion de la puerta (en model.cfg + AnimationSources)
	string ExorGetDoorAnimSource() { return "Lid"; }
	// filtro de cargo de la subclase (ej: solo comida). Default: acepta todo.
	bool ExorCanStore(EntityAI item) { return true; }

	// Filtro compartido de los LOCKERS (negro y rojo): guardan gear/ropa/armas/MEDICO,
	// pero NO comida ni bebida (esas van a la nevera). Las pastillas medicas heredan
	// Edible_Base, asi que se detectan por NOMBRE (no hay clase Pill_Base) y se PERMITEN.
	bool ExorLockerCanStore(EntityAI item)
	{
		if (!item)
			return true;
		string t = item.GetType();
		t.ToLower();
		// medico: pastillas/tablets/vitaminas/antibioticos/carbon/purificadoras -> SI se guardan
		if (t.Contains("tablet") || t.Contains("pill") || t.Contains("vitamin") || t.Contains("antibiotic") || t.Contains("charcoal") || t.Contains("purification"))
			return true;
		if (item.IsInherited(Edible_Base) || item.IsInherited(Bottle_Base))
			return false;	// comida / bebida -> a la nevera
		return true;
	}
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
		{
			// cortar un restore por lotes en curso: sin esto quedaria un CallLater apuntando
			// a una entidad que ya no existe
			if (m_ExorJobFile)
			{
				m_ExorJobFile = null;
				GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(ExorRestorePump);
			}
			ExorVO_Manager.UnregisterOpenable(this);
			// el JSON lateral de la clave muere con el mueble (si no, queda basura por cada
			// mueble que alguna vez tuvo clave)
			if (!ExorVO_Manager.s_ShuttingDown && m_ExorPid > 0 && m_ExorLockKey != "")
				ExorLockStore.Borrar(m_ExorPid);
			// DIAGNOSTICO (paso 1): loguear TODA baja de un mueble en sesion viva -> tipo + pos +
			// estado. Un empaque deliberado ademas deja "Mueble empaquetado"; un DESPAWN del motor
			// (bug de pared/invalid-location/CE) deja SOLO esta linea, sin "empaquetado" -> asi se
			// caza el bug con la posicion exacta. No loguea en el apagado (s_ShuttingDown).
			if (!ExorVO_Manager.s_ShuttingDown)
			{
				int openInt = 0;
				if (m_IsOpened)
					openInt = 1;
				int loadedInt = 0;
				if (m_ExorLoadDone)
					loadedInt = 1;
				Print(string.Format("%1 MUEBLE-REMOVIDO tipo=%2 pos=%3 abierto=%4 cargado=%5 (si NO hay 'Mueble empaquetado' cerca -> es DESPAWN del motor, revisar si estaba en una pared)",
					ExorStorageConstants.LOG, GetType(), GetPosition().ToString(), openInt, loadedInt));
			}
		}
		super.EEDelete(parent);
	}

	// ========================= PERSISTENCIA =========================
	// El stream del engine lleva SOLO DOS ENTEROS: el magico y el id numerico.
	// Nada de strings: un ctx.Read(string) sobre un stream corrido tira una Virtual Machine
	// Exception que MATA el arranque del server, y no se puede atrapar desde script (la tira
	// el engine adentro del Read). Leer un int de un stream corrido devuelve basura, pero
	// nunca lanza. Ver ExorPid.
	//
	// La clave del locker y quien la ingreso viven en un JSON lateral indexado por el id
	// (ExorLockStore). Agregarles campos a futuro es gratis: no tocan el stream. Cuando SI
	// estaban en el stream, agregarlos rompio la migracion (v2.10.0) y sacarlos rompio la
	// carga de lo ya guardado (v2.10.1). Ese problema deja de existir.
	override void OnStoreSave(ParamsWriteContext ctx)
	{
		super.OnStoreSave(ctx);
		ctx.Write(ExorPid.EXOR_MAGIC);
		ctx.Write(ExorGetPid());
	}

	// CARGA SEGURA (arranque anterior roto): si el arranque previo dejo rastro de que ESTA
	// clase no se puede deserializar, no se toca el stream. Ver ExorBootRepair.SaltearTipo.
	override bool OnStoreLoad(ParamsReadContext ctx, int version)
	{
		if (GetGame().IsServer() && ExorBootRepair.SaltearTipo(GetType()))
		{
			Print(string.Format("%1 CARGA-SEGURA: '%2' no se deserializa en este arranque -> lo recrea el self-heal con su contenido del JSON", ExorStorageConstants.LOG, GetType()));
			return false;
		}
		if (!super.OnStoreLoad(ctx, version))
			return false;

		int magic;
		if (!ctx.Read(magic))
			return false;
		if (magic != ExorPid.EXOR_MAGIC)
		{
			// El stream venia corrido. Antes esto era una loteria: se leia un string y el
			// engine reventaba. Ahora se corta limpio -> el motor descarta ESTE mueble (no el
			// archivo entero) y el self-heal lo recrea con su contenido, que vive en el JSON.
			Print(string.Format("%1 GUARD: stream de mueble DESALINEADO (magico %2) -> mueble descartado, se recrea del registro", ExorStorageConstants.LOG, magic));
			return false;
		}

		int pid;
		if (!ctx.Read(pid))
			return false;
		if (!ExorPid.Plausible(pid))
		{
			Print(string.Format("%1 GUARD: id de mueble ilegible (%2) -> mueble descartado, se recrea del registro", ExorStorageConstants.LOG, pid));
			return false;
		}
		m_ExorPid = pid;
		m_ExorID = pid.ToString();

		// clave y lista de "ya la ingreso": del JSON lateral, no del stream
		ExorLockState st = ExorLockStore.Cargar(pid);
		m_ExorLockKey = st.clave;
		m_ExorKeySetterSid = st.setter;
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

			// ADOPCION DE HUERFANO. Si este mueble llego SIN id, es porque su OnStoreLoad no
			// pudo leerlo: nevera salteada por BootRepair, stream corrupto, cualquier cosa que
			// devuelva false. Sin esto, ExorGetID() le inventa un id NUEVO y pasan dos cosas:
			//   1) queda un registro huerfano con el id viejo -> el self-heal intenta recrearlo
			//      cada 30 min, choca con este mueble vivo y loguea "NO se recrea: ya hay otro
			//      encima" para siempre (12 lineas por pase en el server del amigo);
			//   2) PEOR: el JSON del contenido virtualizado esta guardado con el id VIEJO, asi
			//      que con un id nuevo el mueble ya no lo encuentra -> el contenido queda
			//      inalcanzable aunque el archivo siga en disco.
			// Recuperar el id viejo del registro arregla las dos: no se duplica el registro y el
			// reconcile vuelve a encontrar su contenido.
			if (m_ExorID == "")
			{
				string adoptado = ExorMuebleRegistry.BuscarHuerfanoEnPos(GetType(), GetPosition());
				if (adoptado != "")
				{
					m_ExorID = adoptado;
					Print(string.Format("%1 ADOPCION: mueble sin id en %2 recupero su registro '%3' (y con el, su contenido guardado)", ExorStorageConstants.LOG, GetPosition().ToString(), adoptado));
				}
			}

			m_ExorVirtualizedSync = ExorHasContent();
			ExorMuebleRegistry.Register(this);	// registro del self-heal (id/pos actuales)
			SetSynchDirty();
		}
		ExorUpdateDoorAnim();
	}

	// ======================= ABRIR / CERRAR =======================
	override void Open()
	{
		// BLOQUEO DE ARRANQUE + COOLDOWN DE REAPERTURA. Va ANTES de tocar m_IsOpened para no
		// desincronizar al cliente (mismo patron que el barril). Ojo: hasta ahora este Open()
		// no tenia NINGUN cooldown -- m_ExorLastCloseMs se escribia en Close() pero no se leia
		// nunca -> los muebles estaban mas expuestos que el barril.
		if (GetGame().IsServer())
		{
			ExorCfgStorage cfgOpen = GetExorConfig().storage;
			int nowOpen = GetGame().GetTime();
			// (la ventana de gracia por jugador se chequea en la accion, que si tiene el player)
			int cdOpenMs = cfgOpen.cooldown_abrir_segundos * 1000;
			if (cdOpenMs > 0 && m_ExorLastCloseMs > 0 && nowOpen - m_ExorLastCloseMs < cdOpenMs)
			{
				ExorDbg("Open BLOQUEADO (cooldown de reapertura anti-dupe)");
				return;
			}
		}

		m_IsOpened = true;
		SetSynchDirty();
		if (GetInventory())
			GetInventory().UnlockInventory(HIDE_INV_FROM_SCRIPT);	// abierta = accesible
		ExorLockAttachments(false);	// las armas/prendas vuelven a ser accesibles
		if (GetGame().IsServer())
		{
			ExorRestoreIfNeeded();		// recrear items del JSON si estaba virtualizado
			m_ExorLastInteractMs = GetGame().GetTime();
			// ABRIR = "puede haber cambiado algo que los hooks NO ven". EECargoIn/EECargoOut y
			// EEItemAttached/Detached solo se disparan por lo que entra o sale del MUEBLE EN SI;
			// lo que el jugador mete o saca DENTRO de una mochila (o una prenda) que ya estaba
			// adentro no dispara NADA. Con el mueble marcado limpio, el JSON se quedaba con la
			// foto vieja y al virtualizar se borraban del mundo items que el archivo no tenia:
			// esas son las balas que los jugadores guardan y no vuelven a aparecer. Marcarlo
			// sucio al abrir hace que los tres caminos de guardado (debounce con el mueble
			// abierto, cierre y virtualizado) vuelvan a leer el contenido REAL.
			ExorMarkSnapDirty();
		}
		ExorUpdateDoorAnim();
	}

	override void Close()
	{
		// Cerrar a mitad de un restore por lotes dejaria el resto de los items sin ubicar
		// (el inventario queda bloqueado un instante despues). Se termina primero.
		if (GetGame().IsServer() && m_ExorJobFile)
			ExorRestoreDrain();
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
			// CERRAR = ultima oportunidad de leer el contenido REAL. Con el mueble cerrado su
			// inventario (y el de sus attachments) queda bloqueado, asi que lo que se guarde
			// aca ya no puede cambiar hasta la proxima apertura. Marcarlo sucio garantiza ese
			// volcado final aunque el ultimo cambio haya sido dentro de una mochila.
			ExorMarkSnapDirty();
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
	// Ese objeto es la proyeccion VIVA del holograma de algun jugador que esta colocando?
	// Se consulta antes de borrar un _Ghost: ver la nota de ExorSweepGhosts.
	static bool ExorEsProyeccionViva(Object o)
	{
		if (!o)
			return false;
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase p = PlayerBase.Cast(players.Get(i));
			if (!p)
				continue;
			Hologram h = p.GetHologramServer();
			if (h && h.GetProjectionEntity() == o)
				return true;
		}
		return false;
	}

	static void ExorSweepGhosts(PlayerBase pb, vector pos, EntityAI keep)
	{
		// NUNCA borrar un _Ghost que todavia es la proyeccion de un holograma VIVO.
		// Vanilla (actiondeployobject.c:67) hace, en CADA frame mientras dura la accion de
		// colocar, GetHologramServer().GetProjectionEntity().GetPosition(): si le sacamos la
		// proyeccion por abajo, el jugador sigue "placing" (m_HologramServer != null) pero
		// GetProjectionEntity() ya es null -> Virtual Machine Exception por frame hasta que
		// termina la accion. Eso son las rafagas de excepciones que aparecen en el RPT del
		// server justo despues de cada "mueble seteado" (167 en 3 dias, una de 152 seguidas):
		// cada excepcion escribe stack trace a disco = hitch de frame.
		// La proyeccion del que ACABA de colocar la borra vanilla una linea despues de
		// nuestro OnPlacementComplete (PlacingCompleteServer), asi que no hay que tocarla.
		// Lo que si hay que barrer son las proyecciones HUERFANAS (hologramas que ya murieron
		// pero dejaron su _Ghost pickeable al lado del mueble): esas no pertenecen a ningun
		// jugador colocando, asi que pasan el filtro y se borran igual que antes.
		array<Object> objs = new array<Object>;
		array<CargoBase> prx = new array<CargoBase>;
		GetGame().GetObjectsAtPosition3D(pos, 4.0, objs, prx);
		foreach (Object o : objs)
		{
			if (!o || o == keep)
				continue;
			if (!o.GetType().Contains("_Ghost"))
				continue;
			if (ExorEsProyeccionViva(o))
				continue;	// holograma vivo (el que coloca, o un companero colocando al lado)
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
		Exor_OpenableStorage furReg = Exor_OpenableStorage.Cast(e);
		if (furReg)
			ExorMuebleRegistry.Register(furReg);		// registrar para el self-heal
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
	// Cache classname -> "es un candado?". El motor llama esto por CADA slot candidato cada
	// vez que arrastras algo, y los lockers tienen decenas de slots: eran decenas de ToLower()
	// (que ALOCA un string nuevo) + su barrido de substring por validacion, repetidos mientras
	// el jugador mueve el mouse. El classname de un item nunca cambia, asi que la respuesta se
	// calcula una vez por tipo en toda la vida del server y despues es un lookup de hash.
	static ref map<string, bool> s_EsCandado;

	static bool ExorEsCandado(string type)
	{
		if (!s_EsCandado)
			s_EsCandado = new map<string, bool>;
		bool r;
		if (s_EsCandado.Find(type, r))
			return r;
		string t = type;
		t.ToLower();
		r = t.Contains("lock");	// cubre codelock / combinationlock / padlock / *lock
		s_EsCandado.Set(type, r);
		return r;
	}

	// No lockeable: bloquea CodeLock / candados de cualquier mod.
	override bool CanReceiveAttachment(EntityAI attachment, int slotId)
	{
		if (attachment && ExorEsCandado(attachment.GetType()))
			return false;
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
		AddAction(ExorActionSetLockerKey);	// "Poner/Cambiar clave" (solo aparece si ExorHasCodeLock)
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
	// Id NUMERICO persistente. Es la unica identidad del contenedor: el string que usan las
	// rutas de archivo y el registro es su representacion decimal. Un solo identificador en
	// vez de dos evita que se desincronicen.
	protected int m_ExorPid;

	int ExorGetPid()
	{
		if (m_ExorPid <= 0)
		{
			m_ExorPid = ExorPid.Nuevo();
			m_ExorID = m_ExorPid.ToString();
		}
		return m_ExorPid;
	}

	string ExorGetID()
	{
		if (m_ExorID == "")
			m_ExorID = ExorGetPid().ToString();
		return m_ExorID;
	}

	// ruta del JSON con el contenido de este contenedor. Ver ExorContainerOps.
	string ExorGetStoragePath()
	{
		return ExorContainerOps.StoragePath(ExorGetID());
	}

	// re-liga un id conocido (lo usa el SELF-HEAL al recrear un mueble despawneado, para que
	// recupere su contenido virtualizado del JSON que quedo en disco con ese id).
	// Re-liga el id al recrear el mueble desde el registro (self-heal): con el id viejo
	// vuelve a encontrar su JSON de contenido. Mantiene pid y string en sincronia.
	void ExorSetIDForHeal(string id)
	{
		if (id == "")
			return;
		m_ExorID = id;
		m_ExorPid = id.ToInt();
	}

	bool ExorIsVirtualized() { return m_ExorVirt; }
	bool ExorHasContent()    { return FileExist(ExorGetStoragePath()); }

	// cantidad de items REALES en el cargo (no cuenta lo virtualizado). Ver ExorContainerOps.
	int ExorCargoCount()
	{
		return ExorContainerOps.CargoCount(this);
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
	// 'forzar' = escribir si o si, aunque el contenido no haya cambiado. Lo usa el camino de
	// VIRTUALIZAR, que ademas de guardar necesita refrescar el sello de tiempo del archivo
	// (de ahi sale, por ejemplo, cuanto envejece la comida de la nevera).
	void ExorWriteSnapshot(bool forzar = false)
	{
		GameInventory inv = GetInventory();
		if (!inv)
			return;
		CargoBase cargo = inv.GetCargo();
		if (!cargo)
			return;

		// armado del DTO: compartido con el barril (ver ExorContainerOps.ArmarSnapshot).
		// Los muebles que guardan cosas en SLOTS (mueble de armas, locker) piden tambien
		// los attachments; el resto solo el cargo.
		// FIRMA PRIMERO, DTO DESPUES. Armar el DTO de un contenedor lleno son ~1500 objetos
		// (un ExorVO_ItemData con sus dos arrays por item, recursivo); en el caso comun -nada
		// cambio- se tiraban enteros. Firmar el inventario VIVO no aloca nada.
		int firma = ExorContainerOps.FirmaViva(this, ExorVirtualizeAttachments());
		if (!forzar && firma == m_ExorSnapFirma && m_ExorSnapFirma != 0 && FileExist(ExorGetStoragePath()))
		{
			m_ExorSnapDirty = false;
			return;
		}

		ExorVO_ContainerFile f = ExorContainerOps.ArmarSnapshot(this, ExorGetID(), ExorVirtualizeAttachments());
		ExorOnSnapshotWrite(f);	// metadata de la subclase (la nevera anota bateria y carga)
		if (f.items.Count() == 0 && f.att.Count() == 0)
		{
			// BLINDAJE: este DeleteFile es el UNICO punto del mod que destruye loot de forma
			// irreversible. Un mueble VIRTUALIZADO tiene el cargo vacio A PROPOSITO y su JSON
			// es la unica copia del contenido -> jamas borrarlo desde aca. Mismo caso mientras
			// se esta restaurando (el cargo todavia no se lleno).
			if (m_ExorVirt || m_ExorRestoring)
			{
				Print(string.Format("%1 GUARD: mueble %2 -> se EVITO borrar el JSON con un cargo vacio (virt=%3 restaurando=%4)", ExorStorageConstants.LOG, ExorGetID(), m_ExorVirt, m_ExorRestoring));
				m_ExorSnapDirty = false;	// no reintentar cada tick: no hay nada real que volcar
				return;
			}
			if (FileExist(ExorGetStoragePath()))
				DeleteFile(ExorGetStoragePath());
		}
		else
		{
			ExorContainerOps.GuardarJL(ExorGetStoragePath(), f);
		}
		m_ExorSnapFirma = firma;
		m_ExorSnapDirty = false;
	}

	// ANTI-DUPE: cierre + virtualizacion FORZADA antes de un reinicio programado
	// (ver ExorStorageBootLock.CercaDeReinicio). Mismo criterio que el barril: se ignora la
	// cercania del jugador y, si alguien esta arrastrando un item justo ahi, se prefiere
	// perder ESE item antes que dejar el mueble explotable. Devuelve true si hizo trabajo.
	bool ExorForzarVirtualizar()
	{
		if (m_ExorVirt || ExorCargoCount() == 0)
			return false;
		if (IsOpen())
			Close();
		ExorMarkSnapDirty();	// que ExorVirtualize vuelque el contenido REAL al JSON
		ExorVirtualize();
		return true;
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

		// Volcar el contenido REAL al JSON antes de sacarlo del mundo. Abrir el mueble ya lo
		// marca sucio (ver Open()), asi que cualquier cambio que los hooks no vean -balas
		// metidas en la mochila de un slot- igual queda guardado antes de borrar los items.
		// EXCEPCION: si el ultimo restore quedo INCOMPLETO (algo no entro y quedo en el
		// piso), re-capturar achicaria el JSON y ESO si perderia loot -> se conserva el
		// archivo entero y se reintenta la proxima vez.
		if (!m_ExorRestoreParcial)
			ExorWriteSnapshot(true);	// FORZADO: el sello de tiempo del archivo tiene que ser el del momento en que el contenido sale del mundo

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
		// GUARD ANTI-DUPE (obligatorio). Mismo agujero que el barril, ver ExorStorage_Barrels.c:
		// ExorRestoreIfNeeded programa un CallLater(ExorDoRestore, 300) y vuelve SIN tocar
		// m_ExorVirt -> dos aperturas dentro de esa ventana recreaban el JSON dos veces en el
		// mismo mueble = contenido duplicado. Aca es PEOR que en el barril porque Open() no
		// tiene cooldown de reapertura (m_ExorLastCloseMs se escribe pero nunca se lee).
		// Afecta a Exor_Locker, Exor_LockerRojo, Exor_Fridge y Exor_MuebleArmas.
		// No perder loot: esto solo puede SALTEAR un restore redundante; nunca escribe ni
		// borra el JSON, y m_ExorVirt queda como estaba (el contenido sigue en disco).
		if (!m_ExorVirt || m_ExorRestoring)
		{
			Print(string.Format("%1 GUARD: mueble %2 -> restore duplicado BLOQUEADO (virt=%3 restaurando=%4)", ExorStorageConstants.LOG, ExorGetID(), m_ExorVirt, m_ExorRestoring));
			return;
		}

		string path = ExorGetStoragePath();
		if (!FileExist(path))
		{
			m_ExorVirt = false;
			return;
		}

		// lector JSON-Lines (tolera el formato viejo). Una linea rota pierde UN item,
		// no el contenedor entero. Ver ExorContainerOps.LeerJL.
		ExorVO_ContainerFile f = ExorContainerOps.LeerJL(path);
		if (!f)
		{
			m_ExorVirt = false;
			return;
		}

		// anti-dupe: SOLO en la 1ra apertura poco despues del arranque, limpiar lo que DayZ
		// tiro al piso por "invalid location" al cargar (contenido anidado). Ventana de 5 min.
		// Solo si el apagado anterior NO fue limpio: con un cierre en regla todos los
		// contenedores quedaron vacios y no hay derrame que barrer (ver ExorApagadoLimpio).
		// El barrido es un GetObjectsAtPosition de 15 m + parseo del JSON: 83 ms medidos en
		// un solo contenedor, y antes lo pagaban TODOS en su primera apertura.
		int floorCleanWindowMs = 300000;
		if (!m_ExorFloorCleaned)
		{
			m_ExorFloorCleaned = true;
			if (GetGame().GetTime() < floorCleanWindowMs && !ExorApagadoLimpio.FueLimpio())
				ExorCleanDroppedNearby();
		}

		vector hidden = GetPosition();
		// GUARD: podar de la data guardada lo imposible de restaurar (classname fantasma,
		// cargador que no calza en su arma). Sin esto se creaban entidades a medio armar que
		// se persistian y tumbaban el arranque del server. Ver ExorVO_Serializer.Sanitize.
		int podados = ExorVO_Serializer.Sanitize(f.att, "", true) + ExorVO_Serializer.Sanitize(f.items, "", false);
		if (podados > 0)
			Print(string.Format("%1 GUARD: mueble %2 -> %3 item(s) corruptos descartados antes de restaurar", ExorStorageConstants.LOG, ExorGetID(), podados));
		// ------------------------------------------------------------------------------
		//  RESTORE INCREMENTAL (por lotes, repartido en varios frames)
		// ------------------------------------------------------------------------------
		// ANTES: todo esto corria de una en UN SOLO frame. Un locker de 500 slots con mochilas
		// adentro son miles de entidades creadas y reubicadas de golpe -> el server se clava
		// segundos enteros. En horario de raid, donde se abren decenas de contenedores, era
		// un hitch por apertura. RESTORE_SPACING_MS solo evitaba que arrancaran DOS restores
		// en el mismo frame; no partia el trabajo de uno.
		//
		// AHORA: el trabajo se corta en lotes de EXOR_RESTORE_LOTE entradas de NIVEL SUPERIOR
		// por frame. Cada entrada se sigue restaurando entera (un bolso con sus cosas es
		// atomico, que es lo que hace falta para no dejar arboles a medias), pero entre lote
		// y lote el server respira. El inventario del mueble sigue BLOQUEADO hasta que termina,
		// asi que el jugador nunca ve un contenedor a medio llenar ni puede sacar de ahi.
		//
		// Patron: una maquina de estados chiquita con el progreso guardado en el propio mueble.
		// Nada de recursion diferida ni de closures: el estado es explicito y se puede leer.
		m_ExorRestoring = true;
		ExorVO_Serializer.ResetFallosUbicacion();
		m_ExorJobFile = f;
		m_ExorJobPos = hidden;
		m_ExorJobIdxAtt = 0;
		m_ExorJobIdxItem = 0;
		ExorRestorePump();
	}

	// Estado del restore en curso (null = no hay ninguno). Ver ExorDoRestore.
	protected ref ExorVO_ContainerFile m_ExorJobFile;
	protected vector m_ExorJobPos;
	protected int m_ExorJobIdxAtt;
	protected int m_ExorJobIdxItem;

	// Procesa UN lote y, si queda trabajo, se re-agenda para el proximo frame.
	void ExorRestorePump()
	{
		if (!m_ExorJobFile)
			return;
		ExorVO_ContainerFile f = m_ExorJobFile;
		int lote = ExorStorageConstants.EXOR_RESTORE_LOTE;
		int hechos = 0;

		// 1) attachments primero (armas/ropa a sus slots del mueble)
		while (f.att && m_ExorJobIdxAtt < f.att.Count() && hechos < lote)
		{
			ExorVO_Serializer.RestoreItem(f.att.Get(m_ExorJobIdxAtt), this, m_ExorJobPos, this, true);
			m_ExorJobIdxAtt++;
			hechos++;
		}
		// 2) despues el cargo
		while (f.items && m_ExorJobIdxItem < f.items.Count() && hechos < lote)
		{
			ExorVO_Serializer.RestoreItemTop(f.items.Get(m_ExorJobIdxItem), this, m_ExorJobPos);
			m_ExorJobIdxItem++;
			hechos++;
		}

		bool quedaAtt = f.att && m_ExorJobIdxAtt < f.att.Count();
		bool quedaItem = f.items && m_ExorJobIdxItem < f.items.Count();
		if (quedaAtt || quedaItem)
		{
			// 1 ms = "el proximo frame". No se usa 0 porque el CallQueue de DayZ trata el 0
			// como "ya mismo" y volveria a correr dentro de este mismo frame.
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorRestorePump, 1, false);
			return;
		}
		ExorRestoreFin();
	}

	// Termina AHORA lo que quede del restore, sin repartir en frames. Se usa cuando algo no
	// puede esperar: cerrar el mueble o darlo de baja a mitad de restore dejaria items sin
	// ubicar (y con el inventario ya bloqueado, sueltos en el piso).
	// El tope de vueltas es una red de seguridad contra un bug propio, no un limite de diseño.
	void ExorRestoreDrain()
	{
		int guard = 0;
		while (m_ExorJobFile && guard < 5000)
		{
			ExorRestorePump();
			guard++;
		}
	}

	// Cierre del restore: lo que antes venia despues del bucle sincronico.
	void ExorRestoreFin()
	{
		ExorVO_ContainerFile f = m_ExorJobFile;
		m_ExorJobFile = null;
		m_ExorRestoring = false;
		// Si algo no entro (quedo suelto en el piso), este mueble NO puede re-capturar su
		// contenido al virtualizar: lo achicaria y ese item se perderia. Ver ExorVirtualize.
		m_ExorRestoreParcial = ExorVO_Serializer.FallosUbicacion() > 0;
		if (m_ExorRestoreParcial)
			Print(string.Format("%1 AVISO: el mueble %2 restauro INCOMPLETO -> no se re-captura el contenido (se conserva el JSON entero)", ExorStorageConstants.LOG, ExorGetID()));

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

	// hook: la subclase agrega metadata propia al JSON justo antes de escribirlo (la nevera
	// marca si tenia bateria). Corre en SERVER, con los items todavia reales.
	void ExorOnSnapshotWrite(ExorVO_ContainerFile f) {}

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

	// Tipos que este contenedor guarda, leidos del texto crudo del JSON. Ver ExorContainerOps.
	TStringArray ExorTypesFromJsonText(string path)
	{
		return ExorContainerOps.TiposDelJson(path);
	}

	// Borra los items SUELTOS cerca que son de un tipo que este contenedor guarda: son los
	// drops de "invalid location" que DayZ tira al cargar el mundo. Ver ExorContainerOps.
	int ExorCleanDroppedNearby(float scanRadius = 15.0, float maxHoriz = 10.0)
	{
		return ExorContainerOps.LimpiarDropsCerca(this, ExorGetStoragePath(), scanRadius, maxHoriz);
	}

	// FAST-SKIP para el tick del manager: un mueble "idle" no necesita NINGUN trabajo este tick
	// (ni auto-cierre, ni snapshot, ni virtualizar) -> el manager lo saltea SIN entrar a ExorTick.
	// Idle = ya reconciliado + CERRADO + ya virtualizado + sin cambios pendientes + no necesita
	// reconcile. Con cientos de muebles, la mayoria esta idle casi siempre -> el tick deja de
	// recorrerlos de verdad (antes entraba a ExorTick en los 92 aunque solo 1-2 hicieran algo).
	// La subclase con periodica propia (ej bateria de la nevera) NO es idle (override abajo).
	bool ExorIsIdle()
	{
		if (!m_ExorLoadDone || ExorNeedsReconcile())
			return false;
		if (m_ExorJobFile)
			return false;			// restaurando por lotes
		if (m_IsOpened)
			return false;			// abierto: hay que chequear auto-cierre
		if (m_ExorSnapDirty)
			return false;			// cambios sin volcar al JSON
		if (!m_ExorVirt)
			return false;			// cerrado pero sin virtualizar todavia -> hay trabajo
		return true;				// cerrado, virtualizado, limpio -> nada que hacer
	}

	// ======================= TICK (auto-cierre + virtualizar) =======================
	// Lo llama el manager (con presupuesto por tick). Devuelve true si virtualizo.
	bool ExorTick(int now, ExorCfgStorage settings, bool allowVirtualize, bool allowSnapshot, array<Man> players, out bool didSnapshot)
	{
		didSnapshot = false;
		if (!m_ExorLoadDone)	// espera su turno de reconcile
			return false;
		// restore por lotes en curso -> no auto-cerrar, no volcar y no virtualizar hasta que
		// termine (dura decimas de segundo). Tocar el mueble a mitad de restore es justo lo
		// que deja items sueltos.
		if (m_ExorJobFile)
		{
			m_ExorLastInteractMs = now;	// que no cuente como inactivo mientras se llena
			return false;
		}

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
				// SIGUE ABIERTO -> se vuelve a marcar sucio. Mientras el mueble esta abierto
				// el jugador puede seguir moviendo cosas DENTRO de una mochila o una prenda
				// que ya estaba adentro, y eso no dispara NINGUN hook del mueble. Si lo
				// dejaramos limpio, ese cambio posterior al guardado quedaba fuera del JSON y
				// al virtualizar se revertia (probado in-game: sacar una pila de la mochila
				// despues del guardado periodico y volver a encontrarla). Re-marcarlo deja el
				// guardado corriendo cada SNAP_DEBOUNCE_MS y garantiza el volcado del cierre.
				ExorMarkSnapDirty();
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
