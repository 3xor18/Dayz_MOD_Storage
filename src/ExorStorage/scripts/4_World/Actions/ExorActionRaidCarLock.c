// ============================================================================
// 3xorStorage - Accion: Quitar Codelock de un AUTO (raid, para NO-miembros)
// ----------------------------------------------------------------------------
// Aparece MIRANDO un auto CON candado, a quien NO es del clan dueño (ni admin) y
// tiene una HERRAMIENTA habilitada en la mano (array del codelock_autos.json). Barra de
// progreso cuyo tiempo sale del config de ESA herramienta; al completarse, tira el
// % de acierto de esa herramienta -> si sale, se quita el candado; si no, no pasa nada
// (puede reintentar). Al empezar suena la alarma del auto (config alarma_sonido).
//
// Defaults (codelock_autos.json): Lockpick 60% / 60s, Screwdriver 45% / 120s, cuchillos (menos
// el de piedra) 35% / 180s. Todo editable por classname.
// ============================================================================
class ExorActionRaidCarLockCB : ActionContinuousBaseCB
{
	override void CreateActionComponent()
	{
		// tiempo = el de la herramienta en la mano (o 120s si por lo que sea no se encuentra)
		float secs = 120;
		if (m_ActionData && m_ActionData.m_MainItem)
		{
			ExorCfgCarLockTool t = GetExorConfig().carlock.ExorToolFor(m_ActionData.m_MainItem.GetType());
			if (t && t.segundos_raid > 0)
				secs = t.segundos_raid;
		}
		m_ActionData.m_ActionComponent = new CAContinuousTime(secs);
	}
}

class ExorActionRaidCarLock : ActionContinuousBase
{
	void ExorActionRaidCarLock()
	{
		m_CallbackClass = ExorActionRaidCarLockCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_FullBody = true;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
		m_Text = "Quitar Codelock";
	}

	override void CreateConditionComponents()
	{
		m_ConditionItem = new CCINone;
		m_ConditionTarget = new CCTObject(4.0);	// mismo target que Voltear vehiculo (probado en autos)
	}

	override bool HasProgress() { return true; }

	override string GetText() { return "Quitar Codelock"; }

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!target || !player || !item || !GetExorConfig().carlock.activado)
			return false;
		CarScript car = CarScript.Cast(target.GetObject());
		if (!car || !car.ExorCarHasLock())
			return false;
		// la herramienta en la mano tiene que estar habilitada (el cuchillo de piedra NO esta)
		if (GetExorConfig().carlock.ExorToolFor(item.GetType()) == null)
			return false;
		string sid = ExorGroupManager.SteamId(player);
		// SERVER estricto: miembros del clan y admins NO raidean (entran con la clave / bypass).
		// CLIENTE: mostrar solo si NO soy miembro (= ajeno). El admin cuenta como miembro (IsMember
		// devuelve true si sos admin) -> tampoco ve raid.
		if (GetGame().IsServer())
			return !car.ExorCarIsAdmin(sid) && !car.ExorCarIsMemberOfLockGroup(sid);
		return !ExorCarAccessClient.IsMember(car);
	}

	// al empezar el raid: arranca la alarma repetida del auto ("TU TU TU")
	override void OnStartServer(ActionData action_data)
	{
		super.OnStartServer(action_data);
		CarScript car = CarScript.Cast(action_data.m_Target.GetObject());
		if (car)
			car.ExorCarStartAlarm();	// sync (para otros cercanos, cuando funcione el hook)
	}

	override void OnEndServer(ActionData action_data)
	{
		CarScript car = CarScript.Cast(action_data.m_Target.GetObject());
		if (car)
			car.ExorCarStopAlarm();
		super.OnEndServer(action_data);
	}

	// (el sonido lo dispara el server con un RPC al auto -> llega a todos los clientes cercanos,
	//  ver ExorCarStartAlarm/ExorCarStopAlarm. No hace falta hook de cliente en la accion.)

	override void OnFinishProgressServer(ActionData action_data)
	{
		CarScript car = CarScript.Cast(action_data.m_Target.GetObject());
		PlayerBase player = action_data.m_Player;
		ItemBase item = action_data.m_MainItem;
		if (!car || !player || !item)
			return;

		ExorCfgCarLockTool t = GetExorConfig().carlock.ExorToolFor(item.GetType());
		if (!t)
			return;	// la herramienta ya no esta (cambio de mano) -> no pasa nada

		if (!car.ExorCarHasLock())
		{
			ExorMuebleRules.SendChat(player, "El auto ya no tiene candado.");
			return;
		}

		float roll = Math.RandomFloat01() * 100.0;
		if (roll < t.acierto_porcentaje)
		{
			car.ExorCarClearLock();
			ExorMuebleRules.SendChat(player, "¡Sacaste el candado del auto!");
		}
		else
		{
			ExorMuebleRules.SendRed(player, "No lograste sacar el candado. Probá de nuevo.");
		}
	}
}
