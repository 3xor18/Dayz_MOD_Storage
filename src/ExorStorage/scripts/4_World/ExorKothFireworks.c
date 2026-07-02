// ============================================================================
// 3xor_Vanilla_Optimization - KOTH: encender fuegos artificiales por script
// OnIgnitedThis() es PROTEGIDO en FireworksBase, asi que no se puede llamar desde
// afuera. Con una clase modded exponemos un metodo publico que sí puede llamarlo
// (esta dentro de la propia clase). Lo usa ExorKoth al completar un koth.
// ============================================================================
modded class FireworksBase
{
	void ExorKothIgnite()
	{
		if (!GetGame().IsServer())
			return;
		OnIgnitedThis(this);
	}
}
