// ============================================================================
// 3xorStorage - Accion de ADMIN: eliminar un barril/mueble DEFINITIVAMENTE
// ============================================================================
// PROBLEMA: desde que los barriles entraron al self-heal, un barril borrado con las
// admin tools VUELVE en el proximo reinicio. Y esta bien que sea asi: el mod no puede
// distinguir "lo borro un admin" de "lo despawneo el motor" -para el son la misma
// desaparicion- y justamente esa regla es la que devuelve los barriles perdidos.
//
// La unica desaparicion que hoy cuenta como "deliberada" es EMPACAR, pero empacar exige
// que el mueble este VACIO y cerrado. Un admin que quiere sacar un barril lleno no tiene
// como decirle al mod "este no lo recrees".
//
// Esto le da esa via: da de baja el registro Y borra la entidad, asi NO vuelve.
//
// SOLO STAFF: aparece unicamente para los SteamID de storage.json -> bypass_lootear_steamids.
// Para cualquier otro jugador la accion no existe (ni la ve).
//
// EL CONTENIDO NO SE BORRA: el JSON de contenido queda en el profile (huerfano). Es a
// proposito -si el admin se equivoco de barril, el loot sigue ahi y se puede devolver
// recreando su registro-. Se loguea el id justamente para poder encontrarlo.
// ============================================================================
class ExorActionAdminRemoveCB : ActionContinuousBaseCB
{
	override void CreateActionComponent()
	{
		m_ActionData.m_ActionComponent = new CAContinuousTime(ExorStorageConstants.PACK_SECONDS);
	}
}

class ExorActionAdminRemove : ActionContinuousBase
{
	void ExorActionAdminRemove()
	{
		m_CallbackClass = ExorActionAdminRemoveCB;
		m_CommandUID = DayZPlayerConstants.CMD_ACTIONFB_INTERACT;
		m_FullBody = true;
		m_StanceMask = DayZPlayerConstants.STANCEMASK_ERECT | DayZPlayerConstants.STANCEMASK_CROUCH;
		m_Text = "Eliminar definitivamente (admin)";
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

	// solo staff. OJO: NO se puede chequear por SteamID aca. ActionCondition se evalua tambien
	// en el CLIENTE para decidir si mostrar la accion, y del lado cliente GetIdentity() es null
	// -> SteamId() devuelve "" -> el chequeo fallaba SIEMPRE y la accion no aparecia nunca en el
	// menu (asi se descubrio). Se usa el bool que el SERVER sincroniza al conectar.
	static bool ExorEsStaff(PlayerBase player)
	{
		if (!player)
			return false;
		return player.ExorIsStaff();
	}

	// Chequeo DURO por SteamID, solo para el server al ejecutar (no depende de lo sincronizado)
	static bool ExorEsStaffServer(PlayerBase player)
	{
		if (!player)
			return false;
		ExorCfgStorage s = GetExorConfig().storage;
		if (!s || !s.bypass_lootear_steamids)
			return false;
		return s.bypass_lootear_steamids.Find(ExorGroupManager.SteamId(player)) != -1;
	}

	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!ExorEsStaff(player))
			return false;

		Object o = target.GetObject();
		if (Exor_Barrel_Base.Cast(o))
			return true;
		if (Exor_OpenableStorage.Cast(o))
			return true;
		return false;
	}

	override void OnFinishProgressServer(ActionData action_data)
	{
		PlayerBase player = action_data.m_Player;
		// re-validar CONTRA EL CONFIG en el server: el bool sincronizado sirve para MOSTRAR la
		// accion, pero quien la ejecuta se chequea contra la lista real (el cliente no decide).
		if (!ExorEsStaffServer(player))
			return;

		Object o = action_data.m_Target.GetObject();
		if (!o)
			return;

		string id = "";
		string tipo = o.GetType();

		Exor_Barrel_Base barrel = Exor_Barrel_Base.Cast(o);
		if (barrel)
			id = barrel.ExorGetID();

		Exor_OpenableStorage fur = Exor_OpenableStorage.Cast(o);
		if (fur)
			id = fur.ExorGetID();

		if (id == "")
			return;

		// baja del registro ANTES de borrar: si el server se cayera justo aca, lo peor que
		// pasa es que quede la entidad sin registro (no vuelve) en vez de al reves.
		ExorMuebleRegistry.Unregister(id);
		GetGame().ObjectDelete(o);

		Print(string.Format("%1 ADMIN: %2 (%3) eliminado definitivamente por %4 -> NO se recrea. Su contenido queda en storage\\%3.json por si hay que devolverlo.", ExorStorageConstants.LOG, tipo, id, ExorGroupManager.SteamId(player)));
	}
}
