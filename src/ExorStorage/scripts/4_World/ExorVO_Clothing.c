// ============================================================================
// 3xor_Vanilla_Optimization - Ropa/mochilas CON items adentro, SOLO en
// barriles 3xor (Fase 3, configurable con permitir_ropa_con_items)
// ============================================================================

class ExorVO_Helpers
{
	// El destino (o su raiz de jerarquia) es un barril 3xor?
	static bool ParentIsExorBarrel(EntityAI parent)
	{
		if (!parent)
			return false;
		if (parent.IsInherited(Exor_Barrel_Base))
			return true;
		EntityAI root = parent.GetHierarchyRoot();
		if (root && root.IsInherited(Exor_Barrel_Base))
			return true;
		return false;
	}
}

modded class Clothing
{
	override bool CanPutInCargo(EntityAI parent)
	{
		// Vanilla bloquea guardar ropa con items adentro; lo permitimos
		// unicamente cuando el destino es un barril 3xor
		if (ExorVO_Helpers.ParentIsExorBarrel(parent))
		{
			ExorStorageSettings settings = GetExorStorageSettings();
			if (settings.permitir_ropa_con_items)
				return true;
		}
		return super.CanPutInCargo(parent);
	}
}
