// ============================================================================
// 3xor_Vanilla_Optimization - Detector forense de FARMEO de kills (SOLO server)
// Caza a los que inflan kills: un mismo asesino que mata al MISMO jugador muchas
// veces en poco tiempo (ej. socios que salen del territorio y se dejan matar por
// uno solo, o cuentas amigas que se feedean aunque NO compartan party).
//   - Indexado por STEAMID (no por nombre) -> inmune a que se cambien el nombre.
//   - Ventana deslizante real (default 4h). Si killer->victima alcanza 'umbral'
//     kills dentro de la ventana, escribe una linea al raidlog (ServerAuditLog).
//   - Persiste a disco: el server reinicia cada ~4h; sin persistir, la ventana se
//     resetearia justo en cada arranque. Los timestamps son minutos del reloj host
//     (ExorTimeUtil.NowMinutes), comparables entre reinicios.
//   - On-kill only + guardado diferido (1x/seg si hubo cambios) -> cero costo por frame.
// ============================================================================

// Una entrada del archivo: par killer->victima (steamid) + timestamps de sus kills.
class ExorKillFarmPair
{
	string k;            // steamid del asesino
	string v;            // steamid de la victima
	ref array<int> t;    // minutos (NowMinutes) de cada kill dentro de la ventana

	void ExorKillFarmPair() { t = new array<int>; }
}

class ExorKillFarmFile
{
	int version = 1;
	ref array<ref ExorKillFarmPair> pairs;

	void ExorKillFarmFile() { pairs = new array<ref ExorKillFarmPair>; }
}

class ExorKillFarm
{
	static ref map<string, ref array<int>> s_Pairs;   // "killerSid|victimSid" -> timestamps
	static bool s_Loaded;
	static bool s_Dirty;

	// Anti-spam del log: por par, el ultimo 'count' que ya se reporto. No se persiste a
	// proposito: tras un reinicio se vuelve a reportar el par si sigue farmeando, que es
	// justamente lo que se quiere ver.
	static ref map<string, int> s_Reported;
	static const int REPORT_STEP = 5;	// re-reportar el mismo par recien cada 5 kills mas

	static void Init()
	{
		Load();
	}

	static string Key(string k, string v)
	{
		return k + "|" + v;
	}

	// Carga el ledger del disco y poda lo que ya cayo fuera de la ventana.
	static void Load()
	{
		s_Pairs = new map<string, ref array<int>>;
		s_Loaded = true;
		s_Dirty = false;
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (!FileExist(ExorStorageConstants.KILLFARM_FILE))
			return;

		ExorKillFarmFile f = new ExorKillFarmFile();
		JsonFileLoader<ExorKillFarmFile>.JsonLoadFile(ExorStorageConstants.KILLFARM_FILE, f);
		if (!f || !f.pairs)
			return;

		int win = GetExorConfig().party.proteccion.farmeo_ventana_minutos;
		int now = ExorTimeUtil.NowMinutes();
		int i, j;
		for (i = 0; i < f.pairs.Count(); i++)
		{
			ExorKillFarmPair p = f.pairs.Get(i);
			if (!p || !p.t || p.k == "" || p.v == "")
				continue;
			ref array<int> keep = new array<int>;
			for (j = 0; j < p.t.Count(); j++)
				if (win <= 0 || now - p.t.Get(j) < win)
					keep.Insert(p.t.Get(j));
			if (keep.Count() > 0)
				s_Pairs.Set(Key(p.k, p.v), keep);
		}
	}

	// Guarda el ledger a disco SOLO si hubo cambios. Lo llama el tick 1Hz del anti-cheat.
	static void FlushIfDirty()
	{
		if (!s_Dirty)
			return;
		Save();
		s_Dirty = false;
	}

	// PURGA EN CALIENTE. El ledger se podaba SOLO al cargarlo del disco, asi que durante la
	// sesion crecia sin techo: la clave es "asesino|victima", o sea O(jugadores^2) en el peor
	// caso, y cada entrada guarda un array de minutos que tampoco se recortaba. Con 70
	// jugadores y varias horas de PvP eso es miles de arrays vivos por nada: los kills fuera
	// de la ventana de farmeo ya no los mira nadie. La llama ExorHousekeeping.
	static void PurgarViejos()
	{
		if (!GetGame() || !GetGame().IsServer() || !s_Pairs)
			return;
		int win = GetExorConfig().party.proteccion.farmeo_ventana_minutos;
		if (win <= 0)
			return;
		int ahora = ExorTimeUtil.NowMinutes();
		array<string> vacias = new array<string>;
		foreach (string key, array<int> ts : s_Pairs)
		{
			if (!ts)
			{
				vacias.Insert(key);
				continue;
			}
			int z;
			for (z = ts.Count() - 1; z >= 0; z--)
			{
				if (ahora - ts.Get(z) > win)
					ts.Remove(z);
			}
			if (ts.Count() == 0)
				vacias.Insert(key);
		}
		int i;
		for (i = 0; i < vacias.Count(); i++)
			s_Pairs.Remove(vacias.Get(i));
	}

	static void Save()
	{
		if (!GetGame() || !GetGame().IsServer() || !s_Pairs)
			return;
		ExorKillFarmFile f = new ExorKillFarmFile();
		foreach (string key, array<int> ts : s_Pairs)
		{
			if (!ts || ts.Count() == 0)
				continue;
			int bar = key.IndexOf("|");
			if (bar < 0)
				continue;
			ExorKillFarmPair p = new ExorKillFarmPair();
			p.k = key.Substring(0, bar);
			p.v = key.Substring(bar + 1, key.Length() - bar - 1);
			int z;
			for (z = 0; z < ts.Count(); z++)
				p.t.Insert(ts.Get(z));
			f.pairs.Insert(p);
		}
		JsonFileLoader<ExorKillFarmFile>.JsonSaveFile(ExorStorageConstants.KILLFARM_FILE, f);
	}

	// Llamar en CADA kill PvP (server). killerPos = posicion del asesino (para el raidlog).
	// teamKill = true si en ese momento eran del mismo grupo (informativo en el log).
	static void OnKill(string killerSid, string killerName, string victimSid, string victimName, vector killerPos, bool teamKill)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		ExorCfgPartyProteccion cfg = GetExorConfig().party.proteccion;
		if (!cfg.log_farmeo_kills || cfg.farmeo_umbral <= 0)
			return;
		if (killerSid == "" || victimSid == "" || killerSid == victimSid)
			return;
		if (!s_Loaded)
			Load();

		int win = cfg.farmeo_ventana_minutos;
		int now = ExorTimeUtil.NowMinutes();
		string key = Key(killerSid, victimSid);

		array<int> ts;
		if (!s_Pairs.Find(key, ts) || !ts)
		{
			ts = new array<int>;
			s_Pairs.Set(key, ts);
		}
		// podar lo que cayo fuera de la ventana (desde atras para borrar por indice sin saltear)
		int i;
		for (i = ts.Count() - 1; i >= 0; i--)
			if (win > 0 && now - ts.Get(i) >= win)
				ts.Remove(i);
		ts.Insert(now);
		s_Dirty = true;

		int count = ts.Count();
		if (count >= cfg.farmeo_umbral)
		{
			// ANTI-SPAM: una vez cruzado el umbral se logueaba CADA kill posterior del mismo
			// par. Un solo caso real (20-jul) genero 20 lineas subiendo "4,5,6...14 veces",
			// todas diciendo lo mismo. Ahora se reporta al CRUZAR el umbral y despues solo
			// cada REPORT_STEP kills -> el patron sigue visible pero sin inundar el audit.
			if (!s_Reported)
				s_Reported = new map<string, int>;
			int lastRep = 0;
			s_Reported.Find(key, lastRep);
			bool debeReportar = (lastRep == 0) || (count >= lastRep + REPORT_STEP);
			if (!debeReportar)
				return;
			s_Reported.Set(key, count);

			string tk = "no";
			if (teamKill)
				tk = "si (mismo grupo)";
			string detalle = string.Format("POSIBLE FARMEO: mato a la MISMA victima steam=%1 (%2) %3 veces en <=%4 min | teamkill=%5",
				victimSid, victimName, count, win, tk);
			// immediate=true: evento sensible, que no se pierda si el server crashea.
			ExorRaidLog.Write("FARMEO_KILLS", killerSid, killerName, killerPos, detalle, true);
			Print(string.Format("%1 FARMEO_KILLS: %2 -> %3 x%4 (ventana %5min)", ExorStorageConstants.LOG, killerSid, victimSid, count, win));
		}
	}
}
