// ============================================================================
// 3xor_Vanilla_Optimization - Atribucion de muertes por GRANADA (server)
// Una granada LANZADA se separa de quien la tiro (GetHierarchyRootPlayer()==null),
// por eso el killfeed no sabia quien mato y la muerte se perdia (caia en "muerte por
// entorno"). Aca la granada recuerda a su ULTIMO portador (= el que la lanzo): mientras
// esta en manos/inventario de un jugador, un tick liviano (1s) guarda su steamid+nombre.
// Al explotar, el killfeed de la victima lo lee (ver ExorStorage_Player.ExorBuildKillfeed).
//   - Solo aplica a las granadas de fragmentacion (Grenade_Base): smoke/gas/etc NO matan
//     por shrapnel y el gas se clasifica aparte por tipo de daño.
//   - El tick solo escribe cuando hay root player -> una granada en un cajon (sin dueño)
//     no consume nada util.
// ============================================================================
modded class Grenade_Base
{
	protected string m_ExorThrowerId;
	protected string m_ExorThrowerName;

	override void EEInit()
	{
		super.EEInit();
		if (GetGame() && GetGame().IsServer())
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorTrackOwner, 2000, true);
	}

	override void EEDelete(EntityAI parent)
	{
		if (GetGame())
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).Remove(ExorTrackOwner);
		super.EEDelete(parent);
	}

	// cada 1s: si la granada esta en poder de un jugador, recordarlo. El ULTIMO recordado
	// antes de salir de manos (al lanzarla) es quien la tiro.
	void ExorTrackOwner()
	{
		PlayerBase p = PlayerBase.Cast(GetHierarchyRootPlayer());
		if (p && p.GetIdentity())
		{
			m_ExorThrowerId = p.GetIdentity().GetPlainId();
			m_ExorThrowerName = p.GetIdentity().GetName();
		}
	}

	string ExorGetThrowerId()   { return m_ExorThrowerId; }
	string ExorGetThrowerName() { return m_ExorThrowerName; }
}
