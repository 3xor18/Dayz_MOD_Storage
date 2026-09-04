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
	int x;          // metros CUANTIZADOS (int) en vez de float JSON largo ("6415.29980..") -> ~40% menos bytes
	int y;
	int z;
	int health;     // 0..100 CUANTIZADO (el cliente lo pasa a 0..1); antes float 0..1
	// is_self ELIMINADO: el cliente lo deriva comparando nid con su propio player -> el server
	// manda 1 solo payload por grupo (sin copia por receptor) y sin este campo en cada miembro.
	int nid_low;    // network ID del player (para que el cliente resuelva su ENTIDAD real,
	int nid_high;   // use su posicion VIVA/interpolada en la burbuja, y derive is_self)
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
	// Ultimo payload de posiciones enviado por grupo. Si el nuevo es IDENTICO no se
	// reenvia: las posiciones van cuantizadas a metros, asi que un grupo quieto (AFK, en
	// base, adentro de un edificio) genera el mismo JSON tick tras tick y se estaba
	// mandando igual por red a cada miembro, 1 vez por segundo.
	ref map<string, string> m_LastLivePayload;
	ref map<string, int> m_LastLiveForceMs;	// groupId -> ultimo reenvio forzado
	static const int LIVE_FORCE_MS = 10000;	// cada 10s se manda igual, aunque no haya cambiado

	static const int MARK_TTL_MS = 600000;	// 10 min: a los 10 min la marca se borra y el contador se reinicia

	void ExorPartyLive()
	{
		m_MarkersByGroup = new map<string, ref ExorMarkersDTO>;
		m_MarkCounter = new map<string, int>;
		m_MarkLastMs = new map<string, int>;
		m_LastLivePayload = new map<string, string>;
		m_LastLiveForceMs = new map<string, int>;
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
		// 1000 ms (1 Hz): la fluidez del nombre del compañero CERCANO ya NO depende de este
		// rate -> el cliente usa la ENTIDAD real (interpolada por el motor) cuando esta en la
		// burbuja (ver ExorNameplates + nid). Este sync solo alimenta a los LEJANOS (HUD/
		// distancia), donde 1 Hz sobra. Bajado de 4 Hz (250 ms) a 1 Hz: a 60 jugadores el push
		// era el mayor emisor de red (O(jugadores*party)) y de CPU (serializacion) del server.
		// El latido de 1 Hz lo da ExorTick1Hz (uno solo para party/KOTH/cofre): asi la lista
		// de jugadores y la hora local se calculan una vez y no tres.
		ExorTick1Hz.Start();
	}

	// Envoltorio cronometrado: mide cuanto tarda ESTE subsistema por tick y solo loguea
	// si se pasa del umbral. Se agenda este en vez de Tick() directo. Ver ExorPerfMonitor.
	void TickTimed()
	{
		int t = ExorPerfMonitor.Now();
		Tick();
		ExorPerfMonitor.Fin("party-live", t);
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

	// PURGA: el cache "ultimo payload enviado" se indexa por id de grupo y NO se limpiaba al
	// disolverse un grupo. Cada entrada es el JSON completo del roster (varios KB), asi que
	// los clanes borrados se quedaban ocupando memoria hasta el reinicio. Se tira todo lo que
	// ya no corresponde a un grupo vivo. La llama ExorHousekeeping.
	void PurgarViejos()
	{
		if (!m_LastLivePayload)
			return;
		array<ref ExorGroup> vivos = ExorGroupManager.Get().m_Groups;
		set<string> idsVivos = new set<string>();
		int i;
		for (i = 0; i < vivos.Count(); i++)
		{
			if (vivos.Get(i))
				idsVivos.Insert(vivos.Get(i).id);
		}
		array<string> muertos = new array<string>;
		foreach (string gid, string payload : m_LastLivePayload)
		{
			if (idsVivos.Find(gid) < 0)
				muertos.Insert(gid);
		}
		for (i = 0; i < muertos.Count(); i++)
		{
			m_LastLivePayload.Remove(muertos.Get(i));
			if (m_LastLiveForceMs)
				m_LastLiveForceMs.Remove(muertos.Get(i));
		}
	}

	void PushGroup(ExorGroup g, map<string, PlayerBase> idx)
	{
		// recolectar miembros online con su pos/vida (ref: si no, se liberan antes de usarlos)
		ExorLiveDTO dto = new ExorLiveDTO();
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
			lm.x = Math.Round(p[0]); lm.y = Math.Round(p[1]); lm.z = Math.Round(p[2]);	// cuantizado a metros
			lm.health = Math.Round(pb.GetHealth01("", "") * 100);	// 0..1 -> 0..100 (menos bytes)
			// network ID del player -> el cliente resuelve su entidad real (si esta en la
			// burbuja) y usa la pos VIVA/interpolada por el motor = nombre sin lag al correr.
			// Ademas el cliente compara este nid con el de SU propio player para saber
			// "soy yo" (is_self) local -> el server ya NO serializa una copia por receptor.
			int nlo, nhi;
			pb.GetNetworkID(nlo, nhi);
			lm.nid_low = nlo;
			lm.nid_high = nhi;
			dto.members.Insert(lm);
		}

		// EARLY-EXIT: grupo sin NADIE online -> no hay a quien mandarle nada. Sin esto se
		// serializaba igual un DTO vacio a JSON y se recorrian los miembros de nuevo para no
		// enviar a nadie. Con ~61 grupos registrados y solo una parte con gente conectada,
		// eran decenas de serializaciones por segundo tiradas a la basura.
		if (dto.members.Count() == 0)
			return;

		// OPTIMIZACION (perf a 60 jugadores): serializar el grupo UNA sola vez y mandar los
		// MISMOS bytes a todos los miembros online. Antes se armaba una copia del DTO por
		// receptor (para marcar is_self) y se re-serializaba a JSON por receptor -> CPU
		// O(miembros^2) por grupo por tick + presion de GC. Ahora is_self lo deriva el
		// cliente comparando nid (ver ExorNameplates/ExorPartyHud) -> O(miembros).
		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(dto, false, data);

		// SIN CAMBIOS -> no reenviar. El cliente ya tiene exactamente estos bytes; mandarlos
		// de nuevo solo gasta ancho de banda y CPU de chunking. Un grupo entero quieto pasa
		// de N envios por segundo a 0.
		// SALVO cada LIVE_FORCE_MS: reenvio de seguridad para que un cliente que se perdio
		// el ultimo update (reconecto, se le cayo un chunk) se ponga al dia solo, sin
		// depender de que alguien del grupo se mueva.
		int nowLive = GetGame().GetTime();
		int lastForce = 0;
		m_LastLiveForceMs.Find(g.id, lastForce);
		bool forzar = (nowLive - lastForce) >= LIVE_FORCE_MS;

		string prevPayload = "";
		if (!forzar && m_LastLivePayload.Find(g.id, prevPayload) && prevPayload == data)
			return;
		m_LastLivePayload.Set(g.id, data);
		if (forzar)
			m_LastLiveForceMs.Set(g.id, nowLive);

		// CHUNKING (como ROSTER/MARKER): con 5+ miembros el JSON supera ~2KB y el
		// motor del cliente CORROMPE el string al leerlo de un RPC de un solo param
		// -> el parse falla -> el cliente se quedaba con los ultimos 4 validos ("no
		// veo mas de 4 en la party"). Partir en trozos lo evita sin importar cuantos
		// miembros ni el largo de los nombres.
		for (i = 0; i < g.members.Count(); i++)
		{
			PlayerBase rcv;
			if (!idx.Find(g.members.Get(i).steamid, rcv) || !rcv || !rcv.GetIdentity())
				continue;
			ExorNetChunk.Send(rcv, rcv.GetIdentity(), ExorRPC.MEMBER_SYNC, data);
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
		ExorNetChunk.Send(p, p.GetIdentity(), ExorRPC.MEMBER_SYNC, d1);

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
		// Borra TODAS las marcas del party (las tuyas Y las de todos los miembros). Las marcas
		// son compartidas por grupo, asi que cualquier miembro puede limpiar el tablero entero.
		ExorMarkersDTO m;
		if (m_MarkersByGroup.Find(g.id, m))
			m.markers.Clear();
		BroadcastMarkers(g);
		player.MessageImportant("Marcas del party limpiadas.");
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
