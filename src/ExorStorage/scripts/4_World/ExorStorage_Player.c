// ============================================================================
// 3xorStorage - Acciones del jugador sobre objetos del mundo
// Las acciones con TARGET (mirar el barril y empaquetarlo) se registran en
// PlayerBase; las de item en manos (desplegar la caja) van en el SetActions
// del item.
// ============================================================================
modded class PlayerBase
{
	override void SetActions(out TInputActionMap InputActionMap)
	{
		super.SetActions(InputActionMap);

		AddAction(ExorActionPackBarrel, InputActionMap);
	}
}
