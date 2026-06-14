// ============================================================================
// 3xor_Vanilla_Optimization - Acciones de gestion del party (Fase E)
// Se entregan como ACCIONES (no como menu P con keybind) para no arriesgar el
// config con cfgInputActions. Cubren: aceptar/rechazar invitacion, salir,
// kickear, y poner/limpiar marcas. El menu P con tecla queda como mejora
// in-game (cuando se afinen los keybinds en cfgInputActions).
// Las acciones self requieren manos vacias (CCINone).
// ============================================================================

// --------------------------- ACEPTAR INVITACION ---------------------------
class ExorActionAcceptInviteCB : ActionContinuousBaseCB
{
	override void CreateActionComponent() { m_ActionData.m_ActionComponent = new CAContinuousTime(1.0); }
}

class ExorActionAcceptInvite : ActionContinuousBase
{
	void ExorActionAcceptInvite()
	{
		m_CallbackClass = ExorActionAcceptInviteCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
		m_Text = "Aceptar invitacion al party";
	}

	override void CreateConditionComponents() { m_ConditionItem = new CCINone; m_ConditionTarget = new CCTNone; }
	override bool HasTarget() { return false; }
	override bool HasProgress() { return true; }

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!GetExorConfig().party.grupo.habilitado)
			return false;
		if (GetGame().IsServer())
			return ExorGroupManager.Get().m_PendingInvites.Contains(ExorGroupManager.SteamId(player));
		return player.ExorGetPendingInvite() != null;
	}

	override void OnStart(ActionData action_data)
	{
		super.OnStart(action_data);
		if (action_data.m_Player)
			action_data.m_Player.ExorClearPendingInvite();	// limpia el estado de cliente
	}

	override void OnFinishProgressServer(ActionData action_data)
	{
		if (action_data.m_Player)
			ExorGroupManager.Get().AcceptInvite(action_data.m_Player);
	}
}

// --------------------------- RECHAZAR INVITACION ---------------------------
class ExorActionDeclineInviteCB : ActionContinuousBaseCB
{
	override void CreateActionComponent() { m_ActionData.m_ActionComponent = new CAContinuousTime(1.0); }
}

class ExorActionDeclineInvite : ActionContinuousBase
{
	void ExorActionDeclineInvite()
	{
		m_CallbackClass = ExorActionDeclineInviteCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
		m_Text = "Rechazar invitacion al party";
	}

	override void CreateConditionComponents() { m_ConditionItem = new CCINone; m_ConditionTarget = new CCTNone; }
	override bool HasTarget() { return false; }
	override bool HasProgress() { return true; }

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!GetExorConfig().party.grupo.habilitado)
			return false;
		if (GetGame().IsServer())
			return ExorGroupManager.Get().m_PendingInvites.Contains(ExorGroupManager.SteamId(player));
		return player.ExorGetPendingInvite() != null;
	}

	override void OnStart(ActionData action_data)
	{
		super.OnStart(action_data);
		if (action_data.m_Player)
			action_data.m_Player.ExorClearPendingInvite();
	}

	override void OnFinishProgressServer(ActionData action_data)
	{
		if (action_data.m_Player)
			ExorGroupManager.Get().DeclineInvite(action_data.m_Player);
	}
}

// --------------------------- SALIR DEL PARTY (en el mastil) ---------------------------
class ExorActionLeavePartyCB : ActionContinuousBaseCB
{
	override void CreateActionComponent() { m_ActionData.m_ActionComponent = new CAContinuousTime(2.0); }
}

class ExorActionLeaveParty : ActionContinuousBase
{
	void ExorActionLeaveParty()
	{
		m_CallbackClass = ExorActionLeavePartyCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
		m_Text = "Salir del party";
	}

	override void CreateConditionComponents() { m_ConditionItem = new CCINone; m_ConditionTarget = new CCTObject(3.0); }
	override bool HasTarget() { return true; }
	override bool HasProgress() { return true; }

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!GetExorConfig().party.grupo.habilitado)
			return false;
		if (!target)
			return false;
		TerritoryFlag mast = TerritoryFlag.Cast(target.GetObject());
		if (!mast)
			return false;
		if (GetGame().IsServer())
		{
			string sid = ExorGroupManager.SteamId(player);
			ExorGroup g = ExorGroupManager.Get().FindById(mast.ExorGetGroupId());
			return g && g.HasMember(sid) && !g.IsOwner(sid);	// el lider usa "Quitar mastil"
		}
		return player.ExorClientInGroup() && !player.ExorClientIsLeader();
	}

	override void OnFinishProgressServer(ActionData action_data)
	{
		if (action_data.m_Player)
			ExorGroupManager.Get().Leave(action_data.m_Player);
	}
}

// --------------------------- KICKEAR (apuntando a un miembro) ---------------------------
class ExorActionKickMemberCB : ActionContinuousBaseCB
{
	override void CreateActionComponent() { m_ActionData.m_ActionComponent = new CAContinuousTime(1.5); }
}

class ExorActionKickMember : ActionContinuousBase
{
	void ExorActionKickMember()
	{
		m_CallbackClass = ExorActionKickMemberCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
		m_Text = "Sacar de mi party";
	}

	override void CreateConditionComponents() { m_ConditionItem = new CCINone; m_ConditionTarget = new CCTMan(3.0); }
	override bool HasTarget() { return true; }
	override bool HasProgress() { return true; }

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!GetExorConfig().party.grupo.habilitado)
			return false;
		if (!target)
			return false;
		PlayerBase tp = PlayerBase.Cast(target.GetObject());
		if (!tp || tp == player)
			return false;
		if (GetGame().IsServer())
		{
			string sid = ExorGroupManager.SteamId(player);
			ExorGroup g = ExorGroupManager.Get().FindByPlayer(sid);
			if (!g)
				return false;
			string tsid = ExorGroupManager.SteamId(tp);
			// cualquier miembro puede sacar a otro miembro (incluido el dueno = disuelve)
			return g.HasMember(sid) && g.HasMember(tsid);
		}
		return player.ExorClientInGroup();
	}

	override void OnFinishProgressServer(ActionData action_data)
	{
		PlayerBase tp = PlayerBase.Cast(action_data.m_Target.GetObject());
		if (action_data.m_Player && tp)
			ExorGroupManager.Get().Kick(action_data.m_Player, ExorGroupManager.SteamId(tp));
	}
}

// --------------------------- MARCAS (poner / limpiar) ---------------------------
class ExorActionPlaceMarkerCB : ActionContinuousBaseCB
{
	override void CreateActionComponent() { m_ActionData.m_ActionComponent = new CAContinuousTime(1.0); }
}

class ExorActionPlaceMarker : ActionContinuousBase
{
	void ExorActionPlaceMarker()
	{
		m_CallbackClass = ExorActionPlaceMarkerCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
		m_Text = "Poner marca para el party";
	}

	override void CreateConditionComponents() { m_ConditionItem = new CCINone; m_ConditionTarget = new CCTNone; }
	override bool HasTarget() { return false; }
	override bool HasProgress() { return true; }

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!GetExorConfig().party.grupo.habilitado || !GetExorConfig().party.grupo.permitir_marker_equipo)
			return false;
		if (GetGame().IsServer())
			return ExorGroupManager.Get().FindByPlayer(ExorGroupManager.SteamId(player)) != null;
		return player.ExorClientInGroup();
	}

	override void OnStart(ActionData action_data)
	{
		super.OnStart(action_data);
		// En el cliente: raycast desde la camara -> posicion del mundo -> avisar al server
		if (GetGame().IsClient() && action_data.m_Player)
		{
			vector aim = ExorMarkerAim(action_data.m_Player);
			action_data.m_Player.ExorReqMarkerAdd(aim);
		}
	}

	override void OnFinishProgressServer(ActionData action_data)
	{
		// la marca la pone el RPC desde OnStart (cliente)
	}

	// Punto del mundo al que apunta la camara (raycast)
	static vector ExorMarkerAim(PlayerBase player)
	{
		vector from = GetGame().GetCurrentCameraPosition();
		vector dir = GetGame().GetCurrentCameraDirection();
		vector to = from + dir * 1500;
		vector hitPos, hitNormal;
		int hitComp;
		set<Object> hitObjs = new set<Object>;
		if (DayZPhysics.RaycastRV(from, to, hitPos, hitNormal, hitComp, hitObjs, null, player))
			return hitPos;
		return to;
	}
}

class ExorActionClearMarkersCB : ActionContinuousBaseCB
{
	override void CreateActionComponent() { m_ActionData.m_ActionComponent = new CAContinuousTime(1.0); }
}

class ExorActionClearMarkers : ActionContinuousBase
{
	void ExorActionClearMarkers()
	{
		m_CallbackClass = ExorActionClearMarkersCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
		m_Text = "Limpiar marcas del party";
	}

	override void CreateConditionComponents() { m_ConditionItem = new CCINone; m_ConditionTarget = new CCTNone; }
	override bool HasTarget() { return false; }
	override bool HasProgress() { return true; }

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!GetExorConfig().party.grupo.habilitado || !GetExorConfig().party.grupo.permitir_marker_equipo)
			return false;
		if (GetGame().IsServer())
			return ExorGroupManager.Get().FindByPlayer(ExorGroupManager.SteamId(player)) != null;
		return player.ExorClientInGroup();
	}

	override void OnStart(ActionData action_data)
	{
		super.OnStart(action_data);
		if (GetGame().IsClient() && action_data.m_Player)
			action_data.m_Player.ExorReqMarkerClear();
	}

	override void OnFinishProgressServer(ActionData action_data)
	{
		// el limpiado lo hace el RPC desde OnStart (cliente)
	}
}
