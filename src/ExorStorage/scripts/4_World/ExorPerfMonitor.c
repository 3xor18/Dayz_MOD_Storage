// ============================================================================
// 3xor_Vanilla_Optimization - Monitor de rendimiento (SOLO server)
// Objetivo: DIAGNOSTICO. Correlacionar los reportes de "se me desaparecio algo /
// la bala no registro" con caidas de FPS del server. Es baratisimo: acumula el
// timeslice de cada frame (2 operaciones por frame) y cada 30s escribe UNA linea
// con FPS promedio, el PEOR frame del intervalo, players online y entidades que
// el mod tiene registradas (barriles/vehiculos/bolsas).
//
// Se alimenta desde MissionServer.OnUpdate(timeslice) (ver ExorStorage_Mission.c).
// ============================================================================
class ExorPerfMonitor
{
	static float s_Accum;        // segundos acumulados en el intervalo
	static int   s_Frames;       // frames contados en el intervalo
	static float s_WorstMs;      // peor (mas largo) frame del intervalo, en ms
	static float s_IntervalSec = 30.0;

	static void Feed(float timeslice)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (timeslice <= 0)
			return;

		s_Frames++;
		s_Accum = s_Accum + timeslice;
		float ms = timeslice * 1000.0;
		if (ms > s_WorstMs)
			s_WorstMs = ms;

		if (s_Accum < s_IntervalSec)
			return;

		float fps = 0;
		if (s_Accum > 0)
			fps = s_Frames / s_Accum;

		int players = ExorVO_Manager.s_PopCount;	// cacheado por BarrelTick (sin GetPlayers extra)
		int barrels = ExorVO_Manager.Get().m_Barrels.Count();
		int vehis   = ExorVO_Manager.Get().m_Vehicles.Count();
		int bags    = ExorVO_Manager.Get().m_BodyBags.Count();

		// marca visible si el intervalo tuvo un frame feo (posible causa de desync)
		string flag = "";
		if (s_WorstMs >= 150.0)        // <~6.6 FPS instantaneo
			flag = "  <<< LAG SPIKE";
		else if (s_WorstMs >= 60.0)    // <~16 FPS instantaneo
			flag = "  <- frame lento";

		Print(string.Format("%1 PERF fps=%2 peorFrame=%3ms players=%4 barriles=%5 vehiculos=%6 bolsas=%7%8",
			ExorStorageConstants.LOG,
			Math.Round(fps),
			Math.Round(s_WorstMs),
			players, barrels, vehis, bags, flag));

		s_Accum = 0;
		s_Frames = 0;
		s_WorstMs = 0;
	}
}
