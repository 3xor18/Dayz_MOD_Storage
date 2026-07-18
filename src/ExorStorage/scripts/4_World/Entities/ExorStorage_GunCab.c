// ============================================================================
// 3xorStorage - MUEBLE DE ARMAS (gun cabinet)
// ----------------------------------------------------------------------------
// Subclase de Exor_OpenableStorage (mueble abrible + virtualizable). Tiene UNA
// puerta principal (con ventana de vidrio) + DOS cajones abajo, los tres animados.
// Guarda armas/equipo; ademas mostrara las armas en el estante (proxies) - eso
// se agrega en la fase de proxies. Toda la maquinaria (abrir/cerrar, virtualiza,
// colision, empaque, persistencia, colocacion) viene de Exor_OpenableStorage.
// ============================================================================

class Exor_MuebleArmas : Exor_OpenableStorage
{
	override string ExorGetPackedType() { return "Exor_MuebleArmas_Packed"; }

	// Virtualizar TAMBIEN las armas de los 8 slots (attachments) al cerrar/alejarse:
	// se guardan en JSON y se sacan del mundo (0 carga con 55 players).
	override bool ExorVirtualizeAttachments() { return true; }

	// TRES partes moviles: puerta principal + 2 cajones. El model.cfg liga cada
	// source a su seleccion (MainDoor=rotacion, Drawer01/02=traslacion).
	override void ExorApplyDoorPhase(float phase)
	{
		SetAnimationPhase("MainDoor", phase);
		SetAnimationPhase("Drawer01", phase);
		SetAnimationPhase("Drawer02", phase);
	}
}

// ============================================================================
//  Mueble de Armas EMPACADO (item transportable, se coloca con HOLOGRAMA)
// ============================================================================
class Exor_MuebleArmas_Packed : ItemBase
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

		// (La rotacion para mirar al jugador va HORNEADA en el modelo -> holograma y
		// objeto seteado coinciden. Sin offset por codigo.)

		// Colocacion compartida (estatico + base sobre superficie real, ignora holograma).
		// baseOffset -0.8255: binarize centra el modelo (altura 1.651 -> base a -0.8255).
		// Ajustar si queda enterrado/flotando en el test.
		EntityAI mueble = Exor_OpenableStorage.ExorDeployFurniture(player, "Exor_MuebleArmas", position, orientation, -0.8255, GetHealth01("", ""));
		if (!mueble)
			return;
		Print("[3xorStorage] Mueble de Armas seteado en " + mueble.GetPosition().ToString());
		Delete();
	}
}
