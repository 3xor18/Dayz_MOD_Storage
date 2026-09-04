// ============================================================================
// 3xorStorage - Accion: Empaquetar barril (barril desplegado -> item caja)
// Solo disponible con el barril cerrado, sano y vacio.
// ============================================================================

class ExorActionPackBarrelCB : ActionContinuousBaseCB
{
	override void CreateActionComponent()
	{
		m_ActionData.m_ActionComponent = new CAContinuousTime(ExorStorageConstants.PACK_SECONDS);
	}
}

class ExorActionPackBarrel : ActionContinuousBase
{
	void ExorActionPackBarrel()
	{
		m_CallbackClass = ExorActionPackBarrelCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_FullBody = true;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
		m_Text = "Empaquetar barril";
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
		Exor_Barrel_Base barrel = Exor_Barrel_Base.Cast(target.GetObject());
		if (!barrel)
			return false;

		return barrel.ExorCanBePacked();
	}

	override void OnFinishProgressServer(ActionData action_data)
	{
		Exor_Barrel_Base barrel = Exor_Barrel_Base.Cast(action_data.m_Target.GetObject());
		if (!barrel)
			return;

		// Re-validar en server al terminar (puede haber cambiado durante la accion)
		if (!barrel.ExorCanBePacked())
			return;

		// GATE DE MIEMBRO: des-setear un barril de una base ajena es sacarle el contenedor a
		// otro clan. Misma regla que abrirlo (ver ExorMuebleRules.CanLootAtPos): solo miembros,
		// con el horario de raid como excepcion. La validacion va en el SERVER al terminar.
		PlayerBase quien = PlayerBase.Cast(action_data.m_Player);
		string denyReason;
		if (quien && !ExorMuebleRules.CanLootAtPos(quien, barrel.GetPosition(), denyReason))
		{
			if (denyReason == "")
				denyReason = "Solo los miembros del clan pueden guardar este barril.";
			ExorMuebleRules.SendRed(quien, denyReason);
			return;
		}

		string packedType = barrel.ExorGetPackedType();
		if (packedType == "")
			return;

		barrel.ExorDbg("PACK (player levantando/empaquetando el barril; debe estar vacio)");
		vector pos = barrel.GetPosition();
		vector ori = barrel.GetOrientation();
		float health = barrel.GetHealth01("", "");

		EntityAI packed = EntityAI.Cast(GetGame().CreateObjectEx(packedType, pos, ECE_PLACE_ON_SURFACE));
		if (!packed)
		{
			Print("[3xorStorage] ERROR: no se pudo crear " + packedType + " al empaquetar");
			return;
		}

		packed.SetOrientation(ori);
		packed.SetHealth01("", "", health);

		// El player lo saco A PROPOSITO -> el self-heal NO lo recrea (sin esto el barril
		// volveria a aparecer solo). Cualquier OTRA desaparicion (despawn del motor, cuarentena
		// de persistencia) deja el registro -> se recrea con su contenido. Ver ExorMuebleRegistry.
		// El registro NO se borra: se marca como empaquetado, asi queda comprobante de quien se
		// lo llevo por si la persistencia despues pierde el item empaquetado.
		ExorMuebleRegistry.RegisterPacked(barrel.ExorGetID(), packedType, pos, action_data.m_Player);

		GetGame().ObjectDelete(barrel);
		Print("[3xorStorage] Barril empaquetado -> " + packedType + " en " + pos.ToString());
	}
}
