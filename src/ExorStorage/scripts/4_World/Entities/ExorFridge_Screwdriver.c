// ============================================================================
// 3xorStorage - El DESTORNILLADOR permite empaquetar muebles abribles
// ----------------------------------------------------------------------------
// Con un destornillador en la mano y mirando un Exor_OpenableStorage (nevera,
// muebles futuros) CERRADO y VACIO aparece la accion "Empaquetar mueble".
// (Patron estandar DayZ: la accion "usar herramienta sobre objeto" vive en la
// herramienta, asi es descubrible al tenerla en la mano.)
// ============================================================================

modded class Screwdriver
{
	override void SetActions()
	{
		super.SetActions();
		AddAction(ExorActionPackFridge);
		AddAction(ExorActionPackParking);
	}
}
