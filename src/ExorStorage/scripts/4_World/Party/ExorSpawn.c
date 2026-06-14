// ============================================================================
// 3xor_Vanilla_Optimization - Sistema de spawn (Fase F, SOLO server)
// - Primer login: manda a un punto de spawns.json (respeta cooldown por punto).
// - Respawn (muerte): si respawn_base esta habilitado y el jugador tiene party
//   con mastil, la bandera esta izada (o bajada_bloquea_respawn=false) y paso el
//   cooldown -> aparece en su base. Si no, va a un punto de spawn.
// Los cooldowns son en memoria (se reinician al reiniciar el server) -> VERIFICAR.
// NOTA: la PANTALLA de seleccion de punto necesita UI (keybind/menu); por ahora
// se elige automaticamente un punto valido. Queda como mejora in-game.
// ============================================================================
// DTO que el server manda al cliente para armar la pantalla de seleccion.
class ExorSpawnMenuDTO
{
	ref TStringArray nombres;
	bool base_enabled;
	void ExorSpawnMenuDTO() { nombres = new TStringArray; }
}

// Cache cliente con la ultima lista recibida (la lee el menu).
class ExorSpawnClient
{
	static ref ExorSpawnMenuDTO s_DTO;
	static void Set(ExorSpawnMenuDTO d) { s_DTO = d; }
}

class ExorSpawn
{
	static ref map<string, int> s_LastBaseMs;   // steamid -> ms del ultimo spawn en base
	static ref map<string, int> s_LastPointMs;  // "steamid|idx" -> ms del ultimo uso del punto
	static ref map<string, bool> s_NeedSelect;  // steamid -> debe mostrarse la pantalla de spawn

	static void Ensure()
	{
		if (!s_LastBaseMs)
			s_LastBaseMs = new map<string, int>;
		if (!s_LastPointMs)
			s_LastPointMs = new map<string, int>;
		if (!s_NeedSelect)
			s_NeedSelect = new map<string, bool>;
	}

	// Marca que este jugador (recien creado/muerto) debe ver la pantalla de spawn.
	static void MarkNeedsSelect(string sid)
	{
		Ensure();
		s_NeedSelect.Set(sid, true);
	}

	// Devuelve true UNA vez si estaba marcado (y lo limpia).
	static bool ConsumeNeedsSelect(string sid)
	{
		Ensure();
		bool v;
		if (s_NeedSelect.Find(sid, v) && v)
		{
			s_NeedSelect.Remove(sid);
			return true;
		}
		return false;
	}

	// Devuelve la posicion de spawn elegida, o vector.Zero para usar el default vanilla.
	static vector ChooseSpawn(string steamid, PlayerBase player, bool firstLogin)
	{
		Ensure();

		if (!firstLogin && player)
		{
			vector basePos = ChooseBase(steamid, player);
			if (basePos != vector.Zero)
				return basePos;
		}

		return ChoosePoint(steamid);
	}

	// Spawn en la base (mastil) si corresponde; zero si no.
	static vector ChooseBase(string steamid, PlayerBase player)
	{
		ExorCfgPartyRespawnBase cfg = GetExorConfig().party.respawn_base;
		if (!cfg.habilitado)
			return vector.Zero;

		ExorGroup g = ExorGroupManager.Get().FindByPlayer(steamid);
		if (!g)
			return vector.Zero;

		TerritoryFlag mast = ExorTerritoryManager.Get().FindMastByGroup(g.id);
		if (!mast)
			return vector.Zero;

		// Bandera abajo bloquea respawn (si esta configurado)
		if (GetExorConfig().party.bandera.bajada_bloquea_respawn && !mast.ExorIsFlagRaised())
			return vector.Zero;

		// Cooldown
		int now = GetGame().GetTime();
		int last;
		if (s_LastBaseMs.Find(steamid, last))
		{
			int cdMs = cfg.cooldown_segundos * 1000;
			if (cdMs > 0 && now - last < cdMs)
				return vector.Zero;	// en cooldown: no spawnea en base
		}
		s_LastBaseMs.Set(steamid, now);

		// Apenas al lado del mastil
		vector p = mast.GetPosition();
		p[0] = p[0] + 1.5;
		return p;
	}

	// Elige un punto de spawns.json que no este en cooldown (aleatorio).
	static vector ChoosePoint(string steamid)
	{
		ExorCfgSpawns spawns = GetExorConfig().spawns;
		if (!spawns.habilitado || spawns.puntos.Count() == 0)
			return vector.Zero;	// sin puntos: default vanilla

		int now = GetGame().GetTime();
		array<int> elegibles = new array<int>;
		int i;
		for (i = 0; i < spawns.puntos.Count(); i++)
		{
			ExorSpawnPunto pt = spawns.puntos.Get(i);
			int last;
			string key = string.Format("%1|%2", steamid, i);
			if (s_LastPointMs.Find(key, last))
			{
				int cdMs = pt.cooldown_segundos * 1000;
				if (cdMs > 0 && now - last < cdMs)
					continue;	// en cooldown
			}
			elegibles.Insert(i);
		}
		if (elegibles.Count() == 0)
			return vector.Zero;

		int idx = elegibles.Get(Math.RandomInt(0, elegibles.Count()));
		ExorSpawnPunto chosen = spawns.puntos.Get(idx);
		s_LastPointMs.Set(string.Format("%1|%2", steamid, idx), now);

		return PuntoToPos(chosen);
	}

	// Posicion final de un punto: aplica el offset aleatorio (distancia_random) + suelo.
	static vector PuntoToPos(ExorSpawnPunto pt)
	{
		vector pos = Vector(pt.x, pt.y, pt.z);
		if (pt.distancia_random > 0)
		{
			pos[0] = pos[0] + Math.RandomFloat(-pt.distancia_random, pt.distancia_random);
			pos[2] = pos[2] + Math.RandomFloat(-pt.distancia_random, pt.distancia_random);
		}
		pos[1] = GetGame().SurfaceY(pos[0], pos[2]);
		return pos;
	}

	// ------------------------- pantalla de seleccion (Fase F UI) -------------------------
	// Manda al cliente la lista para abrir el menu de spawn.
	static void SendOpen(PlayerBase player)
	{
		if (!GetGame().IsServer() || !player || !player.GetIdentity())
			return;
		ExorCfgSpawns spawns = GetExorConfig().spawns;
		if (!spawns.habilitado || spawns.puntos.Count() == 0)
			return;	// nada para elegir -> queda el spawn vanilla

		ExorSpawnMenuDTO dto = new ExorSpawnMenuDTO();
		int i;
		for (i = 0; i < spawns.puntos.Count(); i++)
			dto.nombres.Insert(spawns.puntos.Get(i).nombre);

		// opcion "mi base" si esta habilitada y el jugador tiene mastil
		dto.base_enabled = false;
		if (GetExorConfig().party.respawn_base.habilitado)
		{
			ExorGroup g = ExorGroupManager.Get().FindByPlayer(ExorGroupManager.SteamId(player));
			if (g && ExorTerritoryManager.Get().FindMastByGroup(g.id))
				dto.base_enabled = true;
		}

		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(dto, false, data);
		player.RPCSingleParam(ExorRPC.SPAWN_OPEN, new Param1<string>(data), true, player.GetIdentity());
	}

	// El jugador eligio (index >=0 = punto; -1 = base). Teleporta si corresponde.
	static void ApplyPick(PlayerBase player, int index)
	{
		if (!GetGame().IsServer() || !player || !player.GetIdentity())
			return;
		Ensure();
		string sid = player.GetIdentity().GetPlainId();
		vector pos = vector.Zero;

		if (index < 0)
		{
			pos = ChooseBase(sid, player);	// respeta bandera + cooldown
			if (pos == vector.Zero)
			{
				player.MessageImportant("No podés aparecer en tu base ahora (bandera abajo o en cooldown).");
				return;
			}
		}
		else
		{
			ExorCfgSpawns spawns = GetExorConfig().spawns;
			if (index >= spawns.puntos.Count())
				return;
			ExorSpawnPunto pt = spawns.puntos.Get(index);

			int now = GetGame().GetTime();
			string key = string.Format("%1|%2", sid, index);
			int last;
			if (s_LastPointMs.Find(key, last))
			{
				int cd = pt.cooldown_segundos * 1000;
				if (cd > 0 && now - last < cd)
				{
					player.MessageImportant("Ese punto está en cooldown.");
					return;
				}
			}
			s_LastPointMs.Set(key, now);

			pos = PuntoToPos(pt);
		}

		if (pos != vector.Zero)
		{
			player.SetPosition(pos);
			player.MessageImportant("Apareciste en el punto elegido.");
		}
	}
}
