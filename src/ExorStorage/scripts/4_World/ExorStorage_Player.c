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

	// --- Bolsa de cadaver (server): ropa copiada + armas/manos (entidades reales a mover) ---
	protected ref array<ref ExorVO_ItemData> m_ExorDeathLoot;
	protected ref array<EntityAI> m_ExorDeathWeapons;

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
	// Fuerza 1ra persona a los pasajeros (y al conductor si conductor_3ra_persona=off).
	// HandleView corre cada frame en el path de camara; re-forzamos despues del toggle vanilla.
	override void HandleView()
	{
		super.HandleView();

		HumanCommandVehicle hcv = GetCommand_Vehicle();
		if (!hcv)
			return;
		if (hcv.IsGettingIn() || hcv.IsGettingOut())
			return;	// no tocar durante las transiciones de entrada/salida

		ExorCfgVehCamara cam = GetExorConfig().vehiculos.camara;
		bool isDriver = (hcv.GetVehicleSeat() == DayZPlayerConstants.VEHICLESEAT_DRIVER);
		if (isDriver)
		{
			if (!cam.conductor_3ra_persona)
				SetIsInThirdPerson(false);	// conductor forzado a 1ra
		}
		else
		{
			if (cam.pasajeros_1ra_persona)
				SetIsInThirdPerson(false);	// pasajeros forzados a 1ra
		}
	}

	// ------------------------- killfeed (server) -------------------------
	// Registra la entidad que causo el ultimo daño (arma o atacante) para poder
	// armar el mensaje al morir.
	override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
		if (GetGame().IsServer())
			m_ExorKfSource = source;
	}

	// Al morir: killfeed + programar la bolsa de cadaver.
	override void EEKilled(Object killer)
	{
		if (GetGame().IsServer())
		{
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

	void ExorBuildKillfeed(Object killer)
	{
		string victimName = "?";
		string victimSid = "";
		if (GetIdentity())
		{
			victimName = GetIdentity().GetName();
			victimSid = GetIdentity().GetPlainId();
		}

		// quien mato: el param killer, o la raiz de la fuente de daño registrada
		PlayerBase kp = PlayerBase.Cast(killer);
		if (!kp && m_ExorKfSource)
			kp = PlayerBase.Cast(m_ExorKfSource.GetHierarchyRootPlayer());

		bool isPvp = (kp && kp != this && kp.GetIdentity());
		bool isSuicide = (!killer || kp == this);
		ExorCfgKillfeed cfg = GetExorConfig().killfeed;

		if (isPvp)
		{
			string killerName = kp.GetIdentity().GetName();
			string killerSid = kp.GetIdentity().GetPlainId();
			string weapon = ExorKfWeaponName(kp);
			int dist = Math.Round(vector.Distance(kp.GetPosition(), GetPosition()));

			// stats (siempre, para el Score)
			ExorStats.Get().AddKill(killerSid, killerName, dist, weapon);
			ExorStats.Get().AddDeath(victimSid, victimName);

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
		else if (isSuicide)
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
		// else: muerte por infectado/animal/entorno -> ni stats ni killfeed
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
			case ExorRPC.VIP_STATUS:
			{
				Param1<bool> vp = new Param1<bool>(false);
				if (ctx.Read(vp))
					ExorVipClient.s_IsVip = vp.param1;
				break;
			}
			case ExorRPC.KILLFEED:
				ExorOnKillfeed(ctx);
				break;
			case ExorRPC.SCORE_REQ:
				if (GetGame().IsServer())
				{
					string sj = ExorStats.Get().BuildJson();
					Print(string.Format("%1 SCORE_REQ recibido -> enviando %2 chars de leaderboard", ExorStorageConstants.LOG, sj.Length()));
					RPCSingleParam(ExorRPC.SCORE_DATA, new Param1<string>(sj), true, GetIdentity());
				}
				break;
			case ExorRPC.SCORE_DATA:
				ExorOnScoreData(ctx);
				break;
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

	// Leaderboard recibido (cliente): lo cachea para que lo lea el menu.
	void ExorOnScoreData(ParamsReadContext ctx)
	{
		Param1<string> p = new Param1<string>("");
		if (!ctx.Read(p))
			return;
		ExorStatsFile f = new ExorStatsFile();
		JsonSerializer js = new JsonSerializer();
		string err;
		if (!js.ReadFromString(f, p.param1, err))
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
		Param1<string> p = new Param1<string>("");
		if (!ctx.Read(p))
			return;

		ExorRosterDTO dto = new ExorRosterDTO();
		JsonSerializer js = new JsonSerializer();
		string err;
		if (js.ReadFromString(dto, p.param1, err))
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
		Param1<string> p = new Param1<string>("");
		if (!ctx.Read(p))
			return;

		ExorTerritoryCacheDTO dto = new ExorTerritoryCacheDTO();
		JsonSerializer js = new JsonSerializer();
		string err;
		if (js.ReadFromString(dto, p.param1, err))
		{
			ExorTerritoryClient.SetCache(dto);
		}
	}

	void ExorOnMemberSync(ParamsReadContext ctx)
	{
		Param1<string> p = new Param1<string>("");
		if (!ctx.Read(p))
			return;
		ExorLiveDTO dto = new ExorLiveDTO();
		JsonSerializer js = new JsonSerializer();
		string err;
		if (js.ReadFromString(dto, p.param1, err))
			ExorPartyClient.SetLive(dto);
	}

	void ExorOnMarkerSync(ParamsReadContext ctx)
	{
		Param1<string> p = new Param1<string>("");
		if (!ctx.Read(p))
			return;
		ExorMarkersDTO dto = new ExorMarkersDTO();
		JsonSerializer js = new JsonSerializer();
		string err;
		if (js.ReadFromString(dto, p.param1, err))
			ExorPartyClient.SetMarkers(dto);
	}

	// Cliente: recibe la config del server y la aplica sobre el singleton local.
	void ExorOnConfigSync(ParamsReadContext ctx)
	{
		Param1<string> p = new Param1<string>("");
		if (!ctx.Read(p))
			return;
		ExorConfig.ApplyClientJson(p.param1);
	}

	// Cliente: recibe la lista y abre la pantalla de seleccion de spawn.
	void ExorOnSpawnOpen(ParamsReadContext ctx)
	{
		Param1<string> p = new Param1<string>("");
		if (!ctx.Read(p))
			return;
		ExorSpawnMenuDTO dto = new ExorSpawnMenuDTO();
		JsonSerializer js = new JsonSerializer();
		string err;
		if (js.ReadFromString(dto, p.param1, err))
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
