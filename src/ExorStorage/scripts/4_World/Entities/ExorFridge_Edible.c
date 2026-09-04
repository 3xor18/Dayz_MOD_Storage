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

	// ------------------------------------------------------------------
	// ENVEJECER: correr la pudricion vanilla por un tiempo que ya paso.
	// ------------------------------------------------------------------
	// Hace falta porque la nevera virtualiza SIEMPRE (ver Exor_Fridge.ExorCanVirtualizeNow):
	// mientras esta virtualizada la comida NO EXISTE como entidad, asi que el motor no le
	// corre la pudricion. Al restaurarla sin bateria hay que cobrarle ese tiempo.
	//
	// No se inventa ninguna formula: se llama al MISMO ProcessDecay de vanilla que corre el
	// motor en cada tick (respeta el multiplicador de decay del server, la vida del item y la
	// tabla de tiempos por tipo de comida). Se va de a tramos porque ProcessDecay avanza como
	// mucho UNA etapa por llamada: con tramos de 30 min hay resolucion de sobra (el timer mas
	// corto de vanilla, carne cruda, son 6 h) sin hacer miles de vueltas.
	// Corta sola en cuanto la comida llega a PODRIDA (CanProcessDecay pasa a false).
	void ExorEnvejecer(float segundos)
	{
		if (!GetGame().IsServer())
			return;
		if (segundos <= 0)
			return;
		if (!g_Game.IsFoodDecayEnabled())
			return;		// el server tiene la pudricion apagada -> no tocar nada

		float restante = segundos;
		int vueltas = 0;
		while (restante > 0 && vueltas < EXOR_ENVEJ_MAX_TRAMOS)
		{
			if (!CanDecay() || !CanProcessDecay())
				return;		// ya esta podrida / congelada: no hay mas que envejecer
			float paso = restante;
			if (paso > EXOR_ENVEJ_TRAMO_SEG)
				paso = EXOR_ENVEJ_TRAMO_SEG;
			ProcessDecay(paso, false);	// false = no esta encima de un player
			restante = restante - paso;
			vueltas++;
		}
	}

	// tramo de 30 min y tope de 336 tramos (= 7 dias, el mismo tope que aplica la nevera)
	protected const float EXOR_ENVEJ_TRAMO_SEG = 1800.0;
	protected const int   EXOR_ENVEJ_MAX_TRAMOS = 336;
}
