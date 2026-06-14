// ============================================================================
// 3xor_Vanilla_Optimization - Territorio (Fase C)
// Registro server de los mastiles vivos + sincronizacion de "zonas" a los
// clientes (para el preview de construccion) + regla central de si se puede
// construir/colocar algo en una posicion.
// Reglas (en orden): blacklist -> permitir_construir_cerca -> whitelist ->
// bloquear si cae en territorio AJENO dentro del radio.
// ============================================================================

// Zona que ve el cliente (una por mastil cercano)
class ExorTerritoryZone
{
	float x;
	float z;
	float radius;
	bool mine;   // el mastil es de mi propio party
}

class ExorTerritoryCacheDTO
{
	ref array<ref ExorTerritoryZone> zones;
	void ExorTerritoryCacheDTO()
	{
		zones = new array<ref ExorTerritoryZone>;
	}
}

// ---------------------------------------------------------------------------
// Regla compartida de "se puede colocar acá". El chequeo de territorio ajeno
// lo resuelve un callback distinto en server (mastiles reales + membresia) y
// en cliente (cache sincronizado).
// ---------------------------------------------------------------------------
class ExorTerritoryRules
{
	// true si el tipo esta permitido SIEMPRE (whitelist) ignorando territorio
	static bool IsWhitelisted(string itemType)
	{
		ExorCfgPartyTerritorio t = GetExorConfig().party.territorio;
		return t.whitelist_construible.Find(itemType) != -1;
	}

	static bool IsBlacklisted(string itemType)
	{
		ExorCfgPartyTerritorio t = GetExorConfig().party.territorio;
		return t.blacklist_construible.Find(itemType) != -1;
	}

	static bool BuildNearAllowed()
	{
		return GetExorConfig().party.territorio.permitir_construir_cerca;
	}

	static float Radius()
	{
		return GetExorConfig().party.territorio.radio_metros;
	}

	static float Dist2D(vector a, vector b)
	{
		float dx = a[0] - b[0];
		float dz = a[2] - b[2];
		return Math.Sqrt(dx * dx + dz * dz);
	}
}

// ===========================================================================
// SERVER: registro de mastiles + sync + chequeo autoritativo
// ===========================================================================
class ExorTerritoryManager
{
	static ref ExorTerritoryManager s_Instance;
	ref array<TerritoryFlag> m_Masts;

	void ExorTerritoryManager()
	{
		m_Masts = new array<TerritoryFlag>;
	}

	static ExorTerritoryManager Get()
	{
		if (!s_Instance)
			s_Instance = new ExorTerritoryManager();
		return s_Instance;
	}

	void RegisterMast(TerritoryFlag m)
	{
		if (m_Masts.Find(m) == -1)
			m_Masts.Insert(m);
		SyncToAll();
	}

	void UnregisterMast(TerritoryFlag m)
	{
		int idx = m_Masts.Find(m);
		if (idx != -1)
			m_Masts.Remove(idx);
		SyncToAll();
	}

	TerritoryFlag FindMastByGroup(string groupId)
	{
		int i;
		for (i = 0; i < m_Masts.Count(); i++)
		{
			if (m_Masts.Get(i) && m_Masts.Get(i).ExorGetGroupId() == groupId)
				return m_Masts.Get(i);
		}
		return null;
	}

	void DespawnMastForGroup(string groupId)
	{
		TerritoryFlag m = FindMastByGroup(groupId);
		if (m)
		{
			m.ExorMarkDisbanding();	// evita re-disband recursivo en EEDelete
			GetGame().ObjectDelete(m);
		}
	}

	// Chequeo autoritativo (server): puede 'player' colocar 'itemType' en 'pos'?
	bool CanPlaceServer(PlayerBase player, vector pos, string itemType)
	{
		if (!GetExorConfig().party.territorio.habilitado)
			return true;	// territorio desactivado: sin restriccion
		if (ExorTerritoryRules.IsBlacklisted(itemType))
			return false;
		if (ExorTerritoryRules.BuildNearAllowed())
			return true;
		if (ExorTerritoryRules.IsWhitelisted(itemType))
			return true;

		string myGroupId = "";
		if (player)
		{
			ExorGroup g = ExorGroupManager.Get().FindByPlayer(ExorGroupManager.SteamId(player));
			if (g)
				myGroupId = g.id;
		}

		float radius = ExorTerritoryRules.Radius();
		int i;
		for (i = 0; i < m_Masts.Count(); i++)
		{
			TerritoryFlag m = m_Masts.Get(i);
			if (!m)
				continue;
			if (m.ExorGetGroupId() == myGroupId && myGroupId != "")
				continue;	// mi propio territorio
			if (ExorTerritoryRules.Dist2D(pos, m.GetPosition()) <= radius)
				return false;	// territorio ajeno
		}
		return true;
	}

	// ------------------------- sync a clientes -------------------------
	void SyncToAll()
	{
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase pb = PlayerBase.Cast(players.Get(i));
			if (pb)
				SyncToPlayer(pb);
		}
	}

	void SyncToPlayer(PlayerBase p)
	{
		if (!p || !p.GetIdentity())
			return;

		string myGroupId = "";
		ExorGroup g = ExorGroupManager.Get().FindByPlayer(ExorGroupManager.SteamId(p));
		if (g)
			myGroupId = g.id;

		float radius = ExorTerritoryRules.Radius();
		ExorTerritoryCacheDTO dto = new ExorTerritoryCacheDTO();
		int i;
		for (i = 0; i < m_Masts.Count(); i++)
		{
			TerritoryFlag m = m_Masts.Get(i);
			if (!m)
				continue;
			ExorTerritoryZone z = new ExorTerritoryZone();
			vector mp = m.GetPosition();
			z.x = mp[0];
			z.z = mp[2];
			z.radius = radius;
			z.mine = (myGroupId != "" && m.ExorGetGroupId() == myGroupId);
			dto.zones.Insert(z);
		}

		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(dto, false, data);
		p.RPCSingleParam(ExorRPC.TERRITORY_SYNC, new Param1<string>(data), true, p.GetIdentity());
	}
}

// ===========================================================================
// CLIENTE: cache de zonas + chequeo para el preview de construccion
// ===========================================================================
class ExorTerritoryClient
{
	static ref ExorTerritoryCacheDTO s_Cache;

	static void SetCache(ExorTerritoryCacheDTO c)
	{
		s_Cache = c;
	}

	// Mismo orden de reglas que el server, con el cache de zonas.
	static bool CanPlaceClient(vector pos, string itemType)
	{
		if (!GetExorConfig().party.territorio.habilitado)
			return true;	// territorio desactivado: sin restriccion
		if (ExorTerritoryRules.IsBlacklisted(itemType))
			return false;
		if (ExorTerritoryRules.BuildNearAllowed())
			return true;
		if (ExorTerritoryRules.IsWhitelisted(itemType))
			return true;
		if (!s_Cache)
			return true;

		int i;
		for (i = 0; i < s_Cache.zones.Count(); i++)
		{
			ExorTerritoryZone z = s_Cache.zones.Get(i);
			if (z.mine)
				continue;
			float dx = pos[0] - z.x;
			float dz = pos[2] - z.z;
			if (Math.Sqrt(dx * dx + dz * dz) <= z.radius)
				return false;
		}
		return true;
	}
}
