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
	ref array<int> punto_cd_seg;  // por punto: segundos restantes de cooldown (0 = disponible)
	bool base_enabled;            // mostrar el boton "Mi base" (permitido + tiene mastil)
	int base_cd_seg;              // segundos restantes de cooldown de base (0 = disponible)
	bool base_flag_down;          // bandera abajo y eso bloquea el respawn en base
	void ExorSpawnMenuDTO()
	{
		nombres = new TStringArray;
		punto_cd_seg = new array<int>;
	}
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

		// Gate VIP: dos toggles independientes (VIP / no-VIP) para spawnear en el mastil
		bool vip = GetExorConfig().vip.IsVip(steamid);
		if (vip && !cfg.permitir_spawn_mastil_vip)
			return vector.Zero;
		if (!vip && !cfg.permitir_spawn_mastil_no_vip)
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

	// ------------------------- cooldowns (peek, NO mutan) -------------------------
	// Segundos restantes de cooldown para spawnear en base (0 = disponible ya).
	static int BaseCdRemainingSec(string sid)
	{
		Ensure();
		ExorCfgPartyRespawnBase cfg = GetExorConfig().party.respawn_base;
		if (cfg.cooldown_segundos <= 0)
			return 0;
		int last;
		if (!s_LastBaseMs.Find(sid, last))
			return 0;
		int rem = (cfg.cooldown_segundos * 1000) - (GetGame().GetTime() - last);
		if (rem <= 0)
			return 0;
		return rem / 1000;
	}

	// Segundos restantes de cooldown del punto 'idx' para 'sid' (0 = disponible ya).
	static int PointCdRemainingSec(string sid, int idx)
	{
		Ensure();
		ExorCfgSpawns spawns = GetExorConfig().spawns;
		if (idx < 0 || idx >= spawns.puntos.Count())
			return 0;
		int cdSec = spawns.puntos.Get(idx).cooldown_segundos;
		if (cdSec <= 0)
			return 0;
		int last;
		string key = string.Format("%1|%2", sid, idx);
		if (!s_LastPointMs.Find(key, last))
			return 0;
		int rem = (cdSec * 1000) - (GetGame().GetTime() - last);
		if (rem <= 0)
			return 0;
		return rem / 1000;
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

		string sidBase = ExorGroupManager.SteamId(player);

		ExorSpawnMenuDTO dto = new ExorSpawnMenuDTO();
		int i;
		for (i = 0; i < spawns.puntos.Count(); i++)
		{
			dto.nombres.Insert(spawns.puntos.Get(i).nombre);
			dto.punto_cd_seg.Insert(PointCdRemainingSec(sidBase, i));	// 0 = disponible
		}

		// opcion "mi base" si esta habilitada, el gate VIP lo permite y el jugador tiene mastil
		dto.base_enabled = false;
		dto.base_cd_seg = 0;
		dto.base_flag_down = false;
		ExorCfgPartyRespawnBase rb = GetExorConfig().party.respawn_base;
		if (rb.habilitado)
		{
			bool vipBase = GetExorConfig().vip.IsVip(sidBase);
			bool permitido = false;
			if (vipBase && rb.permitir_spawn_mastil_vip)
				permitido = true;
			if (!vipBase && rb.permitir_spawn_mastil_no_vip)
				permitido = true;
			if (permitido)
			{
				ExorGroup g = ExorGroupManager.Get().FindByPlayer(sidBase);
				if (g)
				{
					TerritoryFlag mast = ExorTerritoryManager.Get().FindMastByGroup(g.id);
					if (mast)
					{
						dto.base_enabled = true;
						dto.base_cd_seg = BaseCdRemainingSec(sidBase);
						if (GetExorConfig().party.bandera.bajada_bloquea_respawn && !mast.ExorIsFlagRaised())
							dto.base_flag_down = true;
					}
				}
			}
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
