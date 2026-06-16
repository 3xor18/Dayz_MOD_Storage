// ============================================================================
// 3xor_Vanilla_Optimization - Estado VIP persistente (SOLO server)
// Por steamid guarda: fecha de ingreso (1ra vez que se lo ve como VIP) + usos de
// equipamiento consumidos en el CICLO actual. El ciclo es mensual pero anclado al
// DIA de ingreso (entro el 15 -> resetea el 15 de cada mes, no el 1ro).
// Persistido en vip_state.json. Tambien aplica el loadout VIP al jugador.
// ============================================================================
// La fecha de ingreso vive en vip.json (la maneja el admin). Aca solo guardamos el
// uso por ciclo + una COPIA del ancla (la fecha contra la que se calculo) para
// detectar si el admin la cambio / re-ingreso al VIP -> resetear.
class ExorVipStateRow
{
	string steamid;
	int anc_y;         // ancla = fecha de ingreso usada la ultima vez
	int anc_m;
	int anc_d;
	int last_ciclo;    // indice de ciclo visto la ultima vez (para detectar reset mensual)
	int usos;          // usos consumidos en el ciclo actual
}

class ExorVipStateFile
{
	ref array<ref ExorVipStateRow> rows;
	void ExorVipStateFile() { rows = new array<ref ExorVipStateRow>; }
}

class ExorVipState
{
	private static ref ExorVipState s_Inst;
	protected ref map<string, ref ExorVipStateRow> m_Rows;
	protected bool m_Loaded;

	static ExorVipState Get()
	{
		if (!s_Inst)
			s_Inst = new ExorVipState();
		return s_Inst;
	}

	void EnsureLoaded()
	{
		if (m_Loaded)
			return;
		m_Loaded = true;
		m_Rows = new map<string, ref ExorVipStateRow>;
		if (FileExist(ExorStorageConstants.VIP_STATE_FILE))
		{
			ExorVipStateFile f = new ExorVipStateFile();
			JsonFileLoader<ExorVipStateFile>.JsonLoadFile(ExorStorageConstants.VIP_STATE_FILE, f);
			int i;
			for (i = 0; i < f.rows.Count(); i++)
			{
				ExorVipStateRow r = f.rows.Get(i);
				if (r && r.steamid != "")
					m_Rows.Set(r.steamid, r);
			}
		}
	}

	void Save()
	{
		if (!m_Loaded)
			return;
		ExorVipStateFile f = new ExorVipStateFile();
		foreach (string k, ExorVipStateRow r : m_Rows)
			f.rows.Insert(r);
		JsonFileLoader<ExorVipStateFile>.JsonSaveFile(ExorStorageConstants.VIP_STATE_FILE, f);
	}

	// ------------------------- ciclo mensual anclado al ingreso -------------------------
	static int DaysInMonth(int y, int m)
	{
		if (m == 2)
		{
			bool leap = (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
			if (leap)
				return 29;
			return 28;
		}
		if (m == 4 || m == 6 || m == 9 || m == 11)
			return 30;
		return 31;
	}

	// Indice de ciclo: cuantos "meses desde el ingreso" pasaron, contando el corte
	// en el DIA de ingreso (jd). Clamp del dia al ultimo del mes (ingreso el 31).
	static int CycleIndex(int jy, int jm, int jd, int y, int m, int d)
	{
		int months = (y - jy) * 12 + (m - jm);
		int eff = jd;
		int dim = DaysInMonth(y, m);
		if (eff > dim)
			eff = dim;
		if (d < eff)
			months = months - 1;
		return months;
	}

	// Parsea "AAAA-MM-DD". Devuelve true si ok (y deja py/pm/pd seteados).
	static bool ParseFecha(string s, out int py, out int pm, out int pd)
	{
		py = 0;
		pm = 0;
		pd = 0;
		if (s.Length() < 10)
			return false;
		py = s.Substring(0, 4).ToInt();
		pm = s.Substring(5, 2).ToInt();
		pd = s.Substring(8, 2).ToInt();
		if (py <= 0 || pm <= 0 || pd <= 0)
			return false;
		return true;
	}

	// Devuelve la fila del VIP. La fecha de ingreso viene de vip.json. Resetea los
	// usos si cambio el ciclo mensual O si cambio la fecha de ingreso (re-ingreso /
	// edicion del admin = ciclo nuevo).
	ExorVipStateRow EnsureAndRefresh(string sid)
	{
		EnsureLoaded();
		int y, m, d;
		GetYearMonthDay(y, m, d);

		// Fecha de ingreso desde vip.json (si no parsea, hoy)
		int jy = y;
		int jm = m;
		int jd = d;
		int py, pm, pd;
		if (ParseFecha(GetExorConfig().vip.FechaIngreso(sid), py, pm, pd))
		{
			jy = py;
			jm = pm;
			jd = pd;
		}

		ExorVipStateRow r;
		if (!m_Rows.Find(sid, r))
		{
			r = new ExorVipStateRow();
			r.steamid = sid;
			r.anc_y = jy;
			r.anc_m = jm;
			r.anc_d = jd;
			r.last_ciclo = CycleIndex(jy, jm, jd, y, m, d);
			r.usos = 0;
			m_Rows.Set(sid, r);
			Save();
			Print(string.Format("%1 VIP %2: estado creado (ingreso %3-%4-%5)", ExorStorageConstants.LOG, sid, jy, jm, jd));
			return r;
		}

		// Cambio la fecha de ingreso -> re-anclar y resetear.
		if (r.anc_y != jy || r.anc_m != jm || r.anc_d != jd)
		{
			r.anc_y = jy;
			r.anc_m = jm;
			r.anc_d = jd;
			r.last_ciclo = CycleIndex(jy, jm, jd, y, m, d);
			r.usos = 0;
			Save();
			return r;
		}

		// Misma fecha: resetear si paso a otro ciclo mensual.
		int idx = CycleIndex(jy, jm, jd, y, m, d);
		if (idx != r.last_ciclo)
		{
			r.last_ciclo = idx;
			r.usos = 0;
			Save();
		}
		return r;
	}

	int RemainingUses(string sid)
	{
		int max = GetExorConfig().vip.UsosPorMes(sid);
		if (max <= 0)
			return 0;
		ExorVipStateRow r = EnsureAndRefresh(sid);
		int rem = max - r.usos;
		if (rem < 0)
			rem = 0;
		return rem;
	}

	bool ConsumeUse(string sid)
	{
		if (RemainingUses(sid) <= 0)
			return false;
		ExorVipStateRow r = EnsureAndRefresh(sid);
		r.usos = r.usos + 1;
		Save();
		return true;
	}

	// ------------------------- aplicar el loadout VIP -------------------------
	// Reemplaza la ropa del jugador por la del loadout y mete los items extra en
	// el cargo de la camisa/pantalon. Server-side.
	static void ApplyLoadout(PlayerBase player)
	{
		if (!GetGame() || !GetGame().IsServer() || !player)
			return;
		ExorCfgVipLoadout lo = GetExorConfig().vip.equip_loadout;
		if (!lo)
			return;

		// 1) sacar la ropa actual (reemplazar): borrar los attachments directos
		int ac = player.GetInventory().AttachmentCount();
		int i;
		for (i = ac - 1; i >= 0; i--)
		{
			EntityAI att = player.GetInventory().GetAttachmentFromIndex(i);
			if (att)
				GetGame().ObjectDelete(att);
		}

		// 2) equipar el loadout (con los slots libres, CreateInInventory los viste)
		EntityAI camisa = ExorCreatePiece(player, lo.camisa);
		EntityAI pantalon = ExorCreatePiece(player, lo.pantalon);
		ExorCreatePiece(player, lo.zapato);
		ExorCreatePiece(player, lo.bolso);

		// 3) items extra: al cargo de camisa, si no de pantalon, si no donde haya
		if (lo.items_extra)
		{
			int j;
			for (j = 0; j < lo.items_extra.Count(); j++)
			{
				string extra = lo.items_extra.Get(j);
				if (extra == "")
					continue;
				EntityAI placed = null;
				if (camisa && camisa.GetInventory())
					placed = camisa.GetInventory().CreateInInventory(extra);
				if (!placed && pantalon && pantalon.GetInventory())
					placed = pantalon.GetInventory().CreateInInventory(extra);
				if (!placed)
					player.GetInventory().CreateInInventory(extra);
			}
		}
	}

	static EntityAI ExorCreatePiece(PlayerBase player, string type)
	{
		if (type == "")
			return null;
		return player.GetInventory().CreateInInventory(type);
	}
}
