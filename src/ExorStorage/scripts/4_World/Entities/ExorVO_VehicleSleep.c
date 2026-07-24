// ============================================================================
// 3xor_Vanilla_Optimization - Sueno automatico de vehiculos (Fase 2.5, v3)
// El vehiculo NUNCA se mueve ni desaparece: queda visible en su lugar.
// Cuando esta inactivo y sin jugadores cerca, se le desactiva la simulacion
// fisica (DisableSimulation = el costo del vehiculo baja a ~cero).
// Cuando un jugador entra al radio configurado, despierta solo.
// Los jugadores no notan nada. El CE lo cuenta normal (no spawnea clones).
// ============================================================================
modded class CarScript
{
	protected int m_ExorLastActiveMs;
	protected bool m_ExorSleeping;
	protected int m_ExorLastNearMs;		// ultimo ms con un jugador cerca (para auto-virtualizar)

	// ===================== CANDADO DE AUTOS (code-lock 3xor) =====================
	// Ancla estable del auto para su JSON de candado (CarLocks/<id>.json). Se persiste
	// con lectura OPCIONAL -> retro-compatible (autos viejos no lo tienen y no se rompe).
	protected string m_ExorCarLockId;
	// Estado del candado en RAM (cargado del store en AfterStoreLoad). NO viven en el stream.
	protected string m_ExorLockKey;			// "" = sin candado
	protected string m_ExorLockSetterSid;	// steamid del que puso la clave
	protected string m_ExorLockGroup;		// clan dueño
	protected ref TStringArray m_ExorUnlockedBy;	// runtime: quienes ya metieron la clave (resetea al reiniciar)
	protected bool m_ExorCarLockedSync;		// SINCRONIZADO: hay candado? (lo mira el cliente para ocultar el baul)

	void CarScript()
	{
		// sync del bool "lockeado" (server -> cliente): cambia solo al poner/sacar candado.
		// Lo usan los gates del cliente (baul/piezas/puertas/subir) para saber si hay candado.
		RegisterNetSyncVariableBool("m_ExorCarLockedSync");
	}

	// NOTA: las acciones del candado se registran en el PLAYER (ver ExorStorage_Player.SetActions),
	// como Voltear vehiculo (la accion continua que SI dispara en autos). Aca NO se cuelgan: una
	// accion INTERACT en un auto la intercepta "subir al vehiculo"; las CONTINUAS registradas en el
	// player funcionan bien.

	override void EEInit()
	{
		super.EEInit();
		if (!m_ExorUnlockedBy)
			m_ExorUnlockedBy = new TStringArray;
		if (GetGame().IsServer())
		{
			m_ExorLastActiveMs = GetGame().GetTime();
			m_ExorLastNearMs = GetGame().GetTime();	// arranca "recien usado" -> no se auto-virtualiza al toque
			m_ExorSleeping = false;
			ExorVO_Manager.RegisterVehicle(this);

			// Fase H: quitar dano a vehiculos (no reciben dano de ningun tipo)
			if (GetExorConfig().vehiculos.dano.quitar_dano_vehiculos)
				SetAllowDamage(false);
		}
	}

	// ------------------ PERSISTENCIA POR POSICION (NO toca el stream) ------------------
	// CRITICO (aprendido en test local 23-jul): NO se hace ctx.Write en el stream de CarScript.
	// Agregar aunque sea 1 campo corrompe los autos ya guardados ("Scripted variables corrupted
	// upon FordRaptor..."), igual que la nevera. El candado se ancla por POSICION en un JSON
	// aparte: el auto siempre carga donde se guardo, asi que tipo+pos redondeada es una clave
	// estable entre save y load. OnStoreSave solo llama super (stream identico a vanilla) y de
	// PASO escribe el JSON del candado si el auto tiene uno.
	protected string m_ExorLockPosKeyGuardado;	// ultima clave-pos escrita (para borrar si el auto se movio)

	override void OnStoreSave(ParamsWriteContext ctx)
	{
		super.OnStoreSave(ctx);	// <-- NADA de ctx.Write nuestro: el stream queda igual a vanilla
		if (!GetGame().IsServer())
			return;
		if (m_ExorLockKey == "")
			return;	// sin candado -> no se escribe nada
		string key = ExorCarLockStore.KeyFor(GetType(), GetPosition());
		// OPT: los cambios de estado (clave/desbloqueo) YA se persisten al instante
		// (ExorCarPersistLockNow). Aca solo hace falta re-anclar si el auto SE MOVIO. Si la clave
		// de posicion no cambio, NO reescribir -> evita I/O redundante en CADA ciclo de guardado
		// para autos candados estacionados (con 15 autos = 15 escrituras menos por ciclo).
		if (key == m_ExorLockPosKeyGuardado)
			return;
		// se movio: borrar el archivo viejo (no dejar huerfano) y escribir en la nueva posicion
		if (m_ExorLockPosKeyGuardado != "")
			ExorCarLockStore.Clear(m_ExorLockPosKeyGuardado);
		ExorCarLockStore.Save(key, ExorCarEnsureId(), m_ExorLockKey, m_ExorLockSetterSid, m_ExorLockGroup, m_ExorUnlockedBy);
		m_ExorLockPosKeyGuardado = key;
	}

	// Al terminar de cargar, buscar el candado por la posicion de carga.
	override void AfterStoreLoad()
	{
		super.AfterStoreLoad();
		if (GetGame().IsServer())
			ExorCarLoadLockState();
	}

	protected void ExorCarLoadLockState()
	{
		m_ExorLockKey = "";
		m_ExorLockSetterSid = "";
		m_ExorLockGroup = "";
		string key = ExorCarLockStore.KeyFor(GetType(), GetPosition());
		ExorCarLockFile f = ExorCarLockStore.Load(key);
		if (!f)
			return;
		m_ExorCarLockId = f.car_id;
		m_ExorLockKey = f.clave;
		m_ExorLockSetterSid = f.setter_sid;
		m_ExorLockGroup = f.group_id;
		m_ExorLockPosKeyGuardado = key;
		// recuperar quienes ya habian ingresado la clave -> NO se la re-pide tras reiniciar
		if (!m_ExorUnlockedBy)
			m_ExorUnlockedBy = new TStringArray;
		m_ExorUnlockedBy.Clear();
		if (f.unlocked_by)
		{
			int u;
			for (u = 0; u < f.unlocked_by.Count(); u++)
				m_ExorUnlockedBy.Insert(f.unlocked_by.Get(u));
		}
		ExorCarSyncLocked();
	}

	// ------------------ API DEL CANDADO ------------------
	// asegura que el auto tenga un id estable (lo genera la 1ra vez que se le pone candado)
	string ExorCarEnsureId()
	{
		if (m_ExorCarLockId == "")
			m_ExorCarLockId = ExorVO_Serializer.GenerateId();
		return m_ExorCarLockId;
	}
	string ExorCarGetLockId()	{ return m_ExorCarLockId; }
	void ExorCarSetLockId(string id)	{ m_ExorCarLockId = id; }	// lo usa la virtualizacion al restaurar

	// SERVER: valor real (m_ExorLockKey). CLIENTE: el bool sincronizado (m_ExorLockKey no se
	// sincroniza). Sin esto, las acciones del cliente creen que el auto NO tiene candado.
	bool ExorCarHasLock()
	{
		if (GetGame().IsServer())
			return m_ExorLockKey != "";
		return m_ExorCarLockedSync;
	}
	string ExorCarGetLockKey()	{ return m_ExorLockKey; }
	string ExorCarGetSetter()	{ return m_ExorLockSetterSid; }
	string ExorCarGetLockGroup(){ return m_ExorLockGroup; }

	bool ExorCarKeyMatches(string k)
	{
		return m_ExorLockKey != "" && m_ExorLockKey == k;
	}

	// PONER/CAMBIAR clave. Persiste al JSON aparte (NO al stream del auto). setterSid/group
	// quedan para permisos y para el cleanup al expulsar al que la puso.
	void ExorCarSetKey(string key, string setterSid, string groupId)
	{
		ExorCarEnsureId();
		m_ExorLockKey = key;
		m_ExorLockSetterSid = setterSid;
		m_ExorLockGroup = groupId;
		if (m_ExorUnlockedBy)
			m_ExorUnlockedBy.Clear();	// al cambiar la clave, todos re-ingresan
		if (GetGame().IsServer())
			ExorCarPersistLockNow();	// escribe YA (no esperar al ciclo de guardado)
		ExorCarSyncLocked();
	}

	// SACAR el candado (raid exitoso o el dueño lo quita). Borra el JSON de su posicion.
	void ExorCarClearLock()
	{
		m_ExorLockKey = "";
		m_ExorLockSetterSid = "";
		m_ExorLockGroup = "";
		if (m_ExorUnlockedBy)
			m_ExorUnlockedBy.Clear();
		if (GetGame().IsServer())
		{
			ExorCarLockStore.Clear(ExorCarLockStore.KeyFor(GetType(), GetPosition()));
			if (m_ExorLockPosKeyGuardado != "")
				ExorCarLockStore.Clear(m_ExorLockPosKeyGuardado);	// por si el auto se movio
			m_ExorLockPosKeyGuardado = "";
		}
		ExorCarSyncLocked();
	}

	// re-aplica el estado del candado en RAM tras recrear el auto (virtualizacion) y lo persiste
	// en su posicion actual.
	void ExorCarApplyLockState(string key, string setterSid, string groupId, TStringArray unlockedBy = null)
	{
		m_ExorLockKey = key;
		m_ExorLockSetterSid = setterSid;
		m_ExorLockGroup = groupId;
		// RESTAURAR la lista de desbloqueados (viene de la virtualizacion). Antes se borraba, lo que
		// dejaba al DUEÑO como "no desbloqueado" tras sacar el auto del parking -> el gate lo trataba
		// como ajeno ("no me deja abrir, como si no fuese mio"). Ahora sobrevive el round-trip.
		if (!m_ExorUnlockedBy)
			m_ExorUnlockedBy = new TStringArray;
		m_ExorUnlockedBy.Clear();
		if (unlockedBy)
		{
			int u;
			for (u = 0; u < unlockedBy.Count(); u++)
				if (m_ExorUnlockedBy.Find(unlockedBy.Get(u)) < 0)
					m_ExorUnlockedBy.Insert(unlockedBy.Get(u));
		}
		if (GetGame().IsServer() && key != "")
			ExorCarPersistLockNow();
		ExorCarSyncLocked();
	}

	// lista de sids que ya desbloquearon (para capturarla en la virtualizacion del parking)
	TStringArray ExorCarGetUnlockedBy() { return m_ExorUnlockedBy; }

	// Tras des-virtualizar (parking OUT), el auto tiene un netid NUEVO -> el cliente no lo tiene en
	// sus sets s_Member/s_Access -> el dueño lo ve "como si no fuese suyo". Este metodo (auto-agendado
	// con delay para que el auto ya este replicado) le re-avisa a cada miembro online: sos miembro de
	// ESTE auto, y si ya estabas desbloqueado, aca tenes el acceso al baul de vuelta.
	void ExorCarScheduleMemberNotify()
	{
		if (!GetGame().IsServer())
			return;
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(ExorCarNotifyMembers, 2500, false);
	}

	void ExorCarNotifyMembers()
	{
		if (!GetGame().IsServer() || m_ExorLockKey == "")
			return;
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase pb = PlayerBase.Cast(players.Get(i));
			if (!pb)
				continue;
			string sid = ExorGroupManager.SteamId(pb);
			if (ExorCarIsMemberOfLockGroup(sid))
			{
				pb.ExorSendCarMember(this);
				if (ExorCarIsUnlockedBy(sid))
					pb.ExorSendCarAccessGrant(this);
			}
		}
	}

	// escribe el JSON del candado en la posicion ACTUAL del auto (y limpia el anterior si se movio)
	protected void ExorCarPersistLockNow()
	{
		if (m_ExorLockKey == "")
			return;
		string key = ExorCarLockStore.KeyFor(GetType(), GetPosition());
		if (m_ExorLockPosKeyGuardado != "" && m_ExorLockPosKeyGuardado != key)
			ExorCarLockStore.Clear(m_ExorLockPosKeyGuardado);
		ExorCarLockStore.Save(key, ExorCarEnsureId(), m_ExorLockKey, m_ExorLockSetterSid, m_ExorLockGroup, m_ExorUnlockedBy);
		m_ExorLockPosKeyGuardado = key;
	}

	// desbloqueo: una vez que un miembro mete la clave bien, no se le pide mas. Se PERSISTE
	// (a diferencia de los lockers viejos) para que sobreviva reconexion/reinicio.
	void ExorCarMarkUnlocked(string sid)
	{
		if (sid == "" || !m_ExorUnlockedBy)
			return;
		if (m_ExorUnlockedBy.Find(sid) >= 0)
			return;	// ya estaba
		m_ExorUnlockedBy.Insert(sid);
		if (GetGame().IsServer())
			ExorCarPersistLockNow();	// guardar la lista actualizada
	}
	bool ExorCarIsUnlockedBy(string sid)
	{
		return m_ExorUnlockedBy && sid != "" && m_ExorUnlockedBy.Find(sid) >= 0;
	}

	// ------------------ CONTROL DE ACCESO ------------------
	// admin del candado (bypass total, del array admin_steamids)
	bool ExorCarIsAdmin(string sid)
	{
		return GetExorConfig().carlock.ExorEsAdmin(sid);
	}

	// el sid tiene DERECHO sobre el candado de ESTE auto? (dueño del candado)
	//   - candado de CLAN (group != "") -> miembro ACTUAL del clan.
	//   - candado SOLO (group == "")     -> solo el que lo puso (setter).
	bool ExorCarIsMemberOfLockGroup(string sid)
	{
		if (sid == "")
			return false;
		if (m_ExorLockGroup == "")
			return sid == m_ExorLockSetterSid;	// candado personal (sin clan)
		ExorGroupManager gm = ExorGroupManager.Get();
		if (!gm)
			return false;
		ExorGroup g = gm.FindById(m_ExorLockGroup);
		return g != null && g.HasMember(sid);
	}

	// El auto le BLOQUEA la entrada a este player? (lo mira el gate de ActionGetInTransport)
	// Deja pasar SOLO si: es admin, O es MIEMBRO ACTUAL del clan dueño Y metio la clave.
	// Atado a la membresia ACTUAL a proposito: si te EXPULSAN del clan dejas de ser miembro
	// -> el gate te bloquea al toque (no hay que esperar al reinicio) -> pasas a ser ajeno y
	// solo podes RAIDEAR. El desbloqueo runtime (m_ExorUnlockedBy) por si solo no alcanza.
	bool ExorCarBlocksEntry(PlayerBase player)
	{
		if (!ExorCarHasLock())	// client-aware (bool sincronizado en el cliente)
			return false;	// sin candado -> nunca bloquea
		if (!player)
			return true;
		string sid = ExorGroupManager.SteamId(player);
		if (ExorCarIsAdmin(sid))
			return false;	// admin entra siempre (la lista de admins esta sincronizada)
		// SERVER: estado real (miembro con clave). CLIENTE: el set de acceso.
		if (GetGame().IsServer())
			return !(ExorCarIsMemberOfLockGroup(sid) && ExorCarIsUnlockedBy(sid));
		return !ExorCarAccessClient.HasAccess(this);
	}

	// --- ALARMA de auto durante el raid: soundset de EFECTO en LOOP (suena continuo) ---
	// El claxon del auto (CivilianSedan_Horn_...) NO se reproduce por SEffectManager (es un sonido
	// interno del vehiculo). Los que SI funcionan por script son los soundsets de efecto/bocina.
	// El SERVER dispara la alarma con un RPC al auto (llega a todos los clientes cercanos) que
	// LLEVA el soundset (el cliente no tiene la config del candado sincronizada). El CLIENTE lo
	// repite cada 1s = "TU TU TU" hasta que el raid termina.
	protected bool m_ExorAlarmClientRunning;	// solo cliente: el loop de bocina esta corriendo?
	protected string m_ExorAlarmClientSs;		// solo cliente: soundset a repetir (llega en el RPC)

	// RPC propio del auto para la alarma (S->C). Numero unico para no chocar con RPCs vanilla/otros mods.
	static const int EXOR_CAR_RPC_ALARM = 3011771;

	// SERVER: arranca la alarma en TODOS los clientes cercanos (RPC directo al auto, guaranteed).
	// El RPC es el unico camino que confirmamos que llega y se procesa en el cliente (el grant del
	// baul usa RPC y anda). Sync/OnStartClient no disparaban el sonido.
	// Manda el soundset DENTRO del RPC (el cliente NO tiene alarma_sonido sincronizado -> si lo
	// leyera de su config estaria vacio y no sonaria. Ese era el bug).
	void ExorCarStartAlarm()
	{
		if (!GetGame().IsServer())
			return;
		ExorWake();	// despertar el auto (dormido no reproduce ni sincroniza)
		RPCSingleParam(EXOR_CAR_RPC_ALARM, new Param2<bool, string>(true, GetExorConfig().carlock.alarma_sonido), true, null);
	}

	void ExorCarStopAlarm()
	{
		if (!GetGame().IsServer())
			return;
		RPCSingleParam(EXOR_CAR_RPC_ALARM, new Param2<bool, string>(false, ""), true, null);
	}

	// CLIENTE: llega el RPC de la alarma (on/off + soundset) -> arrancar/parar el sonido.
	override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
	{
		if (rpc_type == EXOR_CAR_RPC_ALARM)
		{
			Param2<bool, string> p = new Param2<bool, string>(false, "");
			if (ctx.Read(p))
			{
				if (p.param1)
					ExorCarStartAlarmClient(p.param2);
				else
					ExorCarStopAlarmClient();
			}
			return;
		}
		super.OnRPC(sender, rpc_type, ctx);
	}

	// CLIENTE: arranca el loop de sonido (idempotente). Lo llaman OnVariablesSynchronized y la
	// accion de raid (OnStartClient) -> el raider lo oye si o si, y los cercanos por el sync.
	void ExorCarStartAlarmClient(string ss)
	{
		if (GetGame().IsServer() || m_ExorAlarmClientRunning || ss == "")
			return;
		m_ExorAlarmClientRunning = true;
		m_ExorAlarmClientSs = ss;
		ExorCarAlarmBeepClient();	// primer bocinazo ya
	}

	void ExorCarStopAlarmClient()
	{
		if (GetGame().IsServer())
			return;
		m_ExorAlarmClientRunning = false;
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).Remove(ExorCarAlarmBeepClient);
	}

	// CLIENTE: un bocinazo + agenda el siguiente cada 1s = "TU TU TU" mientras dure el raid.
	void ExorCarAlarmBeepClient()
	{
		if (!m_ExorAlarmClientRunning)
			return;
		if (m_ExorAlarmClientSs != "")
		{
			EffectSound snd = SEffectManager.PlaySoundOnObject(m_ExorAlarmClientSs, this);
			if (snd)
				snd.SetAutodestroy(true);
		}
		GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(ExorCarAlarmBeepClient, 1000, false);
	}

	// mantiene el bool sincronizado al dia (lo llaman set/clear/load/apply)
	protected void ExorCarSyncLocked()
	{
		if (!GetGame().IsServer())
			return;
		bool locked = m_ExorLockKey != "";
		if (m_ExorCarLockedSync != locked)
		{
			m_ExorCarLockedSync = locked;
			SetSynchDirty();
		}
	}

	// ------------------ BLOQUEO TOTAL DEL INVENTARIO (auto lockeado, sin acceso) ------------------
	// El ajeno NO debe tocar NADA: ni el baul (cargo), ni las piezas (ruedas/bateria/puertas como
	// attachment), ni abrir puertas. Estos gates corren en el CLIENTE (cada refresco del inventario
	// -> O(1)); si el auto esta lockeado (bool sync) y el player local NO tiene acceso, se ocultan/
	// bloquean. El dueño que metio la clave (en el set) los ve normal. En el SERVER se deja el
	// vanilla (los metodos no reciben el player; el cliente ya lo gatea, y puertas/entrada tienen
	// su propio gate server-authoritative con el player).
	protected bool ExorCarClientNoAccess()
	{
		if (GetGame().IsServer())
			return false;
		if (!GetExorConfig().carlock.activado)
			return false;
		if (!m_ExorCarLockedSync)
			return false;	// sin candado (segun el sync)
		return !ExorCarAccessClient.HasAccess(this);
	}

	override bool CanDisplayCargo()
	{
		if (ExorCarClientNoAccess())
			return false;
		return super.CanDisplayCargo();
	}

	override bool CanDisplayAttachmentSlot(int slot_id)
	{
		if (ExorCarClientNoAccess())
			return false;
		return super.CanDisplayAttachmentSlot(slot_id);
	}

	override bool CanReleaseAttachment(EntityAI attachment)
	{
		if (ExorCarClientNoAccess())
			return false;	// no sacar ruedas/bateria/puertas de un auto ajeno lockeado
		return super.CanReleaseAttachment(attachment);
	}

	override bool CanReleaseCargo(EntityAI cargo)
	{
		if (ExorCarClientNoAccess())
			return false;
		return super.CanReleaseCargo(cargo);
	}

	bool ExorIsSleeping()
	{
		return m_ExorSleeping;
	}

	void ExorSleep()
	{
		if (m_ExorSleeping)
			return;
		m_ExorSleeping = true;
		DisableSimulation(true);
	}

	void ExorWake()
	{
		if (!m_ExorSleeping)
			return;
		m_ExorSleeping = false;
		DisableSimulation(false);
		m_ExorLastActiveMs = GetGame().GetTime();
	}

	bool ExorIsActive()
	{
		if (EngineIsOn())
			return true;
		int i;
		for (i = 0; i < CrewSize(); i++)
		{
			if (CrewMember(i))
				return true;
		}
		return false;
	}

	void ExorMarkActive(int now)
	{
		m_ExorLastActiveMs = now;
	}

	int ExorGetLastActive()
	{
		return m_ExorLastActiveMs;
	}

	// --- auto-virtualizado: timer de "ultimo jugador cerca" ---
	void ExorSetLastNear(int now)
	{
		m_ExorLastNearMs = now;
	}
	int ExorGetLastNear()
	{
		return m_ExorLastNearMs;
	}
}
