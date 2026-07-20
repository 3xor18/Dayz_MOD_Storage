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

	// Peor frame desde la ULTIMA lectura del manager. Se lleva aparte de s_WorstMs porque
	// ese se resetea cada 30s (el intervalo del log) y el BarrelTick corre cada 5s: necesita
	// una senal fresca para ajustar el presupuesto sin esperar al log.
	static float s_WorstSinceRead;

	// Devuelve el peor frame (ms) desde la llamada anterior y resetea el acumulador.
	// 0 = todavia no hubo muestra. Lo consume ExorVO_Manager.AdaptBudgetFactor().
	static float ConsumeWorstMs()
	{
		float v = s_WorstSinceRead;
		s_WorstSinceRead = 0;
		return v;
	}

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
		if (ms > s_WorstSinceRead)
			s_WorstSinceRead = ms;	// senal para el presupuesto adaptativo (se lee cada 5s)

		if (s_Accum < s_IntervalSec)
			return;

		float fps = 0;
		if (s_Accum > 0)
			fps = s_Frames / s_Accum;

		int players = ExorVO_Manager.s_PopCount;	// cacheado por BarrelTick (sin GetPlayers extra)
		int barrels = ExorVO_Manager.Get().m_Barrels.Count();
		int vehis   = ExorVO_Manager.Get().m_Vehicles.Count();
		int bags    = ExorVO_Manager.Get().m_BodyBags.Count();
		// muebles abribles (nevera/lockers/gun cabinet): tickean en el MISMO BarrelTick y con
		// presupuesto propio, asi que suman trabajo por tick igual que los barriles. Sin este
		// contador no se puede correlacionar una caida de FPS con cuantos muebles hay puestos.
		int muebles = ExorVO_Manager.Get().m_Openables.Count();

		// marca visible si el intervalo tuvo un frame feo (posible causa de desync)
		string flag = "";
		if (s_WorstMs >= 150.0)        // <~6.6 FPS instantaneo
			flag = "  <<< LAG SPIKE";
		else if (s_WorstMs >= 60.0)    // <~16 FPS instantaneo
			flag = "  <- frame lento";

		Print(string.Format("%1 PERF fps=%2 peorFrame=%3ms players=%4 barriles=%5 muebles=%6 vehiculos=%7 bolsas=%8%9",
			ExorStorageConstants.LOG,
			Math.Round(fps),
			Math.Round(s_WorstMs),
			players, barrels, muebles, vehis, bags, flag));

		s_Accum = 0;
		s_Frames = 0;
		s_WorstMs = 0;
	}
}
