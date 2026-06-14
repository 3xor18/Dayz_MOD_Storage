// ============================================================================
// 3xor_Vanilla_Optimization - Territorio: items y hook de colocacion (Fase C)
// - Exor_MastKit: el kit en manos, con la accion de desplegar el mastil.
// - Rope: se le agrega la accion de armar el kit (soga + 3 palos).
// - ItemBase.CanBePlaced: bloquea colocar/construir en territorio AJENO
//   (con whitelist para claymore/explosivos/minas). Cubre deployables y kits.
//   Nota: las PARTES de base (paredes sobre un frame) se construyen sobre tu
//   propia base, que ya esta en tu territorio; el gate principal es el kit.
// ============================================================================

modded class ItemBase
{
	override bool CanBePlaced(Man player, vector position)
	{
		if (!super.CanBePlaced(player, position))
			return false;

		PlayerBase pb = PlayerBase.Cast(player);
		if (GetGame().IsServer())
		{
			if (!ExorTerritoryManager.Get().CanPlaceServer(pb, position, GetType()))
				return false;
		}
		else
		{
			if (!ExorTerritoryClient.CanPlaceClient(position, GetType()))
				return false;
		}
		return true;
	}
}
