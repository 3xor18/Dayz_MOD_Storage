// ============================================================================
// 3xor_Vanilla_Optimization - GATE del candado de autos
// ----------------------------------------------------------------------------
// Bloquea SUBIRSE a un auto con candado si el player no tiene acceso (no es admin,
// no es el dueño/miembro que ya metio la clave). Se hace modeando la CONDICION de
// la accion vanilla de subirse (ActionGetInTransport): si devuelve false, la accion
// de "subir" simplemente NO aparece -> no se puede entrar. El resto de la logica
// vanilla (asiento libre, puerta, alcance) la sigue evaluando super.
//
// Por que aca y no en CrewCanGetThrough: esa funcion NO recibe el player, asi que no
// podria distinguir al dueño del raider. ActionCondition SI tiene el player.
//
// El dueño mete la clave con "Ingresar clave" (ExorActionEnterCarKey) -> queda
// desbloqueado (runtime) -> a partir de ahi el gate lo deja subir normal. El que no
// es del clan usa "Quitar Codelock" (ExorActionRaidCarLock) con herramienta.
// ============================================================================
modded class ActionGetInTransport
{
	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!super.ActionCondition(player, target, item))
			return false;
		// solo aplica si el candado de autos esta activo
		if (!GetExorConfig().carlock.activado)
			return true;
		CarScript car = CarScript.Cast(target.GetObject());
		if (car && car.ExorCarBlocksEntry(player))
			return false;	// candado puesto y sin acceso -> no aparece "subir"
		return true;
	}
}
