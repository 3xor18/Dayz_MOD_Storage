// ============================================================================
// 3xor_Vanilla_Optimization - modded PlayerBase (UNICO del mod)
// Enforce no permite dos 'modded class PlayerBase' en el mismo mod, asi que
// TODO lo del jugador vive aca: acciones (storage) + party (Fase B).
// ============================================================================

modded class PlayerBase
{
	// --- Party ---
	protected string m_ExorGroupId;              // server: id del grupo al que pertenece ("" = ninguno)
	protected ref ExorRosterDTO m_ExorRoster;    // cliente: copia del roster (menu P / HUD)
	protected ref ExorPendingInvite m_ExorInvite;// cliente: invitacion pendiente para mostrar en el menu

	// --- Killfeed (server): ultimo daño recibido, para saber arma/atacante al morir ---
	protected EntityAI m_ExorKfSource;           // entidad que causo el ultimo daño (arma/atacante)
	protected string m_ExorKfAmmo;               // tipo de municion/daño del ultimo golpe (clasifica gas/mina/explosivo)
	protected int m_ExorKfMs;                    // uptime ms del ultimo golpe (para no clasificar como "caida" un F11/suicidio despues de una caida vieja)

	// --- Bolsa de cadaver (server): ropa copiada + armas/manos (entidades reales a mover) ---
	protected ref array<ref ExorVO_ItemData> m_ExorDeathLoot;
	protected ref array<EntityAI> m_ExorDeathWeapons;	// 'moveItems' de SpawnFromLoot: MUEVE la entidad real (hoy vacio)
	protected ref array<EntityAI> m_ExorDeathOriginals;	// originales del loadout, a destruir tras recrearlo (anti-dupe)

	// --- Auto-run (cliente, jugador local) ---
	protected bool m_ExorAutoRun;       // auto-run activo
	protected int  m_ExorLastScoreReqMs;	// throttle del pedido de leaderboard (SCORE_REQ)
	protected bool m_ExorArKeyPrev;     // estado previo de la tecla (deteccion de flanco)
	protected bool m_ExorArApplied;     // el override esta puesto (para soltarlo 1 sola vez al apagar)
	protected bool m_ExorArTired;       // sin stamina -> baja a trote sin sprint hasta recuperar (histeresis)

	// --- Camara en vehiculo (cliente): el conductor elige 1ra/3ra con V ---
	protected bool m_ExorVeh1pp;        // el conductor eligio 1ra persona (default false = 3ra)
	protected bool m_ExorVehVPrev;      // estado previo de la tecla V (deteccion de flanco)
	protected bool m_ExorIsStaff;       // sincronizado: esta en bypass_lootear_steamids (staff)

	void PlayerBase()
	{
		// El SERVER decide el cansancio del auto-run (tiene la stamina real) y lo SINCRONIZA
		// al cliente con esta variable -> ambos lados aplican el MISMO speed -> sin rubber-band
		// (glisheo atras-adelante). Ver ExorAutoRunTick.
		RegisterNetSyncVariableBool("m_ExorArTired");
		// STAFF sincronizado: el CLIENTE no puede saber si sos staff. GetIdentity() es null del
		// lado cliente, asi que cualquier chequeo por SteamID ahi da "" y falla en silencio (era
		// por esto que la accion de admin no aparecia en el menu). El SERVER lo resuelve al
		// conectar y lo sincroniza; el cliente solo lee este bool.
		RegisterNetSyncVariableBool("m_ExorIsStaff");
	}

	// true = este jugador esta en storage.json -> bypass_lootear_steamids. Valido en AMBOS
	// lados (el server lo setea, el cliente lo recibe sincronizado).
	bool ExorIsStaff()
	{
		return m_ExorIsStaff;
	}

	// lo llama el server al conectar/respawnear
	void ExorSetStaff(bool v)
	{
		if (m_ExorIsStaff == v)
			return;
		m_ExorIsStaff = v;
		SetSynchDirty();
	}

	// ------------------------- TEST LOCAL: equipar NPC dummy de VPP -------------------------
	// Al spawnear "player" con VPP Admin Tools sale un PlayerBase SIN identidad (dummy). Si el
	// flag spawns.equipar_npc_test esta on (SOLO local), lo equipamos con ropa+mochila+armas
	// para poder matarlo y probar la bolsa de cadaver con loot real. Un jugador REAL tiene
	// identidad a los pocos segundos -> el chequeo diferido (4s) lo descarta y nunca lo toca.
	// --- Anti-dupe: momento en que este jugador entro (uptime ms del server) ---
	// El bloqueo de apertura se mide POR JUGADOR, no desde el arranque de la mision: el que
	// rushea el barril despues de un reinicio es el que acaba de conectarse, y medirlo desde
	// la mision no lo agarra (cuando el 1er player entra ya pasaron los segundos).
	protected int m_ExorConnectMs;
	int ExorConnectMs() { return m_ExorConnectMs; }

	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
			m_ExorConnectMs = GetGame().GetTime();
		if (GetGame().IsServer() && GetExorConfig().spawns.equipar_npc_test)
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorEquipDummyIfNpc, 4000, false);
	}

	void ExorEquipDummyIfNpc()
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (GetIdentity())
			return;	// jugador real -> NO tocar
		if (!IsAlive())
			return;
		ExorGiveTestKit();
		Print(string.Format("%1 TEST: NPC dummy equipado (ropa+mochila+armas) para probar la bolsa", ExorStorageConstants.LOG));
	}

	void ExorGiveTestKit()
	{
		GameInventory inv = GetInventory();
		if (!inv)
			return;
		// ropa + mochila (la bolsa las COPIA en sus slots al morir)
		inv.CreateInInventory("Mich2001Helmet");
		inv.CreateInInventory("TacticalShirt_Black");
		inv.CreateInInventory("Jeans_Blue");
		inv.CreateInInventory("MilitaryBoots_Black");
		inv.CreateInInventory("PlateCarrierVest");
		inv.CreateInInventory("PlateCarrierHolster");
		inv.CreateInInventory("PlateCarrierPouches");
		inv.CreateInInventory("AssaultBag_Black");
		// arma EN MANOS + cargador + mira (la bolsa MUEVE el arma real -> conserva mag/miras)
		EntityAI rifle = GetHumanInventory().CreateInHands("M4A1");
		if (rifle && rifle.GetInventory())
		{
			rifle.GetInventory().CreateAttachment("Mag_STANAG_30Rnd");
			rifle.GetInventory().CreateAttachment("ACOGOptic");
		}
		// arma AL HOMBRO/ESPALDA: CreateAttachment fuerza el slot del arma (Shoulder/Melee).
		// OJO: CreateInInventory la metia en la MOCHILA, no en el hombro. Con cargador para
		// probar que la bolsa conserva el arma REAL + su mag al matarla de lejos.
		EntityAI backRifle = inv.CreateAttachment("AKM");
		if (backRifle && backRifle.GetInventory())
			backRifle.GetInventory().CreateAttachment("Mag_AKM_30Rnd");
		// items sueltos para que la bolsa tenga loot variado
		inv.CreateInInventory("Rangefinder");
		inv.CreateInInventory("FieldShovel");
		inv.CreateInInventory("CombatKnife");
		inv.CreateInInventory("BandageDressing");
	}

	// Auto-run: se corre DENTRO del command handler (timing correcto, no en el update de
	// la mision) para que el override de movimiento sea fluido, como los mods de auto-run.
	override bool ModCommandHandlerBefore(float pDt, int pCurrentCommandID, bool pCurrentCommandFinished)
	{
		ExorAutoRunTick();
		return super.ModCommandHandlerBefore(pDt, pCurrentCommandID, pCurrentCommandFinished);
	}

	void ExorAutoRunTick()
	{
		// EARLY-OUT SERVER: esto corre POR JUGADOR y POR FRAME (desde ModCommandHandlerBefore),
		// o sea es el unico codigo del mod en el camino critico del hilo que es el cuello de
		// botella. Para la enorme mayoria de jugadores el auto-run esta APAGADO y no hay nada
		// que aplicar ni que soltar, asi que no vale la pena ni pedir el input controller ni
		// evaluar canRun (5 llamadas virtuales). El cliente sigue entrando siempre porque es
		// el que lee la tecla; el server solo simula cuando hay estado real.
		if (GetGame().IsServer() && GetGame().GetPlayer() != this && !m_ExorAutoRun && !m_ExorArApplied)
			return;

		HumanInputController hic = GetInputController();
		if (!hic)
			return;

		ExorCfgAutorun cfg = GetExorConfig().autorun;
		if (!cfg || !cfg.habilitado)
		{
			ExorAutoRunRelease(hic);
			return;
		}

		bool canRun = IsAlive() && !IsUnconscious() && !IsRestrained() && !GetCommand_Vehicle() && !IsSwimming();

		// === CLIENTE (jugador local): detecta la tecla y avisa al server ===
		// El server NO lee teclado; recibe el estado por RPC AUTORUN_SET y simula igual.
		if (GetGame().GetPlayer() == this)
		{
			bool blocked = GetGame().IsInventoryOpen();
			if (GetGame().GetUIManager() && GetGame().GetUIManager().GetMenu())
				blocked = true;

			bool want = m_ExorAutoRun;
			bool keyDown = KeyState(cfg.tecla) > 0;
			if (keyDown && !m_ExorArKeyPrev && !blocked && canRun)
				want = !m_ExorAutoRun;
			m_ExorArKeyPrev = keyDown;

			// tocar CUALQUIER tecla de movimiento (W/A/S/D) cancela (el jugador retoma el control)
			if (want && cfg.parar_con_movimiento)
			{
				bool movKey = KeyState(KeyCode.KC_W) > 0 || KeyState(KeyCode.KC_A) > 0 || KeyState(KeyCode.KC_S) > 0 || KeyState(KeyCode.KC_D) > 0;
				if (movKey)
					want = false;
			}
			if (!canRun)
				want = false;

			if (want != m_ExorAutoRun)
			{
				m_ExorAutoRun = want;
				// avisar al server para que simule el mismo movimiento (anti rubber-band)
				RPCSingleParam(ExorRPC.AUTORUN_SET, new Param1<bool>(m_ExorAutoRun), true, null);
			}
		}

		// === AMBOS LADOS: aplicar/soltar el override de movimiento ===
		if (m_ExorAutoRun && canRun)
		{
			// ENABLED = se mantiene hasta pasar DISABLED. velocidad 1=caminar/2=trotar/3=sprint.
			float speed = cfg.velocidad;
			// Si pide SPRINT (3) y se queda SIN stamina, forzar sprint traba/para al personaje
			// -> bajar a TROTE/sin-sprint (2), el movimiento natural sin stamina (como los mods
			// de auto-run), asi sigue avanzando fluido. Vuelve a sprint cuando recupera.
			// Histeresis (m_ExorArTired): se cansa a ~0 y recien re-sprinta al recuperar ~40.
			// SOLO el SERVER decide (tiene la stamina real) y lo SINCRONIZA -> cliente y server
			// aplican el MISMO speed -> sin glisheo. Cliente solo LEE.
			if (speed >= 3)
			{
				if (GetGame().IsServer())
				{
					PlayerStat<float> stStam = GetStatStamina();
					if (stStam)
					{
						float stam = stStam.Get();
						bool tired = m_ExorArTired;
						if (tired)
						{
							if (stam >= 40)
								tired = false;
						}
						else if (stam <= 1)
						{
							tired = true;
						}
						if (tired != m_ExorArTired)
						{
							m_ExorArTired = tired;
							SetSynchDirty();
						}
					}
				}
				if (m_ExorArTired)
					speed = 2;	// trote sin sprint (movimiento natural sin stamina) mientras recupera
			}
			hic.OverrideMovementSpeed(HumanInputControllerOverrideType.ENABLED, speed);
			hic.OverrideMovementAngle(HumanInputControllerOverrideType.ENABLED, 0);
			m_ExorArApplied = true;
		}
		else
		{
			if (m_ExorArTired)	// reset al apagar el auto-run (server sincroniza)
			{
				m_ExorArTired = false;
				if (GetGame().IsServer())
					SetSynchDirty();
			}
			if (m_ExorAutoRun && !canRun)
				m_ExorAutoRun = false;	// server: limpiar si el jugador ya no puede correr
			ExorAutoRunRelease(hic);
		}
	}

	// suelta el override 1 sola vez (si estaba puesto), si no seguiria corriendo
	void ExorAutoRunRelease(HumanInputController hic)
	{
		if (!m_ExorArApplied || !hic)
			return;
		hic.OverrideMovementSpeed(HumanInputControllerOverrideType.DISABLED, 0);
		hic.OverrideMovementAngle(HumanInputControllerOverrideType.DISABLED, 0);
		m_ExorArApplied = false;
	}

	// ------------------------- acciones -------------------------
	override void SetActions(out TInputActionMap InputActionMap)
	{
		super.SetActions(InputActionMap);

		AddAction(ExorActionPackBarrel, InputActionMap);
		// "Eliminar definitivamente (admin)": se hace MIRANDO el barril/mueble con las manos
		// vacias, asi que va aca (registrarla solo en ActionConstructor NO alcanza: no aparece).
		// La accion misma solo se muestra a los SteamID de staff. Ver ExorActionAdminRemove.
		AddAction(ExorActionAdminRemove, InputActionMap);
		AddAction(ExorActionFlipVehicle, InputActionMap);
		// candado de autos: continuas (mantener F), targetean el auto -> mismo patron que Voltear
		AddAction(ExorActionSetCarKey, InputActionMap);
		AddAction(ExorActionEnterCarKey, InputActionMap);
		AddAction(ExorActionRaidCarLock, InputActionMap);
		AddAction(ExorActionOpenInvite, InputActionMap);
		AddAction(ExorActionJoinGroup, InputActionMap);
		AddAction(ExorActionCancelInvite, InputActionMap);
		AddAction(ExorActionOpenPartyMenu, InputActionMap);
		AddAction(ExorActionRaiseFlag, InputActionMap);
		AddAction(ExorActionLowerFlag, InputActionMap);
		// Salir / Sacar miembro: desde el menu "Administrar party" (RPC).
		// Poner/Limpiar marca ya NO van en la rueda del mouse: se usan con T / Y
		// (ver ExorMarkerKeys en ExorPartyHud.c). Las clases de accion siguen
		// existiendo pero no se registran aca.
	}

	// ------------------------- camara por asiento en vehiculos (cliente) -------------------------
	// HandleView corre cada frame en el path de camara. super() ya resolvio el toggle
	// vanilla (y en server 1pp puso m_Camera3rdPerson=false). Nosotros re-forzamos
	// DESPUES segun el asiento + config:
	//   - Pasajeros con pasajeros_1ra_persona=on  -> 1ra forzada (siempre, anti-peek).
	//   - Conductor con conductor_3ra_persona=off  -> 1ra forzada.
	//   - Conductor con conductor_3ra_persona=on   -> ELIGE 1ra/3ra con la tecla V (default
	//     3ra). En server 1pp el engine resetea a 1ra cada frame y bloquea el toggle nativo,
	//     asi que detectamos la V nosotros y forzamos la vista ELEGIDA cada frame.
	//   - Idem pasajeros si pasajeros_1ra_persona=off (3ra permitida).
	override void HandleView()
	{
		super.HandleView();

		// SOLO CLIENTE: toda la logica de abajo es de camara/UI/input (SetIsInThirdPerson,
		// GetUIManager, IsInventoryOpen, KeyState) y es ILEGAL en el server. HandleView corre
		// tambien server-side para cada ocupante de un vehiculo -> sin este guard tiraba la
		// excepcion "Calling GetUIManager on server is illegal" CADA FRAME por jugador en auto
		// (cientos de miles en el RPT del server real). La camara es 100% client-side; el server
		// no necesita nada de esto (el 1pp forzado lo aplica el engine via Is3rdPersonDisabled).
		if (!GetGame().IsClient())
			return;

		HumanCommandVehicle hcv = GetCommand_Vehicle();
		if (!hcv)
			return;
		if (hcv.IsGettingIn() || hcv.IsGettingOut())
			return;	// no tocar durante las transiciones de entrada/salida

		ExorCfgVehCamara cam = GetExorConfig().vehiculos.camara;
		bool server1pp = GetGame().GetWorld().Is3rdPersonDisabled();
		bool isDriver = (hcv.GetVehicleSeat() == DayZPlayerConstants.VEHICLESEAT_DRIVER);

		if (isDriver)
		{
			if (!cam.conductor_3ra_persona)
			{
				SetIsInThirdPerson(false);	// 3ra NO permitida para el conductor -> 1ra forzada
			}
			else
			{
				// 3ra permitida: el conductor ELIGE con V (default 3ra). Detectamos el flanco
				// de la tecla (no togglear mientras un menu/inventario esta abierto) y forzamos
				// la vista elegida cada frame (en server 1pp el engine la resetearia si no).
				bool vBlocked = GetGame().IsInventoryOpen();
				if (GetGame().GetUIManager() && GetGame().GetUIManager().GetMenu())
					vBlocked = true;
				bool vDown = KeyState(KeyCode.KC_V) > 0;
				if (vDown && !m_ExorVehVPrev && !vBlocked)
					m_ExorVeh1pp = !m_ExorVeh1pp;
				m_ExorVehVPrev = vDown;
				SetIsInThirdPerson(!m_ExorVeh1pp);	// false=3ra (default), true=1ra
			}
		}
		else
		{
			if (cam.pasajeros_1ra_persona)
				SetIsInThirdPerson(false);	// pasajeros forzados a 1ra
			else if (server1pp)
				SetIsInThirdPerson(true);	// server 1pp + 3ra permitida: re-forzar a pasajeros
		}
	}

	// ------------------------- killfeed (server) -------------------------
	// Registra la entidad que causo el ultimo daño (arma o atacante) para poder
	// armar el mensaje al morir.
	override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
		if (!GetGame().IsServer())
			return;
		m_ExorKfSource = source;
		m_ExorKfAmmo = ammo;	// para clasificar muerte por gas/mina/claymore/explosivo improvisado
		m_ExorKfMs = GetGame().GetTime();	// cuando fue (para descartar caidas viejas al morir por F11/suicidio)

		// forense de la tumba: cachear la identidad AHORA (el player esta vivo). En muertes
		// por explosivo/granada GetIdentity() ya es null en EEKilled -> sin este cache el
		// "muerto" quedaria "?". Se cachea en cada hit (barato) mientras la identidad es valida.
		if (GetIdentity())
		{
			m_ExorCachedName = GetIdentity().GetName();
			m_ExorCachedSid = GetIdentity().GetPlainId();
		}

		// Combat-log (modelo de ZONA): si el daño viene de OTRO jugador, registrar una
		// zona de combate en AMBOS extremos (victima y atacante) -> cubre PvP corto y largo.
		// La deteccion al desloguearse es por PRESENCIA en la zona, no por intercambio de daño.
		if (!source || !GetIdentity())
			return;
		PlayerBase atk = PlayerBase.Cast(source.GetHierarchyRootPlayer());
		if (!atk || atk == this || !atk.GetIdentity())
			return;
		ExorCombatZones.Register(GetPosition());
		ExorCombatZones.Register(atk.GetPosition());
	}

	protected bool m_ExorDeathDone;   // guard: killfeed+tumba UNA sola vez por muerte
	protected string m_ExorDeathName; // forense de la tumba: quien murio (capturado con la identidad viva)
	protected string m_ExorDeathSid;
	protected string m_ExorCachedName; // ultima identidad valida (cache de EEHitBy) para muertes por explosivo
	protected string m_ExorCachedSid;

	// Al morir: killfeed + programar la bolsa de cadaver.
	// GUARD: una granada/explosion dispara EEKilled ~11 veces en el mismo instante (cada
	// fragmento cuenta) -> sin guard salian 6-7 killfeeds y 6-7 tumbas de UNA sola muerte.
	// Ahora se procesa SOLO la primera vez (m_ExorDeathDone). Al respawnear es otra entidad
	// PlayerBase (flag nuevo en false), asi que la proxima muerte se procesa normal.
	override void EEKilled(Object killer)
	{
		if (GetGame().IsServer() && !m_ExorDeathDone)
		{
			m_ExorDeathDone = true;
			ExorBuildKillfeed(killer);
			ExorScheduleBodyBag();
		}
		super.EEKilled(killer);
	}

	// Programa la conversion del cuerpo en bolsa de cadaver (server).
	void ExorScheduleBodyBag()
	{
		ExorCfgBodyCadaver cfg = GetExorConfig().bodycadaver;
		if (!cfg.habilitado)
			return;

		// forense: capturar quien murio. GetIdentity() vale para muertes normales; en muertes
		// por explosivo/granada ya es null aca -> fallback al cache de EEHitBy (ultima identidad
		// viva). Un dummy del VPP admin no tiene identidad ni cache -> queda "?" (correcto).
		m_ExorDeathName = "?";
		m_ExorDeathSid = "";
		if (GetIdentity())
		{
			m_ExorDeathName = GetIdentity().GetName();
			m_ExorDeathSid = GetIdentity().GetPlainId();
		}
		else if (m_ExorCachedSid != "")
		{
			m_ExorDeathName = m_ExorCachedName;
			m_ExorDeathSid = m_ExorCachedSid;
		}

		// Con el cuerpo INTACTO: la ROPA se copia (capturar+recrear, cae en sus slots)
		// y las ARMAS se guardan para MOVER la entidad real (copiar un arma pierde el
		// cargador). 1s despues el motor ya pudo dropear cosas, por eso se hace aca.
		// TODO el loadout se CAPTURA a datos y se RECREA dentro de la bolsa (ropa Y armas).
		// Antes las armas se MOVIAN como entidad real desde el cuerpo y el cuerpo se borraba
		// en el mismo frame -> el cliente perdia el re-parent del arma (quedaba en la bolsa
		// server-side pero INVISIBLE client-side). Recrearlas (mismo camino que la ropa, que
		// SI se ve) garantiza que se repliquen. El serializer conserva cargador (ammo) + miras
		// + supresor via SpawnAttachedMagazine/TakeEntityAsAttachment (solo se pierde la bala
		// en recamara, despreciable).
		m_ExorDeathLoot = new array<ref ExorVO_ItemData>;
		// ANTI-DUPE: guardar la ENTIDAD REAL de todo lo que se captura, no solo sus datos.
		// El loadout se recrea dentro de la bolsa, asi que el original TIENE que desaparecer.
		// Borrar el cadaver no alcanza: durante el delay el motor de DayZ dropea parte del
		// equipo AL PISO (el comentario de arriba ya lo dice), y una vez en el piso el item
		// dejo de ser hijo del cadaver -> ObjectDelete(this) no se lo lleva y queda duplicado
		// (uno en la tumba, otro tirado). Por eso "a veces se dupea el arma": depende de si
		// el motor alcanzo a dropearla antes del delete, o sea DEPENDE DEL LAG.
		// La referencia EntityAI sigue siendo valida aunque el item cambie de padre, asi que
		// guardarlas y borrarlas explicitamente cubre los dos casos.
		// OJO: m_ExorDeathWeapons se pasa a SpawnFromLoot como 'moveItems' y ESE camino MUEVE
		// la entidad real a la bolsa. Como aca se recrea TODO el loadout desde datos, tiene
		// que quedar VACIO: si se llenara, cada item entraria dos veces (uno recreado + uno
		// movido). Los originales a destruir van en su propio array.
		m_ExorDeathWeapons = new array<EntityAI>;	// vacio a proposito (todo se recrea)
		m_ExorDeathOriginals = new array<EntityAI>;	// entidades originales a destruir
		GameInventory inv = GetInventory();
		int natt = 0;
		if (inv)
		{
			natt = inv.AttachmentCount();
			int i;
			for (i = 0; i < natt; i++)
			{
				EntityAI att = inv.GetAttachmentFromIndex(i);
				if (!att)
					continue;
				m_ExorDeathLoot.Insert(ExorVO_Serializer.CaptureItem(att));	// ropa Y armas puestas -> recrear
				m_ExorDeathOriginals.Insert(att);	// y anotar el original para destruirlo
			}
		}
		// lo que tenga en manos (arma u otro) tambien se captura -> recrear en la bolsa
		EntityAI hands = null;
		if (GetHumanInventory())
			hands = GetHumanInventory().GetEntityInHands();
		if (hands)
		{
			m_ExorDeathLoot.Insert(ExorVO_Serializer.CaptureItem(hands));
			m_ExorDeathOriginals.Insert(hands);
		}

		Print(string.Format("%1 muerte: %2 items (loadout completo) para la bolsa (attachments=%3)", ExorStorageConstants.LOG, m_ExorDeathLoot.Count(), natt));

		int delayMs = cfg.delay_segundos * 1000;
		if (delayMs < 1)
			delayMs = 1;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorDoSpawnBodyBag, delayMs, false);
	}

	// 'this' es el cadaver -> spawnear la bolsa con TODO el loadout recreado (ropa + armas).
	void ExorDoSpawnBodyBag()
	{
		// GUARD "NOQUEADO NO ES MUERTO": entre EEKilled y este callback pasa delay_segundos.
		// Si en ese lapso resulta que el jugador NO esta muerto (quedo inconsciente y el
		// motor lo revivio, o EEKilled entro por un desync/rollback), NO hay que generar
		// ninguna tumba: se le estaria clonando el loadout a un jugador vivo.
		// Se chequea el estado REAL ahora, no el de hace un segundo.
		if (IsAlive())
		{
			Print(string.Format("%1 tumba CANCELADA para %2: el jugador sigue vivo (noqueado/desync, no muerte real)", ExorStorageConstants.LOG, m_ExorDeathName));
			m_ExorDeathDone = false;	// si mas tarde muere de verdad, que se procese normal
			m_ExorDeathLoot = null;
			m_ExorDeathOriginals = null;
			return;
		}

		// GUARD ANTI-TUMBA-DOBLE: si ya se genero una tumba para este steamid hace muy poco,
		// no crear otra. m_ExorDeathDone protege contra el EEKilled multiple de una granada,
		// pero es POR INSTANCIA de PlayerBase; si el motor tiene dos instancias del mismo
		// personaje (desync, combat-log, reconexion durante el logout timer) cada una entra
		// aca con su flag propio en false y salen 2 tumbas del mismo muerto.
		if (m_ExorDeathSid != "" && ExorBodyBagGuard.YaGenero(m_ExorDeathSid))
		{
			Print(string.Format("%1 tumba DUPLICADA evitada para %2 (%3): ya se genero una hace instantes", ExorStorageConstants.LOG, m_ExorDeathName, m_ExorDeathSid));
			GetGame().ObjectDelete(this);
			return;
		}

		Exor_BodyBag bag = Exor_BodyBag.SpawnFromLoot(GetPosition(), m_ExorDeathLoot, m_ExorDeathWeapons);
		if (!bag)
			return;
		if (m_ExorDeathSid != "")
			ExorBodyBagGuard.Marcar(m_ExorDeathSid);

		// forense: registrar la tumba (muerto/pos/fecha/items) en tumbas\<id>.json
		ExorTumbaForense.Registrar(bag.ExorGetID(), bag.GetPosition(), m_ExorDeathName, m_ExorDeathSid, ExorTimeUtil.NowMinutes(), m_ExorDeathLoot);

		// ANTI-DUPE: destruir los ORIGINALES. El loot ya se recreo dentro de la bolsa, asi que
		// todo lo que quedo del loadout es una copia sobrante. Hay que borrarlo explicitamente
		// porque durante el delay el motor pudo dropear parte al piso, y eso ya no cuelga del
		// cadaver -> el ObjectDelete de abajo no lo alcanzaria (arma duplicada: una en la
		// tumba, otra tirada en el suelo).
		int borrados = 0;
		if (m_ExorDeathOriginals)
		{
			int i;
			for (i = 0; i < m_ExorDeathOriginals.Count(); i++)
			{
				EntityAI orig = m_ExorDeathOriginals.Get(i);
				if (!orig)
					continue;	// ya lo destruyo el motor
				// no tocar lo que haya terminado DENTRO de la bolsa (el camino moveItems no
				// se usa hoy, pero si alguien lo reactiva esto evita borrar el loot bueno)
				if (orig.GetHierarchyParent() == bag)
					continue;
				GetGame().ObjectDelete(orig);
				borrados++;
			}
		}
		// 1 linea por muerte, a proposito NO en debug: es la confirmacion de que el anti-dupe
		// actuo. Si un arma aparece a la vez en la tumba y en el piso, este numero dice
		// cuantos originales se alcanzaron a destruir y permite comparar con el loadout.
		if (borrados > 0)
			Print(string.Format("%1 tumba: %2 originales del loadout destruidos (anti-dupe)", ExorStorageConstants.LOG, borrados));

		// Ya es seguro borrar el cuerpo: las armas se RECREARON en la bolsa (no se movio la
		// entidad real) y los originales se destruyeron arriba.
		GetGame().ObjectDelete(this);
	}

	void ExorDbgBag(string ev)
	{
		if (ExorStorageConstants.DEBUG_BARRELS)
			Print(string.Format("%1 [dbg] tumba: %2", ExorStorageConstants.LOG, ev));
	}

	// Nombre legible del arma usada (server).
	string ExorKfWeaponName(PlayerBase killer)
	{
		Weapon_Base w = Weapon_Base.Cast(m_ExorKfSource);
		if (w)
			return w.GetDisplayName();
		ItemBase ib = ItemBase.Cast(m_ExorKfSource);
		if (ib)
			return ib.GetDisplayName();
		if (killer && killer.GetHumanInventory())
		{
			ItemBase inHands = ItemBase.Cast(killer.GetHumanInventory().GetEntityInHands());
			if (inHands)
				return inHands.GetDisplayName();
		}
		return "puños";
	}

	// Classname del arma usada (server). Para el umbral de distancia por arma del anti-cheat.
	string ExorKfWeaponClass(PlayerBase killer)
	{
		if (m_ExorKfSource)
			return m_ExorKfSource.GetType();
		if (killer && killer.GetHumanInventory())
		{
			EntityAI inHands = killer.GetHumanInventory().GetEntityInHands();
			if (inHands)
				return inHands.GetType();
		}
		return "";
	}

	void ExorBuildKillfeed(Object killer)
	{
		string victimName = "?";
		string victimSid = "";
		if (GetIdentity())
		{
			victimName = GetIdentity().GetName();
			victimSid = GetIdentity().GetPlainId();
		}

		ExorCfgKillfeed cfg = GetExorConfig().killfeed;

		// quien mato: el param killer, o la raiz de la fuente de daño registrada
		PlayerBase kp = PlayerBase.Cast(killer);
		if (!kp && m_ExorKfSource)
			kp = PlayerBase.Cast(m_ExorKfSource.GetHierarchyRootPlayer());

		// ---- clasificar la fuente del daño por classname + tipo de municion ----
		string st = "";
		if (m_ExorKfSource)
			st = m_ExorKfSource.GetType();
		st.ToLower();
		string am = m_ExorKfAmmo;
		am.ToLower();

		bool isGas      = st.Contains("contaminat") || st.Contains("toxic") || st.Contains("chemgas") || st.Contains("poison") || am.Contains("contaminat") || am.Contains("toxic") || am.Contains("poison") || am.Contains("chemgas");
		bool isClaymore = st.Contains("claymore");
		bool isImprov   = st.Contains("improvis") || am.Contains("improvis");
		bool isMine     = !isClaymore && st.Contains("mine");
		bool isGrenade  = (Grenade_Base.Cast(m_ExorKfSource) != null) || am.Contains("grenade") || (am.Contains("40mm") && am.Contains("explo"));
		bool isTrap     = st.Contains("beartrap") || st.Contains("bear_trap");	// trampa de osos (killer = la trampa, no-jugador -> antes no salia en el killfeed)

		// ---- muerte AMBIENTAL (sin asesino): "X murio por <cause>" ----
		// Prioridad: el gas gana a todo (granada toxica de mano o de lanzagranadas = "gas").
		string cause = "";
		if (isGas)            cause = "gas";
		else if (isClaymore)  cause = "un Claymore";
		else if (isImprov)    cause = "un explosivo improvisado";
		else if (isMine)      cause = "una mina";
		else if (isTrap)      cause = "una trampa de osos";

		// caida de altura: el daño llega con ammo "FallDamage" (sin entidad atacante).
		// SOLO si el golpe fue RECIENTE: una caida mata al instante, asi que si el ultimo
		// golpe registrado (una caida que NO mato) fue hace rato y el jugador muere ahora
		// (F11/respawn o suicidio), NO es una caida -> no usar el ammo viejo y stale.
		bool recentHit = (m_ExorKfMs > 0 && GetGame().GetTime() - m_ExorKfMs < 2500);
		if (cause == "" && am.Contains("fall") && recentHit)
			cause = "una caída";

		// ---- GRANADA de fragmentacion: atribuir al LANZADOR (frag de mano o lanzagranadas) ----
		string grenWeapon = "";
		if (cause == "" && isGrenade)
		{
			grenWeapon = "una granada";
			if (am.Contains("40mm"))
				grenWeapon = "un lanzagranadas";
			// la granada recuerda a su ultimo portador (ver ExorGrenadeKill.c)
			if (!kp)
			{
				Grenade_Base gren = Grenade_Base.Cast(m_ExorKfSource);
				if (gren && gren.ExorGetThrowerId() != "")
					kp = ExorGroupManager.Get().FindOnline(gren.ExorGetThrowerId());
			}
			// no se pudo resolver al lanzador (offline / granada sin dueño) -> al menos avisar la muerte
			if (!kp || kp == this || !kp.GetIdentity())
				cause = grenWeapon;
		}

		// DIAGNOSTICO (temporal): en muertes explosivas, loguear el classname+municion REAL
		// para poder afinar los Contains() de arriba si alguna categoria no matchea. Se puede
		// sacar una vez confirmado que gas/mina/claymore/improvisado/granada se clasifican bien.
		if (cause != "" || isGrenade)
			Print(string.Format("%1 muerte explosiva: source='%2' ammo='%3' -> cause='%4' grenade=%5", ExorStorageConstants.LOG, st, am, cause, isGrenade));

		bool isPvp = (cause == "" && kp && kp != this && kp.GetIdentity());
		bool isSelfKill = (cause == "" && !isPvp && kp == this);                       // se mato con su propia arma (disparo/cuchillo)
		bool isGenericDeath = (cause == "" && !isPvp && !isSelfKill && !killer);       // enfermedad / hambre / sed / sangrado sin fuente

		// ---- AMBIENTAL ----
		if (cause != "")
		{
			ExorStats.Get().AddDeath(victimSid, victimName);	// cuenta como muerte propia
			if (cfg.habilitado)
			{
				ExorKfDTO dtoE = new ExorKfDTO();
				dtoE.dur = cfg.duracion_segundos;
				dtoE.max = cfg.max_lineas;
				dtoE.victim = victimName;
				dtoE.cause = cause;
				ExorBroadcastKillfeed(dtoE);
			}
			return;
		}

		if (isPvp)
		{
			string killerName = kp.GetIdentity().GetName();
			string killerSid = kp.GetIdentity().GetPlainId();
			string weapon = grenWeapon;	// "" salvo granada/lanzagranadas
			if (weapon == "")
				weapon = ExorKfWeaponName(kp);
			int dist = Math.Round(vector.Distance(kp.GetPosition(), GetPosition()));

			// TEAMKILL (mismo clan/party/territorio): el kill IGUAL sale en el killfeed
			// (mas abajo), pero NO suma al Score -> ni kill al asesino ni death a la
			// victima. Asi no inflan stats matandose entre socios del mismo mastil.
			bool teamKill = ExorGroupManager.Get().SameParty(killerSid, victimSid);

			// stats (para el Score) - salvo teamkill
			if (!teamKill)
			{
				ExorStats.Get().AddKill(killerSid, killerName, dist, weapon);
				ExorStats.Get().AddDeath(victimSid, victimName);
			}

			// FORENSE anti-farmeo: registra el par killer->victima (por steamid, a prueba
			// de cambio de nombre) y loguea al raidlog si supera el umbral en la ventana.
			ExorKillFarm.OnKill(killerSid, killerName, victimSid, victimName, kp.GetPosition(), teamKill);

			// killfeed (si esta activo)
			if (cfg.habilitado)
			{
				ExorKfDTO dto = new ExorKfDTO();
				dto.dur = cfg.duracion_segundos;
				dto.max = cfg.max_lineas;
				dto.victim = victimName;
				dto.suicide = false;
				dto.killer = killerName;
				dto.weapon = weapon;
				dto.dist = dist;
				ExorBroadcastKillfeed(dto);
			}
		}
		else if (isSelfKill)
		{
			// stats (siempre)
			ExorStats.Get().AddSuicide(victimSid, victimName);

			// killfeed (si esta activo)
			if (cfg.habilitado && cfg.mostrar_suicidios)
			{
				ExorKfDTO dto2 = new ExorKfDTO();
				dto2.dur = cfg.duracion_segundos;
				dto2.max = cfg.max_lineas;
				dto2.victim = victimName;
				dto2.suicide = true;
				ExorBroadcastKillfeed(dto2);
			}
		}
		else if (isGenericDeath)
		{
			// enfermedad / hambre / sed / sangrado: "X murió" (sin causa ni asesino)
			ExorStats.Get().AddDeath(victimSid, victimName);
			if (cfg.habilitado)
			{
				ExorKfDTO dto3 = new ExorKfDTO();
				dto3.dur = cfg.duracion_segundos;
				dto3.max = cfg.max_lineas;
				dto3.victim = victimName;
				dto3.generic = true;
				ExorBroadcastKillfeed(dto3);
			}
		}
		else
		{
			// muerte por infectado/animal/otro con atacante NO-jugador -> ni stats ni killfeed.
			// DIAG (temporal): loguear source+ammo+killer real para afinar la clasificacion
			// (asi si una trampa/animal deberia salir y no sale, vemos su classname exacto).
			string killerT = "null";
			if (killer)
				killerT = killer.GetType();
			Print(string.Format("%1 muerte sin categoria (no killfeed): source='%2' ammo='%3' killer='%4'", ExorStorageConstants.LOG, st, am, killerT));
		}
	}

	// Manda el evento a TODOS los clientes (cada uno lo recibe en su propio PlayerBase).
	void ExorBroadcastKillfeed(ExorKfDTO dto)
	{
		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(dto, false, data);
		Param1<string> p = new Param1<string>(data);

		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase pb = PlayerBase.Cast(players.Get(i));
			if (pb && pb.GetIdentity())
				pb.RPCSingleParam(ExorRPC.KILLFEED, p, true, pb.GetIdentity());
		}
	}

	// ------------------------- estado de party (server) -------------------------
	void ExorSetGroupId(string id)
	{
		m_ExorGroupId = id;
	}

	string ExorGetGroupId()
	{
		return m_ExorGroupId;
	}

	bool ExorIsInGroup()
	{
		return m_ExorGroupId != "";
	}

	// ------------------------- estado de party (cliente) -------------------------
	ExorRosterDTO ExorGetRoster()
	{
		return m_ExorRoster;
	}

	bool ExorClientInGroup()
	{
		return m_ExorRoster && m_ExorRoster.group_id != "";
	}

	bool ExorClientIsLeader()
	{
		return m_ExorRoster && m_ExorRoster.you_are_owner;
	}

	ExorPendingInvite ExorGetPendingInvite()
	{
		return m_ExorInvite;
	}

	void ExorClearPendingInvite()
	{
		m_ExorInvite = null;
	}

	// ------------------------- requests cliente -> server (los usa el menu P) -------------------------
	void ExorReqAccept()
	{
		RPCSingleParam(ExorRPC.ACCEPT, new Param1<int>(0), true, null);
	}

	void ExorReqDecline()
	{
		RPCSingleParam(ExorRPC.DECLINE, new Param1<int>(0), true, null);
	}

	void ExorReqLeave()
	{
		RPCSingleParam(ExorRPC.LEAVE, new Param1<int>(0), true, null);
	}

	void ExorReqKick(string steamid)
	{
		RPCSingleParam(ExorRPC.KICK, new Param1<string>(steamid), true, null);
	}

	void ExorReqSpawnPick(int index)
	{
		RPCSingleParam(ExorRPC.SPAWN_PICK, new Param1<int>(index), true, null);
	}

	void ExorReqMarkerAdd(vector pos)
	{
		RPCSingleParam(ExorRPC.MARKER_ADD, new Param3<float, float, float>(pos[0], pos[1], pos[2]), true, null);
	}

	void ExorReqMarkerClear()
	{
		RPCSingleParam(ExorRPC.MARKER_CLEAR, new Param1<int>(0), true, null);
	}

	// ------------------------- RPC -------------------------
	// true si el rpc_type pertenece al rango reservado del mod (ver ExorRPC en ExorParty_Net.c)
	bool ExorIsOwnRPC(int rpc_type)
	{
		return rpc_type >= ExorRPC.RANGE_MIN && rpc_type <= ExorRPC.RANGE_MAX;
	}

	override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
	{
		// NO delegar los RPC PROPIOS a super. Antes se llamaba super.OnRPC() SIEMPRE y
		// PRIMERO, asi que cada RPC nuestro atravesaba la cadena de los otros mods; el
		// XMLEditor de VPPAdminTools intentaba parsearlo como suyo y reventaba con
		// "NULL pointer to instance. Variable 'data'" (visto en produccion 19-jul 22:06).
		// Ademas es peligroso: si otro mod LEE el ctx, el cursor del stream avanza y
		// nuestro handler despues lee basura.
		if (!ExorIsOwnRPC(rpc_type))
		{
			super.OnRPC(sender, rpc_type, ctx);
			return;
		}

		switch (rpc_type)
		{
			// ---- server -> cliente ----
			case ExorRPC.ROSTER_SYNC:
				ExorOnRosterSync(ctx);
				break;
			case ExorRPC.INVITE:
				ExorOnInviteReceived(ctx);
				break;
			case ExorRPC.TERRITORY_SYNC:
				ExorOnTerritorySync(ctx);
				break;
			case ExorRPC.MEMBER_SYNC:
				ExorOnMemberSync(ctx);
				break;
			case ExorRPC.MARKER_SYNC:
				ExorOnMarkerSync(ctx);
				break;
			case ExorRPC.SPAWN_OPEN:
				ExorOnSpawnOpen(ctx);
				break;
			case ExorRPC.PARKING_OPEN:
				ExorOnParkingOpen(ctx);
				break;
			case ExorRPC.LOCK_MODAL_OPEN:
				ExorOnLockModalOpen(ctx);
				break;
			case ExorRPC.CAR_ACCESS_GRANT:
				ExorOnCarAccessGrant(ctx);
				break;
			case ExorRPC.CAR_MEMBER:
				ExorOnCarMember(ctx);
				break;
			case ExorRPC.CONFIG_SYNC:
				ExorOnConfigSync(ctx);
				break;
			case ExorRPC.SERVERINFO_SYNC:
			{
				// Llega en TROZOS (ExorNetChunk): reensamblar; aplicar SOLO cuando esta completo.
				// Asi nunca se lee un string > ~2KB (que reventaba la VM: "String CORRUPTED").
				string siFull = ExorBigStringRx.Feed(ExorRPC.SERVERINFO_SYNC, ctx);
				if (siFull != "")
					ExorConfig.ApplyServerInfoJson(siFull);
				break;
			}
			case ExorRPC.VIP_STATUS:
			{
				Param1<bool> vp = new Param1<bool>(false);
				if (ctx.Read(vp))
					ExorVipClient.s_IsVip = vp.param1;
				break;
			}
			case ExorRPC.AUTORUN_SET:
				if (GetGame().IsServer())
				{
					Param1<bool> ap = new Param1<bool>(false);
					if (ctx.Read(ap))
						m_ExorAutoRun = ap.param1;	// el server simula el mismo movimiento (anti rubber-band)
				}
				break;
			case ExorRPC.KILLFEED:
				ExorOnKillfeed(ctx);
				break;
			case ExorRPC.SCORE_REQ:
				if (GetGame().IsServer())
				{
					// THROTTLE por jugador: el cliente puede pedir el leaderboard tan rapido
					// como abra/cierre el panel (se vieron 3 pedidos en 1 segundo, 301 en 8h).
					// Cada uno rearma el JSON completo (~3.8 KB) y lo manda en trozos por red.
					// 1 pedido cada 5s por jugador alcanza de sobra para un panel de UI.
					int nowScore = GetGame().GetTime();
					if (nowScore - m_ExorLastScoreReqMs < 5000)
						break;
					m_ExorLastScoreReqMs = nowScore;
					string sj = ExorStats.Get().BuildJson();
					ExorNetChunk.Send(this, GetIdentity(), ExorRPC.SCORE_DATA, sj);
				}
				break;
			case ExorRPC.SCORE_DATA:
				ExorOnScoreData(ctx);
				break;
			case ExorRPC.POP_REQ:
				if (GetGame().IsServer())
				{
					// usar el conteo CACHEADO (lo refresca BarrelTick cada 5s) -> sin GetPlayers por request
					RPCSingleParam(ExorRPC.POP_COUNT, new Param1<int>(ExorVO_Manager.s_PopCount), true, GetIdentity());
				}
				break;
			case ExorRPC.POP_COUNT:
			{
				Param1<int> pc = new Param1<int>(0);
				if (ctx.Read(pc))
					ExorPopClient.s_Count = pc.param1;
				break;
			}
			case ExorRPC.KOTH_SYNC:
			{
				Param1<string> kp = new Param1<string>("");
				if (ctx.Read(kp))
				{
					ExorKothMarkerDTO km = new ExorKothMarkerDTO();
					JsonSerializer kjs = new JsonSerializer();
					string kerr;
					if (kjs.ReadFromString(km, kp.param1, kerr))
						ExorKothClient.Apply(km);
				}
				break;
			}
			case ExorRPC.CHAT_SEND:
				if (GetGame().IsServer())
				{
					Param2<string, int> cp = new Param2<string, int>("", 0);
					if (ctx.Read(cp))
						ExorChatServer.Handle(this, cp.param1, cp.param2);
				}
				break;
			case ExorRPC.CHAT_MSG:
				ExorOnChatMsg(ctx);
				break;
			case ExorRPC.SPAWN_PICK:
				if (GetGame().IsServer())
				{
					Param1<int> sp = new Param1<int>(0);
					if (ctx.Read(sp))
						ExorSpawn.ApplyPick(this, sp.param1);
				}
				break;
			case ExorRPC.MARKER_ADD:
				if (GetGame().IsServer())
				{
					Param3<float, float, float> mp = new Param3<float, float, float>(0, 0, 0);
					if (ctx.Read(mp))
						ExorPartyLive.Get().AddMarker(this, Vector(mp.param1, mp.param2, mp.param3));
				}
				break;
			case ExorRPC.MARKER_CLEAR:
				if (GetGame().IsServer())
					ExorPartyLive.Get().ClearMarkers(this);
				break;

			// ---- cliente -> server ----
			case ExorRPC.ACCEPT:
				if (GetGame().IsServer())
					ExorGroupManager.Get().AcceptInvite(this);
				break;
			case ExorRPC.DECLINE:
				if (GetGame().IsServer())
					ExorGroupManager.Get().DeclineInvite(this);
				break;
			case ExorRPC.LEAVE:
				if (GetGame().IsServer())
					ExorGroupManager.Get().Leave(this);
				break;
			case ExorRPC.KICK:
				if (GetGame().IsServer())
				{
					Param1<string> pk = new Param1<string>("");
					if (ctx.Read(pk))
						ExorGroupManager.Get().Kick(this, pk.param1);
				}
				break;
			case ExorRPC.PARKING_VIRT:
				if (GetGame().IsServer())
				{
					Param2<int, int> pkvp = new Param2<int, int>(0, 0);
					if (ctx.Read(pkvp))
						ExorDoParkingVirt(pkvp.param1, pkvp.param2);
				}
				break;
			case ExorRPC.PARKING_SPAWN:
				if (GetGame().IsServer())
				{
					Param1<string> pksp = new Param1<string>("");
					if (ctx.Read(pksp))
						ExorDoParkingSpawn(pksp.param1);
				}
				break;
			case ExorRPC.LOCK_MODAL_SUBMIT:
				if (GetGame().IsServer())
				{
					Param2<int, string> lkp = new Param2<int, string>(0, "");
					if (ctx.Read(lkp))
						ExorDoLockSubmit(lkp.param1, lkp.param2);
				}
				break;
		}
	}

	// Killfeed recibido (cliente): parsea el JSON y lo empuja a la UI.
	void ExorOnKillfeed(ParamsReadContext ctx)
	{
		Param1<string> p = new Param1<string>("");
		if (!ctx.Read(p))
			return;
		ExorKfDTO dto = new ExorKfDTO();
		JsonSerializer js = new JsonSerializer();
		string err;
		if (!js.ReadFromString(dto, p.param1, err))
			return;
		ExorKillfeedQueue.Enqueue(dto);	// la UI (5_Mission) lo drena cada frame
	}

	// Pide el leaderboard al server (cliente, al abrir el panel).
	void ExorReqScore()
	{
		RPCSingleParam(ExorRPC.SCORE_REQ, new Param1<int>(0), true, null);
	}

	// Pide al server la cantidad de jugadores conectados (cliente, al abrir/refrescar el mapa).
	void ExorReqPop()
	{
		RPCSingleParam(ExorRPC.POP_REQ, new Param1<int>(0), true, null);
	}

	// Leaderboard recibido (cliente): lo cachea para que lo lea el menu.
	void ExorOnScoreData(ParamsReadContext ctx)
	{
		string full = ExorBigStringRx.Feed(ExorRPC.SCORE_DATA, ctx);
		if (full == "")
			return;	// aun faltan trozos
		ExorStatsFile f = new ExorStatsFile();
		JsonSerializer js = new JsonSerializer();
		string err;
		if (!js.ReadFromString(f, full, err))
		{
			Print(string.Format("%1 cliente SCORE_DATA parse FALLO: %2", ExorStorageConstants.LOG, err));
			return;
		}
		ExorScoreClient.s_Data = f;
		Print(string.Format("%1 cliente SCORE_DATA ok, filas=%2", ExorStorageConstants.LOG, f.rows.Count()));
	}

	// ------------------------- chat -------------------------
	// Cliente -> server: manda un mensaje de chat con el canal actual.
	void ExorReqChat(string text, int channel)
	{
		RPCSingleParam(ExorRPC.CHAT_SEND, new Param2<string, int>(text, channel), true, null);
	}

	// Cliente: recibe un mensaje de chat y lo encola para la UI.
	void ExorOnChatMsg(ParamsReadContext ctx)
	{
		Param1<string> p = new Param1<string>("");
		if (!ctx.Read(p))
			return;
		ExorChatMsg m = new ExorChatMsg();
		JsonSerializer js = new JsonSerializer();
		string err;
		if (!js.ReadFromString(m, p.param1, err))
			return;
		ExorChatQueue.Enqueue(m);
	}

	void ExorOnRosterSync(ParamsReadContext ctx)
	{
		string full = ExorBigStringRx.Feed(ExorRPC.ROSTER_SYNC, ctx);
		if (full == "")
			return;	// aun faltan trozos

		ExorRosterDTO dto = new ExorRosterDTO();
		JsonSerializer js = new JsonSerializer();
		string err;
		if (js.ReadFromString(dto, full, err))
		{
			m_ExorRoster = dto;
			if (dto.group_id != "")
				m_ExorInvite = null;	// ya estoy en un grupo: limpiar invitacion pendiente
		}
	}

	void ExorOnInviteReceived(ParamsReadContext ctx)
	{
		Param2<string, string> p = new Param2<string, string>("", "");
		if (!ctx.Read(p))
			return;

		ExorPendingInvite inv = new ExorPendingInvite();
		inv.group_id = p.param1;
		inv.inviter_name = p.param2;
		inv.created_ms = GetGame().GetTime();
		m_ExorInvite = inv;
	}

	void ExorOnTerritorySync(ParamsReadContext ctx)
	{
		string full = ExorBigStringRx.Feed(ExorRPC.TERRITORY_SYNC, ctx);
		if (full == "")
			return;	// aun faltan trozos

		ExorTerritoryCacheDTO dto = new ExorTerritoryCacheDTO();
		JsonSerializer js = new JsonSerializer();
		string err;
		if (js.ReadFromString(dto, full, err))
		{
			ExorTerritoryClient.SetCache(dto);
		}
	}

	void ExorOnMemberSync(ParamsReadContext ctx)
	{
		// reensamblado por trozos (el server ahora chunkea MEMBER_SYNC, igual que
		// ROSTER/MARKER, para no corromper el string con 5+ miembros).
		string full = ExorBigStringRx.Feed(ExorRPC.MEMBER_SYNC, ctx);
		if (full == "")
			return;	// aun faltan trozos
		ExorLiveDTO dto = new ExorLiveDTO();
		JsonSerializer js = new JsonSerializer();
		string err;
		if (js.ReadFromString(dto, full, err))
			ExorPartyClient.SetLive(dto);
	}

	void ExorOnMarkerSync(ParamsReadContext ctx)
	{
		string full = ExorBigStringRx.Feed(ExorRPC.MARKER_SYNC, ctx);
		if (full == "")
			return;	// aun faltan trozos
		ExorMarkersDTO dto = new ExorMarkersDTO();
		JsonSerializer js = new JsonSerializer();
		string err;
		if (js.ReadFromString(dto, full, err))
			ExorPartyClient.SetMarkers(dto);
	}

	// Cliente: recibe la config del server (en trozos) y la aplica sobre el singleton local.
	void ExorOnConfigSync(ParamsReadContext ctx)
	{
		string full = ExorBigStringRx.Feed(ExorRPC.CONFIG_SYNC, ctx);
		if (full == "")
			return;	// aun faltan trozos
		ExorConfig.ApplyClientJson(full);
		ExorCarAccessClient.RefreshAdmin();	// ya llego la lista de admins -> saber si soy admin del baul
	}

	// Cliente: recibe la lista y abre la pantalla de seleccion de spawn.
	void ExorOnSpawnOpen(ParamsReadContext ctx)
	{
		string full = ExorBigStringRx.Feed(ExorRPC.SPAWN_OPEN, ctx);
		if (full == "")
			return;	// aun faltan trozos
		ExorSpawnMenuDTO dto = new ExorSpawnMenuDTO();
		JsonSerializer js = new JsonSerializer();
		string err;
		if (js.ReadFromString(dto, full, err))
		{
			ExorSpawnClient.Set(dto);
			Print("[3xorVO] cliente: SPAWN_OPEN recibido, abriendo menu");
			GetGame().GetCallQueue(CALL_CATEGORY_GAMEPLAY).CallLater(ExorOpenSpawnMenu, 800, false);
		}
	}

	void ExorOpenSpawnMenu()
	{
		// IDEMPOTENTE: el server reintenta el SPAWN_OPEN hasta que el jugador elige, asi que
		// este handler puede correr varias veces. Si el menu YA esta abierto no hay que
		// re-abrirlo (reabrirlo pisaria la seleccion que el jugador esta por confirmar);
		// los datos igual se refrescaron via ExorSpawnClient.Set() antes de llamar aca.
		UIManager ui = GetGame().GetUIManager();
		if (!ui)
			return;
		UIScriptedMenu abierto = ui.FindMenu(ExorMenuIDs.SPAWN);
		if (abierto)
			return;
		ui.EnterScriptedMenu(ExorMenuIDs.SPAWN, null);
	}

	// ========================================================================
	//  PARKING (administrar autos del clan) - menu con 2 paneles
	// ========================================================================
	// SERVER: parking que este jugador esta gestionando (para refrescar/actuar tras cada click).
	protected vector	m_ExorParkingPos;
	protected bool		m_ExorParkingActive;

	// SERVER: el jugador interactuo con un parking (permiso ya chequeado en la accion).
	// Guarda el parking y le manda la lista de autos al cliente.
	void ExorOpenParkingManager(vector pos)
	{
		if (!GetGame().IsServer())
			return;
		m_ExorParkingPos = pos;
		m_ExorParkingActive = true;
		ExorSendParkingList();
	}

	// SERVER: clan DUEÑO del parking que se esta gestionando = el del territorio donde ESTA
	// la maquina, NO el del jugador que la abre.
	//
	// Antes se usaba el grupo del jugador y eso hacia dos cosas mal: abrir el parking de otro
	// clan mostraba TUS autos (no los de esa base), y peor, en horario de raid te dejaba
	// SACAR tus propios autos adentro de la base ajena (teletransporte de flota). Con el
	// grupo del territorio, el parking siempre habla de los autos de ESA base: los miembros
	// los guardan y los sacan, y un raider en horario libre solo puede sacar los del dueño
	// (aparecen ahi, hay que robarlos manejando) pero nunca meter los suyos.
	//
	// Si el punto no cae en ningun territorio (al dueño se le cayo el mastil), se cae al
	// grupo del jugador para no dejar al clan sin acceso a sus propios autos guardados.
	string ExorParkingOwnerGroup()
	{
		ExorTerritoryManager tm = ExorTerritoryManager.Get();
		string owner;
		if (tm)
			owner = tm.GroupAtPos(m_ExorParkingPos);
		if (owner == "")
			owner = ExorGetGroupId();
		return owner;
	}

	// SERVER: arma el JSON (almacenados + reales) y lo manda al cliente (en trozos).
	// El mismo RPC ABRE el menu (si no estaba) o lo REFRESCA (si ya estaba).
	void ExorSendParkingList()
	{
		if (!GetGame().IsServer() || !m_ExorParkingActive)
			return;
		string json = ExorParkingNet.BuildJson(m_ExorParkingPos, ExorParkingOwnerGroup());
		ExorNetChunk.Send(this, GetIdentity(), ExorRPC.PARKING_OPEN, json);
	}

	// SERVER: virtualizar (guardar) el auto real con ese network id. Re-escanea el radio
	// del parking guardado y lo re-ubica por netId (robusto ante indices que se movieron).
	void ExorDoParkingVirt(int netLow, int netHigh)
	{
		if (!GetGame().IsServer() || !m_ExorParkingActive)
			return;
		// re-chequear permiso (el jugador pudo salir del clan / cambiar el horario)
		string deny;
		if (!ExorMuebleRules.CanLootAtPos(this, m_ExorParkingPos, deny))
		{
			ExorMuebleRules.SendRed(this, deny);
			return;
		}
		array<CarScript> cars;
		ExorVehicleGarage.NearbyReal(m_ExorParkingPos, ExorParkingNet.ScanRadius(), cars);
		CarScript target;
		int i;
		for (i = 0; i < cars.Count(); i++)
		{
			CarScript car = cars.Get(i);
			if (!car)
				continue;
			int lo, hi;
			car.GetNetworkID(lo, hi);
			if (lo == netLow && hi == netHigh)
			{
				target = car;
				break;
			}
		}
		if (!target)
		{
			ExorSendParkingList();	// ya no esta: refrescar y salir
			return;
		}
		// el auto se guarda a nombre del clan DUEÑO de la base, no del que aprieta el boton
		// (asi un raider no puede quedarse con el auto ajeno metiendolo a su propio garage;
		// de hecho el guard de "jugadores ajenos cerca" ya le va a rebotar la accion).
		string reason = ExorVehicleGarage.Virtualize(target, ExorParkingOwnerGroup());
		if (reason != "")
			ExorMuebleRules.SendRed(this, "No se pudo guardar el auto: " + reason);
		else
			ExorMuebleRules.SendChat(this, "Auto guardado en el parking.");
		ExorSendParkingList();	// refrescar ambas listas
	}

	// SERVER: desvirtualizar (sacar) el auto almacenado con ese id. Reaparece donde estaba.
	void ExorDoParkingSpawn(string id)
	{
		if (!GetGame().IsServer() || !m_ExorParkingActive)
			return;
		string deny;
		if (!ExorMuebleRules.CanLootAtPos(this, m_ExorParkingPos, deny))
		{
			ExorMuebleRules.SendRed(this, deny);
			return;
		}
		// se saca contra el clan DUEÑO del parking: un miembro saca los suyos, y un raider en
		// horario libre puede sacar los de la base que esta raideando (aparecen ahi, hay que
		// llevarselos manejando), pero NUNCA los de su propio clan en una base ajena.
		string reason = ExorVehicleGarage.Spawn(id, ExorParkingOwnerGroup());
		if (reason != "")
			ExorMuebleRules.SendRed(this, "No se pudo sacar el auto: " + reason);
		else
			ExorMuebleRules.SendChat(this, "Auto sacado del parking.");
		ExorSendParkingList();	// refrescar ambas listas
	}

	// CLIENTE: pide guardar/sacar un auto (los llama el menu).
	void ExorReqParkingVirt(int netLow, int netHigh)
	{
		RPCSingleParam(ExorRPC.PARKING_VIRT, new Param2<int, int>(netLow, netHigh), true, null);
	}
	void ExorReqParkingSpawn(string id)
	{
		RPCSingleParam(ExorRPC.PARKING_SPAWN, new Param1<string>(id), true, null);
	}

	// CLIENTE: llega la lista de autos -> cachearla y abrir/refrescar el menu.
	void ExorOnParkingOpen(ParamsReadContext ctx)
	{
		string full = ExorBigStringRx.Feed(ExorRPC.PARKING_OPEN, ctx);
		if (full == "")
			return;	// aun faltan trozos
		ExorParkingMenuDTO dto = new ExorParkingMenuDTO();
		JsonSerializer js = new JsonSerializer();
		string err;
		if (!js.ReadFromString(dto, full, err))
			return;
		ExorParkingClient.Set(dto);	// bump de version -> el menu se auto-refresca en su Update

		UIManager ui = GetGame().GetUIManager();
		if (!ui)
			return;
		// Si el menu YA esta abierto no lo re-abrimos (el bump de version de arriba hace que
		// se refresque solo). Usamos el tipo base UIScriptedMenu: el menu concreto vive en
		// 5_Mission y no es referenciable desde 4_World.
		UIScriptedMenu open = ui.FindMenu(ExorMenuIDs.PARKING);
		if (!open)
			ui.EnterScriptedMenu(ExorMenuIDs.PARKING, null);
	}

	// ========================================================================
	//  CLAVE de lockers (code-lock)
	// ========================================================================
	// SERVER: locker sobre el que este jugador esta poniendo/metiendo clave (guardado entre
	// que abre el modal y confirma). Es estatico/indestructible -> ref seguro con null-check.
	protected Exor_OpenableStorage m_ExorKeyTarget;
	protected CarScript m_ExorCarKeyTarget;	// candado de AUTOS: reusa el mismo modal, target aparte

	// SERVER: abre el modal de clave en el cliente (mode 0=meter, 1=setear). Guarda el locker.
	void ExorOpenLockKeyModal(Exor_OpenableStorage fur, int mode)
	{
		if (!GetGame().IsServer() || !fur)
			return;
		m_ExorKeyTarget = fur;
		m_ExorCarKeyTarget = null;	// es locker, no auto
		RPCSingleParam(ExorRPC.LOCK_MODAL_OPEN, new Param1<int>(mode), true, GetIdentity());
	}

	// SERVER: abre el mismo modal pero apuntando a un AUTO.
	void ExorOpenCarKeyModal(CarScript car, int mode)
	{
		if (!GetGame().IsServer() || !car)
			return;
		m_ExorCarKeyTarget = car;
		m_ExorKeyTarget = null;	// es auto, no locker
		RPCSingleParam(ExorRPC.LOCK_MODAL_OPEN, new Param1<int>(mode), true, GetIdentity());
	}

	// SERVER: le da a ESTE cliente acceso al baul de 'car' (metio la clave OK / es dueño).
	// Manda el network-id del auto; el cliente lo guarda en su set -> CanDisplayCargo lo deja ver.
	void ExorSendCarAccessGrant(CarScript car)
	{
		if (!GetGame().IsServer() || !car)
			return;
		int low, high;
		car.GetNetworkID(low, high);
		RPCSingleParam(ExorRPC.CAR_ACCESS_GRANT, new Param2<int, int>(low, high), true, GetIdentity());
	}

	// CLIENTE: llega un grant de acceso a un baul -> guardarlo en el set (O(1)).
	void ExorOnCarAccessGrant(ParamsReadContext ctx)
	{
		Param2<int, int> p = new Param2<int, int>(0, 0);
		if (!ctx.Read(p))
			return;
		ExorCarAccessClient.Grant(string.Format("%1_%2", p.param1, p.param2));
	}

	// SERVER: le avisa a ESTE cliente que es MIEMBRO del clan dueño de 'car'.
	void ExorSendCarMember(CarScript car)
	{
		if (!GetGame().IsServer() || !car)
			return;
		int low, high;
		car.GetNetworkID(low, high);
		RPCSingleParam(ExorRPC.CAR_MEMBER, new Param2<int, int>(low, high), true, GetIdentity());
	}

	// CLIENTE: soy miembro del clan dueño de este auto -> guardarlo (para "Ingresar clave").
	void ExorOnCarMember(ParamsReadContext ctx)
	{
		Param2<int, int> p = new Param2<int, int>(0, 0);
		if (!ctx.Read(p))
			return;
		ExorCarAccessClient.MarkMember(string.Format("%1_%2", p.param1, p.param2));
	}

	// SERVER: el jugador confirmo el modal.
	void ExorDoLockSubmit(int mode, string key)
	{
		if (!GetGame().IsServer())
			return;
		// candado de AUTO tiene prioridad si fue lo ultimo que se abrio
		if (m_ExorCarKeyTarget)
		{
			ExorDoCarKeySubmit(mode, key);
			return;
		}
		Exor_OpenableStorage fur = m_ExorKeyTarget;
		if (!fur)
			return;
		string sid = ExorGroupManager.SteamId(this);

		if (mode == ExorLockKeyClient.MODE_SET)
		{
			// re-chequear permiso (miembro/staff) + que cambiar tenga la clave previa
			string denyS;
			if (!ExorMuebleRules.CanPackAtPos(this, fur.GetPosition(), denyS))
			{
				ExorMuebleRules.SendRed(this, "Solo los miembros del clan pueden ponerle clave.");
				return;
			}
			if (fur.ExorHasKey() && !fur.ExorIsUnlockedBy(sid))
			{
				ExorMuebleRules.SendRed(this, "Necesitás ingresar la clave actual antes de cambiarla.");
				return;
			}
			if (key == "")
			{
				ExorMuebleRules.SendRed(this, "La clave no puede estar vacía.");
				return;
			}
			fur.ExorSetKey(key, sid);
			ExorMuebleRules.SendChat(this, "Clave del locker guardada.");
		}
		else	// MODE_ENTER: meter la clave para abrir
		{
			if (fur.ExorKeyMatches(key))
			{
				fur.ExorMarkUnlocked(sid);
				fur.Open();
			}
			else
			{
				ExorMuebleRules.SendRed(this, "Clave incorrecta.");
			}
		}
		m_ExorKeyTarget = null;
	}

	// SERVER: confirmacion del modal cuando el target es un AUTO.
	void ExorDoCarKeySubmit(int mode, string key)
	{
		CarScript car = m_ExorCarKeyTarget;
		m_ExorCarKeyTarget = null;
		if (!car)
			return;
		string sid = ExorGroupManager.SteamId(this);
		ExorCfgCarLock cfg = GetExorConfig().carlock;

		if (mode == ExorLockKeyClient.MODE_SET)
		{
			bool admin = cfg.ExorEsAdmin(sid);
			// validar largo + alfanumerico
			if (!ExorCarKeyValida(key))
			{
				ExorMuebleRules.SendRed(this, string.Format("La clave debe tener %1-%2 caracteres, solo letras y números.", cfg.clave_min_largo, cfg.clave_max_largo));
				return;
			}
			if (car.ExorCarHasLock())
			{
				// CAMBIAR: miembro ACTUAL del clan con la clave (o admin). El clan dueño NO cambia.
				if (!(car.ExorCarIsMemberOfLockGroup(sid) && car.ExorCarIsUnlockedBy(sid)) && !admin)
				{
					ExorMuebleRules.SendRed(this, "Necesitás ingresar la clave actual antes de cambiarla.");
					return;
				}
					car.ExorCarSetKey(key, sid, car.ExorCarGetLockGroup());	// preservar el clan dueño
			}
			else
			{
				// COLOCAR: si estas en un clan, el candado es del clan (sus miembros meten la clave);
				// si no, queda a tu nombre (candado personal, solo vos lo abris). Anda con o sin party.
				ExorGroup g = ExorGroupManager.Get().FindByPlayer(sid);
				string grp = "";
				if (g)
					grp = g.id;
				car.ExorCarSetKey(key, sid, grp);
				// CONSUMIR el keypad: recien AHORA (candado ya escrito con exito). Si el jugador
				// hubiera cancelado el modal, este codigo no corre -> no se pierde el item.
				ItemBase kp = GetItemInHands();
				if (kp && kp.IsInherited(Exor_CarCodeLock))
					GetGame().ObjectDelete(kp);
			}
			car.ExorCarMarkUnlocked(sid);	// el que la pone queda desbloqueado (puede arrancar ya)
			ExorSendCarAccessGrant(car);	// + acceso al baul en su cliente
			if (car.ExorCarIsMemberOfLockGroup(sid))
				ExorSendCarMember(car);		// + el cliente sabe que es miembro (para "Ingresar clave")
			ExorMuebleRules.SendChat(this, "Candado del auto guardado.");
		}
		else	// MODE_ENTER: meter la clave para desbloquear el auto
		{
			if (car.ExorCarKeyMatches(key))
			{
				car.ExorCarMarkUnlocked(sid);
				ExorSendCarAccessGrant(car);	// + acceso al baul en su cliente
				if (car.ExorCarIsMemberOfLockGroup(sid))
					ExorSendCarMember(car);		// + el cliente sabe que es miembro (para "Ingresar clave")
				ExorMuebleRules.SendChat(this, "Clave correcta. Ya podés usar el auto.");
			}
			else
			{
				ExorMuebleRules.SendRed(this, "Clave incorrecta.");
			}
		}
	}

	// clave valida: largo entre min/max del config y solo letras+numeros (sin espacios/simbolos).
	// Charset por IndexOf (la comparacion de strings por orden en EnforceScript no es confiable).
	bool ExorCarKeyValida(string key)
	{
		ExorCfgCarLock cfg = GetExorConfig().carlock;
		int n = key.Length();
		if (n < cfg.clave_min_largo || n > cfg.clave_max_largo)
			return false;
		string ok = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
		int i;
		for (i = 0; i < n; i++)
		{
			if (ok.IndexOf(key.Substring(i, 1)) < 0)
				return false;
		}
		return true;
	}

	// CLIENTE: pide confirmar el modal (lo llama el menu).
	void ExorReqLockSubmit(int mode, string key)
	{
		RPCSingleParam(ExorRPC.LOCK_MODAL_SUBMIT, new Param2<int, string>(mode, key), true, null);
	}

	// CLIENTE: llega la orden de abrir el modal -> cachear el modo y abrirlo.
	void ExorOnLockModalOpen(ParamsReadContext ctx)
	{
		Param1<int> p = new Param1<int>(0);
		if (!ctx.Read(p))
			return;
		ExorLockKeyClient.Set(p.param1);	// modo + bump de version (redibuja si ya estaba abierto)
		UIManager ui = GetGame().GetUIManager();
		if (!ui)
			return;
		UIScriptedMenu open = ui.FindMenu(ExorMenuIDs.LOCKKEY);
		if (!open)
			ui.EnterScriptedMenu(ExorMenuIDs.LOCKKEY, null);
	}
}
