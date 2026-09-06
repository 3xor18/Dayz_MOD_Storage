// ============================================================================
// 3xor_Vanilla_Optimization - Accion: Administrar party (Fase E)
// Apuntando al MASTIL, cualquier miembro del party abre el panel para
// administrar (ver miembros / expulsar). Abre el menu en el CLIENTE (OnStart).
// ============================================================================
class ExorActionOpenPartyMenuCB : ActionContinuousBaseCB
{
	override void CreateActionComponent()
	{
		m_ActionData.m_ActionComponent = new CAContinuousTime(0.1);
	}
}

class ExorActionOpenPartyMenu : ActionContinuousBase
{
	void ExorActionOpenPartyMenu()
	{
		m_CallbackClass = ExorActionOpenPartyMenuCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH | DayZPlayerConstants.STANCEMASK_PRONE;
		m_Text = "Administrar party";
	}

	override void CreateConditionComponents()
	{
		m_ConditionItem = new CCINone;
		m_ConditionTarget = new CCTObject(3.0);
	}

	override bool HasTarget() { return true; }
	override bool HasProgress() { return true; }

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!GetExorConfig().party.grupo.habilitado)
			return false;
		if (!target || !player)
			return false;
		TerritoryFlag mast = TerritoryFlag.Cast(target.GetObject());
		if (!mast)
			return false;

		if (GetGame().IsServer())
		{
			ExorGroup g = ExorGroupManager.Get().FindById(mast.ExorGetGroupId());
			return g && g.HasMember(ExorGroupManager.SteamId(player));
		}
		// cliente: solo si estoy en un grupo Y este mastil es de MI territorio
		// (evita que "Administrar" aparezca en mastiles ajenos)
		return ExorTerritoryClient.MastEsMio(player, mast.GetPosition());
	}

	override void OnStart(ActionData action_data)
	{
		super.OnStart(action_data);
		if (GetGame().IsClient())
			GetGame().GetUIManager().EnterScriptedMenu(ExorMenuIDs.PARTY, null);
	}

	override void OnFinishProgressServer(ActionData action_data)
	{
		// nada server-side
	}
}
