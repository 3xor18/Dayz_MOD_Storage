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
// PUENTE a la mision. MissionServer vive en 5_Mission y desde 4_World no se ve, pero
// para dejar a un personaje recien creado como un freshie vanilla hace falta su
// StartingEquipSetup (vendaje + chemlight + fruta). La implementacion real la registra
// ExorStorage_Mission.c al arrancar; si no hay nadie registrado, no pasa nada malo: el
// personaje igual viene con la ropa por default de su tipo.
class ExorMissionBridge
{
	static ref ExorMissionBridge s_Inst;
	void EquiparFreshie(PlayerBase player) { }

	static void Freshie(PlayerBase player)
	{
		if (s_Inst && player)
			s_Inst.EquiparFreshie(player);
	}
}

// DTO que el server manda al cliente para armar la pantalla de seleccion.
class ExorSpawnMenuDTO
{
	ref TStringArray nombres;
	ref array<int> punto_cd_seg;  // por punto: segundos restantes de cooldown (0 = disponible)
	// INDICE REAL en spawns.json de cada entrada de la lista.
	// La lista que ve el jugador va FILTRADA (los puntos solo_vip no se le mandan si no es
	// VIP), asi que la posicion en la lista YA NO coincide con la posicion en la config.
	// Sin este mapeo, elegir el 2do boton spawnearia en el punto equivocado. El cliente
	// devuelve el indice REAL, y el server igual lo vuelve a validar (ver ApplyPick).
	ref array<int> punto_idx;
	bool base_enabled;            // mostrar el boton "Mi base" (permitido + tiene mastil)
	int base_cd_seg;              // segundos restantes de cooldown de base (0 = disponible)
	bool base_flag_down;          // bandera abajo y eso bloquea el respawn en base
	// Equipamiento VIP: ya NO es un boton de "spawn en base + equipo", es un INTERRUPTOR
	// de la pantalla de spawn (el VIP lo prende si quiere y despues elige el punto que
	// sea). Por eso no depende de la base: sirve con la base apagada en party.json.
	bool equip_enabled;           // mostrar el interruptor (es VIP + equip_habilitado + su pack viste algo)
	int equip_remaining;          // usos de equipamiento que le quedan (se muestran en el boton)
	string equip_pack;            // nombre del pack que le toca (para mostrarlo en el boton)
	// Hombre/mujer: es para TODOS, no solo VIP (spawns.json -> elegir_genero).
	bool genero_enabled;          // mostrar los dos botones
	int genero_actual;            // 0 = hombre, 1 = mujer (el sexo del personaje que tiene AHORA)
	void ExorSpawnMenuDTO()
	{
		nombres = new TStringArray;
		punto_cd_seg = new array<int>;
		punto_idx = new array<int>;
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
		bool esVip = GetExorConfig().vip.IsVip(steamid);
		array<int> elegibles = new array<int>;
		int i;
		for (i = 0; i < spawns.puntos.Count(); i++)
		{
			ExorSpawnPunto pt = spawns.puntos.Get(i);
			// punto exclusivo VIP: el reparto automatico no se lo da a un no-VIP
			if (pt.solo_vip && !esVip)
				continue;
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

		// Los puntos SOLO-VIP no se le mandan al que no es VIP: no los ve en la lista.
		// Se filtra en el SERVER y no en el cliente a proposito -mandarle el punto y pedirle
		// que lo esconda es contarle donde esta y confiar en que no lo use-.
		bool esVipMenu = GetExorConfig().vip.IsVip(sidBase);
		ExorSpawnMenuDTO dto = new ExorSpawnMenuDTO();
		int i;
		for (i = 0; i < spawns.puntos.Count(); i++)
		{
			ExorSpawnPunto ptm = spawns.puntos.Get(i);
			if (ptm.solo_vip && !esVipMenu)
				continue;
			dto.nombres.Insert(ptm.nombre);
			dto.punto_cd_seg.Insert(PointCdRemainingSec(sidBase, i));	// 0 = disponible
			dto.punto_idx.Insert(i);	// indice REAL en spawns.json (la lista va filtrada)
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

		// Interruptor "Equipamiento VIP": requiere ser VIP y que su pack (equip_loadouts[]
		// de vip.json) vista algo. NO depende de la base: el VIP lo prende y aparece con
		// el equipo en el punto que elija. El cliente lo muestra apagado por default y
		// con los usos que le quedan.
		dto.equip_enabled = false;
		dto.equip_remaining = 0;
		dto.equip_pack = "";
		ExorCfgVip vipcfg = GetExorConfig().vip;
		if (vipcfg.equip_habilitado && vipcfg.IsVip(sidBase) && vipcfg.TieneLoadout(sidBase))
		{
			dto.equip_enabled = true;
			dto.equip_remaining = ExorVipState.Get().RemainingUses(sidBase);
			dto.equip_pack = vipcfg.NombrePack(sidBase);
		}

		// Hombre/mujer (para todos). Se manda el sexo que tiene AHORA para pintar el
		// boton que corresponde ya seleccionado.
		dto.genero_enabled = spawns.elegir_genero;
		dto.genero_actual = GeneroDe(player);

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

	// ------------------------- hombre / mujer -------------------------
	// El sexo se deduce del TIPO del personaje: los femeninos vanilla son "SurvivorF_*".
	// 0 = hombre, 1 = mujer.
	static int GeneroDe(PlayerBase player)
	{
		if (!player)
			return 0;
		string tipo = player.GetType();
		tipo.ToLower();
		if (tipo.IndexOf("survivorf_") == 0)
			return 1;
		return 0;
	}

	// Cambia el sexo del personaje CREANDO uno nuevo del otro sexo en 'pos' y pasandole
	// el control (SelectPlayer). No hay API de "cambiar sexo": la unica forma es
	// reemplazar la entidad. Se hace SOLO en el spawn -donde el personaje es un freshie
	// y no hay inventario que perder- y solo si el sexo pedido es distinto al que tiene.
	// Devuelve el personaje nuevo, o null si no se pudo (en ese caso se sigue con el viejo).
	static PlayerBase CambiarSexo(PlayerBase viejo, vector pos, bool mujer)
	{
		if (!viejo || !viejo.GetIdentity())
			return null;
		string tipo = GetExorConfig().spawns.TipoAlAzar(mujer);
		if (tipo == "")
		{
			Print(string.Format("%1 SPAWN: no hay tipos de personaje usables para el sexo pedido (spawns.json)", ExorStorageConstants.LOG));
			return null;
		}

		PlayerIdentity id = viejo.GetIdentity();
		string sid = id.GetPlainId();
		PlayerBase nuevo = PlayerBase.Cast(GetGame().CreatePlayer(id, tipo, pos, 0, "NONE"));
		if (!nuevo)
		{
			Print(string.Format("%1 SPAWN: CreatePlayer fallo para '%2' (%3)", ExorStorageConstants.LOG, tipo, sid));
			return null;
		}

		GetGame().SelectPlayer(id, nuevo);

		// Equipo de freshie: el personaje nuevo viene con la ropa por default de su tipo,
		// pero sin el vendaje/chemlight/fruta que reparte la mision. Se le corre el mismo
		// StartingEquipSetup (via el puente: MissionServer no se ve desde 4_World).
		ExorMissionBridge.Freshie(nuevo);

		// el cuerpo viejo ya no lo maneja nadie -> se va (si no, queda parado en el mapa)
		GetGame().ObjectDelete(viejo);

		Print(string.Format("%1 SPAWN: %2 cambio de sexo -> %3", ExorStorageConstants.LOG, sid, tipo));
		return nuevo;
	}

	// El jugador eligio (index >=0 = punto; -1 = base). 'equip' = pidio aparecer con el
	// equipamiento VIP (el interruptor de la pantalla). 'genero' = 0 hombre / 1 mujer /
	// -1 no tocar; solo se cambia si es distinto al que tiene.
	static void ApplyPick(PlayerBase player, int index, bool equip, int genero)
	{
		if (!GetGame().IsServer() || !player || !player.GetIdentity())
			return;
		Ensure();
		string sid = player.GetIdentity().GetPlainId();
		// eligio -> cortar los reintentos del menu
		if (s_OpenTries)
			s_OpenTries.Remove(sid);
		vector pos = vector.Zero;

		// GATE VIP AUTORITATIVO del equipamiento. El interruptor del cliente es cortesia
		// visual: esto es lo que de verdad decide, porque el flag llega por RPC y un
		// cliente modificado puede mandar lo que quiera. Si no califica, el spawn SIGUE
		// (aparece igual, sin equipo) y se le avisa por que.
		ExorCfgVipLoadout pack = null;
		if (equip)
		{
			ExorCfgVip vip = GetExorConfig().vip;
			if (!vip.equip_habilitado || !vip.IsVip(sid) || !vip.TieneLoadout(sid))
			{
				ExorAviso.Enviar(player, "El equipamiento VIP no está disponible.");
				equip = false;
			}
			else if (ExorVipState.Get().RemainingUses(sid) <= 0)
			{
				ExorAviso.Enviar(player, "No te quedan usos de equipamiento VIP (avisá al admin para renovar).");
				equip = false;
			}
			else
			{
				pack = vip.PackFor(sid);
			}
		}

		if (index < 0)
		{
			pos = ChooseBase(sid, player);	// respeta bandera + cooldown
			if (pos == vector.Zero)
			{
				ExorAviso.Enviar(player, "No podés aparecer en tu base ahora (bandera abajo o en cooldown).");
				return;
			}
		}
		else
		{
			ExorCfgSpawns spawns = GetExorConfig().spawns;
			if (index >= spawns.puntos.Count())
				return;
			ExorSpawnPunto pt = spawns.puntos.Get(index);

			// GATE VIP AUTORITATIVO. El boton bloqueado del cliente es cortesia visual; esto
			// es lo que de verdad impide usar un punto VIP, porque el indice llega por RPC y
			// un cliente modificado puede mandar cualquiera.
			if (pt.solo_vip && !GetExorConfig().vip.IsVip(sid))
			{
				ExorAviso.Enviar(player, "Ese punto de aparición es solo para VIP.");
				Print(string.Format("%1 SPAWN: %2 intento usar el punto VIP '%3' sin serlo", ExorStorageConstants.LOG, sid, pt.nombre));
				return;
			}

			int now = GetGame().GetTime();
			string key = string.Format("%1|%2", sid, index);
			int last;
			if (s_LastPointMs.Find(key, last))
			{
				int cd = pt.cooldown_segundos * 1000;
				if (cd > 0 && now - last < cd)
				{
					ExorAviso.Enviar(player, "Ese punto está en cooldown.");
					return;
				}
			}
			s_LastPointMs.Set(key, now);

			pos = PuntoToPos(pt);
		}

		if (pos == vector.Zero)
			return;

		// Hombre/mujer: si pidio el otro sexo, el personaje se REEMPLAZA por uno nuevo
		// creado ya en 'pos' (por eso no hace falta el SetPosition en ese caso). Si algo
		// falla, se sigue con el personaje que tenia: el spawn nunca se pierde.
		bool cambio = false;
		if (genero >= 0 && GetExorConfig().spawns.elegir_genero && genero != GeneroDe(player))
		{
			PlayerBase nuevo = CambiarSexo(player, pos, genero == 1);
			if (nuevo)
			{
				player = nuevo;
				cambio = true;
			}
			else
				ExorAviso.Enviar(player, "No se pudo cambiar el personaje; apareciste con el que tenías.");
		}
		if (!cambio)
			player.SetPosition(pos);

		// Equipamiento VIP: recien ACA se gasta el uso (ya esta puesto en el mundo).
		if (equip && pack)
		{
			ExorVipState.Get().ConsumeUse(sid);
			ExorVipState.ApplyLoadout(player, pack);
			int rem = ExorVipState.Get().RemainingUses(sid);
			ExorAviso.Enviar(player, string.Format("Apareciste con el equipamiento VIP (%1). Usos restantes: %2", pack.nombre, rem));
			return;
		}

		ExorAviso.Enviar(player, "Apareciste en el punto elegido.");
	}
}
