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
	// "ESTE classname es un kit combinable?" MEMOIZADO.
	// El motor llama CanBeCombined por cada candidato mientras el jugador arrastra algo en
	// el inventario -o sea decenas de veces por segundo, y para items que casi nunca son
	// kits-. Antes cada llamada recorria la lista de la config comparando strings. El
	// classname de un item no cambia nunca, asi que la respuesta se calcula una vez por tipo
	// en toda la vida del proceso y despues es un lookup de hash.
	static ref map<string, bool> s_EsKit;

	static bool EsKitCombinable(string type)
	{
		if (!s_EsKit)
			s_EsKit = new map<string, bool>;
		bool r;
		if (s_EsKit.Find(type, r))
			return r;
		ExorCfgReparacion cfg = GetExorConfig().reparacion;
		r = false;
		if (cfg)
			r = cfg.EsStackeable(type);
		s_EsKit.Set(type, r);
		return r;
	}

	// true si 'a' puede recibir a 'other' como union de dos kits gastados iguales.
	// ORDEN DE LOS CHEQUEOS: primero lo que descarta al 99,9% de los items (es un kit?) y
	// recien despues el resto. GetType() devuelve un string NUEVO en cada llamada, asi que
	// pedirlo dos veces en el caso comun -que es "no, no es un kit"- era allocar de gusto en
	// pleno arrastre de inventario.
	static bool CanStack(ItemBase a, EntityAI other)
	{
		if (!a || !other)
			return false;
		ItemBase b = ItemBase.Cast(other);
		if (!b || b == a)
			return false;
		string ta = a.GetType();
		if (!EsKitCombinable(ta))
			return false;
		if (ta != b.GetType())
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
