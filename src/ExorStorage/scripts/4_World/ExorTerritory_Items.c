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
			// zonas de NO construccion definidas por el admin (nobuild.json)
			if (!ExorNoBuild.CanBuildAt(position, GetType()))
				return false;
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

	// Backstop SERVER de las zonas de NO construccion (nobuild.json): si un kit/
	// deployable se coloca DENTRO de una zona prohibida (y no esta whitelisteado),
	// se borra. Cubre fence/watchtower/tienda/barril/fogata y los kits de
	// BuildEverywhere / BaseBuildingPlus (todos son ItemBase y pasan por aca).
	override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
	{
		super.OnPlacementComplete(player, position, orientation);
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (ExorNoBuild.CanBuildAt(GetPosition(), GetType()))
			return;
		ExorNoBuild.Warn(player);
		GetGame().ObjectDelete(this);
	}

	// #3 (anti-raid): cuando este item entra al inventario de un jugador, si el
	// jugador esta parado dentro de territorio AJENO y no es del party, se loguea
	// "tomo X" para forense. Solo server. Fuera de territorio ajeno: no hace nada.
	override void OnInventoryEnter(Man player)
	{
		super.OnInventoryEnter(player);
		if (!GetGame() || !GetGame().IsServer())
			return;
		PlayerBase pb = PlayerBase.Cast(player);
		if (pb)
			ExorAntiRaid.OnPickupInEnemyTerritory(pb, this);
	}

	// FORENSE de tumbas: si este item SALIO de una tumba y fue a un JUGADOR, registrar
	// quien lo looteo en el JSON de esa tumba. Gate barato (bool + cast) para no lagear;
	// el I/O solo ocurre en un loot real de tumba. Se puede apagar con forense_registrar_
	// looteadores=false en bodycadaver.json.
	override void EEItemLocationChanged(notnull InventoryLocation oldLoc, notnull InventoryLocation newLoc)
	{
		super.EEItemLocationChanged(oldLoc, newLoc);
		if (!GetGame() || !GetGame().IsServer())
			return;
		// Bandera ya leida de la config (bool estatico). Este hook corre en CADA movimiento
		// de inventario de CADA item del mundo -incluidos los miles que genera restaurar un
		// contenedor-, asi que la salida temprana tiene que ser un solo acceso a memoria.
		if (!ExorHotFlags.ForenseLooteadores())
			return;
		EntityAI oldParent = oldLoc.GetParent();
		if (!oldParent)
			return;
		Exor_BodyBag tumba = Exor_BodyBag.Cast(oldParent);
		if (!tumba)
			return;	// no salio de una tumba -> nada
		PlayerBase looter = PlayerBase.Cast(GetHierarchyRootPlayer());
		if (!looter || !looter.GetIdentity())
			return;	// no fue a un jugador (ej. al piso) -> no es loot
		ExorTumbaForense.Looteo(tumba.ExorGetID(), GetType(), looter.GetIdentity().GetName(), looter.GetIdentity().GetPlainId());
	}
}

// ============================================================================
// Refuerzo SERVER-SIDE del campo de plantacion (GardenPlot deriva de ItemBase y
// se cava por el sistema de hologram, que ya pasa por CanBePlaced en el cliente).
// Aca, al COMPLETARSE la colocacion en el server, si cae en territorio ajeno (la
// MISMA regla que CanPlaceServer: respeta territorio propio / sin mastil / whitelist
// / permitir_construir_cerca) -> se borra. Backstop contra bypass del cliente.
// Cubre tambien GardenPlotGreenhouse/Polytunnel (heredan de GardenPlot).
// ============================================================================
modded class GardenPlot
{
	override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
	{
		super.OnPlacementComplete(player, position, orientation);
		if (!GetGame() || !GetGame().IsServer())
			return;

		PlayerBase pb = PlayerBase.Cast(player);
		if (!ExorTerritoryManager.Get().CanPlaceServer(pb, GetPosition(), GetType()))
		{
			if (pb)
				ExorAviso.Enviar(pb, "No podés hacer campos de plantación en territorio ajeno.");
			GetGame().ObjectDelete(this);
		}
	}
}
