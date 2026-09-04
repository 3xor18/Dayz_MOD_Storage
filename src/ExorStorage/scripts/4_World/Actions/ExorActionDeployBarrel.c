// ============================================================================
// 3xorStorage - Accion: Desplegar barril (item caja en manos -> barril funcional)
// El barril aparece frente al jugador, apoyado en el suelo.
// ============================================================================

class ExorActionDeployBarrelCB : ActionContinuousBaseCB
{
	override void CreateActionComponent()
	{
		m_ActionData.m_ActionComponent = new CAContinuousTime(ExorStorageConstants.DEPLOY_SECONDS);
	}
}

class ExorActionDeployBarrel : ActionContinuousBase
{
	void ExorActionDeployBarrel()
	{
		m_CallbackClass = ExorActionDeployBarrelCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_FullBody = true;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
		m_Text = "Desplegar barril";
	}

	override void CreateConditionComponents()
	{
		m_ConditionItem = new CCINonRuined;
		m_ConditionTarget = new CCTNone;
	}

	override bool HasTarget()
	{
		return false;
	}

	override bool HasProgress()
	{
		return true;
	}

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		Exor_Barrel_Packed_Base packed = Exor_Barrel_Packed_Base.Cast(item);
		if (!packed)
			return false;

		return true;
	}

	override void OnFinishProgressServer(ActionData action_data)
	{
		Exor_Barrel_Packed_Base packed = Exor_Barrel_Packed_Base.Cast(action_data.m_MainItem);
		if (!packed)
			return;

		string deployedType = packed.ExorGetDeployedType();
		if (deployedType == "")
			return;

		PlayerBase player = action_data.m_Player;
		if (!player)
			return;

		// Posicion: 1.5 m frente al jugador, apoyado en la superficie
		vector dir = player.GetDirection();
		dir[1] = 0;
		dir.Normalize();
		vector pos = player.GetPosition() + dir * 1.5;

		// El barril solo se setea DENTRO del radio del mastil de tu propia base, y respetando
		// el tope de BARRILES por base (cupo propio, aparte del de muebles). Se valida en el
		// SERVER al terminar la accion y no en ActionCondition, que corre en el cliente y se
		// puede saltear con un cliente modificado. Ver ExorMuebleRules.CanPlaceBarril.
		string denyReason;
		if (!ExorMuebleRules.CanPlaceBarril(player, pos, denyReason))
		{
			ExorMuebleRules.SendRed(player, denyReason);
			return;		// la caja NO se consume: el jugador se queda con su barril
		}

		EntityAI barrel = EntityAI.Cast(GetGame().CreateObjectEx(deployedType, pos, ECE_PLACE_ON_SURFACE));
		if (!barrel)
		{
			Print("[3xorStorage] ERROR: no se pudo crear " + deployedType + " al desplegar");
			return;
		}

		float health = packed.GetHealth01("", "");
		barrel.SetHealth01("", "", health);

		packed.Delete();
		Print("[3xorStorage] Barril desplegado -> " + deployedType + " en " + pos.ToString());
		Exor_Barrel_Base eb = Exor_Barrel_Base.Cast(barrel);
		if (eb)
			eb.ExorDbg("DEPLOY (player desplego/seteo el barril desde la caja)");
	}
}
