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
