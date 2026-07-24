// ============================================================================
// 3xorStorage - Accion: Ingresar la clave de un AUTO (miembro del clan)
// ----------------------------------------------------------------------------
// CONTINUA (mantener F), NO interact (en autos el tap F = subir). Aparece MIRANDO
// un auto CON candado, SOLO a un MIEMBRO del clan dueño que TODAVIA NO ingreso la
// clave. Al completar el hold, abre el modal en modo METER; si la clave coincide,
// el server lo marca desbloqueado y el gate lo deja subir + le muestra el baul.
// ============================================================================
class ExorActionEnterCarKeyCB : ActionContinuousBaseCB
{
	override void CreateActionComponent()
	{
		m_ActionData.m_ActionComponent = new CAContinuousTime(1.0);
	}
}

class ExorActionEnterCarKey : ActionContinuousBase
{
	void ExorActionEnterCarKey()
	{
		m_CallbackClass = ExorActionEnterCarKeyCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_FullBody = true;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
		m_Text = "Ingresar clave";
	}

	override void CreateConditionComponents()
	{
		m_ConditionItem = new CCINone;
		m_ConditionTarget = new CCTObject(4.0);
	}

	override bool HasProgress() { return true; }

	override string GetText() { return "Ingresar clave"; }

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!target || !player || !GetExorConfig().carlock.activado)
			return false;
		CarScript car = CarScript.Cast(target.GetObject());
		if (!car || !car.ExorCarHasLock())
			return false;
		string sid = ExorGroupManager.SteamId(player);
		// SERVER: solo miembro del clan que aun no desbloqueo. CLIENTE: soy MIEMBRO (me llego el
		// signal) y todavia no tengo acceso. El ajeno NO es miembro -> no ve esto (solo raidea).
		if (GetGame().IsServer())
			return car.ExorCarIsMemberOfLockGroup(sid) && !car.ExorCarIsUnlockedBy(sid);
		return ExorCarAccessClient.IsMember(car) && !ExorCarAccessClient.HasAccess(car);
	}

	override void OnFinishProgressServer(ActionData action_data)
	{
		CarScript car = CarScript.Cast(action_data.m_Target.GetObject());
		PlayerBase player = action_data.m_Player;
		if (!car || !player)
			return;
		// SERVER estricto: solo un miembro del clan dueño puede ingresar la clave. El ajeno raidea.
		string sid = ExorGroupManager.SteamId(player);
		if (!car.ExorCarIsMemberOfLockGroup(sid))
		{
			ExorMuebleRules.SendRed(player, "Este auto es de otro clan. Usá una herramienta para quitarle el candado.");
			return;
		}
		player.ExorOpenCarKeyModal(car, ExorLockKeyClient.MODE_ENTER);
	}
}
