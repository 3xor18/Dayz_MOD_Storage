// ============================================================================
// 3xorStorage - LOCKER DE EQUIPO (2 puertas)
// ----------------------------------------------------------------------------
// Subclase de Exor_OpenableStorage (mueble abrible + virtualizable). Guarda
// ROPA / ARMAS / GEAR (NO comida ni bebida -> eso va a la nevera). Tiene DOS
// puertas (izquierda + derecha) que abren opuestas, ambas animadas.
// Toda la maquinaria (abrir/cerrar, virtualizacion, colision, empaque,
// persistencia, colocacion) viene de Exor_OpenableStorage.
// ============================================================================

class Exor_Locker : Exor_OpenableStorage
{
	override string ExorGetPackedType() { return "Exor_Locker_Packed"; }

	// Guarda ropa/armas/gear; NO comida (Edible_Base) ni bebida/agua (Bottle_Base).
	override bool ExorCanStore(EntityAI item)
	{
		if (item.IsInherited(Edible_Base) || item.IsInherited(Bottle_Base))
			return false;
		return true;
	}

	// DOS puertas: sources "L_Door" y "R_Door" (ver model.cfg + config AnimationSources).
	// Abren juntas al abrir el locker.
	override void ExorApplyDoorPhase(float phase)
	{
		SetAnimationPhase("L_Door", phase);
		SetAnimationPhase("R_Door", phase);
	}
}

// ============================================================================
//  Locker EMPACADO (item transportable, se coloca con HOLOGRAMA)
// ============================================================================
class Exor_Locker_Packed : ItemBase
{
	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
			SetAllowDamage(false);
	}

	override bool IsDeployable()
	{
		return true;
	}

	override bool CanBePlaced(Man player, vector position)
	{
		return true;
	}

	override string CanBePlacedFailMessage(Man player, vector position)
	{
		return "";
	}

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

		// Colocacion compartida (estatico + base sobre superficie real, ignora holograma).
		// baseOffset -1.0236: binarize centra el modelo (bbox Y -1.02..+1.02) -> compensa para
		// apoyar la base en la superficie. Colocacion CONFIRMADA por el user (no tocar).
		EntityAI locker = Exor_OpenableStorage.ExorDeployFurniture(player, "Exor_Locker", position, orientation, -1.0236, GetHealth01("", ""));
		if (!locker)
			return;
		Print("[3xorStorage] Locker seteado en " + locker.GetPosition().ToString());
		Delete();
	}
}
