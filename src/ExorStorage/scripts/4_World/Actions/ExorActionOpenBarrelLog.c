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
		if (!action_data || !action_data.m_Target)
		{
			super.OnExecuteServer(action_data);
			return;
		}

		Exor_Barrel_Base barril = Exor_Barrel_Base.Cast(action_data.m_Target.GetObject());
		PlayerBase quien = PlayerBase.Cast(action_data.m_Player);

		// GATE DE MIEMBRO (igual que los lockers): un barril 3xor dentro de un territorio solo
		// lo abren los miembros de ese clan. La EXCEPCION es el horario de raid, donde lo abre
		// cualquiera. Se chequea ANTES de super() para que el barril ni siquiera se abra.
		// Fuera de todo territorio el barril no tiene dueño -> lo abre cualquiera.
		if (barril && quien)
		{
			string denyReason;
			if (!ExorMuebleRules.CanLootAtPos(quien, barril.GetPosition(), denyReason))
			{
				if (denyReason == "")
					denyReason = "Solo los miembros del clan pueden abrir este barril.";
				ExorMuebleRules.SendRed(quien, denyReason);
				return;		// no se llama a super -> el barril no se abre
			}
		}

		// TECHO DE CONTENEDORES REALES POR BASE (mismo criterio que los muebles): si la base
		// ya llego al maximo, se guarda solo el mas viejo sin usar antes de abrir este.
		if (barril)
			ExorMuebleRules.HacerLugarParaAbrir(quien, barril, barril.GetPosition());

		ExorStorageBootLock.s_Abriendo = quien;
		super.OnExecuteServer(action_data);
		ExorStorageBootLock.s_Abriendo = null;

		// Log forense de apertura de barril ajeno. Sale gratis si esta apagado en config
		// (log_abrir_barril_ajeno): en horario de raid con mucha gente abriendo contenedores
		// esto era I/O y busquedas de territorio por cada apertura.
		if (barril)
			ExorAntiRaid.OnOpenBarrelInEnemyTerritory(quien, barril);
	}
}
