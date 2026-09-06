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

	// Igual pero AL CUADRADO. Para "esta dentro del radio?" da exactamente el mismo
	// resultado (a^2 <= r^2 equivale a a <= r con valores no negativos) y ahorra una raiz
	// cuadrada por mastil y por llamada. Se usa en los barridos, que es donde se nota.
	static float Dist2DSq(vector a, vector b)
	{
		float dx = a[0] - b[0];
		float dz = a[2] - b[2];
		return (dx * dx) + (dz * dz);
	}
}


// ===========================================================================
//  INDICE DE TERRITORIO: la protección NO depende de que la bandera exista
// ===========================================================================
// EL PROBLEMA QUE RESUELVE
// ---------------------------------------------------------------------------
// Hasta ahora, "esta posicion es territorio de alguien?" se contestaba recorriendo
// las ENTIDADES bandera vivas. Eso ata la proteccion de una base a la entidad mas
// fragil del mod: el TerritoryFlag es lo que el motor despawnea (limpieza del CE por
// lifetime, ruina, persistencia corrupta). Historial real: banderas que desaparecian
// solas y clanes que, mientras tanto, quedaban SIN territorio -o sea sin proteccion
// de looteo ni de construccion- hasta que el self-heal las recreaba.
//
// Y ademas era caro: cada consulta llamaba GetPosition(), ExorGetGroupId() e
// ExorIsBuilt() -tres llamadas al motor- POR BANDERA, y esas consultas corren en
// caminos calientes (colocar, abrir un contenedor, levantar un item).
//
// LA IDEA
// ---------------------------------------------------------------------------
// El dato que define un territorio no es la entidad: es la POSICION DEL MASTIL, y esa
// ya vive en el JSON de cada grupo (mast_x/mast_z). Asi que el indice se arma desde
// los GRUPOS, no desde el mundo:
//   - la base sigue protegida aunque el motor se lleve la bandera puesta;
//   - la consulta es un barrido sobre floats en memoria, sin una sola llamada al motor;
//   - se rearma solo cuando cambia algo (mismo sello de version que ya usaba el cache
//     de "en que territorio estoy"), no en cada consulta.
//
// La entidad bandera sigue existiendo, claro: es lo que se ve, lo que se iza y donde
// se interactua. Simplemente deja de ser la fuente de la verdad.
class ExorZonaTerr
{
	float x;
	float z;
	string gid;
}

class ExorTerritoryIndex
{
	static ref array<ref ExorZonaTerr> s_Zonas;
	static int s_Ver = -1;

	// Rearma el indice si cambio algo (mastil puesto/sacado, roster, claim). Barato:
	// una comparacion de enteros en el caso normal.
	static void Asegurar()
	{
		int ver = ExorTerritoryProbe.Version();
		if (s_Zonas && s_Ver == ver)
			return;
		s_Ver = ver;
		s_Zonas = new array<ref ExorZonaTerr>;
		array<ref ExorGroup> gs = ExorGroupManager.Get().m_Groups;
		if (!gs)
			return;
		int i;
		for (i = 0; i < gs.Count(); i++)
		{
			ExorGroup g = gs.Get(i);
			if (!g)
				continue;
			if (g.mast_x == 0 && g.mast_z == 0)
				continue;	// grupo sin territorio reclamado
			ExorZonaTerr z = new ExorZonaTerr();
			z.x = g.mast_x;
			z.z = g.mast_z;
			z.gid = g.id;
			s_Zonas.Insert(z);
		}
	}

	static array<ref ExorZonaTerr> Zonas()
	{
		Asegurar();
		return s_Zonas;
	}

	// Id del grupo dueño del territorio en 'pos' ("" si no hay ninguno).
	static string GrupoEn(vector pos)
	{
		if (!ExorHotFlags.TerritorioOn())
			return "";
		Asegurar();
		float r = ExorTerritoryRules.Radius();
		float r2 = r * r;
		float px = pos[0];
		float pz = pos[2];
		int i;
		for (i = 0; i < s_Zonas.Count(); i++)
		{
			ExorZonaTerr z = s_Zonas.Get(i);
			float dx = px - z.x;
			float dz = pz - z.z;
			if ((dx * dx) + (dz * dz) <= r2)
				return z.gid;
		}
		return "";
	}

	// Posicion (horizontal) del mastil de un grupo. "0 0 0" si el grupo no tiene territorio.
	// Sale del dato GUARDADO, asi que responde igual con el mastil despawneado.
	static vector PosDe(string gid)
	{
		if (gid == "")
			return "0 0 0";
		Asegurar();
		int i;
		for (i = 0; i < s_Zonas.Count(); i++)
		{
			ExorZonaTerr z = s_Zonas.Get(i);
			if (z.gid == gid)
				return Vector(z.x, 0, z.z);
		}
		return "0 0 0";
	}

	// Id del grupo AJENO cuyo territorio cubre 'pos' ("" = ninguno, o es el propio).
	// 'miGid' vacio = el jugador no tiene clan -> cualquier territorio le es ajeno.
	static string GrupoAjenoEn(string miGid, vector pos)
	{
		if (!ExorHotFlags.TerritorioOn())
			return "";
		Asegurar();
		float r = ExorTerritoryRules.Radius();
		float r2 = r * r;
		float px = pos[0];
		float pz = pos[2];
		int i;
		for (i = 0; i < s_Zonas.Count(); i++)
		{
			ExorZonaTerr z = s_Zonas.Get(i);
			if (miGid != "" && z.gid == miGid)
				continue;	// mi propio territorio
			float dx = px - z.x;
			float dz = pz - z.z;
			if ((dx * dx) + (dz * dz) <= r2)
				return z.gid;
		}
		return "";
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
	bool m_SyncPending;	// hay un SyncToAll diferido ya agendado (debounce anti-burst)

	void ExorTerritoryManager()
	{
		m_Masts = new array<TerritoryFlag>;
	}

	// DEBOUNCE: cada register/unregister/claim/repair de bandera llamaba SyncToAll() de una,
	// y SyncToAll reenvia el cache de territorio a TODOS los online (O(jugadores*banderas)).
	// En el arranque (carga de N banderas) o en churn de self-heal eso son N reenvios de golpe.
	// Ahora coalescemos: marcar pendiente y hacer UN solo SyncToAll diferido 500ms (imperceptible
	// para el preview de construccion). Multiples triggers en esa ventana = 1 solo reenvio.
	void RequestSyncToAll()
	{
		if (m_SyncPending)
			return;
		m_SyncPending = true;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(DoDeferredSyncToAll, 500, false);
	}

	void DoDeferredSyncToAll()
	{
		m_SyncPending = false;
		SyncToAll();
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
		{
			m_Masts.Insert(m);
			ExorTerritoryProbe.Bump();	// caduca el cache de "en que territorio estoy"
		}
		RequestSyncToAll();
	}

	void UnregisterMast(TerritoryFlag m)
	{
		int idx = m_Masts.Find(m);
		if (idx != -1)
		{
			m_Masts.Remove(idx);
			ExorTerritoryProbe.Bump();	// caduca el cache de "en que territorio estoy"
		}
		RequestSyncToAll();
	}

	// Le renueva el reloj del CE a TODOS los mastiles vivos. Lo llama el mantenimiento cada
	// 10 minutos. Es un barrido sobre un array chico (un mastil por clan) y dos llamadas por
	// mastil, o sea nada; a cambio, el CE deja de poder borrarlos por lifetime, que es la
	// causa de que "el mastil se fuera solo a los dias o semanas".
	// Sigue existiendo el self-heal como red: si igual se pierde uno (crash, otro mod), se
	// recrea. Pero ahora es la excepcion y no el mecanismo principal.
	void RefrescarVidaMastiles()
	{
		int i;
		for (i = 0; i < m_Masts.Count(); i++)
		{
			TerritoryFlag m = m_Masts.Get(i);
			if (m)
				m.ExorRefrescarVida();
		}
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

	// GRUPO dueño del territorio que contiene 'pos' (mastil construido a <=Radius). "" si el
	// punto no esta en ningun territorio. Lo usa el auto-virtualizado para taggear el auto con
	// el clan de la base donde esta el parking (sin persistir el grupo en el parking).
	// Del INDICE, no de las entidades: asi el parking de una base sigue sabiendo de que
	// clan es aunque la bandera este momentaneamente despawneada. Ver ExorTerritoryIndex.
	string GroupAtPos(vector pos)
	{
		return ExorTerritoryIndex.GrupoEn(pos);
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

	// Devuelve el ID DEL GRUPO ajeno cuyo territorio cubre 'pos' ("" si no hay ninguno o
	// si 'pos' cae solo en territorio propio). 'player' puede ser null (= todo ajeno).
	// Base reutilizable para: anti-construccion, anti-desmantelar y logs anti-raid.
	string FindEnemyTerritoryAt(PlayerBase player, vector pos)
	{
		if (!ExorHotFlags.TerritorioOn())
			return "";

		string myGroupId = "";
		if (player)
		{
			ExorGroup g = ExorGroupManager.Get().FindByPlayer(ExorGroupManager.SteamId(player));
			if (g)
				myGroupId = g.id;
		}
		return FindEnemyTerritoryAtEx(myGroupId, pos);
	}

	// Igual que FindEnemyTerritoryAt pero recibiendo el id de grupo YA resuelto. Existe
	// para los caminos calientes (ver ExorTerritoryProbe): resolver el grupo del jugador y
	// barrer los territorios son dos costos distintos y el primero se puede cachear aparte.
	//
	// Devuelve el ID DEL GRUPO ajeno ("" = ninguno). Antes devolvia la ENTIDAD mastil, pero
	// ningun llamador la usaba: todos preguntaban "es territorio ajeno?" y "de que clan?".
	// Con el id, la proteccion deja de depender de que la entidad exista -que es justo la
	// que el motor despawnea-. Ver ExorTerritoryIndex.
	string FindEnemyTerritoryAtEx(string myGroupId, vector pos)
	{
		return ExorTerritoryIndex.GrupoAjenoEn(myGroupId, pos);
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

		if (FindEnemyTerritoryAt(player, pos) != "")
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

		// Candidatos en rango con su distancia^2 (sin sqrt en el loop).
		// Salen del INDICE y no de las entidades vivas: el preview de construccion del
		// cliente tiene que respetar el territorio de un clan aunque su mastil este
		// despawneado en ese momento (si no, el jugador ve "se puede construir" justo donde
		// el server le va a decir que no). Ver ExorTerritoryIndex.
		array<ref ExorZonaTerr> zonas = ExorTerritoryIndex.Zonas();
		array<ref ExorZonaTerr> near = new array<ref ExorZonaTerr>;
		array<float> nearD = new array<float>;
		int i;
		for (i = 0; i < zonas.Count(); i++)
		{
			ExorZonaTerr zt = zonas.Get(i);
			float ddx = zt.x - ppos[0];
			float ddz = zt.z - ppos[2];
			float d2 = ddx * ddx + ddz * ddz;	// horizontal (ignora altura)
			if (d2 > maxD2)
				continue;
			near.Insert(zt);
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
				ExorZonaTerr tf = near.Get(k); near.Set(k, near.Get(best)); near.Set(best, tf);
				float df = nearD.Get(k); nearD.Set(k, nearD.Get(best)); nearD.Set(best, df);
			}
		}

		// solo se mandan las top-N zonas mas cercanas (en TROZOS via ExorNetChunk).
		ExorTerritoryCacheDTO dto = new ExorTerritoryCacheDTO();
		JsonSerializer js = new JsonSerializer();
		string data = "";
		for (i = 0; i < lim; i++)
		{
			ExorZonaTerr z2 = near.Get(i);
			ExorTerritoryZone z = new ExorTerritoryZone();
			z.x = z2.x;
			z.z = z2.z;
			z.radius = radius;
			// "mine" sale directo del id del grupo dueño de la zona: el indice se arma desde
			// el mastil GUARDADO de cada grupo, asi que ya no hace falta el desempate por
			// posicion que cubria los bindings inconsistentes del mastil vivo.
			z.mine = (myGroupId != "" && z2.gid == myGroupId);
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
	static int s_UltimoPedidoMs;	// ultimo TERRITORY_REQ mandado (cooldown del sync por demanda)

	static void SetCache(ExorTerritoryCacheDTO c)
	{
		s_Cache = c;
	}

	// ------------------------------------------------------------------------
	//  SYNC POR DEMANDA (nunca periodico)
	// ------------------------------------------------------------------------
	// El cache se manda solo en eventos puntuales (conectarse, cambios de grupo) y ademas
	// se arma RELATIVO A LA POSICION del jugador (top-40 zonas dentro de 1500 m). O sea que
	// si te conectas lejos de tu base y caminas hasta ella sin que pase nada en el grupo, el
	// cache no tiene tu zona -> el mastil solo ofrece "Bajar bandera", sin "Administrar
	// party" ni "Invitar". Ese es el bug reportado.
	//
	// Refrescar cada N segundos a todos NO va: SyncToPlayer arma un DTO, lo serializa a
	// JSON y lo manda chunkeado POR JUGADOR; con 60 online son envios constantes que nadie
	// pidio. Aca lo pide el CLIENTE, y solo cuando esta mirando un mastil y le falta el
	// dato, con cooldown. Si el cache ya sirve -el 99% del tiempo- no se manda nada.
	static const int PEDIDO_COOLDOWN_MS = 10000;

	static void PedirSyncSiFalta(PlayerBase p)
	{
		if (!p || !GetGame() || !GetGame().IsClient())
			return;
		int ahora = GetGame().GetTime();
		if (s_UltimoPedidoMs != 0 && (ahora - s_UltimoPedidoMs) < PEDIDO_COOLDOWN_MS)
			return;
		s_UltimoPedidoMs = ahora;
		p.ExorReqTerritorySync();	// el server contesta con ROSTER_SYNC + TERRITORY_SYNC
	}

	// CLIENTE: "este mastil es de MI grupo?" -> condicion de todas las acciones de miembro
	// del mastil. Cuando da false pide el sync (una vez por cooldown): si fue por cache
	// faltante o viejo, a los pocos segundos la accion aparece sola sin relogear.
	static bool MastEsMio(PlayerBase p, vector pos)
	{
		if (!p)
			return false;
		if (p.ExorClientInGroup() && IsOwnMastNear(pos))
			return true;
		PedirSyncSiFalta(p);
		return false;
	}

	// true si 'pos' cae dentro del radio de un mastil de MI PROPIO territorio
	// (zona con mine=true). Lo usan las acciones de miembro del mastil para NO
	// mostrarse en mastiles AJENOS (bug: "Administrar/Invitar" salia en cualquier
	// mastil si estabas en algun grupo, aunque no fuera el tuyo).
	static bool IsOwnMastNear(vector pos)
	{
		if (!s_Cache)
			return false;
		// AL CUADRADO: esto corre por FRAME en el cliente mientras el jugador tiene un
		// holograma en la mano (ItemBase.CanBePlaced), una vez por territorio conocido.
		int i;
		for (i = 0; i < s_Cache.zones.Count(); i++)
		{
			ExorTerritoryZone z = s_Cache.zones.Get(i);
			if (!z.mine)
				continue;
			float dx = pos[0] - z.x;
			float dz = pos[2] - z.z;
			if ((dx * dx) + (dz * dz) <= z.radius * z.radius)
				return true;
		}
		return false;
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

		// AL CUADRADO, por lo mismo que arriba: por frame mientras se coloca algo.
		int i;
		for (i = 0; i < s_Cache.zones.Count(); i++)
		{
			ExorTerritoryZone z = s_Cache.zones.Get(i);
			if (z.mine)
				continue;
			float dx = pos[0] - z.x;
			float dz = pos[2] - z.z;
			if ((dx * dx) + (dz * dz) <= z.radius * z.radius)
				return false;
		}
		return true;
	}
}
