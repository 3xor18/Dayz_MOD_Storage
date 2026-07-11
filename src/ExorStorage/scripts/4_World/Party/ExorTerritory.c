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
	static bool s_Healing;	// self-heal en curso: el mastil recreado NO reclama territorio (guard, como ExorKoth.s_SpawningKothMast)
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

	// NOTA: la "inmortalidad" del mastil (que no lo borre el CE por lifetime) se maneja en el
	// types.xml del server subiendo el <lifetime> del TerritoryFlag. La API de script para refrescar
	// el lifetime en vivo (CEApi.SetItemLifetime) NO existe en esta version de DayZ. Igual, si por
	// cualquier causa el objeto-bandera se pierde, el self-heal recrea el mastil (party se conserva).

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

	// como FindMastByGroup pero SOLO devuelve un mastil REAL (con el poste construido);
	// ignora los "fantasmas" (registrados pero sin construir tras un crash/load fallido).
	TerritoryFlag FindBuiltMastByGroup(string groupId)
	{
		int i;
		for (i = 0; i < m_Masts.Count(); i++)
		{
			TerritoryFlag m = m_Masts.Get(i);
			if (m && m.ExorGetGroupId() == groupId && m.ExorIsBuilt())
				return m;
		}
		return null;
	}

	// SELF-HEAL: si el grupo tiene posicion de mastil guardada y NO hay un mastil real vivo,
	// recrea el TerritoryFlag en esa posicion y lo re-liga al grupo. Necesita un 'builder'
	// (Man) para armar el poste (OnPartBuiltServer) -> se llama al CONECTAR un miembro (al
	// arrancar el server no hay nadie para construirlo).
	void HealGroupMast(ExorGroup g, Man builder)
	{
		if (!GetGame().IsServer() || !g || !builder)
			return;
		if (g.mast_x == 0 && g.mast_z == 0)
			return;	// el grupo no tiene mastil guardado
		if (FindBuiltMastByGroup(g.id))
		{
			// el mastil del grupo ya esta vivo -> nada que reparar; limpiar la marca de perdido
			if (g.mast_lost != 0)
			{
				g.mast_lost = 0;
				ExorGroupManager.Get().SaveGroup(g);
			}
			return;
		}

		vector pos = Vector(g.mast_x, g.mast_y, g.mast_z);

		// "SALVO que ya exista una bandera NUEVA creada en esa base": si hay un mastil
		// construido de OTRO grupo justo en esta posicion, la base ya fue reconstruida.
		// No restauramos (evitar 2 territorios superpuestos) y descartamos el grupo viejo.
		TerritoryFlag other = FindBuiltMastNear(pos, 15.0, g.id);
		if (other)
		{
			Print(string.Format("%1 self-heal: la base del grupo %2 ya fue reconstruida (bandera nueva del grupo %3) -> se descarta el grupo viejo", ExorStorageConstants.LOG, g.id, other.ExorGetGroupId()));
			ExorGroupManager.Get().MarkGroupDeleted(g, "");	// no borra el file (baneo de clan); solo lo saca de activos
			return;
		}

		// OPTIMIZACION anti-churn: si el mastil del grupo YA persiste (fantasma con el group
		// id correcto: cargo sin la construccion "built"), NO lo borramos+recreamos cada
		// arranque -> lo REUSAMOS: re-construimos el poste + re-ligamos sobre el MISMO objeto
		// persistido. Asi el objeto sobrevive (no hay delete/create), no se pierde nada, y no
		// deja kit. (Antes: delete + CreateObjectEx nuevo -> churn en cada reinicio.)
		TerritoryFlag ghost = FindMastByGroup(g.id);
		if (ghost)
		{
			ghost.ExorHealBind(g.id, builder);
			g.mast_lost = 0;
			ExorGroupManager.Get().SaveGroup(g);
			Print(string.Format("%1 self-heal: mastil del grupo %2 RE-CONSTRUIDO reusando el persistido en %3 (el objeto SI persiste; se reconstruye el poste)", ExorStorageConstants.LOG, g.id, ghost.GetPosition()));
			return;
		}
		// No hay mastil persistido -> el objeto NO sobrevivio al reinicio -> crear uno nuevo.
		ExorTerritoryManager.s_Healing = true;
		Object mo = GetGame().CreateObjectEx("TerritoryFlag", pos, ECE_PLACE_ON_SURFACE);
		ExorTerritoryManager.s_Healing = false;
		TerritoryFlag m = TerritoryFlag.Cast(mo);
		if (m)
		{
			m.ExorHealBind(g.id, builder);
			g.mast_lost = 0;	// restaurado -> ya no esta perdido
			ExorGroupManager.Get().SaveGroup(g);
			Print(string.Format("%1 self-heal: mastil del grupo %2 CREADO NUEVO en %3 (el objeto persistido NO sobrevivio)", ExorStorageConstants.LOG, g.id, pos));
		}
		else
			Print(string.Format("%1 self-heal: NO se pudo recrear el mastil del grupo %2", ExorStorageConstants.LOG, g.id));
	}

	// Devuelve un mastil CONSTRUIDO (real) dentro de 'radius' de 'pos' que NO sea del grupo
	// 'excludeGroupId'. Sirve para saber si la base ya fue reconstruida por OTRA bandera.
	TerritoryFlag FindBuiltMastNear(vector pos, float radius, string excludeGroupId)
	{
		int i;
		for (i = 0; i < m_Masts.Count(); i++)
		{
			TerritoryFlag m = m_Masts.Get(i);
			if (!m || !m.ExorIsBuilt())
				continue;
			if (excludeGroupId != "" && m.ExorGetGroupId() == excludeGroupId)
				continue;
			if (ExorTerritoryRules.Dist2D(pos, m.GetPosition()) <= radius)
				return m;
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

	// Devuelve el mastil AJENO en cuyo radio cae 'pos' (o null si no hay ninguno o
	// si 'pos' cae solo en territorio propio). 'player' puede ser null (= todo ajeno).
	// Base reutilizable para: anti-construccion, anti-desmantelar y logs anti-raid.
	TerritoryFlag FindEnemyTerritoryAt(PlayerBase player, vector pos)
	{
		if (!GetExorConfig().party.territorio.habilitado)
			return null;

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
			{
				// DIAG TEMPORAL (bug "no deja construir en base propia"): loguea por que
				// este mastil se considera ajeno. Comparar myGroupId vs mastil_groupId:
				//  - myGroupId VACIO  -> la membresia del jugador no resolvio (FindByPlayer)
				//  - myGroupId OK pero mastil_groupId distinto/vacio -> el mastil perdio su binding (self-heal)
				string diagSid = "";
				if (player)
					diagSid = ExorGroupManager.SteamId(player);
				Print(string.Format("%1 DIAG-TERRITORIO BLOQUEO: sid=%2 myGroupId='%3' | mastil_groupId='%4' built=%5 pos=%6", ExorStorageConstants.LOG, diagSid, myGroupId, m.ExorGetGroupId(), m.ExorIsBuilt(), m.GetPosition()));
				return m;	// territorio ajeno
			}
		}
		return null;
	}

	// ROBUSTEZ (fix "no deja construir en base propia"): true si 'pos' cae dentro del
	// territorio del PROPIO grupo del jugador segun el mastil GUARDADO del grupo
	// (mast_x/z del ExorGroup, dato confiable en disco). NO depende del binding del
	// mastil VIVO, que un self-heal o una persistencia rota puede dejar inconsistente.
	// Asi, aunque el mastil vivo este mal ligado, el dueno/miembro SIEMPRE puede
	// construir dentro de su territorio.
	bool IsInOwnGroupTerritory(PlayerBase player, vector pos)
	{
		if (!player)
			return false;
		ExorGroup g = ExorGroupManager.Get().FindByPlayer(ExorGroupManager.SteamId(player));
		if (!g)
			return false;
		if (g.mast_x == 0 && g.mast_z == 0)
			return false;	// el grupo no tiene mastil guardado
		vector myMast = Vector(g.mast_x, 0, g.mast_z);
		return ExorTerritoryRules.Dist2D(pos, myMast) <= ExorTerritoryRules.Radius();
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
		if (IsInOwnGroupTerritory(player, pos))
			return true;	// tu propia base (por mastil guardado del grupo) -> siempre permitido

		if (FindEnemyTerritoryAt(player, pos))
			return false;	// cae en territorio ajeno
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
		vector ppos = p.GetPosition();

		// SOLO los mastiles CERCA del jugador. El cliente usa este cache para el preview de
		// construccion (estas en territorio de alguien?), NO necesita todos los del mapa.
		// El cache viaja en TROZOS (ExorNetChunk) -> ya no hay tope de bytes; el filtro por
		// cercania queda igual porque es lo CORRECTO (solo importa el territorio donde estas).
		float SYNC_RANGE = 1500.0;	// alcance horizontal (sobra para el preview)
		float maxD2 = SYNC_RANGE * SYNC_RANGE;
		int MAX_ZONES = 40;	// tope de zonas a sincronizar (mas que suficiente para el preview cercano)

		// candidatos en rango con su distancia^2 (sin sqrt en el loop)
		array<TerritoryFlag> near = new array<TerritoryFlag>;
		array<float> nearD = new array<float>;
		int i;
		for (i = 0; i < m_Masts.Count(); i++)
		{
			TerritoryFlag m = m_Masts.Get(i);
			if (!m)
				continue;
			vector dd = m.GetPosition() - ppos;
			float d2 = dd[0] * dd[0] + dd[2] * dd[2];	// horizontal (ignora altura)
			if (d2 > maxD2)
				continue;
			near.Insert(m);
			nearD.Insert(d2);
		}

		// SELECCION PARCIAL: dejar ordenados solo los primeros min(MAX_ZONES, count) por
		// distancia (O(n*N), N=40 acotado) en vez de un bubble sort O(n^2) de todos los
		// mastiles -> escala con muchas bases. Solo esos top-N se sincronizan.
		int cnt = near.Count();
		int lim = cnt;
		if (lim > MAX_ZONES)
			lim = MAX_ZONES;
		int k;
		for (k = 0; k < lim; k++)
		{
			int best = k;
			int j;
			for (j = k + 1; j < cnt; j++)
			{
				if (nearD.Get(j) < nearD.Get(best))
					best = j;
			}
			if (best != k)
			{
				TerritoryFlag tf = near.Get(k); near.Set(k, near.Get(best)); near.Set(best, tf);
				float df = nearD.Get(k); nearD.Set(k, nearD.Get(best)); nearD.Set(best, df);
			}
		}

		// solo se mandan las top-N zonas mas cercanas (en TROZOS via ExorNetChunk).
		ExorTerritoryCacheDTO dto = new ExorTerritoryCacheDTO();
		JsonSerializer js = new JsonSerializer();
		string data = "";
		for (i = 0; i < lim; i++)
		{
			TerritoryFlag m2 = near.Get(i);
			ExorTerritoryZone z = new ExorTerritoryZone();
			vector mp = m2.GetPosition();
			z.x = mp[0];
			z.z = mp[2];
			z.radius = radius;
			// "mine" por binding del mastil vivo O por el mastil GUARDADO de mi grupo
			// (robusto: si el binding del mastil vivo quedo inconsistente por un
			// self-heal/persistencia, igual reconozco mi propia base por su posicion).
			z.mine = (myGroupId != "" && m2.ExorGetGroupId() == myGroupId);
			if (!z.mine && g && (g.mast_x != 0 || g.mast_z != 0))
			{
				vector myMastPos = Vector(g.mast_x, 0, g.mast_z);
				if (ExorTerritoryRules.Dist2D(mp, myMastPos) <= 5.0)
					z.mine = true;	// esta zona es el mastil guardado de mi grupo
			}
			dto.zones.Insert(z);
		}
		js.WriteToString(dto, false, data);
		ExorNetChunk.Send(p, p.GetIdentity(), ExorRPC.TERRITORY_SYNC, data);
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
