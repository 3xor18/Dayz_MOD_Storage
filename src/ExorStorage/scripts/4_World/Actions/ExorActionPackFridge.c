// ============================================================================
// 3xorStorage - Accion: Empaquetar refrigerador (nevera desplegada -> item caja)
// Solo con la nevera CERRADA, sana y VACIA (sin comida ni bateria). Espejo de
// ExorActionPackBarrel pero para Exor_Fridge (Container_Base, ya no barril).
// ============================================================================

class ExorActionPackFridgeCB : ActionContinuousBaseCB
{
	override void CreateActionComponent()
	{
		m_ActionData.m_ActionComponent = new CAContinuousTime(ExorStorageConstants.PACK_SECONDS);
	}
}

class ExorActionPackFridge : ActionContinuousBase
{
	void ExorActionPackFridge()
	{
		m_CallbackClass = ExorActionPackFridgeCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_FullBody = true;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
		m_Text = "Empaquetar refrigerador";
	}

	override void CreateConditionComponents()
	{
		m_ConditionItem = new CCINone;
		m_ConditionTarget = new CCTNonRuined(UAMaxDistances.DEFAULT);
	}

	override bool HasProgress()
	{
		return true;
	}

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		Exor_Fridge fridge = Exor_Fridge.Cast(target.GetObject());
		if (!fridge)
			return false;

		return fridge.ExorCanBePacked();
	}

	override void OnFinishProgressServer(ActionData action_data)
	{
		Exor_Fridge fridge = Exor_Fridge.Cast(action_data.m_Target.GetObject());
		if (!fridge)
			return;

		// Re-validar en server al terminar (pudo cambiar durante la accion)
		if (!fridge.ExorCanBePacked())
			return;

		string packedType = fridge.ExorGetPackedType();
		if (packedType == "")
			return;

		vector pos = fridge.GetPosition();
		vector ori = fridge.GetOrientation();
		float health = fridge.GetHealth01("", "");

		EntityAI packed = EntityAI.Cast(GetGame().CreateObjectEx(packedType, pos, ECE_PLACE_ON_SURFACE));
		if (!packed)
		{
			Print("[3xorStorage] ERROR: no se pudo crear " + packedType + " al empaquetar la nevera");
			return;
		}

		packed.SetOrientation(ori);
		packed.SetHealth01("", "", health);

		GetGame().ObjectDelete(fridge);
		Print("[3xorStorage] Refrigerador empaquetado -> " + packedType + " en " + pos.ToString());
	}
}
