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
	// true  = ademas de guardar los datos, ABRI el hub (es el envio de siempre, al aparecer)
	// false = solo datos. El cliente los necesita ANTES de morir, porque la pantalla de
	//         muerte arma su lista de zonas con este cache y estando muerto ya no se le
	//         puede mandar nada. Se le manda al conectar y despues de cada aparicion.
	bool abrir;
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

	// steamid -> ya eligio zona en ESTA vida (la eligio en la pantalla de muerte y la mando
	// apenas revivio). Sirve para no abrirle ademas el hub viejo. Se limpia al crearse un
	// personaje nuevo.
	static ref map<string, bool> s_YaEligio;

	static void LimpiarEleccion(string sid)
	{
		if (!s_YaEligio)
			s_YaEligio = new map<string, bool>;
		s_YaEligio.Remove(sid);
	}

	static bool YaEligio(string sid)
	{
		if (!s_YaEligio)
			return false;
		bool v;
		return s_YaEligio.Find(sid, v) && v;
	}

	// ------------------------- ventana para aceptar el pick de spawn -------------------------
	// steamid -> momento (ms del server) en que ese jugador estreno personaje. ApplyPick MUEVE
	// al jugador, asi que solo se acepta dentro de esta ventana, o sea cuando de verdad acaba
	// de nacer o de reaparecer tras morir. Sin esto, cualquier SPAWN_PICK que llegue movia a un
	// jugador vivo: el que se reconectaba aparecia en el spawn con todas sus cosas (9-sep-2026),
	// y un cliente modificado tenia un teletransporte gratis a cualquier punto.
	// La ventana se abre SOLO en OnClientNewEvent (personaje nuevo = primer login o respawn por
	// muerte). Reconectar con el personaje de siempre pasa por OnClientReadyEvent y no la abre.
	static ref map<string, int> s_VentanaPickMs;
	static const int PICK_VENTANA_MS = 300000;	// 5 min: el hub reintenta ~30s y el jugador puede tardar en elegir

	static void AbrirVentanaPick(string sid)
	{
		if (!s_VentanaPickMs)
			s_VentanaPickMs = new map<string, int>;
		s_VentanaPickMs.Set(sid, GetGame().GetTime());
	}

	static void CerrarVentanaPick(string sid)
	{
		if (s_VentanaPickMs)
			s_VentanaPickMs.Remove(sid);
	}

	static bool VentanaPickAbierta(string sid)
	{
		if (!s_VentanaPickMs)
			return false;
		int desde;
		if (!s_VentanaPickMs.Find(sid, desde))
			return false;
		return GetGame().GetTime() - desde <= PICK_VENTANA_MS;
	}

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
		Ensure();
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
		// Ensure() ANTES que nada: esto ahora tambien se llama desde CreateCharacter (primer
		// login), que corre antes que cualquier otro camino, y ahi los mapas de cooldown
		// todavia no existian -> "NULL pointer to instance" y el jugador se quedaba con el
		// spawn vanilla.
		Ensure();
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
	// Manda SOLO los datos (no abre nada). Es lo que llena el cache que despues usa la
	// pantalla de muerte para ofrecer zonas y el interruptor VIP.
	static void SendDatos(PlayerBase player)
	{
		SendOpen(player, false);
	}

	static void SendOpen(PlayerBase player, bool abrir = true)
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

		dto.abrir = abrir;

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
	static int GeneroDeTipo(string tipo)
	{
		tipo.ToLower();
		if (tipo.IndexOf("survivorf_") == 0)
			return 1;
		return 0;
	}

	static int GeneroDe(PlayerBase player)
	{
		if (!player)
			return 0;
		return GeneroDeTipo(player.GetType());
	}

	// El jugador eligio (index >=0 = punto; -1 = base). 'equip' = pidio aparecer con el
	// equipamiento VIP (el interruptor de la pantalla). El sexo NO se elige aca: se elige
	// en la pantalla de muerte, antes de que exista el personaje (ver ExorJugadorSpawn).
	static void ApplyPick(PlayerBase player, int index, bool equip)
	{
		if (!GetGame().IsServer() || !player || !player.GetIdentity())
			return;
		Ensure();
		string sid = player.GetIdentity().GetPlainId();

		// CANDADO: solo se mueve a alguien que acaba de estrenar personaje. Fuera de esa
		// ventana el pick se descarta sin tocar la posicion (ver VentanaPickAbierta).
		if (!VentanaPickAbierta(sid))
		{
			Print(string.Format("%1 SPAWN: se descarta el pick de %2 (indice=%3): no viene de un respawn", ExorStorageConstants.LOG, sid, index));
			return;
		}

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

		player.SetPosition(pos);

		// Ya aparecio donde queria: no hace falta ofrecerle el hub de nuevo en esta vida.
		if (!s_YaEligio)
			s_YaEligio = new map<string, bool>;
		s_YaEligio.Set(sid, true);
		// Un solo traslado por vida: se cierra la ventana recien cuando el pick SALIO BIEN
		// (los rechazos de arriba cortan antes, asi que el jugador puede reintentar).
		CerrarVentanaPick(sid);

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
