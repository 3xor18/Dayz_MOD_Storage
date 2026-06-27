// ============================================================================
// 3xor_Vanilla_Optimization - Party en vivo (Fase E): sync de posicion/vida de
// los miembros + marcas. El server empuja cada ~2 s a cada miembro la lista de
// su grupo (los jugadores lejanos no estan replicados, por eso se empuja data
// liviana). El cliente lo usa para el HUD (nombre + vida + distancia) y las
// marcas. Las marcas son efimeras (no se persisten).
// ============================================================================

class ExorLiveMember
{
	string steamid;
	string name;
	float x;
	float y;
	float z;
	float health;   // 0..1
	bool is_self;   // si es el propio jugador que recibe (el HUD lo saltea)
	int nid_low;    // network ID del player (para que el cliente resuelva su ENTIDAD real
	int nid_high;   // y use su posicion VIVA/interpolada cuando esta en la burbuja = sin lag)
}

class ExorLiveDTO
{
	ref array<ref ExorLiveMember> members;
	void ExorLiveDTO() { members = new array<ref ExorLiveMember>; }
}

class ExorMarker
{
	string label;
	float x;
	float y;
	float z;
	string owner;	// steamid del que la puso (server-only, para borrar con la Y)
	int placed_ms;	// uptime ms en que se puso (server-only, para expirar a los 10 min)
}

class ExorMarkersDTO
{
	ref array<ref ExorMarker> markers;
	void ExorMarkersDTO() { markers = new array<ref ExorMarker>; }
}

// ===========================================================================
// SERVER
// ===========================================================================
class ExorPartyLive
{
	static ref ExorPartyLive s_Instance;
	ref map<string, ref ExorMarkersDTO> m_MarkersByGroup;	// groupId -> marcas
	ref map<string, int> m_MarkCounter;	// groupId -> contador correlativo (1,2,3...)
	ref map<string, int> m_MarkLastMs;	// groupId -> uptime ms de la ultima marca
	int m_LastSweepMs;	// ultima vez que se barrieron marcas vencidas

	static const int MARK_TTL_MS = 600000;	// 10 min: a los 10 min la marca se borra y el contador se reinicia

	void ExorPartyLive()
	{
		m_MarkersByGroup = new map<string, ref ExorMarkersDTO>;
		m_MarkCounter = new map<string, int>;
		m_MarkLastMs = new map<string, int>;
	}

	static ExorPartyLive Get()
	{
		if (!s_Instance)
			s_Instance = new ExorPartyLive();
		return s_Instance;
	}

	static void Start()
	{
		if (!GetGame().IsServer())
			return;
		// 250 ms (4 Hz): la fluidez del nombre del compañero CERCANO ya NO depende de este
		// rate -> el cliente usa la ENTIDAD real (interpolada por el motor) cuando esta en la
		// burbuja (ver ExorNameplates + nid). Este sync solo alimenta a los LEJANOS (HUD/
		// distancia), donde 4 Hz sobra. Antes era 8 Hz (125 ms) para tapar el lag del nombre.
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Get().Tick, 250, true);
	}

	void Tick()
	{
		// Barrer marcas vencidas (>10 min) cada ~5s, independiente de la config de posicion
		int nowMs = GetGame().GetTime();
		if (nowMs - m_LastSweepMs > 5000)
		{
			m_LastSweepMs = nowMs;
			SweepExpiredMarkers(nowMs);
		}

		// Respetar config: si el party esta off o no se comparte posicion, no empujar nada
		ExorCfgPartyGrupo cg = GetExorConfig().party.grupo;
		if (!cg.habilitado || !cg.mostrar_posicion_miembros)
			return;

		// indice online armado UNA vez por tick (antes PushGroup llamaba FindOnline ->
		// GetPlayers() 2 veces por miembro por grupo = O(grupos*miembros*players) a 4 Hz).
		map<string, PlayerBase> idx = new map<string, PlayerBase>;
		ExorGroupManager.Get().BuildOnlineIndex(idx);

		array<ref ExorGroup> groups = ExorGroupManager.Get().m_Groups;
		int gi;
		for (gi = 0; gi < groups.Count(); gi++)
		{
			ExorGroup g = groups.Get(gi);
			PushGroup(g, idx);
		}
	}

	void PushGroup(ExorGroup g, map<string, PlayerBase> idx)
	{
		// recolectar miembros online con su pos/vida (ref: si no, se liberan antes de usarlos)
		array<ref ExorLiveMember> live = new array<ref ExorLiveMember>;
		int i;
		for (i = 0; i < g.members.Count(); i++)
		{
			PlayerBase pb;
			if (!idx.Find(g.members.Get(i).steamid, pb) || !pb)
				continue;
			ExorLiveMember lm = new ExorLiveMember();
			lm.steamid = g.members.Get(i).steamid;
			lm.name = g.members.Get(i).name;
			vector p = pb.GetPosition();
			lm.x = p[0]; lm.y = p[1]; lm.z = p[2];
			lm.health = pb.GetHealth01("", "");
			// network ID del player -> el cliente resuelve su entidad real (si esta en la
			// burbuja) y usa la pos VIVA/interpolada por el motor = nombre sin lag al correr.
			int nlo, nhi;
			pb.GetNetworkID(nlo, nhi);
			lm.nid_low = nlo;
			lm.nid_high = nhi;
			live.Insert(lm);
		}

		// enviar a cada miembro online (con is_self marcado para ese receptor)
		for (i = 0; i < g.members.Count(); i++)
		{
			PlayerBase rcv;
			if (!idx.Find(g.members.Get(i).steamid, rcv) || !rcv || !rcv.GetIdentity())
				continue;
			string rsid = g.members.Get(i).steamid;

			ExorLiveDTO dto = new ExorLiveDTO();
			int j;
			for (j = 0; j < live.Count(); j++)
			{
				ExorLiveMember src = live.Get(j);
				ExorLiveMember cp = new ExorLiveMember();
				cp.steamid = src.steamid;
				cp.name = src.name;
				cp.x = src.x; cp.y = src.y; cp.z = src.z;
				cp.health = src.health;
				cp.nid_low = src.nid_low;
				cp.nid_high = src.nid_high;
				cp.is_self = (src.steamid == rsid);
				dto.members.Insert(cp);
			}

			JsonSerializer js = new JsonSerializer();
			string data;
			js.WriteToString(dto, false, data);
			rcv.RPCSingleParam(ExorRPC.MEMBER_SYNC, new Param1<string>(data), false, rcv.GetIdentity());
		}
	}

	// Vacia el HUD/nameplates/marcas de un jugador que dejo de estar en el party
	// (salio / fue expulsado / se disolvio). Sin esto el server simplemente deja
	// de empujarle data y el cliente conserva la ULTIMA recibida (seguia viendo a
	// los ex-compañeros y sus marcas hasta morir/reloguear). Mandamos un live y
	// unas marcas VACIAS (confiable) para que el cliente limpie.
	void ClearForPlayer(PlayerBase p)
	{
		if (!GetGame().IsServer() || !p || !p.GetIdentity())
			return;
		JsonSerializer js = new JsonSerializer();

		ExorLiveDTO emptyLive = new ExorLiveDTO();
		string d1;
		js.WriteToString(emptyLive, false, d1);
		p.RPCSingleParam(ExorRPC.MEMBER_SYNC, new Param1<string>(d1), true, p.GetIdentity());

		ExorMarkersDTO emptyMk = new ExorMarkersDTO();
		string d2;
		js.WriteToString(emptyMk, false, d2);
		ExorNetChunk.Send(p, p.GetIdentity(), ExorRPC.MARKER_SYNC, d2);
	}

	// ------------------------- marcas -------------------------
	void AddMarker(PlayerBase player, vector pos)
	{
		if (!player)
			return;
		ExorCfgPartyGrupo cg = GetExorConfig().party.grupo;
		if (!cg.habilitado || !cg.permitir_marker_equipo)
		{
			player.MessageImportant("Las marcas del party estan desactivadas.");
			return;
		}
		ExorGroup g = ExorGroupManager.Get().FindByPlayer(ExorGroupManager.SteamId(player));
		if (!g)
		{
			player.MessageImportant("No estás en ningún party.");
			return;
		}
		ExorMarkersDTO m;
		if (!m_MarkersByGroup.Find(g.id, m))
		{
			m = new ExorMarkersDTO();
			m_MarkersByGroup.Set(g.id, m);
		}
		// Etiqueta = contador CORRELATIVO del grupo (1,2,3,4...): sube en CADA marca de
		// CUALQUIER miembro y es compartido (BroadcastMarkers). Se reinicia a 0 si paso
		// mas de MARK_TTL_MS (10 min) sin marcar (a la par que se vencen las marcas).
		string sid = ExorGroupManager.SteamId(player);
		int nowMs = GetGame().GetTime();
		int last;
		if (!m_MarkLastMs.Find(g.id, last))
			last = 0;
		int cnt;
		if (!m_MarkCounter.Find(g.id, cnt))
			cnt = 0;
		if (last == 0 || nowMs - last > MARK_TTL_MS)
			cnt = 0;	// reinicio tras 10 min sin marcar
		cnt = cnt + 1;
		m_MarkCounter.Set(g.id, cnt);
		m_MarkLastMs.Set(g.id, nowMs);

		ExorMarker mk = new ExorMarker();
		mk.label = cnt.ToString();
		mk.owner = sid;
		mk.placed_ms = nowMs;
		mk.x = pos[0];
		mk.y = pos[1];
		mk.z = pos[2];
		m.markers.Insert(mk);
		BroadcastMarkers(g);
		player.MessageImportant("Marca puesta: " + mk.label);
	}

	void ClearMarkers(PlayerBase player)
	{
		if (!player)
			return;
		ExorGroup g = ExorGroupManager.Get().FindByPlayer(ExorGroupManager.SteamId(player));
		if (!g)
			return;
		// borra TUS marcas (las que pusiste vos, identificadas por steamid)
		string sid = ExorGroupManager.SteamId(player);
		ExorMarkersDTO m;
		if (m_MarkersByGroup.Find(g.id, m))
		{
			int i;
			for (i = m.markers.Count() - 1; i >= 0; i--)
			{
				if (m.markers.Get(i).owner == sid)
					m.markers.Remove(i);
			}
		}
		BroadcastMarkers(g);
		player.MessageImportant("Tus marcas limpiadas.");
	}

	// Borra las marcas con mas de MARK_TTL_MS (10 min) de antiguedad y re-broadcastea
	// al grupo que haya cambiado. Llamado periodicamente desde Tick.
	void SweepExpiredMarkers(int nowMs)
	{
		array<ref ExorGroup> groups = ExorGroupManager.Get().m_Groups;
		int gi;
		for (gi = 0; gi < groups.Count(); gi++)
		{
			ExorGroup g = groups.Get(gi);
			ExorMarkersDTO m;
			if (!m_MarkersByGroup.Find(g.id, m))
				continue;
			bool changed = false;
			int i;
			for (i = m.markers.Count() - 1; i >= 0; i--)
			{
				if (nowMs - m.markers.Get(i).placed_ms > MARK_TTL_MS)
				{
					m.markers.Remove(i);
					changed = true;
				}
			}
			if (changed)
				BroadcastMarkers(g);
		}
	}

	void BroadcastMarkers(ExorGroup g)
	{
		ExorMarkersDTO m;
		if (!m_MarkersByGroup.Find(g.id, m))
			m = new ExorMarkersDTO();

		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(m, false, data);

		int i;
		for (i = 0; i < g.members.Count(); i++)
		{
			PlayerBase rcv = ExorGroupManager.Get().FindOnline(g.members.Get(i).steamid);
			if (rcv && rcv.GetIdentity())
				ExorNetChunk.Send(rcv, rcv.GetIdentity(), ExorRPC.MARKER_SYNC, data);
		}
	}
}

// ===========================================================================
// CLIENTE
// ===========================================================================
class ExorPartyClient
{
	static ref ExorLiveDTO s_Live;
	static ref ExorMarkersDTO s_Markers;

	static void SetLive(ExorLiveDTO d) { s_Live = d; }
	static void SetMarkers(ExorMarkersDTO d) { s_Markers = d; }
}
