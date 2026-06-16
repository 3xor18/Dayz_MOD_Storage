// ============================================================================
// 3xor_Vanilla_Optimization - Reparar a pristine + combinar kits gastados
//  1) CanBeRepairedToPristine -> true: al reparar con cualquier kit, el item
//     llega hasta PRISTINE (verde) en vez de toparse en "gastado" (quita el cap).
//  2) CanBeCombined: permite COMBINAR 2 kits del mismo tipo (lista en
//     reparacion.json) sumando su uso (quantity) hasta el maximo -> 2 al 50%
//     se unen en 1 al 100%. Solo aplica a kits cuyo "uso" es QUANTITY.
// Toggles/lista vienen de reparacion.json (sincronizado al cliente para que la
// accion de combinar aparezca en la UI; el repair lo valida el server).
// ============================================================================
class ExorRepairStack
{
	// true si 'a' puede recibir a 'other' como union de dos kits gastados iguales.
	static bool CanStack(ItemBase a, EntityAI other)
	{
		if (!a || !other)
			return false;
		ItemBase b = ItemBase.Cast(other);
		if (!b || b == a)
			return false;
		if (a.GetType() != b.GetType())
			return false;
		ExorCfgReparacion cfg = GetExorConfig().reparacion;
		if (!cfg || !cfg.EsStackeable(a.GetType()))
			return false;
		// 'a' tiene que tener lugar para recibir mas uso (si esta lleno, no combinar)
		if (a.GetQuantity() >= a.GetQuantityMax())
			return false;
		return true;
	}
}

modded class ItemBase
{
	// Feature 1: la reparacion lleva el item hasta pristine (verde).
	override bool CanBeRepairedToPristine()
	{
		ExorCfgReparacion cfg = GetExorConfig().reparacion;
		if (cfg && cfg.reparar_a_pristine)
			return true;
		return super.CanBeRepairedToPristine();
	}

	// Feature 2: combinar 2 kits gastados del mismo tipo (suma su quantity).
	override bool CanBeCombined(EntityAI other_item, bool reservation_check = true, bool stack_max_limit = false)
	{
		if (ExorRepairStack.CanStack(this, other_item))
			return true;
		return super.CanBeCombined(other_item, reservation_check, stack_max_limit);
	}
}
