// ============================================================================
// 3xorStorage - Registro de acciones custom en el motor
// ============================================================================
modded class ActionConstructor
{
	override void RegisterActions(TTypenameArray actions)
	{
		super.RegisterActions(actions);

		actions.Insert(ExorActionPackBarrel);
		actions.Insert(ExorActionDeployBarrel);
	}
}
