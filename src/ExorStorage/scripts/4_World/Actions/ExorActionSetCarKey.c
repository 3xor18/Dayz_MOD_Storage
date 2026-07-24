// ============================================================================
// 3xorStorage - Accion: Poner/Cambiar la clave de un AUTO (candado propio)
// ----------------------------------------------------------------------------
// CONTINUA (mantener F un instante), NO interact: en un auto, la accion interact
// (tap F) se la come "subir al vehiculo". Las acciones CONTINUAS (como voltear el
// auto) SI disparan en los vehiculos. Por eso esta se mantiene apretada un momento.
//
//   - Auto SIN candado: requiere el keypad (Exor_CarCodeLock) EN LA MANO -> "Colocar
//     candado" (el que lo coloca setea la 1ra clave; si esta en un clan es del clan,
//     si no queda a su nombre).
//   - Auto CON candado: sin item -> "Cambiar clave", solo a quien TIENE la clave
//     (miembro que la ingreso) o admin.
// Al completar el hold, el server abre el modal (ExorDoCarKeySubmit hace la escritura).
// ============================================================================
class ExorActionSetCarKeyCB : ActionContinuousBaseCB
{
	override void CreateActionComponent()
	{
		m_ActionData.m_ActionComponent = new CAContinuousTime(1.0);	// 1s de hold para abrir el modal
	}
}

class ExorActionSetCarKey : ActionContinuousBase
{
	void ExorActionSetCarKey()
	{
		m_CallbackClass = ExorActionSetCarKeyCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_FullBody = true;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
		m_Text = "Poner/Cambiar clave";
	}

	override void CreateConditionComponents()
	{
		m_ConditionItem = new CCINone;
		m_ConditionTarget = new CCTObject(4.0);	// mismo target que Voltear vehiculo (probado en autos)
	}

	override bool HasProgress() { return true; }

	override string GetText() { return "Poner/Cambiar clave"; }

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!target || !GetExorConfig().carlock.activado)
			return false;
		CarScript car = CarScript.Cast(target.GetObject());
		if (!car)
			return false;
		if (!GetExorConfig().carlock.ExorAutoPermitido(car.GetType()))
			return false;

		string sid = ExorGroupManager.SteamId(player);
		bool tieneKeypad = item != null && item.IsInherited(Exor_CarCodeLock);
		if (!car.ExorCarHasLock())
		{
			// COLOCAR: solo CON el keypad en la mano (solo o clan).
			return tieneKeypad;
		}
		// El auto YA tiene candado:
		//   - CON keypad en la mano -> NO mostrar nada (no se instala un 2do; el keypad es para OTRO auto).
		//   - SIN keypad -> "Cambiar clave", solo al que TIENE la clave.
		if (tieneKeypad)
			return false;
		// SERVER: chequeo real (miembro con clave o admin). CLIENTE: el set de acceso (lo unico
		// que el cliente puede saber; el server re-valida al ejecutar).
		if (GetGame().IsServer())
			return (car.ExorCarIsMemberOfLockGroup(sid) && car.ExorCarIsUnlockedBy(sid)) || car.ExorCarIsAdmin(sid);
		return ExorCarAccessClient.HasAccess(car);
	}

	override void OnFinishProgressServer(ActionData action_data)
	{
		CarScript car = CarScript.Cast(action_data.m_Target.GetObject());
		PlayerBase player = action_data.m_Player;
		if (!car || !player)
			return;
		string sid = ExorGroupManager.SteamId(player);
		ExorCfgCarLock cfg = GetExorConfig().carlock;

		if (car.ExorCarHasLock())
		{
			// CAMBIAR: hay que TENER la clave (o admin).
			if (!(car.ExorCarIsMemberOfLockGroup(sid) && car.ExorCarIsUnlockedBy(sid)) && !cfg.ExorEsAdmin(sid))
			{
				ExorMuebleRules.SendRed(player, "Ingresá la clave actual antes de cambiarla.");
				return;
			}
		}
		else
		{
			// COLOCAR: keypad en la mano (NO se consume: si cancela el modal no pierde nada).
			ItemBase held = player.GetItemInHands();
			if (!held || !held.IsInherited(Exor_CarCodeLock))
			{
				ExorMuebleRules.SendRed(player, "Necesitás el candado (keypad) en la mano.");
				return;
			}
		}

		player.ExorOpenCarKeyModal(car, ExorLockKeyClient.MODE_SET);
	}
}
