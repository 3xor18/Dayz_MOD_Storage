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
	protected ref ExorKillShot m_ExorKillShot;   // snapshot geometrico del ultimo impacto de un player (para el detector por kill, ambos vivos)

	ExorKillShot ExorGetKillShot() { return m_ExorKillShot; }
	void ExorSetKillShot(ExorKillShot s) { m_ExorKillShot = s; }

	// --- Bolsa de cadaver (server): ropa copiada + armas/manos (entidades reales a mover) ---
	protected ref array<ref ExorVO_ItemData> m_ExorDeathLoot;
	protected ref array<EntityAI> m_ExorDeathWeapons;

	// --- Auto-run (cliente, jugador local) ---
	protected bool m_ExorAutoRun;       // auto-run activo
	protected bool m_ExorArKeyPrev;     // estado previo de la tecla (deteccion de flanco)
	protected bool m_ExorArApplied;     // el override esta puesto (para soltarlo 1 sola vez al apagar)
	protected bool m_ExorArTired;       // sin stamina -> baja a trote sin sprint hasta recuperar (histeresis)

	// --- Camara en vehiculo (cliente): el conductor elige 1ra/3ra con V ---
	protected bool m_ExorVeh1pp;        // el conductor eligio 1ra persona (default false = 3ra)
	protected bool m_ExorVehVPrev;      // estado previo de la tecla V (deteccion de flanco)

	void PlayerBase()
	{
		// El SERVER decide el cansancio del auto-run (tiene la stamina real) y lo SINCRONIZA
		// al cliente con esta variable -> ambos lados aplican el MISMO speed -> sin rubber-band
		// (glisheo atras-adelante). Ver ExorAutoRunTick.
		RegisterNetSyncVariableBool("m_ExorArTired");
	}

	// ------------------------- TEST LOCAL: equipar NPC dummy de VPP -------------------------
	// Al spawnear "player" con VPP Admin Tools sale un PlayerBase SIN identidad (dummy). Si el
	// flag spawns.equipar_npc_test esta on (SOLO local), lo equipamos con ropa+mochila+armas
	// para poder matarlo y probar la bolsa de cadaver con loot real. Un jugador REAL tiene
	// identidad a los pocos segundos -> el chequeo diferido (4s) lo descarta y nunca lo toca.
	override void EEInit()
	{
		super.EEInit();
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
		// arma en manos + cargador + mira (la bolsa MUEVE el arma real -> conserva mag/miras)
		EntityAI rifle = GetHumanInventory().CreateInHands("M4A1");
		if (rifle && rifle.GetInventory())
		{
			rifle.GetInventory().CreateAttachment("Mag_STANAG_30Rnd");
			rifle.GetInventory().CreateAttachment("ACOGOptic");
		}
		// arma a la espalda
		inv.CreateInInventory("Mosin9130");
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
		AddAction(ExorActionFlipVehicle, InputActionMap);
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

		// anti-cheat: god mode (recibe impactos reales seguidos sin perder vida). Solo
		// actua si el vigilado esta en la watchlist -> sale al toque para el resto.
		ExorAnticheat.OnWatchedHit(this, source);

		// aim-track: registrar el impacto contra el engagement del ATACANTE (si esta vigilado).
		// El filtro (vigilado/feature) lo hace OnHit -> para el resto sale con 1 lookup.
		if (source)
		{
			PlayerBase aimAtk = PlayerBase.Cast(source.GetHierarchyRootPlayer());
			if (aimAtk && aimAtk != this)
			{
				ExorAimTrack.OnHit(aimAtk, this, dmgZone, ammo, Math.Round(vector.Distance(aimAtk.GetPosition(), GetPosition())));
				// snapshot geometrico para el detector por kill (geometria con AMBOS vivos, no post-muerte)
				ExorAnticheat.CaptureKillShot(aimAtk, this, dmgZone);
			}
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

	// Al morir: killfeed + programar la bolsa de cadaver.
	override void EEKilled(Object killer)
	{
		if (GetGame().IsServer())
		{
			ExorBuildKillfeed(killer);
			ExorScheduleBodyBag();
			// aim-track: cerrar engagements del que murio + volcar el resumen de su vida
			if (GetIdentity())
				ExorAimTrack.OnPlayerGone(this, GetIdentity().GetPlainId());
		}
		super.EEKilled(killer);
	}

	// Programa la conversion del cuerpo en bolsa de cadaver (server).
	void ExorScheduleBodyBag()
	{
		ExorCfgBodyCadaver cfg = GetExorConfig().bodycadaver;
		if (!cfg.habilitado)
			return;

		// Con el cuerpo INTACTO: la ROPA se copia (capturar+recrear, cae en sus slots)
		// y las ARMAS se guardan para MOVER la entidad real (copiar un arma pierde el
		// cargador). 1s despues el motor ya pudo dropear cosas, por eso se hace aca.
		m_ExorDeathLoot = new array<ref ExorVO_ItemData>;
		m_ExorDeathWeapons = new array<EntityAI>;
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
				if (Weapon_Base.Cast(att))
					m_ExorDeathWeapons.Insert(att);	// arma puesta (espalda/hombro) -> mover real
				else
					m_ExorDeathLoot.Insert(ExorVO_Serializer.CaptureItem(att));	// ropa -> copiar
			}
		}
		// lo que tenga en manos (arma u otro) tambien se mueve real
		EntityAI hands = null;
		if (GetHumanInventory())
			hands = GetHumanInventory().GetEntityInHands();
		if (hands)
			m_ExorDeathWeapons.Insert(hands);

		Print(string.Format("%1 muerte: %2 prendas + %3 armas/manos para la bolsa (attachments=%4)", ExorStorageConstants.LOG, m_ExorDeathLoot.Count(), m_ExorDeathWeapons.Count(), natt));

		int delayMs = cfg.delay_segundos * 1000;
		if (delayMs < 1)
			delayMs = 1;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorDoSpawnBodyBag, delayMs, false);
	}

	// 'this' es el cadaver -> spawnear la bolsa con el loot recreado + el arma real movida.
	void ExorDoSpawnBodyBag()
	{
		Exor_BodyBag bag = Exor_BodyBag.SpawnFromLoot(GetPosition(), m_ExorDeathLoot, m_ExorDeathWeapons);
		if (!bag)
			return;
		GetGame().ObjectDelete(this);	// borrar el cuerpo - las armas reales ya se movieron
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

		// ---- muerte AMBIENTAL (sin asesino): "X murio por <cause>" ----
		// Prioridad: el gas gana a todo (granada toxica de mano o de lanzagranadas = "gas").
		string cause = "";
		if (isGas)            cause = "gas";
		else if (isClaymore)  cause = "un Claymore";
		else if (isImprov)    cause = "un explosivo improvisado";
		else if (isMine)      cause = "una mina";

		// caida de altura: el daño llega con ammo "FallDamage" (sin entidad atacante)
		if (cause == "" && am.Contains("fall"))
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

			// anti-cheat: evaluar el kill (LOS/angulo/distancia) -> log si hay indicios.
			// Se saltea para granadas/lanzagranadas: el explosivo no necesita LOS ni punteria
			// (mataria detras de cobertura) -> daria falsos indicios.
			if (grenWeapon == "")
			{
				ExorAnticheat.OnKill(kp, this, dist, weapon, ExorKfWeaponClass(kp));
				ExorAimTrack.OnKill(kp, this, dist);	// aim-track: loguea el kill con la accuracy de la vida
			}

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
		// else: muerte por infectado/animal/otro con atacante no-jugador -> ni stats ni killfeed
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
	override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
	{
		super.OnRPC(sender, rpc_type, ctx);

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
					string sj = ExorStats.Get().BuildJson();
					Print(string.Format("%1 SCORE_REQ recibido -> enviando %2 chars de leaderboard (en trozos)", ExorStorageConstants.LOG, sj.Length()));
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
		GetGame().GetUIManager().EnterScriptedMenu(ExorMenuIDs.SPAWN, null);
	}
}
