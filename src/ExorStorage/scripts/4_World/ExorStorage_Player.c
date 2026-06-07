// ============================================================================
// 3xor_Vanilla_Optimization - Acciones del jugador sobre objetos del mundo
// Las acciones con TARGET (empaquetar barril, quitar cobertura) se registran
// en PlayerBase; las de item en manos (desplegar la caja) van en el SetActions
// del item.
// ============================================================================
modded class PlayerBase
{
	override void SetActions(out TInputActionMap InputActionMap)
	{
		super.SetActions(InputActionMap);

		AddAction(ExorActionPackBarrel, InputActionMap);
		AddAction(ExorActionUncoverVehicle, InputActionMap);
		AddAction(ExorActionFlipVehicle, InputActionMap);
	}
}
