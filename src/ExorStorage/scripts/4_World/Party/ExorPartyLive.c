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

	void ExorPartyLive()
	{
		m_MarkersByGroup = new map<string, ref ExorMarkersDTO>;
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
		// 250 ms (antes 1000): a 1 Hz el nombre/HUD del compañero se quedaba quieto
		// y "saltaba" al moverse. A 4 Hz sigue fluido. Carga de red despreciable
		// (party chico, MEMBER_SYNC va sin confirmacion).
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Get().Tick, 250, true);
	}

	void Tick()
	{
		// Respetar config: si el party esta off o no se comparte posicion, no empujar nada
		ExorCfgPartyGrupo cg = GetExorConfig().party.grupo;
		if (!cg.habilitado || !cg.mostrar_posicion_miembros)
			return;

		array<ref ExorGroup> groups = ExorGroupManager.Get().m_Groups;
		int gi;
		for (gi = 0; gi < groups.Count(); gi++)
		{
			ExorGroup g = groups.Get(gi);
			PushGroup(g);
		}
	}

	void PushGroup(ExorGroup g)
	{
		// recolectar miembros online con su pos/vida (ref: si no, se liberan antes de usarlos)
		array<ref ExorLiveMember> live = new array<ref ExorLiveMember>;
		int i;
		for (i = 0; i < g.members.Count(); i++)
		{
			PlayerBase pb = ExorGroupManager.Get().FindOnline(g.members.Get(i).steamid);
			if (!pb)
				continue;
			ExorLiveMember lm = new ExorLiveMember();
			lm.steamid = g.members.Get(i).steamid;
			lm.name = g.members.Get(i).name;
			vector p = pb.GetPosition();
			lm.x = p[0]; lm.y = p[1]; lm.z = p[2];
			lm.health = pb.GetHealth01("", "");
			live.Insert(lm);
		}

		// enviar a cada miembro online (con is_self marcado para ese receptor)
		for (i = 0; i < g.members.Count(); i++)
		{
			PlayerBase rcv = ExorGroupManager.Get().FindOnline(g.members.Get(i).steamid);
			if (!rcv || !rcv.GetIdentity())
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
		p.RPCSingleParam(ExorRPC.MARKER_SYNC, new Param1<string>(d2), true, p.GetIdentity());
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
		string pname = ExorGroupManager.PlayerName(player);
		// numero: nombre-1, nombre-2, ... (cuantas marcas ya tiene este jugador)
		int num = 1;
		int n;
		for (n = 0; n < m.markers.Count(); n++)
		{
			if (m.markers.Get(n).label.IndexOf(pname + "-") == 0)
				num++;
		}

		ExorMarker mk = new ExorMarker();
		mk.label = pname + "-" + num.ToString();
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
		string pname = ExorGroupManager.PlayerName(player);
		ExorMarkersDTO m;
		if (m_MarkersByGroup.Find(g.id, m))
		{
			int i;
			for (i = m.markers.Count() - 1; i >= 0; i--)
			{
				if (m.markers.Get(i).label.IndexOf(pname + "-") == 0)
					m.markers.Remove(i);
			}
		}
		BroadcastMarkers(g);
		player.MessageImportant("Tus marcas limpiadas.");
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
				rcv.RPCSingleParam(ExorRPC.MARKER_SYNC, new Param1<string>(data), true, rcv.GetIdentity());
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
