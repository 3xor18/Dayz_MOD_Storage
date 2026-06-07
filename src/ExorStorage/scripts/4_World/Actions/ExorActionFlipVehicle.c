// ============================================================================
// 3xor_Vanilla_Optimization - Accion: Voltear vehiculo
// Devuelve a posicion natural un vehiculo volcado o de costado.
// Dura voltear_segundos (default 40) como anti-abuso en combate.
// Condiciones: inclinado mas de ~60 grados, motor apagado, sin ocupantes.
// ============================================================================

class ExorActionFlipVehicleCB : ActionContinuousBaseCB
{
	override void CreateActionComponent()
	{
		ExorStorageSettings settings = GetExorStorageSettings();
		m_ActionData.m_ActionComponent = new CAContinuousTime(settings.voltear_segundos);
	}
}

class ExorActionFlipVehicle : ActionContinuousBase
{
	void ExorActionFlipVehicle()
	{
		m_CallbackClass = ExorActionFlipVehicleCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_FullBody = true;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
		m_Text = "Voltear vehiculo";
	}

	override void CreateConditionComponents()
	{
		m_ConditionItem = new CCINone;
		m_ConditionTarget = new CCTObject(UAMaxDistances.DEFAULT);
	}

	override bool HasProgress()
	{
		return true;
	}

	static bool ExorIsFlipped(CarScript car)
	{
		// up[1] = componente vertical del eje "arriba" del auto:
		// 1 = derecho, 0 = de costado, -1 = boca abajo
		vector up = car.GetDirectionUp();
		if (up[1] > 0.5)
			return false;
		return true;
	}

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!target)
			return false;
		CarScript car = CarScript.Cast(target.GetObject());
		if (!car)
			return false;

		ExorStorageSettings settings = GetExorStorageSettings();
		if (!settings.voltear_vehiculos)
			return false;
		if (!ExorIsFlipped(car))
			return false;
		if (car.EngineIsOn())
			return false;
		int i;
		for (i = 0; i < car.CrewSize(); i++)
		{
			if (car.CrewMember(i))
				return false;
		}
		return true;
	}

	override void OnFinishProgressServer(ActionData action_data)
	{
		if (!action_data || !action_data.m_Target)
			return;
		CarScript car = CarScript.Cast(action_data.m_Target.GetObject());
		if (!car)
			return;
		if (!ExorIsFlipped(car))
			return;

		// Enderezar: conservar la direccion (yaw), anular vuelco (pitch/roll)
		vector ori = car.GetOrientation();
		ori[1] = 0;
		ori[2] = 0;
		vector pos = car.GetPosition();
		pos[1] = pos[1] + 0.5;	// levantarlo un poco para que no quede clavado

		car.SetOrientation(ori);
		car.SetPosition(pos);

		Print(string.Format("%1 Vehiculo %2 volteado a posicion natural", ExorStorageConstants.LOG, car.GetType()));
	}
}
