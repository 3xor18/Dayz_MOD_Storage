// ============================================================================
// 3xor_Vanilla_Optimization - Izar / Bajar bandera (Fase D, v3)
// Acciones CUSTOM en la RUEDA del scroll (input default), que ejecutan al
// instante via OnStartServer (las acciones continuas sobre el mastil no
// completan OnFinishProgressServer; OnStartServer es confiable). Reglas:
//   - Izar: SOLO miembros del party.
//   - Bajar: miembros siempre; ajenos solo si party.bandera.ajenos_pueden_bajar.
// Se SUPRIME la accion vanilla de bandera en nuestros mastiles (no duplicar en F).
// El estado izada/bajada alimenta el bloqueo de respawn en base.
// ============================================================================

class ExorPartyHelper
{
	static bool IsMemberOfMast(PlayerBase player, TerritoryFlag mast)
	{
		if (!player || !mast)
			return false;
		ExorGroup g = ExorGroupManager.Get().FindById(mast.ExorGetGroupId());
		if (!g)
			return false;
		return g.HasMember(ExorGroupManager.SteamId(player));
	}
}

// ------------------------------ IZAR (custom, rueda) ------------------------------
class ExorActionRaiseFlagCB : ActionContinuousBaseCB
{
	override void CreateActionComponent() { m_ActionData.m_ActionComponent = new CAContinuousTime(15.0); }
}

class ExorActionRaiseFlag : ActionContinuousBase
{
	void ExorActionRaiseFlag()
	{
		m_CallbackClass = ExorActionRaiseFlagCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_FullBody = true;	// CLAVE: sin full-body la barra no completa (no dispara OnFinishProgressServer)
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
		m_Text = "Izar bandera";
	}

	override void CreateConditionComponents() { m_ConditionItem = new CCINone; m_ConditionTarget = new CCTObject(3.0); }
	override bool HasTarget() { return true; }
	override bool HasProgress() { return true; }

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!target)
			return false;
		TerritoryFlag mast = TerritoryFlag.Cast(target.GetObject());
		if (!mast)
			return false;
		return !mast.ExorIsFlagRaised();	// se muestra cuando esta abajo
	}

	// Efecto al COMPLETAR el cast (mantener ~15s). Con m_FullBody, OnFinishProgressServer SI dispara.
	override void OnFinishProgressServer(ActionData action_data)
	{
		TerritoryFlag mast = TerritoryFlag.Cast(action_data.m_Target.GetObject());
		PlayerBase player = action_data.m_Player;
		if (!mast || !player)
			return;
		if (!ExorPartyHelper.IsMemberOfMast(player, mast))
		{
			player.MessageImportant("Solo los miembros del party pueden izar la bandera.");
			return;
		}
		mast.ExorServerSetFlagRaised(true);
		player.MessageImportant("Bandera izada.");
	}
}

// ------------------------------ BAJAR (custom, rueda) ------------------------------
class ExorActionLowerFlagCB : ActionContinuousBaseCB
{
	override void CreateActionComponent() { m_ActionData.m_ActionComponent = new CAContinuousTime(15.0); }
}

class ExorActionLowerFlag : ActionContinuousBase
{
	void ExorActionLowerFlag()
	{
		m_CallbackClass = ExorActionLowerFlagCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_FullBody = true;	// CLAVE: sin full-body la barra no completa
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
		m_Text = "Bajar bandera";
	}

	override void CreateConditionComponents() { m_ConditionItem = new CCINone; m_ConditionTarget = new CCTObject(3.0); }
	override bool HasTarget() { return true; }
	override bool HasProgress() { return true; }

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!target)
			return false;
		TerritoryFlag mast = TerritoryFlag.Cast(target.GetObject());
		if (!mast)
			return false;
		return mast.ExorIsFlagRaised();	// se muestra cuando esta arriba
	}

	// Efecto al COMPLETAR el cast (~15s). Con m_FullBody, OnFinishProgressServer SI dispara.
	override void OnFinishProgressServer(ActionData action_data)
	{
		TerritoryFlag mast = TerritoryFlag.Cast(action_data.m_Target.GetObject());
		PlayerBase player = action_data.m_Player;
		if (!mast || !player)
			return;
		if (!ExorPartyHelper.IsMemberOfMast(player, mast) && !GetExorConfig().party.bandera.ajenos_pueden_bajar)
		{
			player.MessageImportant("No podés bajar esta bandera (no sos del party).");
			return;
		}
		mast.ExorServerSetFlagRaised(false);
		player.MessageImportant("Bandera bajada.");
	}
}

// Suprimir las acciones VANILLA de bandera en NUESTROS mastiles (no duplicar en F).
modded class ActionRaiseFlag
{
	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		TerritoryFlag totem = TerritoryFlag.Cast(target.GetObject());
		if (totem && totem.ExorGetGroupId() != "")
			return false;
		return super.ActionCondition(player, target, item);
	}
}

modded class ActionLowerFlag
{
	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		TerritoryFlag totem = TerritoryFlag.Cast(target.GetObject());
		if (totem && totem.ExorGetGroupId() != "")
			return false;
		return super.ActionCondition(player, target, item);
	}
}
