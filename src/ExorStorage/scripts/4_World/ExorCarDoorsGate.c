// ============================================================================
// 3xor_Vanilla_Optimization - GATE de PUERTAS del candado de autos
// ----------------------------------------------------------------------------
// Un auto con candado no le deja ABRIR/CERRAR las puertas a quien no tiene acceso
// (mismo criterio que subirse: ExorCarBlocksEntry, que es client-aware). Sin esto,
// el ajeno abria las puertas del auto ajeno. Se modean las 2 acciones de puerta:
// desde afuera (Outside, target.GetParent()=auto) y desde adentro (target=auto).
// ============================================================================
modded class ActionCarDoorsOutside
{
	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!super.ActionCondition(player, target, item))
			return false;
		if (!GetExorConfig().carlock.activado)
			return true;
		CarScript car = CarScript.Cast(target.GetParent());	// afuera: el auto es el PADRE de la puerta
		if (car && car.ExorCarBlocksEntry(player))
			return false;
		return true;
	}
}

modded class ActionCarDoors
{
	override bool ActionCondition(PlayerBase player, ActionTarget target, ItemBase item)
	{
		if (!super.ActionCondition(player, target, item))
			return false;
		if (!GetExorConfig().carlock.activado)
			return true;
		// adentro: el auto es el vehiculo en el que va el player (como en vanilla). El ajeno no
		// puede entrar (gate de entrada), asi que casi siempre es no-op; se deja por consistencia.
		if (!player || !player.GetCommand_Vehicle())
			return true;
		CarScript car = CarScript.Cast(player.GetCommand_Vehicle().GetTransport());
		if (car && car.ExorCarBlocksEntry(player))
			return false;
		return true;
	}
}
