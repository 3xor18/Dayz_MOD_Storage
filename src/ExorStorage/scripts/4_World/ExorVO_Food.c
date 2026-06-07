// ============================================================================
// 3xor_Vanilla_Optimization - Comida dura mas dentro del barril 3xor (Fase 2)
// El deterioro corre N veces mas lento (multiplicador_comida, default 2.0).
// Virtualizado, el deterioro queda congelado por completo (los items no existen).
// ============================================================================
modded class Edible_Base
{
	override void ProcessDecay(float delta, bool hasRootAsPlayer)
	{
		if (GetGame() && GetGame().IsServer())
		{
			EntityAI root = GetHierarchyRoot();
			if (root && root.IsInherited(Exor_Barrel_Base))
			{
				ExorStorageSettings settings = GetExorStorageSettings();
				if (settings.multiplicador_comida > 1)
				{
					delta = delta / settings.multiplicador_comida;
				}
			}
		}
		super.ProcessDecay(delta, hasRootAsPlayer);
	}
}
