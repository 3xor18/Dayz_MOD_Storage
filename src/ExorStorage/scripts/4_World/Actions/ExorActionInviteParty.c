// ============================================================================
// 3xor_Vanilla_Optimization - Acciones de ingreso al party (apuntando al MASTIL)
// Nuevo flujo (pedido): para que alguien pueda entrar, un MIEMBRO tiene que abrir
// la invitacion mirando el mastil ("Invitar a grupo"). Recien ahi el otro jugador
// ve "Unirse al grupo" en el mastil. Una invitacion = un ingreso (hay que reabrir
// para invitar a otro). Un miembro puede "Cancelar invitacion"; ademas se auto-
// cancela a los 10 min. El estado abierto/cerrado se sincroniza por el mastil.
// ============================================================================

// --------------------------- INVITAR (abrir invitacion) ---------------------------
class ExorActionOpenInviteCB : ActionContinuousBaseCB
{
	override void CreateActionComponent() { m_ActionData.m_ActionComponent = new CAContinuousTime(0.1); }
}

class ExorActionOpenInvite : ActionContinuousBase
{
	void ExorActionOpenInvite()
	{
		m_CallbackClass = ExorActionOpenInviteCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH | DayZPlayerConstants.STANCEMASK_PRONE;
		m_Text = "Invitar a grupo";
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
		if (mast.ExorIsInviteOpen())
			return false;	// ya abierta: se muestra "Cancelar invitacion"
		if (GetGame().IsServer())
		{
			ExorGroup g = ExorGroupManager.Get().FindById(mast.ExorGetGroupId());
			return g && g.HasMember(ExorGroupManager.SteamId(player));
		}
		return player.ExorClientInGroup();
	}

	// Efecto en OnStartServer (no en OnFinishProgressServer): las acciones continuas
	// sobre el mastil no completaban (OnFinishProgressServer nunca disparaba). Esto
	// dispara al instante en el server, con el target ya disponible.
	override void OnStartServer(ActionData action_data)
	{
		TerritoryFlag mast = TerritoryFlag.Cast(action_data.m_Target.GetObject());
		if (action_data.m_Player && mast)
			ExorGroupManager.Get().OpenInviteAtMast(action_data.m_Player, mast);
	}
}

// --------------------------- UNIRSE (el invitado) ---------------------------
class ExorActionJoinGroupCB : ActionContinuousBaseCB
{
	override void CreateActionComponent() { m_ActionData.m_ActionComponent = new CAContinuousTime(0.5); }
}

class ExorActionJoinGroup : ActionContinuousBase
{
	void ExorActionJoinGroup()
	{
		m_CallbackClass = ExorActionJoinGroupCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH | DayZPlayerConstants.STANCEMASK_PRONE;
		m_Text = "Unirse al grupo";
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
		if (!mast.ExorIsInviteOpen())
			return false;	// solo si hay invitacion abierta
		if (GetGame().IsServer())
			return ExorGroupManager.Get().FindByPlayer(ExorGroupManager.SteamId(player)) == null;
		return !player.ExorClientInGroup();	// solo si NO estoy ya en un party
	}

	override void OnStartServer(ActionData action_data)
	{
		TerritoryFlag mast = TerritoryFlag.Cast(action_data.m_Target.GetObject());
		if (action_data.m_Player && mast)
			ExorGroupManager.Get().JoinAtMast(action_data.m_Player, mast);
	}
}

// --------------------------- CANCELAR INVITACION ---------------------------
class ExorActionCancelInviteCB : ActionContinuousBaseCB
{
	override void CreateActionComponent() { m_ActionData.m_ActionComponent = new CAContinuousTime(0.1); }
}

class ExorActionCancelInvite : ActionContinuousBase
{
	void ExorActionCancelInvite()
	{
		m_CallbackClass = ExorActionCancelInviteCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH | DayZPlayerConstants.STANCEMASK_PRONE;
		m_Text = "Cancelar invitacion";
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
		if (!mast.ExorIsInviteOpen())
			return false;	// solo si hay una abierta
		if (GetGame().IsServer())
		{
			ExorGroup g = ExorGroupManager.Get().FindById(mast.ExorGetGroupId());
			return g && g.HasMember(ExorGroupManager.SteamId(player));
		}
		return player.ExorClientInGroup();
	}

	override void OnStartServer(ActionData action_data)
	{
		TerritoryFlag mast = TerritoryFlag.Cast(action_data.m_Target.GetObject());
		if (action_data.m_Player && mast)
			ExorGroupManager.Get().CancelInviteAtMast(action_data.m_Player, mast);
	}
}
