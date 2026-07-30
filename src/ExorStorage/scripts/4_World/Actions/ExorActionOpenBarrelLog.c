// ============================================================================
// 3xor_Vanilla_Optimization - Anti-raid #5: log de apertura de barril ajeno.
// Hook a la accion VANILLA de abrir barril. Al ejecutarse en el server, si el
// objetivo es un barril 3xor (Exor_Barrel_Base) y cae dentro de territorio
// AJENO, ExorAntiRaid escribe la linea forense (fecha+hora, steamID, nombre,
// pos del barril). super() primero para no alterar el abrir vanilla.
// ============================================================================
modded class ActionOpenBarrel
{
	// ANTI-DUPE: ventana de gracia al entrar. El que rushea el barril justo despues de un
	// reinicio es el que acaba de conectarse. El chequeo real vive en Exor_Barrel_Base.Open(),
	// que es server-side; aca solo le decimos QUIEN esta abriendo (la accion es la unica que
	// tiene el player). Se hace asi y no en ActionCondition porque esa corre en el CLIENTE.
	override void OnExecuteServer(ActionData action_data)
	{
		if (action_data)
			ExorStorageBootLock.s_Abriendo = PlayerBase.Cast(action_data.m_Player);
		super.OnExecuteServer(action_data);
		ExorStorageBootLock.s_Abriendo = null;

		if (!action_data || !action_data.m_Target)
			return;

		Exor_Barrel_Base barril = Exor_Barrel_Base.Cast(action_data.m_Target.GetObject());
		if (barril)
			ExorAntiRaid.OnOpenBarrelInEnemyTerritory(action_data.m_Player, barril);
	}
}
