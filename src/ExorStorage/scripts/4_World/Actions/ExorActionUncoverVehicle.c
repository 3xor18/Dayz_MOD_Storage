// ============================================================================
// 3xor_Vanilla_Optimization - Accion: Quitar la cobertura del vehiculo
// El target puede ser la red camo o el auto estatico visual.
// ============================================================================

class ExorActionUncoverVehicleCB : ActionContinuousBaseCB
{
	override void CreateActionComponent()
	{
		m_ActionData.m_ActionComponent = new CAContinuousTime(ExorStorageConstants.UNCOVER_SECONDS);
	}
}

class ExorActionUncoverVehicle : ActionContinuousBase
{
	void ExorActionUncoverVehicle()
	{
		m_CallbackClass = ExorActionUncoverVehicleCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_FullBody = true;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
		m_Text = "Quitar la cobertura";
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

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!target)
			return false;
		Exor_VehicleCover cover = ExorVO_Manager.GetCoverForObject(target.GetObject());
		if (!cover)
			return false;
		return true;
	}

	override void OnFinishProgressServer(ActionData action_data)
	{
		if (!action_data || !action_data.m_Target)
			return;
		Exor_VehicleCover cover = ExorVO_Manager.GetCoverForObject(action_data.m_Target.GetObject());
		if (!cover)
			return;
		ExorVO_VehicleCoverSystem.UncoverVehicle(cover);
	}
}
