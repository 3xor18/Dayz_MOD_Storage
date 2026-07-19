// ============================================================================
// 3xorStorage - Reglas de muebles ligadas al TERRITORIO (todo EVENT-TIME).
// ----------------------------------------------------------------------------
// NADA corre por-frame ni periodicamente: los chequeos se disparan SOLO en
// eventos puntuales (colocar mueble, abrir/lotear, poner mastil, quitar mastil)
// -> costo cero a 50-60 players. Los conteos usan los registros globales de
// ExorVO_Manager (m_Openables / m_Barrels), no escaneo espacial.
//
// Config en storage.json (ExorCfgStorage):
//   setear_muebles_solo_cerca_mastil, cantidad_maxima_muebles_por_base,
//   solo_miembros_lotean_muebles, offset_horas, horario_looteo_libre[].
// ============================================================================
class ExorMuebleRules
{
	// -------- mensaje ROJO al chat del mod (kind=2) --------
	static void SendRed(PlayerBase p, string text)
	{
		if (!p || !p.GetIdentity() || !GetGame().IsServer() || text == "")
			return;
		ExorChatMsg m = new ExorChatMsg();
		m.name = "";
		m.text = text;
		m.channel = 0;
		m.dur = 8;
		m.max = 3;
		m.kind = 2;      // 2 = ROJO (error/aviso)
		m.chars = 60;
		m.maxlin = 3;
		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(m, false, data);
		Param1<string> pr = new Param1<string>(data);
		p.RPCSingleParam(ExorRPC.CHAT_MSG, pr, true, p.GetIdentity());
	}

	// -------- conteo de muebles (openables) dentro de 'radius' de 'center' --------
	static int CountMueblesNear(vector center, float radius, Object exclude)
	{
		ExorVO_Manager vo = ExorVO_Manager.Get();
		if (!vo || !vo.m_Openables)
			return 0;
		int n = 0;
		int i;
		for (i = 0; i < vo.m_Openables.Count(); i++)
		{
			Exor_OpenableStorage f = vo.m_Openables.Get(i);
			if (!f || f == exclude)
				continue;
			if (ExorTerritoryRules.Dist2D(center, f.GetPosition()) <= radius)
				n++;
		}
		return n;
	}

	static bool HasMueblesNear(vector center, float radius)
	{
		return CountMueblesNear(center, radius, null) > 0;
	}

	// -------- horario: estamos en una ventana de looteo LIBRE (ej raid)? --------
	static bool IsLootFreeNow()
	{
		ExorCfgStorage s = GetExorConfig().storage;
		if (!s || !s.horario_looteo_libre || s.horario_looteo_libre.Count() == 0)
			return false;
		int minOfDay, weekday, dayKey;
		ExorCofre.NowLocal(s.offset_horas, minOfDay, weekday, dayKey);
		int i;
		for (i = 0; i < s.horario_looteo_libre.Count(); i++)
		{
			ExorHorarioLibre h = s.horario_looteo_libre.Get(i);
			if (!h)
				continue;
			if (!DayMatches(h.dia, weekday))
				continue;
			int desde = ParseHHMM(h.desde);
			int hasta = ParseHHMM(h.hasta);
			if (desde < 0 || hasta < 0)
				continue;
			if (hasta >= desde)
			{
				if (minOfDay >= desde && minOfDay <= hasta)
					return true;
			}
			else	// la ventana cruza medianoche (ej 22:00 -> 02:00)
			{
				if (minOfDay >= desde || minOfDay <= hasta)
					return true;
			}
		}
		return false;
	}

	static bool DayMatches(string dia, int weekday)
	{
		string d = dia;
		d.ToLower();
		if (d == "" || d == "todos")
			return true;
		return DayIndex(d) == weekday;
	}

	// 0=domingo .. 6=sabado (igual convencion que ExorCofre.NowLocal)
	static int DayIndex(string d)
	{
		if (d == "domingo") return 0;
		if (d == "lunes") return 1;
		if (d == "martes") return 2;
		if (d == "miercoles" || d == "miércoles") return 3;
		if (d == "jueves") return 4;
		if (d == "viernes") return 5;
		if (d == "sabado" || d == "sábado") return 6;
		return -1;
	}

	// "HH:MM" -> minuto del dia (0..1439); -1 si invalido
	static int ParseHHMM(string s)
	{
		if (s == "")
			return -1;
		int colon = s.IndexOf(":");
		if (colon < 1)
			return -1;
		int hh = s.Substring(0, colon).ToInt();
		int mm = s.Substring(colon + 1, s.Length() - colon - 1).ToInt();
		if (hh < 0 || hh > 23 || mm < 0 || mm > 59)
			return -1;
		return hh * 60 + mm;
	}

	// centro de la base del player (su mastil guardado) o null-vector si no tiene
	static bool BaseCenter(PlayerBase player, out vector center)
	{
		center = "0 0 0";
		ExorGroup g = ExorGroupManager.Get().FindByPlayer(ExorGroupManager.SteamId(player));
		if (!g || (g.mast_x == 0 && g.mast_z == 0))
			return false;
		center = Vector(g.mast_x, 0, g.mast_z);
		return true;
	}

	// -------- puede 'player' COLOCAR un mueble en 'pos'? (reason = msg rojo) --------
	static bool CanPlaceMueble(PlayerBase player, vector pos, out string reason)
	{
		reason = "";
		ExorCfgStorage s = GetExorConfig().storage;
		if (!s || !s.setear_muebles_solo_cerca_mastil)
			return true;	// feature off -> sin restriccion
		if (!player)
			return true;

		ExorTerritoryManager tm = ExorTerritoryManager.Get();
		if (!tm || !tm.IsInOwnGroupTerritory(player, pos))
		{
			reason = "Solo podés colocar muebles dentro del radio del mástil de tu base.";
			return false;
		}
		if (s.cantidad_maxima_muebles_por_base > 0)
		{
			vector center;
			if (!BaseCenter(player, center))
				center = pos;
			int count = CountMueblesNear(center, ExorTerritoryRules.Radius(), null);
			if (count >= s.cantidad_maxima_muebles_por_base)
			{
				reason = string.Format("Alcanzaste el máximo de muebles en tu base (%1).", s.cantidad_maxima_muebles_por_base);
				return false;
			}
		}
		return true;
	}

	// -------- puede 'player' LOOTEAR (abrir) el mueble 'fur'? --------
	static bool CanLootMueble(PlayerBase player, Exor_OpenableStorage fur, out string reason)
	{
		reason = "";
		ExorCfgStorage s = GetExorConfig().storage;
		if (!s || !s.solo_miembros_lotean_muebles || !player || !fur)
			return true;
		// STAFF whitelist: estos steamids SIEMPRE pueden lotear (ignora miembro + horario)
		if (s.bypass_lootear_steamids && s.bypass_lootear_steamids.Find(ExorGroupManager.SteamId(player)) != -1)
			return true;
		if (IsLootFreeNow())
			return true;	// horario de raid -> cualquiera lootea

		vector pos = fur.GetPosition();
		ExorTerritoryManager tm = ExorTerritoryManager.Get();
		if (tm && tm.IsInOwnGroupTerritory(player, pos))
			return true;	// sos miembro de esta base
		if (tm && tm.FindEnemyTerritoryAt(player, pos))
		{
			reason = "Solo los miembros del territorio pueden lotear estos muebles.";
			return false;	// base ajena y no sos miembro
		}
		return true;	// mueble fuera de todo territorio -> sin dueño -> permitido
	}

	// -------- al PONER un mastil nuevo en 'pos': hay mas muebles que el maximo cerca? --------
	// (evita fundar/reconstruir base donde ya hay demasiado storage)
	static bool CanPlaceMast(vector pos, out string reason)
	{
		reason = "";
		ExorCfgStorage s = GetExorConfig().storage;
		if (!s || s.cantidad_maxima_muebles_por_base <= 0)
			return true;
		int count = CountMueblesNear(pos, ExorTerritoryRules.Radius(), null);
		if (count > s.cantidad_maxima_muebles_por_base)
		{
			reason = string.Format("No podés poner el mástil acá: hay %1 muebles cerca (máximo %2). Sacá algunos primero.", count, s.cantidad_maxima_muebles_por_base);
			return false;
		}
		return true;
	}

	// -------- se puede QUITAR/disolver el territorio con mastil en 'pos'? --------
	// (bloquea si quedan muebles seteados cerca -> hay que sacarlos primero)
	static bool CanRemoveMast(vector pos, out string reason)
	{
		reason = "";
		int count = CountMueblesNear(pos, ExorTerritoryRules.Radius(), null);
		if (count > 0)
		{
			reason = string.Format("No podés eliminar el territorio: quedan %1 muebles cerca. Sacalos todos primero.", count);
			return false;
		}
		return true;
	}
}
