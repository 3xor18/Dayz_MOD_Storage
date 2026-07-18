// ============================================================================
// 3xorStorage - Conservacion de comida en el refrigerador
// ----------------------------------------------------------------------------
// La pudricion de la comida en DayZ corre por el motor via CanProcessDecay():
// si devuelve false, la comida NO avanza de estado (no se pudre). Vanilla ya lo
// usa (la comida CONGELADA no se pudre).
//
// Aca lo extendemos: si la comida esta DENTRO de una Exor_Fridge que tiene
// bateria con carga (ExorIsPowered), su decay se CONGELA -> no se pudre mientras
// haya energia. Al sacarla, o cuando la bateria se agota, el decay sigue desde
// donde estaba (el m_DecayTimer se preserva; no se resetea). Es "detener la
// pudricion" de verdad, no solo ralentizarla.
// ============================================================================

modded class Edible_Base
{
	override bool CanProcessDecay()
	{
		// El contenedor directo donde vive este item.
		Exor_Fridge fridge = Exor_Fridge.Cast(GetHierarchyParent());
		if (fridge && fridge.ExorIsPowered())
			return false;	// nevera con energia -> no se pudre

		return super.CanProcessDecay();
	}
}
