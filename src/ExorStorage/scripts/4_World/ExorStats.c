// ============================================================================
// 3xor_Vanilla_Optimization - Stats persistentes (SOLO server)
// Cuenta por jugador (steamid): kills (PvP, el del killfeed), deaths (te mato otro
// jugador), suicidios (muerte sin atacante: caida/hambre/vos), y la distancia
// maxima de un kill PvP + el arma de ese kill. Persistido en stats.json.
// Alimenta la tab Score del panel de server info.
// ============================================================================
class ExorStats
{
	private static ref ExorStats s_Inst;
	protected ref map<string, ref ExorStatRow> m_Rows;
	protected bool m_Loaded;
	protected bool m_Dirty;	// hay cambios sin volcar a disco (se flushea por tick, no por evento)

	static ExorStats Get()
	{
		if (!s_Inst)
			s_Inst = new ExorStats();
		return s_Inst;
	}

	void EnsureLoaded()
	{
		if (m_Loaded)
			return;
		m_Loaded = true;
		m_Rows = new map<string, ref ExorStatRow>;
		if (FileExist(ExorStorageConstants.STATS_FILE))
		{
			ExorStatsFile f = new ExorStatsFile();
			JsonFileLoader<ExorStatsFile>.JsonLoadFile(ExorStorageConstants.STATS_FILE, f);
			int i;
			for (i = 0; i < f.rows.Count(); i++)
			{
				ExorStatRow r = f.rows.Get(i);
				if (r && r.steamid != "")
					m_Rows.Set(r.steamid, r);
			}
		}
	}

	ExorStatRow Ensure(string sid, string name)
	{
		EnsureLoaded();
		ExorStatRow r;
		if (!m_Rows.Find(sid, r))
		{
			r = new ExorStatRow();
			r.steamid = sid;
			m_Rows.Set(sid, r);
		}
		if (name != "")
			r.name = name;
		return r;
	}

	void AddKill(string sid, string name, int dist, string weapon)
	{
		if (sid == "")
			return;
		ExorStatRow r = Ensure(sid, name);
		r.kills = r.kills + 1;
		if (dist > r.max_dist)
		{
			r.max_dist = dist;
			r.max_weapon = weapon;
		}
		m_Dirty = true;	// se vuelca a disco en el flush periodico, no en cada kill
	}

	void AddDeath(string sid, string name)
	{
		if (sid == "")
			return;
		ExorStatRow r = Ensure(sid, name);
		r.deaths = r.deaths + 1;
		m_Dirty = true;
	}

	void AddSuicide(string sid, string name)
	{
		if (sid == "")
			return;
		ExorStatRow r = Ensure(sid, name);
		r.suicides = r.suicides + 1;
		m_Dirty = true;
	}

	// Vuelca a disco SOLO si hay cambios. El server lo llama periodicamente (tick de 30s)
	// y al apagar, en vez de escribir el archivo entero en CADA kill/death (I/O sincrona
	// en el hilo del juego -> hitches en picos de PvP a 50 pop).
	void FlushIfDirty()
	{
		if (!m_Dirty)
			return;
		Save();
	}

	void Save()
	{
		if (!m_Loaded)
			return;
		ExorStatsFile f = new ExorStatsFile();
		foreach (string k, ExorStatRow r : m_Rows)
			f.rows.Insert(r);
		JsonFileLoader<ExorStatsFile>.JsonSaveFile(ExorStorageConstants.STATS_FILE, f);
		m_Dirty = false;
	}

	// El leaderboard se manda por SCORE_DATA. DayZ revienta la VM del cliente
	// ("String CORRUPTED - FIX OnStoreLoad()") al LEER un string de RPC > ~2KB. ANTES se
	// recortaba por TAMAÑO (se perdian filas: el Top 30 daba ~3752 chars y solo entraban
	// ~12-14). AHORA el envio va en TROZOS (ExorNetChunk) y el cliente reensambla -> se
	// manda el Top completo sin perder filas. Queda solo el tope DURO de filas (decision de
	// producto: un leaderboard "Top N", no los 188 jugadores).
	static const int SCORE_MAX_ROWS  = 30;		// Top N del leaderboard

	// JSON del leaderboard: Top por kills (hasta SCORE_MAX_ROWS filas), completo.
	string BuildJson()
	{
		EnsureLoaded();

		// pasar el map a un array para poder ordenarlo y recortarlo
		array<ref ExorStatRow> all = new array<ref ExorStatRow>;
		foreach (string k2, ExorStatRow r2 : m_Rows)
			all.Insert(r2);

		// orden descendente por kills (listas chicas -> bubble sort)
		int n = all.Count();
		int i, j;
		for (i = 0; i < n - 1; i++)
		{
			for (j = 0; j < n - 1 - i; j++)
			{
				if (all.Get(j + 1).kills > all.Get(j).kills)
				{
					ExorStatRow tmp = all.Get(j);
					all.Set(j, all.Get(j + 1));
					all.Set(j + 1, tmp);
				}
			}
		}

		int hardLim = all.Count();
		if (hardLim > SCORE_MAX_ROWS)
			hardLim = SCORE_MAX_ROWS;

		ExorStatsFile f = new ExorStatsFile();
		for (i = 0; i < hardLim; i++)
			f.rows.Insert(all.Get(i));

		JsonSerializer js = new JsonSerializer();
		string data = "";
		js.WriteToString(f, false, data);
		return data;
	}
}
