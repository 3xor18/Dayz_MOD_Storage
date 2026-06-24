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

	// El leaderboard se manda como UN string en un solo RPC (SCORE_DATA). DayZ tiene un
	// LIMITE DURO (~2 KB) para leer un string de un RPC: si se pasa, el cliente tira
	// "String CORRUPTED - FIX OnStoreLoad()" en ExorOnScoreData y la tab Score sale VACIA.
	// (El "Top 30" viejo daba ~3752 chars y se pasaba.) Por eso NO recortamos por un numero
	// fijo de filas, sino por TAMAÑO: agregamos filas (Top por kills) mientras el JSON
	// serializado quede por debajo de SCORE_MAX_BYTES. Asi nunca se corrompe y mostramos
	// tantos del top como entren (~12-14). El cliente reordena esas filas por columna.
	static const int SCORE_MAX_BYTES = 1700;	// margen seguro bajo el ~2KB del RPC
	static const int SCORE_MAX_ROWS  = 30;		// tope duro adicional de filas

	// JSON del leaderboard: Top por kills, recortado por TAMAÑO (no por cantidad fija)
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

		// agregar fila por fila mientras el JSON quepa bajo el limite del RPC
		ExorStatsFile f = new ExorStatsFile();
		JsonSerializer js = new JsonSerializer();
		string data = "";
		for (i = 0; i < hardLim; i++)
		{
			f.rows.Insert(all.Get(i));
			string probe;
			js.WriteToString(f, false, probe);
			if (probe.Length() > SCORE_MAX_BYTES)
			{
				// esta fila lo paso del limite -> sacarla y cortar
				f.rows.Remove(f.rows.Count() - 1);
				break;
			}
			data = probe;
		}
		// si NINGUNA fila entro (raro) igual serializar el vacio valido
		if (data == "")
			js.WriteToString(f, false, data);
		return data;
	}
}
