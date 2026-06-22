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
		Save();
	}

	void AddDeath(string sid, string name)
	{
		if (sid == "")
			return;
		ExorStatRow r = Ensure(sid, name);
		r.deaths = r.deaths + 1;
		Save();
	}

	void AddSuicide(string sid, string name)
	{
		if (sid == "")
			return;
		ExorStatRow r = Ensure(sid, name);
		r.suicides = r.suicides + 1;
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
	}

	// Cuantas filas como maximo viajan al cliente. El leaderboard se manda como UN
	// string en un solo RPC (SCORE_DATA) y DayZ corta los strings grandes: con muchos
	// jugadores el JSON completo (>12k chars con ~100 players) se pasaba del limite y
	// no llegaba -> la tab Score salia vacia. Recortando al Top N por kills nunca se
	// pasa, y es lo que un leaderboard muestra igual. El cliente igual puede re-ordenar
	// estas N filas por la columna que clickee.
	static const int SCORE_MAX_ROWS = 30;

	// JSON del leaderboard: Top N por kills (ver SCORE_MAX_ROWS)
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

		ExorStatsFile f = new ExorStatsFile();
		int lim = all.Count();
		if (lim > SCORE_MAX_ROWS)
			lim = SCORE_MAX_ROWS;
		for (i = 0; i < lim; i++)
			f.rows.Insert(all.Get(i));

		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(f, false, data);
		return data;
	}
}
