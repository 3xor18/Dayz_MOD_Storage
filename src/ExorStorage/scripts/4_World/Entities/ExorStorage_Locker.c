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

	// tiene candado: aparece la accion "Poner/Cambiar clave" y pide clave al abrir (a miembros).
	override bool ExorHasCodeLock() { return true; }

	// Virtualizar TAMBIEN la ropa/gear de los slots de equipo (attachments) al cerrar/
	// alejarse: se guardan en JSON y se sacan del mundo (0 carga con 55 players).
	override bool ExorVirtualizeAttachments() { return true; }

	// Guarda ropa/armas/gear/MEDICO; NO comida ni bebida/agua (esas van a la nevera).
	override bool ExorCanStore(EntityAI item)
	{
		return ExorLockerCanStore(item);
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
