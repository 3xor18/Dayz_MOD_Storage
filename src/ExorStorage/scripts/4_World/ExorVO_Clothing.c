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

	// El destino (o su raiz) es un vehiculo (auto vanilla)?
	static bool ParentIsVehicle(EntityAI parent)
	{
		if (!parent)
			return false;
		if (parent.IsInherited(CarScript))
			return true;
		EntityAI root = parent.GetHierarchyRoot();
		if (root && root.IsInherited(CarScript))
			return true;
		return false;
	}
}

modded class Clothing
{
	override bool CanPutInCargo(EntityAI parent)
	{
		// Vanilla bloquea guardar ropa con items adentro; lo permitimos cuando
		// el destino es un barril 3xor, o el baul de un vehiculo (toggle aparte).
		if (ExorVO_Helpers.ParentIsExorBarrel(parent))
		{
			if (GetExorConfig().storage.permitir_ropa_con_items)
				return true;
		}
		if (ExorVO_Helpers.ParentIsVehicle(parent))
		{
			if (GetExorConfig().vehiculos.inv_items_anidados)
				return true;
		}
		return super.CanPutInCargo(parent);
	}
}
