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

	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			m_ExorLastActiveMs = GetGame().GetTime();
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
}
