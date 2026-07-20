// ============================================================================
// 3xor_Vanilla_Optimization - Guard anti tumba duplicada (SOLO server)
// ============================================================================
// PROBLEMA: se reportaron 2 tumbas de una sola muerte. El guard existente
// (m_ExorDeathDone en PlayerBase) cubre el caso de la granada -que dispara EEKilled
// ~11 veces sobre la MISMA entidad- pero es un flag POR INSTANCIA. Si el motor llega a
// tener dos instancias PlayerBase del mismo personaje (desync, combat-log, reconexion
// durante el logout timer, rollback), cada una entra al spawn de la bolsa con su propio
// flag en false y genera su tumba.
//
// SOLUCION: llevar la cuenta por STEAMID a nivel global, con una ventana corta. Sin
// importar cuantas instancias haya, una segunda tumba del mismo jugador dentro de la
// ventana se descarta.
//
// La ventana es corta a proposito: si el jugador respawnea y lo matan DE VERDAD otra vez
// pocos segundos despues, esa segunda tumba es legitima y tiene que salir. VENTANA_MS
// cubre el delay de creacion + un margen, no una vida entera.
// ============================================================================
class ExorBodyBagGuard
{
	static ref map<string, int> s_UltimaMs;	// steamid -> uptime ms de la ultima tumba generada
	static const int VENTANA_MS = 15000;	// 15s: cubre el delay de spawn + margen de desync

	// true si este steamid ya genero una tumba dentro de la ventana (=> no crear otra)
	static bool YaGenero(string sid)
	{
		if (!GetGame() || !GetGame().IsServer() || sid == "")
			return false;
		if (!s_UltimaMs)
			return false;
		int last = 0;
		if (!s_UltimaMs.Find(sid, last))
			return false;
		return (GetGame().GetTime() - last) < VENTANA_MS;
	}

	static void Marcar(string sid)
	{
		if (!GetGame() || !GetGame().IsServer() || sid == "")
			return;
		if (!s_UltimaMs)
			s_UltimaMs = new map<string, int>;
		s_UltimaMs.Set(sid, GetGame().GetTime());
	}

	// Purga entradas viejas para que el map no crezca toda la sesion. Lo llama el tick lento.
	static void Purgar()
	{
		if (!s_UltimaMs || s_UltimaMs.Count() == 0)
			return;
		int now = GetGame().GetTime();
		array<string> viejas = new array<string>;
		int i;
		for (i = 0; i < s_UltimaMs.Count(); i++)
		{
			if (now - s_UltimaMs.GetElement(i) >= VENTANA_MS)
				viejas.Insert(s_UltimaMs.GetKey(i));
		}
		for (i = 0; i < viejas.Count(); i++)
			s_UltimaMs.Remove(viejas.Get(i));
	}
}
