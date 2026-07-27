// ============================================================================
// 3xorStorage - Accion: Empaquetar el parking (desplegado -> item caja)
// Requisitos: destornillador en mano + permiso (miembro/staff) + SIN autos
// virtualizados del clan (empaquetarlo con autos dentro los dejaria huerfanos).
// ============================================================================
class ExorActionPackParkingCB : ActionContinuousBaseCB
{
	override void CreateActionComponent()
	{
		m_ActionData.m_ActionComponent = new CAContinuousTime(ExorStorageConstants.PACK_SECONDS);
	}
}

class ExorActionPackParking : ActionContinuousBase
{
	void ExorActionPackParking()
	{
		m_CallbackClass = ExorActionPackParkingCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_FullBody = true;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
		m_Text = "Empaquetar parking";
	}

	override void CreateConditionComponents()
	{
		m_ConditionItem = new CCINone;
		m_ConditionTarget = new CCTNonRuined(UAMaxDistances.DEFAULT);
	}

	override bool HasProgress() { return true; }

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!item || !item.IsInherited(Screwdriver))
			return false;
		return Exor_Parking.Cast(target.GetObject()) != null;
	}

	override void OnFinishProgressServer(ActionData action_data)
	{
		Exor_Parking parking = Exor_Parking.Cast(action_data.m_Target.GetObject());
		PlayerBase player = action_data.m_Player;
		if (!parking || !player)
			return;

		// PERMISO: solo miembros del clan o staff (bypass).
		string denyReason;
		if (!ExorMuebleRules.CanPackAtPos(player, parking.GetPosition(), denyReason))
		{
			ExorMuebleRules.SendRed(player, denyReason);
			return;
		}

		// GUARD "sin autos dentro": si el clan tiene autos VIRTUALIZADOS, no dejar empaquetar
		// (se quedarian sin donde recuperarse). Hay que sacarlos todos primero.
		// el grupo a chequear es el DUEÑO del territorio donde esta el parking, no el del que
		// empaqueta (un staff con bypass tiene otro grupo -o ninguno- y el chequeo miraba la
		// lista equivocada). Sin territorio, se cae al grupo del jugador.
		string groupId;
		ExorTerritoryManager tmPack = ExorTerritoryManager.Get();
		if (tmPack)
			groupId = tmPack.GroupAtPos(parking.GetPosition());
		if (groupId == "")
			groupId = player.ExorGetGroupId();
		array<string> virtIds, virtNames;
		ExorVehicleGarage.ListForGroup(groupId, virtIds, virtNames);
		if (virtIds.Count() > 0)
		{
			ExorMuebleRules.SendRed(player, string.Format("No podés empaquetar el parking: tenés %1 auto(s) virtualizado(s). Sacalos primero.", virtIds.Count()));
			return;
		}

		vector pos = parking.GetPosition();
		vector ori = parking.GetOrientation();
		float health = parking.GetHealth01("", "");

		EntityAI packed = EntityAI.Cast(GetGame().CreateObjectEx("Exor_Parking_Packed", pos, ECE_PLACE_ON_SURFACE));
		if (!packed)
		{
			Print("[3xorStorage] ERROR: no se pudo crear Exor_Parking_Packed al empaquetar");
			return;
		}
		packed.SetOrientation(ori);
		packed.SetHealth01("", "", health);
		GetGame().ObjectDelete(parking);
		Print("[3xorStorage] Parking empaquetado en " + pos.ToString());
	}
}
