// ============================================================================
// 3xor_Vanilla_Optimization - Atribucion de muertes por GRANADA (server)
// ----------------------------------------------------------------------------
// Una granada LANZADA se separa de quien la tiro (GetHierarchyRootPlayer() pasa a ser
// null), por eso el killfeed no sabia quien mato y la muerte caia en "muerte por
// entorno". Aca la granada recuerda a su ULTIMO portador (= el que la lanzo) y el
// killfeed de la victima lo lee (ver ExorStorage_Player.ExorBuildKillfeed).
// Solo aplica a las de fragmentacion (Grenade_Base): las de humo/gas no matan por
// shrapnel y el gas se clasifica aparte por tipo de daño.
//
// POR EVENTOS, NO POR TICK
// ----------------------------------------------------------------------------
// Antes esto era un CallLater REPETITIVO cada 2 s que arrancaba en el EEInit de CADA
// granada y no paraba nunca. O sea: una granada tirada en el piso de un cuartel, otra
// dentro de un barril, otra en la mochila de cada jugador... todas con su propio timer
// vivo en el CallQueue del motor, preguntando dos veces por minuto "seguis en manos de
// alguien?" para una respuesta que casi siempre es no. Con las granadas que el CE
// reparte por el mapa eso son cientos de timers permanentes para un dato que cambia
// solo en dos momentos muy concretos.
//
// Esos dos momentos ya los avisa el motor:
//   - OnInventoryEnter        -> la agarro un jugador (queda anotado el portador);
//   - EEItemLocationChanged   -> se movio; si SALIO de un jugador, ese es el que la tiro.
// Cero timers, cero costo mientras nadie toca nada, y el dato queda mas fresco que con
// un muestreo cada 2 s (con el tick, una granada agarrada y lanzada dentro de la misma
// ventana no se registraba).
// ============================================================================
modded class Grenade_Base
{
	protected string m_ExorThrowerId;
	protected string m_ExorThrowerName;

	// La agarro un jugador: es el candidato a lanzador.
	override void OnInventoryEnter(Man player)
	{
		super.OnInventoryEnter(player);
		if (GetGame() && GetGame().IsServer())
			ExorRecordarPortador(PlayerBase.Cast(player));
	}

	// Se movio. Si venia DE un jugador, ese es el que la esta soltando o lanzando: es el
	// ultimo instante en que la granada y su dueño estan relacionados.
	override void EEItemLocationChanged(notnull InventoryLocation oldLoc, notnull InventoryLocation newLoc)
	{
		super.EEItemLocationChanged(oldLoc, newLoc);
		if (!GetGame() || !GetGame().IsServer())
			return;
		EntityAI padre = oldLoc.GetParent();
		if (!padre)
			return;
		ExorRecordarPortador(PlayerBase.Cast(padre.GetHierarchyRootPlayer()));
	}

	void ExorRecordarPortador(PlayerBase p)
	{
		if (!p || !p.GetIdentity())
			return;
		m_ExorThrowerId = p.GetIdentity().GetPlainId();
		m_ExorThrowerName = p.GetIdentity().GetName();
	}

	string ExorGetThrowerId()   { return m_ExorThrowerId; }
	string ExorGetThrowerName() { return m_ExorThrowerName; }
}
