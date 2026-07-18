// ============================================================================
// 3xorStorage - Accion: Abrir/Cerrar el refrigerador (toggle)
// Patron MMG (ActionOpenCloseCrate_noLock): una interaccion que alterna el
// estado abierto/cerrado del contenedor. El texto cambia segun el estado.
// ============================================================================

class ExorActionOpenCloseFridge : ActionInteractBase
{
	void ExorActionOpenCloseFridge()
	{
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_OPENDOORFW;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_CROUCH | DayZPlayerConstants.STANCEMASK_ERECT;
		m_HUDCursorIcon = CursorIcons.OpenDoors;
		m_Text = "Abrir";
	}

	override void CreateConditionComponents()
	{
		m_ConditionItem = new CCINone;
		m_ConditionTarget = new CCTNonRuined(UAMaxDistances.DEFAULT);
	}

	override string GetText()
	{
		return m_Text;
	}

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!target)
			return false;

		Exor_OpenableStorage fur = Exor_OpenableStorage.Cast(target.GetObject());
		if (!fur)
			return false;

		if (fur.IsOpen())
			m_Text = "Cerrar";
		else
			m_Text = "Abrir";

		return true;
	}

	// El toggle corre en el SERVER (Open/Close net-sincroniza a los clientes).
	override void OnStartServer(ActionData action_data)
	{
		Exor_OpenableStorage fur = Exor_OpenableStorage.Cast(action_data.m_Target.GetObject());
		if (!fur)
			return;

		if (fur.IsOpen())
			fur.Close();
		else
			fur.Open();
	}
}
