// ============================================================================
// 3xorStorage - Accion: Administrar autos (parking)
// ============================================================================
// Al interactuar con el parking: chequea permiso (miembro del clan, salvo en horario
// libre) y muestra los autos en el radio. Por AHORA lista por mensaje (validar la
// cadena colocar->interactuar->escanear). El menu de botones (virtualizar/desvirtualizar)
// se conecta despues via RPC + UI cliente.
// ============================================================================
class ExorActionOpenParking : ActionInteractBase
{
	void ExorActionOpenParking()
	{
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_OPENDOORFW;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_CROUCH | DayZPlayerConstants.STANCEMASK_ERECT;
		m_HUDCursorIcon = CursorIcons.OpenDoors;
		m_Text = "Administrar autos";
	}

	override void CreateConditionComponents()
	{
		m_ConditionItem = new CCINone;
		m_ConditionTarget = new CCTNonRuined(UAMaxDistances.DEFAULT);
	}

	override string GetText() { return "Administrar autos"; }

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!target)
			return false;
		return Exor_Parking.Cast(target.GetObject()) != null;
	}

	override void OnStartServer(ActionData action_data)
	{
		Exor_Parking parking = Exor_Parking.Cast(action_data.m_Target.GetObject());
		PlayerBase player = action_data.m_Player;
		if (!parking || !player)
			return;

		// PERMISO: solo miembros del clan, salvo en horario libre (config).
		string denyReason;
		if (!ExorMuebleRules.CanLootAtPos(player, parking.GetPosition(), denyReason))
		{
			ExorMuebleRules.SendRed(player, denyReason);
			return;
		}

		// Abre el menu de administracion de autos en el cliente: el server guarda el
		// parking que esta gestionando (para refrescar/actuar) y le manda la lista por RPC.
		player.ExorOpenParkingManager(parking.GetPosition());
	}
}
