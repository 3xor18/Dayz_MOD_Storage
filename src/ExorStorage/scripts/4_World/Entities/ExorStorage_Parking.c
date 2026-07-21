// ============================================================================
// 3xor_Vanilla_Optimization - PARKING (maquina para administrar autos del clan)
// ============================================================================
// Objeto ESTATICO que se setea en la base (dentro del territorio, como los muebles).
// Al interactuar abre el menu de autos del clan (virtualizar / desvirtualizar).
// Reusa el helper de colocacion de los muebles (ExorDeployFurniture: chequea territorio
// + limite, coloca estatico apoyado en la superficie, limpia el holograma).
//
// baseOffset -0.545: binarize centra el modelo (bbox Y 0..1.09 -> centro ~0.545) -> se
// compensa para que la base apoye en el piso. CALIBRAR IN-GAME: si se hunde, subir (menos
// negativo); si flota, bajar (mas negativo).
// ============================================================================

// -------- EMPACADO: el item que se lleva en la mano y se setea --------
class Exor_Parking_Packed : ItemBase
{
	override bool CanPutInCargo(EntityAI parent) { return true; }
	override bool CanPutIntoHands(EntityAI parent) { return true; }

	override bool CanBePlaced(Man player, vector position) { return true; }
	override string CanBePlacedFailMessage(Man player, vector position) { return ""; }

	override void SetActions()
	{
		super.SetActions();
		AddAction(ActionTogglePlaceObject);
		AddAction(ActionDeployObject);
	}

	override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
	{
		super.OnPlacementComplete(player, position, orientation);
		if (!GetGame().IsServer())
			return;

		// SOLO 1 PARKING POR BASE: si ya hay un parking dentro del radio del territorio, no dejar
		// colocar otro (el auto-virtualizado y el menu asumen 1 por base). El packed queda en la mano.
		vector dummyPos;
		if (ExorVehicleGarage.NearParking(position, ExorTerritoryRules.Radius(), dummyPos))
		{
			PlayerBase pb = PlayerBase.Cast(player);
			if (pb)
				ExorMuebleRules.SendRed(pb, "Solo 1 parking por base");
			return;
		}

		// Colocacion compartida (estatico + apoyo en superficie + chequeo territorio/limite).
		// baseOffset 0: con autocenter=0 en el modelo, binarize NO re-centra -> el origen queda
		// en la BASE, asi que holograma y desplegado apoyan igual sin compensar.
		EntityAI parking = Exor_OpenableStorage.ExorDeployFurniture(player, "Exor_Parking", position, orientation, 0.0, GetHealth01("", ""));
		if (!parking)
			return;	// bloqueado (fuera de territorio / limite): el packed queda en la mano
		Print("[3xorStorage] Parking seteado en " + parking.GetPosition().ToString());
		Delete();	// consumir el item empacado
	}
}

// -------- DESPLEGADO: la maquina estatica en la base --------
class Exor_Parking : ItemBase
{
	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			SetAllowDamage(false);	// indestructible (como los muebles)
			// bloquear el inventario propio: no tiene cargo, es solo una maquina
			if (GetInventory())
				GetInventory().LockInventory(HIDE_INV_FROM_SCRIPT);
			ExorVehicleGarage.RegisterParking(this);
		}
	}

	override void EEDelete(EntityAI parent)
	{
		if (GetGame().IsServer())
			ExorVehicleGarage.UnregisterParking(this);
		super.EEDelete(parent);
	}

	// ESTATICO: no se puede levantar ni meter en otro contenedor (se re-empaca con destornillador).
	override bool CanPutInCargo(EntityAI parent) { return false; }
	override bool CanPutIntoHands(EntityAI parent) { return false; }
	override bool IsTakeable() { return false; }

	// se apunta facil (misma idea que los muebles)
	override bool IsHeavyBehaviour() { return true; }

	// Accion de interaccion: abrir el menu de autos del clan.
	override void SetActions()
	{
		super.SetActions();
		AddAction(ExorActionOpenParking);
	}
}
