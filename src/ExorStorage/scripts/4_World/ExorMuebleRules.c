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
	// Mensaje al CHAT del mod, verde (kind=1). Para info (no error).
	static void SendChat(PlayerBase p, string text)
	{
		SendMsg(p, text, 1);
	}

	static void SendVerde(PlayerBase p, string text)
	{
		SendMsg(p, text, 1);	// 1 = verde (info)
	}

	static void SendRed(PlayerBase p, string text)
	{
		SendMsg(p, text, 2);
	}

	// EMPACAR/sacar un objeto de la base: solo MIEMBROS del territorio o STAFF (bypass).
	// NO usa el horario libre (a diferencia de lootear): en raid igual no te lo pueden sacar.
	static bool CanPackAtPos(PlayerBase player, vector pos, out string reason)
	{
		reason = "";
		if (!player)
			return false;
		ExorCfgStorage s = GetExorConfig().storage;
		if (s && s.bypass_lootear_steamids && s.bypass_lootear_steamids.Find(ExorGroupManager.SteamId(player)) != -1)
			return true;	// staff
		ExorTerritoryManager tm = ExorTerritoryManager.Get();
		if (tm && tm.IsInOwnGroupTerritory(player, pos))
			return true;	// miembro del clan de esta base
		reason = "Solo los miembros del clan pueden sacar el parking.";
		return false;
	}

	static void SendMsg(PlayerBase p, string text, int kind)
	{
		if (!p || !p.GetIdentity() || !GetGame().IsServer() || text == "")
			return;
		ExorChatMsg m = new ExorChatMsg();
		m.name = "";
		m.text = text;
		m.channel = 0;
		m.dur = 8;
		m.max = 3;
		m.kind = kind;   // 1 = verde (info), 2 = rojo (error)
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

	// -------- conteo de BARRILES dentro de 'radius' de 'center' --------
	// Aparte de los muebles a proposito: los barriles tienen su propio cupo por base
	// (cantidad_maxima_barriles_por_base). Recorre el registro vivo del manager, no el
	// mundo: es un array en memoria, sin scan de objetos ni consultas al motor.
	static int CountBarrilesNear(vector center, float radius, Object exclude)
	{
		ExorVO_Manager vo = ExorVO_Manager.Get();
		if (!vo || !vo.m_Barrels)
			return 0;
		float r2 = radius * radius;
		int n = 0;
		int i;
		for (i = 0; i < vo.m_Barrels.Count(); i++)
		{
			Exor_Barrel_Base b = vo.m_Barrels.Get(i);
			if (!b || b == exclude)
				continue;
			// los barriles ATADOS (slot de un auto, dentro de un cargo) no ocupan lugar en la
			// base: no estan puestos en el piso del territorio.
			if (b.GetHierarchyParent())
				continue;
			if (ExorMath.Dist2DSq(center, b.GetPosition()) <= r2)
				n++;
		}
		return n;
	}

	// -------- puede 'player' COLOCAR un barril en 'pos'? (reason = msg rojo) --------
	// Misma politica que los muebles pero con sus propios parametros: el barril se setea
	// solo dentro del radio del mastil de tu base y con su propio tope por base.
	static bool CanPlaceBarril(PlayerBase player, vector pos, out string reason)
	{
		reason = "";
		ExorCfgStorage s = GetExorConfig().storage;
		if (!s || !player)
			return true;

		if (s.setear_barriles_solo_cerca_mastil)
		{
			ExorTerritoryManager tm = ExorTerritoryManager.Get();
			if (!tm || !tm.IsInOwnGroupTerritory(player, pos))
			{
				reason = "Solo podés setear barriles dentro del radio del mástil de tu base.";
				return false;
			}
		}

		if (s.cantidad_maxima_barriles_por_base > 0)
		{
			vector center;
			if (!BaseCenter(player, center))
				center = pos;
			int count = CountBarrilesNear(center, ExorTerritoryRules.Radius(), null);
			if (count >= s.cantidad_maxima_barriles_por_base)
			{
				reason = string.Format("Alcanzaste el máximo de barriles en tu base (%1).", s.cantidad_maxima_barriles_por_base);
				return false;
			}
		}
		return true;
	}

	static bool HasMueblesNear(vector center, float radius)
	{
		return CountMueblesNear(center, radius, null) > 0;
	}

	// -------- horario: estamos en una ventana de looteo LIBRE (ej raid)? --------
	// Pasa por ExorLootWindow, que memoiza el resultado por MINUTO: el calculo de abajo
	// parsea strings ("22:00" -> minutos) y compara dias, y lo llaman cada apertura de
	// mueble, cada tick del manager y cada pase del self-heal. El resultado solo puede
	// cambiar cuando cambia el minuto del reloj.
	static bool IsLootFreeNow()
	{
		return ExorLootWindow.IsRaidNow();
	}

	// Calculo real (sin cache). No llamarlo directo: usar IsLootFreeNow().
	static bool CalcularLootFree()
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

	// ========================================================================
	//  TECHO DE CONTENEDORES "REALES" POR BASE
	// ========================================================================
	// EL PROBLEMA.  El limite que ya existia (cantidad_maxima_muebles_por_base) acota
	// cuantos muebles hay, no cuantos estan ABIERTOS. Y lo que le cuesta al server no es el
	// mueble: es su contenido de-virtualizado. Un locker lleno son cientos de entidades que
	// el motor simula y replica a todos los clientes cercanos. En horario de raid el clan
	// abre TODO lo suyo a la vez y ahi es donde el server se cae o se arrastra.
	//
	// LA REGLA.  La base puede tener los muebles que quiera, pero solo N pueden estar
	// reales al mismo tiempo. Al abrir el N+1, el que hace mas rato que nadie usa se cierra
	// y se guarda solo. Es un techo DURO al pico de entidades por base, y no le saca nada
	// al jugador: lo que se guarda vuelve entero al abrirlo.
	//
	// POR QUE SE EXPULSA EN VEZ DE BLOQUEAR.  Bloquear en medio de un raid es una funcion
	// rota desde el punto de vista del jugador ("no me deja abrir mi propio locker").
	// Cerrar el mas viejo hace lo mismo para el server y es invisible en la practica: si
	// nadie lo estaba mirando hace rato, cerrarlo no interrumpe nada.
	//
	// COSTO.  Un recorrido de los registros vivos del manager (arrays en memoria, sin scan
	// del mundo) y SOLO cuando alguien abre algo. Cero costo por frame.
	// No devuelve nada a proposito: el contenedor SIEMPRE se abre. Esto solo hace lugar
	// antes. Trabarle el locker al jugador seria peor que un pico de entidades, y ademas
	// dejaria el techo dependiendo de que la expulsion nunca falle.
	static void HacerLugarParaAbrir(PlayerBase quien, Object nuevo, vector pos)
	{
		ExorCfgStorage s = GetExorConfig().storage;
		if (!s || s.maximo_contenedores_reales_por_base <= 0)
			return;

		ExorVO_Manager vo = ExorVO_Manager.Get();
		if (!vo)
			return;

		// El radio de la base sale del territorio. Si el modulo de territorio esta apagado
		// (radio 0) el techo igual tiene que servir, asi que se cae a un radio fijo: sin esto
		// no habria candidatos y la proteccion no existiria justo en los servers sin party.
		float radio = ExorTerritoryRules.Radius();
		if (radio <= 0)
			radio = 60.0;
		float r2 = radio * radio;

		// candidatos = contenedores REALES de esta base, con su ultimo uso y si estan abiertos
		array<Object> cand = new array<Object>;
		array<int> usoMs = new array<int>;
		array<int> abierto = new array<int>;
		int i;

		if (vo.m_Openables)
		{
			for (i = 0; i < vo.m_Openables.Count(); i++)
			{
				Exor_OpenableStorage f = vo.m_Openables.Get(i);
				if (!f || f == nuevo || !f.ExorEsReal())
					continue;
				if (ExorMath.Dist2DSq(pos, f.GetPosition()) > r2)
					continue;
				cand.Insert(f);
				usoMs.Insert(f.ExorUltimoUsoMs());
				if (f.IsOpen())
					abierto.Insert(1);
				else
					abierto.Insert(0);
			}
		}
		if (vo.m_Barrels)
		{
			for (i = 0; i < vo.m_Barrels.Count(); i++)
			{
				Exor_Barrel_Base b = vo.m_Barrels.Get(i);
				if (!b || b == nuevo || !b.ExorEsReal())
					continue;
				if (b.GetHierarchyParent())		// barril atado a un auto: no es de la base
					continue;
				if (ExorMath.Dist2DSq(pos, b.GetPosition()) > r2)
					continue;
				cand.Insert(b);
				usoMs.Insert(b.ExorUltimoUsoMs());
				if (b.IsOpen())
					abierto.Insert(1);
				else
					abierto.Insert(0);
			}
		}

		int limite = s.maximo_contenedores_reales_por_base;
		if (cand.Count() < limite)
			return;

		// hay que liberar hasta dejar lugar para el que se esta por abrir
		int liberar = cand.Count() - limite + 1;
		int liberados = 0;
		int vuelta;
		for (vuelta = 0; vuelta < liberar; vuelta++)
		{
			// ELEGIR VICTIMA: primero los CERRADOS (nadie los esta usando) y, dentro de cada
			// grupo, el que hace mas rato que nadie toca. Con pocos candidatos, buscar el
			// minimo es mas simple -y mas facil de leer- que ordenar la lista.
			int mejor = -1;
			int mejorUso = 0;
			int mejorAbierto = 2;
			for (i = 0; i < cand.Count(); i++)
			{
				if (!cand.Get(i))
					continue;
				if (abierto.Get(i) > mejorAbierto)
					continue;
				if (abierto.Get(i) < mejorAbierto || mejor < 0 || usoMs.Get(i) < mejorUso)
				{
					mejor = i;
					mejorUso = usoMs.Get(i);
					mejorAbierto = abierto.Get(i);
				}
			}
			if (mejor < 0)
				break;

			Object victima = cand.Get(mejor);
			cand.Set(mejor, null);		// no volver a elegirla

			Exor_OpenableStorage fv = Exor_OpenableStorage.Cast(victima);
			if (fv && fv.ExorForzarVirtualizar())
				liberados++;
			else
			{
				Exor_Barrel_Base bv = Exor_Barrel_Base.Cast(victima);
				if (bv && bv.ExorForzarVirtualizar())
					liberados++;
			}
		}

		if (liberados > 0)
		{
			Print(string.Format("%1 CUPO: la base en %2 llego a %3 contenedores abiertos (max %4) -> %5 guardado(s) automaticamente",
				ExorStorageConstants.LOG, pos.ToString(), cand.Count(), limite, liberados));
			SendVerde(quien, string.Format("Tu base ya tenia %1 contenedores abiertos: se guardo el mas viejo para no lagear el server.", limite));
		}
	}

	// -------- puede 'player' LOOTEAR (abrir) el mueble 'fur'? --------
	// Misma regla que CanLootMueble pero por POSICION (para objetos que no son
	// Exor_OpenableStorage, ej. el parking): solo miembros salvo en horario libre.
	static bool CanLootAtPos(PlayerBase player, vector pos, out string reason)
	{
		reason = "";
		ExorCfgStorage s = GetExorConfig().storage;
		if (!s || !s.solo_miembros_lotean_muebles || !player)
			return true;
		if (s.bypass_lootear_steamids && s.bypass_lootear_steamids.Find(ExorGroupManager.SteamId(player)) != -1)
			return true;
		if (IsLootFreeNow())
			return true;	// horario libre -> cualquiera
		ExorTerritoryManager tm = ExorTerritoryManager.Get();
		if (tm && tm.IsInOwnGroupTerritory(player, pos))
			return true;	// miembro de esta base
		if (tm && tm.FindEnemyTerritoryAt(player, pos))
		{
			reason = "Solo los miembros del clan pueden usar el parking.";
			return false;
		}
		return true;	// fuera de todo territorio
	}

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
