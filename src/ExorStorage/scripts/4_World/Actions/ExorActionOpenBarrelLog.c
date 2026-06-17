// ============================================================================
// 3xor_Vanilla_Optimization - Anti-raid #5: log de apertura de barril ajeno.
// Hook a la accion VANILLA de abrir barril. Al ejecutarse en el server, si el
// objetivo es un barril 3xor (Exor_Barrel_Base) y cae dentro de territorio
// AJENO, ExorAntiRaid escribe la linea forense (fecha+hora, steamID, nombre,
// pos del barril). super() primero para no alterar el abrir vanilla.
// ============================================================================
modded class ActionOpenBarrel
{
	override void OnExecuteServer(ActionData action_data)
	{
		super.OnExecuteServer(action_data);

		if (!action_data || !action_data.m_Target)
			return;

		Exor_Barrel_Base barril = Exor_Barrel_Base.Cast(action_data.m_Target.GetObject());
		if (barril)
			ExorAntiRaid.OnOpenBarrelInEnemyTerritory(action_data.m_Player, barril);
	}
}
