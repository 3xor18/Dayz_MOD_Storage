// ============================================================================
// 3xor_Vanilla_Optimization - Agrupador de ROBO_ITEM (SOLO server)
// ============================================================================
// PROBLEMA: el log de saqueo en base ajena escribia UNA LINEA POR ITEM. Vaciar una
// mochila = 20 escrituras sincronas al audit en el mismo segundo. En 2 dias fueron
// 1913 lineas, y un solo jugador (PaTo) genero 713. Es I/O en el hilo del juego
// disparada por una accion que el jugador puede repetir muy rapido.
//
// SOLUCION: acumular por (jugador, territorio) y escribir UNA linea cuando la sesion
// de saqueo termina. Baja el I/O ~10x y ADEMAS mejora el reporte: en vez de 20 lineas
// sueltas queda "se llevo 20 items de la base de X: AK x2, Mag x5, ...", que es
// exactamente la tabla de looteo que se pide despues.
//
// La sesion se cierra cuando pasa cualquiera de estas:
//   - FLUSH_IDLE_MS sin tomar nada mas (dejo de saquear)
//   - MAX_ITEMS acumulados (no dejar crecer la linea sin limite)
//   - cambia de territorio (empezo a saquear otra base)
//   - se desconecta / apaga el server (no perder evidencia)
// ============================================================================
class ExorRoboSesion
{
	string sid;
	string nombre;
	string territorio;
	vector pos;
	int lastMs;
	int total;
	ref map<string, int> items;	// tipo -> cantidad (agrupa repetidos: "Ammo x37")

	void ExorRoboSesion()
	{
		items = new map<string, int>;
	}

	void Add(string tipo)
	{
		int n = 0;
		items.Find(tipo, n);
		items.Set(tipo, n + 1);
		total++;
	}

	// "AKM x2, Mag_STANAG x5, Rice" - ordenado por insercion, cantidad solo si >1
	string ItemsToString()
	{
		string s = "";
		int i;
		for (i = 0; i < items.Count(); i++)
		{
			if (i > 0)
				s = s + ", ";
			string k = items.GetKey(i);
			int v = items.GetElement(i);
			if (v > 1)
				s = s + k + " x" + v.ToString();
			else
				s = s + k;
		}
		return s;
	}
}

class ExorRoboBuffer
{
	static ref map<string, ref ExorRoboSesion> s_Abiertas;	// steamid -> sesion en curso

	static const int FLUSH_IDLE_MS = 20000;	// 20s sin tomar nada = termino de saquear
	static const int MAX_ITEMS     = 60;	// tope por linea (si sigue, abre otra sesion)

	static void EnsureInit()
	{
		if (!s_Abiertas)
			s_Abiertas = new map<string, ref ExorRoboSesion>;
	}

	// Registra un item tomado. NO escribe a disco: solo acumula.
	static void Add(string sid, string nombre, string territorio, vector pos, string tipo)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (sid == "")
			return;
		EnsureInit();

		int now = GetGame().GetTime();
		ExorRoboSesion s;
		if (s_Abiertas.Find(sid, s))
		{
			// cambio de base -> cerrar la anterior antes de empezar la nueva
			if (s.territorio != territorio)
			{
				Flush(sid);
				s = null;
			}
		}

		if (!s)
		{
			s = new ExorRoboSesion();
			s.sid = sid;
			s.nombre = nombre;
			s.territorio = territorio;
			s_Abiertas.Set(sid, s);
		}

		s.nombre = nombre;	// refrescar (pudo cambiar de nombre)
		s.pos = pos;
		s.lastMs = now;
		s.Add(tipo);

		if (s.total >= MAX_ITEMS)
			Flush(sid);
	}

	// Cierra la sesion de un jugador y escribe la linea agrupada.
	static void Flush(string sid)
	{
		EnsureInit();
		ExorRoboSesion s;
		if (!s_Abiertas.Find(sid, s))
			return;
		if (s && s.total > 0)
		{
			string detalle = string.Format("se llevo %1 items de territorio de %2: %3",
				s.total, s.territorio, s.ItemsToString());
			ExorRaidLog.Write("ROBO_ITEM", s.sid, s.nombre, s.pos, detalle);
		}
		s_Abiertas.Remove(sid);
	}

	// Cierra las sesiones que quedaron inactivas. Lo llama el BarrelTick (cada 5s), asi
	// que no agrega un timer propio.
	static void Tick()
	{
		if (!s_Abiertas || s_Abiertas.Count() == 0)
			return;
		int now = GetGame().GetTime();
		// recolectar primero: no se puede borrar del map mientras se lo recorre
		array<string> vencidas = new array<string>;
		int i;
		for (i = 0; i < s_Abiertas.Count(); i++)
		{
			ExorRoboSesion s = s_Abiertas.GetElement(i);
			if (s && now - s.lastMs >= FLUSH_IDLE_MS)
				vencidas.Insert(s_Abiertas.GetKey(i));
		}
		for (i = 0; i < vencidas.Count(); i++)
			Flush(vencidas.Get(i));
	}

	// Vacia TODO (apagado del server / desconexion masiva) para no perder evidencia.
	static void FlushAll()
	{
		if (!s_Abiertas || s_Abiertas.Count() == 0)
			return;
		array<string> todas = new array<string>;
		int i;
		for (i = 0; i < s_Abiertas.Count(); i++)
			todas.Insert(s_Abiertas.GetKey(i));
		for (i = 0; i < todas.Count(); i++)
			Flush(todas.Get(i));
	}
}
