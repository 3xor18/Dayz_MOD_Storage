// ============================================================================
// 3xor_Vanilla_Optimization - Sueno automatico de vehiculos (Fase 2.5, v3)
// El vehiculo NUNCA se mueve ni desaparece: queda visible en su lugar.
// Cuando esta inactivo y sin jugadores cerca, se le desactiva la simulacion
// fisica (DisableSimulation = el costo del vehiculo baja a ~cero).
// Cuando un jugador entra al radio configurado, despierta solo.
// Los jugadores no notan nada. El CE lo cuenta normal (no spawnea clones).
// ============================================================================
modded class CarScript
{
	protected int m_ExorLastActiveMs;
	protected bool m_ExorSleeping;
	protected int m_ExorLastNearMs;		// ultimo ms con un jugador cerca (para auto-virtualizar)

	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			m_ExorLastActiveMs = GetGame().GetTime();
			m_ExorLastNearMs = GetGame().GetTime();	// arranca "recien usado" -> no se auto-virtualiza al toque
			m_ExorSleeping = false;
			ExorVO_Manager.RegisterVehicle(this);

			// Fase H: quitar dano a vehiculos (no reciben dano de ningun tipo)
			if (GetExorConfig().vehiculos.dano.quitar_dano_vehiculos)
				SetAllowDamage(false);
		}
	}

	bool ExorIsSleeping()
	{
		return m_ExorSleeping;
	}

	void ExorSleep()
	{
		if (m_ExorSleeping)
			return;
		m_ExorSleeping = true;
		DisableSimulation(true);
	}

	void ExorWake()
	{
		if (!m_ExorSleeping)
			return;
		m_ExorSleeping = false;
		DisableSimulation(false);
		m_ExorLastActiveMs = GetGame().GetTime();
	}

	bool ExorIsActive()
	{
		if (EngineIsOn())
			return true;
		int i;
		for (i = 0; i < CrewSize(); i++)
		{
			if (CrewMember(i))
				return true;
		}
		return false;
	}

	void ExorMarkActive(int now)
	{
		m_ExorLastActiveMs = now;
	}

	int ExorGetLastActive()
	{
		return m_ExorLastActiveMs;
	}

	// --- auto-virtualizado: timer de "ultimo jugador cerca" ---
	void ExorSetLastNear(int now)
	{
		m_ExorLastNearMs = now;
	}
	int ExorGetLastNear()
	{
		return m_ExorLastNearMs;
	}
}
