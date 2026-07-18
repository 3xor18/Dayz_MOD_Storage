// ============================================================================
// 3xorStorage - Accion: Empaquetar mueble abrible (desplegado -> item caja)
// Requisitos: mueble CERRADO, sano y VACIO + un DESTORNILLADOR en la mano.
// Generico para cualquier Exor_OpenableStorage (nevera y muebles futuros).
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
		m_Text = "Empaquetar mueble";
	}

	override void CreateConditionComponents()
	{
		// La accion vive en el destornillador (ver ExorFridge_Screwdriver.c) -> el item
		// en mano YA es el destornillador. Sin condicion extra de item.
		m_ConditionItem = new CCINone;
		m_ConditionTarget = new CCTNonRuined(UAMaxDistances.DEFAULT);
	}

	override bool HasProgress()
	{
		return true;
	}

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		// destornillador en la mano
		if (!item || !item.IsInherited(Screwdriver))
			return false;

		Exor_OpenableStorage fur = Exor_OpenableStorage.Cast(target.GetObject());
		if (!fur)
			return false;

		return fur.ExorCanBePacked();
	}

	override void OnFinishProgressServer(ActionData action_data)
	{
		Exor_OpenableStorage fur = Exor_OpenableStorage.Cast(action_data.m_Target.GetObject());
		if (!fur)
			return;

		// Re-validar en server al terminar (pudo cambiar durante la accion)
		if (!fur.ExorCanBePacked())
			return;

		string packedType = fur.ExorGetPackedType();
		if (packedType == "")
			return;

		vector pos = fur.GetPosition();
		vector ori = fur.GetOrientation();
		float health = fur.GetHealth01("", "");

		// soltar los attachments (ej bateria) al piso para NO perderlos al borrar el mueble
		GameInventory finv = fur.GetInventory();
		if (finv)
		{
			int a;
			for (a = finv.AttachmentCount() - 1; a >= 0; a--)
			{
				EntityAI att = finv.GetAttachmentFromIndex(a);
				if (att)
					finv.DropEntity(InventoryMode.SERVER, fur, att);
			}
		}

		EntityAI packed = EntityAI.Cast(GetGame().CreateObjectEx(packedType, pos, ECE_PLACE_ON_SURFACE));
		if (!packed)
		{
			Print("[3xorStorage] ERROR: no se pudo crear " + packedType + " al empaquetar el mueble");
			return;
		}

		packed.SetOrientation(ori);
		packed.SetHealth01("", "", health);

		GetGame().ObjectDelete(fur);
		Print("[3xorStorage] Mueble empaquetado -> " + packedType + " en " + pos.ToString());
	}
}
