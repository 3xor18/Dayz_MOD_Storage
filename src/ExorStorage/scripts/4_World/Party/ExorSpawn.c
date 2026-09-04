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
	bool equip_enabled;           // mostrar "Spawn en base + Equipamiento" (VIP + loadout + base)
	int equip_remaining;          // usos de equipamiento restantes en el ciclo
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

	// PURGA: mismo caso que el chat. Las entradas solo sirven para cooldowns de minutos;
	// lo mas viejo que 'ttlMs' no lo va a consultar nadie. La llama ExorHousekeeping.
	static void PurgarViejos(int now, int ttlMs)
	{
		int i;
		if (s_LastBaseMs)
		{
			array<string> m1 = new array<string>;
			foreach (string k1, int v1 : s_LastBaseMs)
			{
				if (now - v1 > ttlMs)
					m1.Insert(k1);
			}
			for (i = 0; i < m1.Count(); i++)
				s_LastBaseMs.Remove(m1.Get(i));
		}
		if (s_LastPointMs)
		{
			array<string> m2 = new array<string>;
			foreach (string k2, int v2 : s_LastPointMs)
			{
				if (now - v2 > ttlMs)
					m2.Insert(k2);
			}
			for (i = 0; i < m2.Count(); i++)
				s_LastPointMs.Remove(m2.Get(i));
		}
	}
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
			// COMPATIBILIDAD DE MAPA: un punto de spawn que quedo fuera del terreno (config de
			// otro mapa) tiraria al jugador al vacio. Se descarta con aviso en el log y se cae
			// al spawn vanilla si no queda ninguno valido. Ver ExorMapBounds.
			if (!ExorMapBounds.Valida("spawns.json", pt.nombre, pt.x, pt.z, 50))
				continue;
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

		// Opcion VIP "Spawn en base + Equipamiento": requiere ser VIP, tener loadout
		// configurado y que la base se pueda mostrar. La disponibilidad final (base
		// arriba/sin cd + usos > 0) la combina el cliente.
		dto.equip_enabled = false;
		dto.equip_remaining = 0;
		ExorCfgVip vipcfg = GetExorConfig().vip;
		if (vipcfg.equip_habilitado && vipcfg.IsVip(sidBase) && vipcfg.TieneLoadout() && dto.base_enabled)
		{
			dto.equip_enabled = true;
			dto.equip_remaining = ExorVipState.Get().RemainingUses(sidBase);
		}

		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(dto, false, data);
		ExorNetChunk.Send(player, player.GetIdentity(), ExorRPC.SPAWN_OPEN, data);
	}

	// ------------------------- entrega garantizada del menu de spawn -------------------------
	// El menu se mandaba UNA sola vez, 5s despues de OnClientNewEvent. Si a los 5s el cliente
	// todavia estaba cargando (server cargado / conexion lenta), el RPC se perdia y el jugador
	// se quedaba SIN pantalla de spawn (reportado el 19-jul con 43+ jugadores). Ahora se
	// reintenta con espaciado creciente hasta que el jugador elige (ApplyPick lo cancela) o
	// se agotan los intentos.
	static ref map<string, int> s_OpenTries;	// steamid -> intentos hechos
	static const int OPEN_MAX_TRIES = 4;
	static const int OPEN_RETRY_MS  = 7000;

	// Reenvia el menu si el jugador sigue sin elegir. Se re-agenda sola.
	static void RetryOpen(PlayerBase player)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (!player || !player.GetIdentity())
			return;	// se fue: nada que reintentar
		if (!s_OpenTries)
			s_OpenTries = new map<string, int>;

		string sid = player.GetIdentity().GetPlainId();
		int tries = 0;
		if (!s_OpenTries.Find(sid, tries))
			return;	// ya eligio (ApplyPick borro la entrada) -> cortar la cadena

		if (tries >= OPEN_MAX_TRIES)
		{
			Print(string.Format("%1 SPAWN_OPEN: %2 no confirmo eleccion tras %3 intentos -> se deja de reintentar", ExorStorageConstants.LOG, sid, tries));
			s_OpenTries.Remove(sid);
			return;
		}

		s_OpenTries.Set(sid, tries + 1);
		SendOpen(player);
		Print(string.Format("%1 SPAWN_OPEN: reintento %2 para %3 (sigue sin elegir)", ExorStorageConstants.LOG, tries + 1, sid));
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(RetryOpen, OPEN_RETRY_MS, false, player);
	}

	// Primer envio + arranque de la cadena de reintentos.
	static void SendOpenTracked(PlayerBase player)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (!player || !player.GetIdentity())
			return;
		if (!s_OpenTries)
			s_OpenTries = new map<string, int>;
		s_OpenTries.Set(player.GetIdentity().GetPlainId(), 1);
		SendOpen(player);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(RetryOpen, OPEN_RETRY_MS, false, player);
	}

	// El jugador eligio (index >=0 = punto; -1 = base). Teleporta si corresponde.
	static void ApplyPick(PlayerBase player, int index)
	{
		if (!GetGame().IsServer() || !player || !player.GetIdentity())
			return;
		Ensure();
		string sid = player.GetIdentity().GetPlainId();
		// eligio -> cortar los reintentos del menu
		if (s_OpenTries)
			s_OpenTries.Remove(sid);
		vector pos = vector.Zero;

		if (index == -2)
		{
			// Spawn en base + Equipamiento VIP (gasta 1 uso del ciclo).
			ExorCfgVip vip = GetExorConfig().vip;
			if (!vip.equip_habilitado || !vip.IsVip(sid) || !vip.TieneLoadout())
			{
				player.MessageImportant("El equipamiento VIP no está disponible.");
				return;
			}
			if (ExorVipState.Get().RemainingUses(sid) <= 0)
			{
				player.MessageImportant("No te quedan usos de equipamiento VIP (avisá al admin para renovar).");
				return;
			}
			pos = ChooseBase(sid, player);	// respeta bandera + cooldown (y consume cd de base)
			if (pos == vector.Zero)
			{
				player.MessageImportant("No podés aparecer en tu base ahora (bandera abajo o en cooldown).");
				return;
			}
			player.SetPosition(pos);
			ExorVipState.Get().ConsumeUse(sid);
			ExorVipState.ApplyLoadout(player);
			int rem = ExorVipState.Get().RemainingUses(sid);
			player.MessageImportant(string.Format("Apareciste en tu base con equipamiento VIP. Usos restantes: %1", rem));
			return;
		}

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
