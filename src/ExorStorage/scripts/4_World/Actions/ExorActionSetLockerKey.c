// ============================================================================
// 3xorStorage - Accion: Poner/Cambiar la clave de un locker
// ----------------------------------------------------------------------------
// Aparece SOLO en muebles con candado (ExorHasCodeLock -> lockers). Abre un modal en
// el cliente para escribir la clave (2 inputs que deben coincidir). Permisos:
//   - Solo MIEMBROS del territorio (o staff) pueden poner/cambiar la clave.
//   - CAMBIAR la clave requiere haber ingresado la clave actual antes (haberla
//     desbloqueado); PONERLA por primera vez (sin clave) es libre para un miembro.
// La escritura real de la clave la hace el server al recibir LOCK_MODAL_SUBMIT.
// ============================================================================
class ExorActionSetLockerKey : ActionInteractBase
{
	void ExorActionSetLockerKey()
	{
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONMOD_OPENDOORFW;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_CROUCH | DayZPlayerConstants.STANCEMASK_ERECT;
		m_HUDCursorIcon = CursorIcons.OpenDoors;
		m_Text = "Poner/Cambiar clave";
	}

	override void CreateConditionComponents()
	{
		m_ConditionItem = new CCINone;
		m_ConditionTarget = new CCTNonRuined(UAMaxDistances.DEFAULT);
	}

	override string GetText() { return "Poner/Cambiar clave"; }

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!target)
			return false;
		Exor_OpenableStorage fur = Exor_OpenableStorage.Cast(target.GetObject());
		return fur != null && fur.ExorHasCodeLock();
	}

	override void OnStartServer(ActionData action_data)
	{
		Exor_OpenableStorage fur = Exor_OpenableStorage.Cast(action_data.m_Target.GetObject());
		PlayerBase player = action_data.m_Player;
		if (!fur || !player)
			return;

		// SOLO MIEMBROS del territorio (o staff). CanPackAtPos NO usa horario libre -> ni en raid
		// un ajeno le puede poner/cambiar clave.
		string deny;
		if (!ExorMuebleRules.CanPackAtPos(player, fur.GetPosition(), deny))
		{
			ExorMuebleRules.SendRed(player, "Solo los miembros del clan pueden ponerle clave al locker.");
			return;
		}

		// CAMBIAR requiere tener la clave previa (haberla desbloqueado). PONERLA (sin clave) es libre.
		string sid = ExorGroupManager.SteamId(player);
		if (fur.ExorHasKey() && !fur.ExorIsUnlockedBy(sid))
		{
			ExorMuebleRules.SendRed(player, "Necesitás ingresar la clave actual antes de poder cambiarla.");
			return;
		}

		player.ExorOpenLockKeyModal(fur, ExorLockKeyClient.MODE_SET);
	}
}
