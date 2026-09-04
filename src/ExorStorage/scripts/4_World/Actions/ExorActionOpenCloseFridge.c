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
		{
			fur.Close();	// CERRAR siempre se permite: nunca dejar un mueble trabado abierto
			return;
		}

		// ANTI-DUPE: ventana de gracia al entrar (ver ExorStorageBootLock). Solo frena ABRIR,
		// y se chequea aca (server) y no en ActionCondition, que corre en el cliente.
		if (ExorStorageBootLock.BloqueadoConAviso(action_data.m_Player))
			return;

		// ABRIR = LOTEAR: si "solo miembros lotean" esta activo y NO es horario libre (raid),
		// solo los miembros del territorio pueden abrirlo. Si no, aviso rojo y no abre.
		string denyReason;
		if (!ExorMuebleRules.CanLootMueble(action_data.m_Player, fur, denyReason))
		{
			ExorMuebleRules.SendRed(action_data.m_Player, denyReason);
			return;
		}

		// CLAVE (locker): si tiene candado + clave seteada, se le pide la clave AL MIEMBRO que
		// abre y que todavia no la ingreso (se abre el modal, no se abre el locker). A los AJENOS
		// NO se les pide (en horario de raid ya pasaron el CanLootMueble): el locker se les abre
		// directo mostrando el loot. Una vez que el miembro mete la clave, no se le pide mas.
		PlayerBase player = action_data.m_Player;
		if (fur.ExorHasCodeLock() && fur.ExorHasKey())
		{
			bool isMember = false;
			ExorTerritoryManager tm = ExorTerritoryManager.Get();
			if (tm && tm.IsInOwnGroupTerritory(player, fur.GetPosition()))
				isMember = true;
			string sid = ExorGroupManager.SteamId(player);
			if (isMember && !fur.ExorIsUnlockedBy(sid))
			{
				player.ExorOpenLockKeyModal(fur, ExorLockKeyClient.MODE_ENTER);
				return;	// no abrir hasta que ingrese la clave correcta
			}
		}
		// TECHO DE CONTENEDORES REALES POR BASE: si la base ya llego al maximo, se guarda
		// solo el que hace mas rato que nadie usa y despues se abre este. Ver
		// ExorMuebleRules.HacerLugarParaAbrir.
		ExorMuebleRules.HacerLugarParaAbrir(player, fur, fur.GetPosition());
		fur.Open();
	}
}
