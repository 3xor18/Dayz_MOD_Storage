// ============================================================================
// 3xorStorage - Anti-dupe: ventana de arranque y cierre previo al reinicio
//
// Contexto (ver el GUARD en ExorDoRestore de ExorStorage_Barrels.c y
// ExorStorage_Openable.c): la ruta de duplicado exige que el contenedor llegue al
// apagado del server SIN virtualizar, o sea con items REALES en el cargo del engine.
// Al cargar, ExorRestoreIfNeeded ve "justReconciled && ExorCargoCount() > 0" y
// difiere el restore 300 ms sin tocar m_ExorVirt -> dos aperturas en esa ventana
// recreaban el JSON dos veces.
//
// Este archivo ataca la PRECONDICION por dos lados:
//   1) Ventana de gracia POR JUGADOR: los primeros N segundos desde que entra no
//      puede abrir contenedores. Se mide desde su conexion y no desde el arranque
//      de la mision, porque cuando el primer player termina de conectar ya pasaron
//      los segundos y el bloqueo no agarraba a nadie. Como anti-dupe aporta poco
//      (el pase de reconcile toca 3 contenedores cada 30 s: recorrer ~980 lleva
//      horas, no minutos), pero saca el rush post-reinicio y alivia el boot.
//   2) Cerrar + virtualizar TODO unos minutos antes de un reinicio programado. Asi
//      el server se apaga con los cargos vacios y el JSON como unica verdad.
//      Cubre los reinicios de reloj, NO las caidas del host (no avisan). Para esas
//      esta el guard, que es el que cierra el agujero al 100%.
// ============================================================================

class ExorStorageBootLock
{
	static int s_UltimoAvisoMs;   // throttle del aviso en rojo
	// Player que disparo el Open() que se esta ejecutando. Lo setea la accion (que si tiene
	// el player) justo antes de abrir, y lo lee el Open() del contenedor. Es para el BARRIL,
	// que usa la accion VANILLA y por eso no puede chequear en su propio OnStartServer.
	// Si queda null (camino no previsto), el bloqueo simplemente no aplica: falla ABIERTO,
	// nunca trabando un contenedor.
	static PlayerBase s_Abriendo;

	// ---- 1) ventana de gracia POR JUGADOR ----
	// Se mide desde que el jugador ENTRO, no desde el arranque de la mision: medido desde la
	// mision no sirve, porque cuando el primer player termina de conectar ya pasaron los
	// segundos. Costo: una resta de enteros sobre un campo del propio PlayerBase -> con 80
	// players conectados es igual de barato que con 1 (no recorre listas ni toca disco).
	// Devuelve los segundos que le faltan, o 0 si ya puede abrir.
	static int SegundosRestantes(PlayerBase p)
	{
		if (!p || !GetGame().IsServer())
			return 0;
		ExorCfgStorage s = GetExorConfig().storage;
		if (!s || s.bloqueo_abrir_al_entrar_segundos <= 0)
			return 0;
		int pasados = (GetGame().GetTime() - p.ExorConnectMs()) / 1000;
		int faltan = s.bloqueo_abrir_al_entrar_segundos - pasados;
		if (faltan < 0)
			faltan = 0;
		return faltan;
	}

	// true = todavia no puede abrir. Avisa en rojo (con throttle) para que el player entienda
	// por que no le sale la accion.
	static bool BloqueadoConAviso(PlayerBase p)
	{
		int faltan = SegundosRestantes(p);
		if (faltan <= 0)
			return false;
		int now = GetGame().GetTime();
		if (now - s_UltimoAvisoMs >= 2000)
		{
			s_UltimoAvisoMs = now;
			ExorMuebleRules.SendRed(p, string.Format("Acabas de entrar: podes abrir contenedores en %1 s.", faltan));
			// Log (dentro del throttle, o sea maximo 1 linea cada 2 s): sin esto no hay forma
			// de verificar en produccion que la ventana de gracia esta actuando.
			Print(string.Format("%1 GRACIA: %2 intento abrir un contenedor y le faltan %3 s desde que entro", ExorStorageConstants.LOG, ExorGroupManager.SteamId(p), faltan));
		}
		return true;
	}

	// ---- 2) cierre previo al reinicio ----
	// true si estamos dentro de la ventana [reinicio - cerrar_antes_reinicio_minutos, reinicio).
	// Usa hora LOCAL (aplica offset_horas), igual que el horario de looteo libre.
	static bool CercaDeReinicio(ExorCfgStorage s)
	{
		if (!s || s.cerrar_antes_reinicio_minutos <= 0)
			return false;
		if (!s.reinicios_horas || s.reinicios_horas.Count() == 0)
			return false;

		int minOfDay, weekday, dayKey;
		ExorCofre.NowLocal(s.offset_horas, minOfDay, weekday, dayKey);

		for (int i = 0; i < s.reinicios_horas.Count(); i++)
		{
			int h = s.reinicios_horas.Get(i);
			if (h < 0 || h > 23)
				continue;
			int minReinicio = h * 60;
			// distancia en minutos hasta el reinicio, envolviendo la medianoche
			int falta = minReinicio - minOfDay;
			if (falta < 0)
				falta = falta + 1440;
			if (falta > 0 && falta <= s.cerrar_antes_reinicio_minutos)
				return true;
		}
		return false;
	}
}
