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

	// JSON del leaderboard completo (el cliente lo ordena segun la columna clickeada)
	string BuildJson()
	{
		EnsureLoaded();
		ExorStatsFile f = new ExorStatsFile();
		foreach (string k2, ExorStatRow r2 : m_Rows)
			f.rows.Insert(r2);
		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(f, false, data);
		return data;
	}
}
