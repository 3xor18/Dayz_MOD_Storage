// ============================================================================
// 3xorStorage - LOCKER ROJO (closet scifi de 2 puertas)
// ----------------------------------------------------------------------------
// Clon funcional del Locker de Equipo (Exor_Locker) con estetica distinta.
// Subclase de Exor_OpenableStorage. Guarda ropa/armas/gear (no comida/bebida).
// ETAPA 2a: modelo estatico (sin animacion de puertas todavia). La Etapa 2b
// agrega las 2 puertas animadas (door_l/door_r) via model.cfg + AnimationSources.
// ============================================================================

class Exor_LockerRojo : Exor_OpenableStorage
{
	override string ExorGetPackedType() { return "Exor_LockerRojo_Packed"; }

	// Virtualizar tambien la ropa/gear de los slots al cerrar/alejarse (perf a 55 players).
	override bool ExorVirtualizeAttachments() { return true; }

	// Guarda ropa/armas/gear; NO comida (Edible_Base) ni bebida (Bottle_Base) -> eso va a la nevera.
	override bool ExorCanStore(EntityAI item)
	{
		if (item.IsInherited(Edible_Base) || item.IsInherited(Bottle_Base))
			return false;
		return true;
	}

	// ETAPA 2b: DOS puertas animadas. sources "L_Door" y "R_Door" (ver model.cfg +
	// config AnimationSources). phase 0=cerrada, 1=abierta.
	override void ExorApplyDoorPhase(float phase)
	{
		SetAnimationPhase("L_Door", phase);
		SetAnimationPhase("R_Door", phase);
	}
}

// ============================================================================
//  Locker Rojo EMPACADO (item transportable, se coloca con HOLOGRAMA)
// ============================================================================
class Exor_LockerRojo_Packed : ItemBase
{
	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
			SetAllowDamage(false);
	}

	override bool IsDeployable() { return true; }
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

		// baseOffset -1.0: el MLOD tiene Y 0..2.0; binarize centra en Y (-> -1.0..+1.0) -> compensar
		// para apoyar la base en la superficie. Ajustar si queda enterrado/flotando en el test.
		EntityAI l = Exor_OpenableStorage.ExorDeployFurniture(player, "Exor_LockerRojo", position, orientation, -1.0, GetHealth01("", ""));
		if (!l)
			return;
		Print("[3xorStorage] Locker Rojo seteado en " + l.GetPosition().ToString());
		Delete();
	}
}
