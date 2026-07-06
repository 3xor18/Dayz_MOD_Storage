// ============================================================================
// 3xor_Vanilla_Optimization - COFRE - manager (SOLO server)
// ----------------------------------------------------------------------------
// Zonas (cofre.json -> lugares[]) donde se pueden abrir cofres segun horario por
// dia. Cada zona:
//   - avisa "en X minutos podran abrir cofres" antes de abrir la ventana,
//   - avisa "ya se puede abrir cofres" al abrirse,
//   - pone una marca en el mapa mientras esta abierta,
//   - avisa cada N minutos cuantos players hay dentro del rango.
// Cuando un player SUELTA una caja cerrada (Exor_Cofre_*_Packed) en el piso dentro
// de una zona ABIERTA, tras tiempo_abrir_un_cofre_minutos la caja se transforma en
// el cofre abierto (1000 slots) relleno con UN bundle aleatorio de su color.
// Reusa el killfeed y la marca de mapa del KOTH (ExorKfDTO / ExorKothMarkerDTO).
// ============================================================================

// Estado runtime de UNA zona
class ExorCofreZone
{
	int m_ZoneIdx;
	bool m_IsOpen;
	int m_WarnDayKey;            // dia (num) para el que valen los m_WarnFired
	int m_WarnBaseMin;           // minuto-del-dia en que se reseteo (para silenciar avisos ya pasados)
	ref array<bool> m_WarnFired;
	int m_LastPlayerAlertMs;
	bool m_MarkerShown;
	ref ExorKothMarkerDTO m_LastMarker;
	// props del evento (mientras la ventana esta abierta)
	ref array<Object> m_StructureObjs;	// objetos de la estructura (mesa + drill + ...)
	ref array<Object> m_Lights;			// luces (roadflare) debajo de la mesa (Exor_CofreLight)
	int m_LightsCheckMs;				// ultimo chequeo de keep-alive de luces (throttle anti-loop)
	ref array<Object> m_TableSlots;		// ocupante de cada slot de la mesa (null = libre)
	int m_GraceUntilMs;					// uptime ms hasta el fin de la gracia post-evento (0 = sin gracia)

	void ExorCofreZone(int idx)
	{
		m_ZoneIdx = idx;
		m_IsOpen = false;
		m_WarnDayKey = -1;
		m_WarnBaseMin = 0;
		m_WarnFired = new array<bool>;
		m_LastPlayerAlertMs = 0;
		m_MarkerShown = false;
		m_StructureObjs = new array<Object>;
		m_Lights = new array<Object>;
		m_TableSlots = new array<Object>;
	}

	// ------------------------- slots de la mesa -------------------------
	// Asegura que m_TableSlots tenga 'n' entradas (null = libre).
	void EnsureSlots(int n)
	{
		while (m_TableSlots.Count() < n)
			m_TableSlots.Insert(null);
	}

	// posicion del slot i sobre la mesa (relativo a la mesa/ancla, a lo largo del eje Z).
	vector SlotPos(int i, ExorCfgCofre g)
	{
		vector anchor = ResolvePos(Cfg());
		anchor[1] = GetGame().SurfaceY(anchor[0], anchor[2]) + g.altura_estructura;
		if (m_StructureObjs && m_StructureObjs.Count() > 0 && m_StructureObjs.Get(0))
			anchor = m_StructureObjs.Get(0).GetPosition();
		// centrar las N cajas a lo largo del eje Z de la mesa
		int n = g.mesa_max_cajas;
		float mid = (n - 1) / 2.0;
		float dz = (i - mid) * g.mesa_espaciado_slots;
		return anchor + Vector(0, g.mesa_altura_slots, dz);
	}

	// primer slot libre (o -1 si estan todos ocupados)
	int FreeSlotIndex(ExorCfgCofre g)
	{
		EnsureSlots(g.mesa_max_cajas);
		int i;
		for (i = 0; i < g.mesa_max_cajas; i++)
		{
			if (!m_TableSlots.Get(i))
				return i;
		}
		return -1;
	}

	void SetSlot(int i, Object o)
	{
		EnsureSlots(i + 1);
		m_TableSlots.Set(i, o);
	}

	void FreeSlot(int i)
	{
		if (i >= 0 && i < m_TableSlots.Count())
			m_TableSlots.Set(i, null);
	}

	// hay mesa spawneada (primer objeto de la estructura vivo)
	bool HasTable()
	{
		return (m_StructureObjs && m_StructureObjs.Count() > 0 && m_StructureObjs.Get(0) != null);
	}

	bool IsGraceActive(int nowMs)
	{
		return (m_GraceUntilMs > 0 && nowMs < m_GraceUntilMs);
	}

	// hay al menos una CAJA CERRADA o un COFRE ABIERTO CON loot ocupando un slot de la mesa
	bool HasContentOnTable()
	{
		if (!m_TableSlots)
			return false;
		int i;
		for (i = 0; i < m_TableSlots.Count(); i++)
		{
			Object o = m_TableSlots.Get(i);
			if (!o)
				continue;
			Exor_Cofre_Packed_Base b = Exor_Cofre_Packed_Base.Cast(o);
			if (b)
				return true;	// caja cerrada
			Exor_Cofre_Base c = Exor_Cofre_Base.Cast(o);
			if (c && c.ExorCargoCount() > 0)
				return true;	// cofre abierto con loot
		}
		return false;
	}

	ExorCfgCofreLugar Cfg()
	{
		ExorCfgCofre g = GetExorConfig().cofre;
		if (!g.lugares || m_ZoneIdx >= g.lugares.Count())
			return null;
		return g.lugares.Get(m_ZoneIdx);
	}

	vector ResolvePos(ExorCfgCofreLugar l)
	{
		if (!l || !l.posicion)
			return "0 0 0";
		vector p = Vector(l.posicion.x, l.posicion.y, l.posicion.z);
		if (p[1] <= 0)
			p[1] = GetGame().SurfaceY(p[0], p[2]);
		return p;
	}

	// entrada de horario de HOY (o null si el dia no esta en la lista)
	ExorCfgCofreDia FindDia(ExorCfgCofreLugar l, string today)
	{
		if (!l || !l.dias)
			return null;
		int i;
		for (i = 0; i < l.dias.Count(); i++)
		{
			ExorCfgCofreDia d = l.dias.Get(i);
			if (d && d.dia == today)
				return d;
		}
		return null;
	}

	void ResetWarn(ExorCfgCofre g, int dayKey, int nowMin)
	{
		m_WarnDayKey = dayKey;
		m_WarnBaseMin = nowMin;
		m_WarnFired = new array<bool>;
		int i;
		int n = 0;
		if (g.aviso_antes_de_spawnear)
			n = g.aviso_antes_de_spawnear.Count();
		for (i = 0; i < n; i++)
			m_WarnFired.Insert(false);
	}

	void Tick(ExorCfgCofre g, int nowMin, string today, int dayKey)
	{
		ExorCfgCofreLugar l = Cfg();
		if (!l || !l.activo)
		{
			if (m_MarkerShown)
				ClearMarker();
			DespawnEventProps();
			m_GraceUntilMs = 0;
			m_IsOpen = false;
			return;
		}

		if (dayKey != m_WarnDayKey)
			ResetWarn(g, dayKey, nowMin);

		ExorCfgCofreDia dia = FindDia(l, today);
		bool scheduledToday = (dia && dia.activado);
		int startMin = -1;
		int endMin = -1;
		if (scheduledToday)
		{
			startMin = ExorCofre.ParseHHMM(dia.hora_inicio);
			endMin = ExorCofre.ParseHHMM(dia.hora_fin);
		}
		bool openNow = (scheduledToday && startMin >= 0 && endMin > startMin && nowMin >= startMin && nowMin < endMin);

		vector pos = ResolvePos(l);
		int ix = Math.Round(pos[0]);
		int iz = Math.Round(pos[2]);
		int argb = ExorCofre.MarkerArgb();
		int nowMs = GetGame().GetTime();

		// --- avisos ANTES de abrir la ventana ---
		if (scheduledToday && !openNow && nowMin < startMin && g.aviso_antes_de_spawnear)
		{
			int w;
			for (w = 0; w < g.aviso_antes_de_spawnear.Count(); w++)
			{
				if (w >= m_WarnFired.Count())
					continue;
				if (m_WarnFired.Get(w))
					continue;
				int mins = g.aviso_antes_de_spawnear.Get(w);
				// umbral ya pasado cuando empezamos a vigilar hoy (arranque tarde) -> marcar sin avisar
				if ((startMin - mins) < m_WarnBaseMin)
				{
					m_WarnFired.Set(w, true);
					continue;
				}
				if (nowMin >= startMin - mins)
				{
					m_WarnFired.Set(w, true);
					ExorCofre.Alert(string.Format("En %1 minutos podran abrir cofres en X:%2 Z:%3", mins, ix, iz), 13, argb);
				}
			}
		}

		// --- transiciones cerrado<->abierto ---
		if (openNow && !m_IsOpen)
		{
			if (g.aviso_al_spawnwar)
				ExorCofre.Alert(string.Format("Ya se puede abrir cofres en X:%1 Z:%2", ix, iz), 15, argb);
			if (g.colocar_marca_mapa)
				SendMarker(true, argb, "Zona de cofres");
			m_LastPlayerAlertMs = nowMs;
			SpawnEventProps(g, pos);
			m_GraceUntilMs = 0;	// reabrio: cancelar gracia previa
		}
		if (!openNow && m_IsOpen)
		{
			if (m_MarkerShown)
				ClearMarker();
			// si hay cajas en la mesa (cerradas o abiertas con loot): GRACIA de N min con TODO
			// puesto; si no, despawnear al toque.
			if (HasContentOnTable())
			{
				m_GraceUntilMs = nowMs + (g.minutos_despawn_cofres_tras_evento * 60 * 1000);
				Print(string.Format("%1 COFRE: evento cerrado con cajas en la mesa -> gracia de %2 min", ExorStorageConstants.LOG, g.minutos_despawn_cofres_tras_evento));
			}
			else
			{
				m_GraceUntilMs = 0;
				DespawnEventProps();
			}
		}
		m_IsOpen = openNow;

		// --- gracia post-evento: mantener mesa/luz/cajas; al vencer, despawnear todo ---
		if (!openNow && m_GraceUntilMs > 0)
		{
			if (nowMs >= m_GraceUntilMs)
			{
				DespawnEventProps();
				ExorCofre.Get().DeleteCofresInZone(m_ZoneIdx);
				m_GraceUntilMs = 0;
				Print(string.Format("%1 COFRE: fin de gracia en zona %2 -> mesa y cajas despawneadas", ExorStorageConstants.LOG, m_ZoneIdx));
			}
			else
			{
				RefreshEventProps(g, pos);	// mantener estructura + luz durante la gracia
			}
		}

		// --- mientras esta abierta: marca + estructura + aviso de players en zona ---
		if (openNow)
		{
			if (g.colocar_marca_mapa && !m_MarkerShown)
				SendMarker(true, argb, "Zona de cofres");
			RefreshEventProps(g, pos);	// self-heal: re-spawnea estructura/bengalas y mantiene las bengalas encendidas

			if (g.aviso_de_players_dentro_del_rango_efecto)
			{
				int gapMs = g.minutos_aviso_players_en_zona * 60 * 1000;
				if (m_LastPlayerAlertMs == 0 || nowMs - m_LastPlayerAlertMs >= gapMs)
				{
					m_LastPlayerAlertMs = nowMs;
					int cnt = ExorCofre.CountPlayersNear(pos, g.rango_detectar_payer_para_aviso_metros);
					if (cnt > 0)
						ExorCofre.Alert(string.Format("Existen %1 players En la zona de apertura del cofre", cnt), 11, argb);
				}
			}
		}
	}

	// ------------------------- marca del mapa -------------------------
	void SendMarker(bool show, int argb, string label)
	{
		ExorKothMarkerDTO d = new ExorKothMarkerDTO();
		d.show = show;
		d.id = ExorCofre.MARKER_ID_BASE + m_ZoneIdx;	// offset propio: no choca con las marcas del KOTH
		vector pos = ResolvePos(Cfg());
		d.x = pos[0];
		d.y = pos[1];
		d.z = pos[2];
		d.argb = argb;
		d.label = label;
		m_LastMarker = d;
		m_MarkerShown = show;
		ExorCofre.BroadcastMarker(d);
	}

	void ClearMarker()
	{
		SendMarker(false, 0, "");
	}

	void SyncMarkerToPlayer(PlayerBase pb)
	{
		if (!m_MarkerShown || !m_LastMarker || !pb || !pb.GetIdentity())
			return;
		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(m_LastMarker, false, data);
		pb.RPCSingleParam(ExorRPC.KOTH_SYNC, new Param1<string>(data), true, pb.GetIdentity());
	}

	bool IsOpenNow(ExorCfgCofre g, int nowMin, string today)
	{
		ExorCfgCofreLugar l = Cfg();
		if (!l || !l.activo)
			return false;
		ExorCfgCofreDia dia = FindDia(l, today);
		if (!dia || !dia.activado)
			return false;
		int s = ExorCofre.ParseHHMM(dia.hora_inicio);
		int e = ExorCofre.ParseHHMM(dia.hora_fin);
		return (s >= 0 && e > s && nowMin >= s && nowMin < e);
	}

	// ------------------------- props del evento -------------------------
	// estructura (mesa + drill + ...) + 4 bengalas fijas, mientras la ventana esta abierta.
	void SpawnEventProps(ExorCfgCofre g, vector pos)
	{
		if (!g.evento_estructura)
			return;
		RefreshEventProps(g, pos);
	}

	// self-heal: crea lo que falte y mantiene las bengalas encendidas.
	void RefreshEventProps(ExorCfgCofre g, vector pos)
	{
		if (!g.evento_estructura)
			return;
		if (!m_StructureObjs)
			m_StructureObjs = new array<Object>;
		if (!m_Lights)
			m_Lights = new array<Object>;

		int nObj = 0;
		if (g.estructura_objetos)
			nObj = g.estructura_objetos.Count();

		// El PRIMER objeto (mesa) es el ANCLA: se apoya en la superficie (ECE_PLACE_ON_SURFACE)
		// para que NO quede hundido. El resto se coloca RELATIVO a la posicion real de la mesa
		// (offset del set), asi el drill queda exactamente sobre la mesa este donde este el piso.
		if (m_StructureObjs.Count() == 0 && nObj > 0)
		{
			ExorCfgCofreObj o0 = g.estructura_objetos.Get(0);
			float ax = pos[0] + o0.dx;
			float az = pos[2] + o0.dz;
			// usar la Y de la ZONA (posicion.y que dio el admin) + altura_estructura, NO la
			// superficie del terreno: asi funciona tambien en pisos ELEVADOS (bases/edificios,
			// donde el terreno queda muy por debajo). ResolvePos ya resolvio pos[1] (posicion.y
			// si >0, o SurfaceY si el admin puso y=0).
			vector a = Vector(ax, pos[1] + g.altura_estructura, az);
			// crear en la posicion EXACTA (sin ECE_PLACE_ON_SURFACE) para que funcione en pisos
			// ELEVADOS: el snap al terreno tiraba la mesa muy abajo y no se reposicionaba.
			Object anchor = GetGame().CreateObjectEx(o0.classname, a, ECE_CREATEPHYSICS);
			if (anchor)
			{
				anchor.SetPosition(a);
				anchor.SetOrientation(Vector(o0.yaw, 0, 0));
				Print(string.Format("%1 COFRE: estructura '%2' creada, pos real=%3 (pedida=%4)", ExorStorageConstants.LOG, o0.classname, anchor.GetPosition(), a));
			}
			else
				Print(string.Format("%1 COFRE: NO se pudo crear estructura '%2'", ExorStorageConstants.LOG, o0.classname));
			m_StructureObjs.Insert(anchor);
		}
		while (m_StructureObjs.Count() < nObj)
		{
			int oi = m_StructureObjs.Count();
			ExorCfgCofreObj oc = g.estructura_objetos.Get(oi);
			ExorCfgCofreObj ob = g.estructura_objetos.Get(0);
			// pos real de la mesa (ancla); fallback a la Y de la zona + altura
			vector anchorPos = pos;
			anchorPos[1] = pos[1] + g.altura_estructura;
			if (m_StructureObjs.Get(0))
				anchorPos = m_StructureObjs.Get(0).GetPosition();
			vector op = anchorPos + Vector(oc.dx - ob.dx, oc.dy - ob.dy, oc.dz - ob.dz);
			Object so = GetGame().CreateObjectEx(oc.classname, op, ECE_CREATEPHYSICS);
			if (so)
			{
				so.SetPosition(op);	// posicion EXACTA sobre la mesa
				so.SetOrientation(Vector(oc.yaw, 0, 0));
				Print(string.Format("%1 COFRE: objeto '%2' creado, pos real=%3 (pedida=%4)", ExorStorageConstants.LOG, oc.classname, so.GetPosition(), op));
			}
			else
				Print(string.Format("%1 COFRE: NO se pudo crear objeto de estructura '%2'", ExorStorageConstants.LOG, oc.classname));
			m_StructureObjs.Insert(so);	// insertar aunque sea null: no reintentar cada tick
		}

		// LUCES (roadflares): lista `luces[]` con offset (respecto al PISO de la zona = pos.Y que
		// dio el admin) + orientacion (roll 90 = parada). Keep-alive throttle 15s (respawnea las
		// consumidas) + re-pin cada tick (para que la roadflare PARADA sobre el drill no se caiga).
		int nFl = 0;
		if (g.luces)
			nFl = g.luces.Count();
		int nowLm = GetGame().GetTime();
		if (nowLm - m_LightsCheckMs >= 15000)
		{
			m_LightsCheckMs = nowLm;
			int li;
			for (li = m_Lights.Count() - 1; li >= 0; li--)	// sacar las consumidas (null)
			{
				if (!m_Lights.Get(li))
					m_Lights.Remove(li);
			}
			while (m_Lights.Count() < nFl)
			{
				int idx = m_Lights.Count();
				ExorCfgCofreObj lz = g.luces.Get(idx);
				vector fp = pos + Vector(lz.dx, lz.dy, lz.dz);
				Object fo = GetGame().CreateObjectEx("Exor_CofreLight", fp, ECE_CREATEPHYSICS);
				if (fo)
				{
					fo.SetPosition(fp);
					fo.SetOrientation(Vector(lz.yaw, lz.pitch, lz.roll));
				}
				else
					Print(string.Format("%1 COFRE: NO se pudo crear luz Exor_CofreLight", ExorStorageConstants.LOG));
				m_Lights.Insert(fo);
			}
		}
		// re-pin cada tick: mantiene cada roadflare en su offset/orientacion (la parada no se cae)
		int rpi;
		for (rpi = 0; rpi < m_Lights.Count() && rpi < nFl; rpi++)
		{
			Object lo = m_Lights.Get(rpi);
			if (!lo)
				continue;
			ExorCfgCofreObj lz2 = g.luces.Get(rpi);
			lo.SetPosition(pos + Vector(lz2.dx, lz2.dy, lz2.dz));
			lo.SetOrientation(Vector(lz2.yaw, lz2.pitch, lz2.roll));
		}
	}

	void DespawnEventProps()
	{
		int i;
		if (m_StructureObjs)
		{
			for (i = 0; i < m_StructureObjs.Count(); i++)
			{
				if (m_StructureObjs.Get(i))
					GetGame().ObjectDelete(m_StructureObjs.Get(i));
			}
			m_StructureObjs.Clear();
		}
		if (m_Lights)
		{
			for (i = 0; i < m_Lights.Count(); i++)
			{
				if (m_Lights.Get(i))
					GetGame().ObjectDelete(m_Lights.Get(i));
			}
			m_Lights.Clear();
		}
		m_LightsCheckMs = 0;	// proximo evento re-spawnea las luces enseguida
		// liberar los slots de la mesa (las cajas/cofres flotantes se van por su deadline)
		if (m_TableSlots)
			m_TableSlots.Clear();
	}
}

// ============================================================================
// Manager: una zona por lugar + lista de cajas cerradas registradas.
// ============================================================================
class ExorCofre
{
	static ref ExorCofre s_Instance;
	static int s_LootCounter;	// contador rotativo para de-correlacionar el bundle de cofres que abren en el mismo tick

	static const int MARKER_ID_BASE = 5000;                 // offset de id de marca (no choca con KOTH)

	// color naranja de la marca/aviso (metodo: ARGB() no se puede usar en un const)
	static int MarkerArgb()
	{
		return ARGB(255, 255, 170, 40);
	}

	ref array<ref ExorCofreZone> m_Zones;
	ref array<Exor_Cofre_Packed_Base> m_Boxes;	// cajas cerradas registradas
	ref array<Exor_Cofre_Base> m_Chests;		// cofres abiertos registrados

	void ExorCofre()
	{
		m_Zones = new array<ref ExorCofreZone>;
		m_Boxes = new array<Exor_Cofre_Packed_Base>;
		m_Chests = new array<Exor_Cofre_Base>;
	}

	static ExorCofre Get()
	{
		if (!s_Instance)
			s_Instance = new ExorCofre();
		return s_Instance;
	}

	void RegisterBox(Exor_Cofre_Packed_Base b)
	{
		if (b && m_Boxes.Find(b) < 0)
			m_Boxes.Insert(b);
	}

	void UnregisterBox(Exor_Cofre_Packed_Base b)
	{
		int i = m_Boxes.Find(b);
		if (i >= 0)
			m_Boxes.Remove(i);
	}

	void RegisterChest(Exor_Cofre_Base c)
	{
		if (c && m_Chests.Find(c) < 0)
			m_Chests.Insert(c);
	}

	void UnregisterChest(Exor_Cofre_Base c)
	{
		int i = m_Chests.Find(c);
		if (i >= 0)
			m_Chests.Remove(i);
	}

	static void Start()
	{
		if (!GetGame().IsServer())
			return;
		ExorCfgCofre cfg = GetExorConfig().cofre;
		if (!cfg.activado)
		{
			Print(string.Format("%1 COFRE desactivado (cofre.json activado=false)", ExorStorageConstants.LOG));
			return;
		}
		ExorCofre self = Get();
		self.m_Zones.Clear();
		int n = 0;
		if (cfg.lugares)
			n = cfg.lugares.Count();
		int i;
		for (i = 0; i < n; i++)
			self.m_Zones.Insert(new ExorCofreZone(i));
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(self.Tick, 1000, true);
		// scan de la mesa cada minuto: despawnea cofres abiertos y vacios (barato para 50 players)
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(self.ScanTablesEmpty, 60000, true);
		// limpiar los cofres/cajas del evento que quedaron persistidos de la sesion anterior
		// (la mesa arranca vacia cada boot). Diferido para que las entidades ya esten cargadas.
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(self.CleanupOnStart, 15000, false);
		Print(string.Format("%1 COFRE iniciado (%2 zonas, abrir=%3min)", ExorStorageConstants.LOG, n, cfg.tiempo_abrir_un_cofre_minutos));
	}

	// Al arrancar: borra los cofres ABIERTOS (transitorios del evento) y las cajas CERRADAS
	// que quedaron sueltas dentro de una zona (estaban sobre la mesa / en el evento). Las
	// cajas en inventario de un jugador (con parent) NO se tocan. Asi la mesa arranca vacia.
	void CleanupOnStart()
	{
		if (!GetGame().IsServer())
			return;
		ExorCfgCofre cfg = GetExorConfig().cofre;
		int removed = 0;
		int i;
		for (i = m_Chests.Count() - 1; i >= 0; i--)
		{
			Exor_Cofre_Base c = m_Chests.Get(i);
			if (c)
			{
				c.Delete();
				removed++;
			}
		}
		for (i = m_Boxes.Count() - 1; i >= 0; i--)
		{
			Exor_Cofre_Packed_Base b = m_Boxes.Get(i);
			if (!b)
				continue;
			if (b.GetHierarchyParent() != null)	// en inventario/mochila -> caja legit del jugador
				continue;
			if (BoxInsideAnyZone(b.GetPosition(), cfg))
			{
				b.Delete();
				removed++;
			}
		}
		if (removed > 0)
			Print(string.Format("%1 COFRE: %2 objetos del evento limpiados al arrancar (mesa vacia)", ExorStorageConstants.LOG, removed));

		CleanFloorItemsInZones(cfg);
		CleanStrayFlares(cfg);
	}

	// Borra las luces (roadflares Exor_CofreLight / legacy Exor_CofreFlare) que quedaron
	// PERSISTIDAS de sesiones anteriores y NO son la del evento actual (no trackeadas en
	// m_Lights). Asi no se acumulan flares viejas apagadas alrededor de la mesa.
	void CleanStrayFlares(ExorCfgCofre cfg)
	{
		if (!cfg.lugares)
			return;
		// luces del evento actual (no tocarlas)
		array<Object> tracked = new array<Object>;
		int z;
		for (z = 0; z < m_Zones.Count(); z++)
		{
			ExorCofreZone zn = m_Zones.Get(z);
			if (zn && zn.m_Lights)
			{
				int k;
				for (k = 0; k < zn.m_Lights.Count(); k++)
				{
					if (zn.m_Lights.Get(k))
						tracked.Insert(zn.m_Lights.Get(k));
				}
			}
		}
		int removed = 0;
		int i;
		for (i = 0; i < cfg.lugares.Count(); i++)
		{
			ExorCfgCofreLugar l = cfg.lugares.Get(i);
			if (!l || !l.posicion)
				continue;
			vector zp = Vector(l.posicion.x, l.posicion.y, l.posicion.z);
			if (zp[1] <= 0)
				zp[1] = GetGame().SurfaceY(zp[0], zp[2]);
			array<Object> objs = new array<Object>;
			array<CargoBase> proxy = new array<CargoBase>;
			GetGame().GetObjectsAtPosition(zp, l.rango_efecto + 10, objs, proxy);
			int j;
			for (j = 0; j < objs.Count(); j++)
			{
				Object o = objs.Get(j);
				if (!o)
					continue;
				string t = o.GetType();
				if (t != "Exor_CofreLight" && t != "Exor_CofreFlare")
					continue;
				if (tracked.Find(o) >= 0)	// es la del evento actual -> no tocar
					continue;
				GetGame().ObjectDelete(o);
				removed++;
			}
		}
		if (removed > 0)
			Print(string.Format("%1 COFRE: %2 flares viejas (persistidas) limpiadas al arrancar", ExorStorageConstants.LOG, removed));
	}

	// Borra los items SUELTOS en el piso dentro de las zonas (loot viejo / testeo), sin tocar
	// los objetos del evento (estructura, luz, cajas/cofres) ni lo que este en inventarios.
	void CleanFloorItemsInZones(ExorCfgCofre cfg)
	{
		if (!cfg.lugares)
			return;
		int removed = 0;
		int i;
		for (i = 0; i < cfg.lugares.Count(); i++)
		{
			ExorCfgCofreLugar l = cfg.lugares.Get(i);
			if (!l || !l.activo || !l.posicion)
				continue;
			vector zp = Vector(l.posicion.x, l.posicion.y, l.posicion.z);
			if (zp[1] <= 0)
				zp[1] = GetGame().SurfaceY(zp[0], zp[2]);
			array<Object> objs = new array<Object>;
			array<CargoBase> proxy = new array<CargoBase>;
			GetGame().GetObjectsAtPosition(zp, l.rango_efecto, objs, proxy);
			int j;
			for (j = 0; j < objs.Count(); j++)
			{
				EntityAI e = EntityAI.Cast(objs.Get(j));
				if (!e)
					continue;
				if (e.GetHierarchyParent())	// en inventario/contenedor -> no tocar
					continue;
				if (!e.IsInherited(ItemBase))	// solo items sueltos (no estructuras/edificios)
					continue;
				if (ExorIsEventOwned(e.GetType(), cfg))	// no borrar objetos del evento
					continue;
				GetGame().ObjectDelete(e);
				removed++;
			}
		}
		if (removed > 0)
			Print(string.Format("%1 COFRE: %2 items sueltos del piso limpiados en las zonas al arrancar", ExorStorageConstants.LOG, removed));
	}

	// true si el classname es del evento (no borrarlo en la limpieza del piso)
	bool ExorIsEventOwned(string t, ExorCfgCofre cfg)
	{
		if (t.IndexOf("Exor_") == 0)
			return true;
		if (cfg.estructura_objetos)
		{
			int i;
			for (i = 0; i < cfg.estructura_objetos.Count(); i++)
			{
				ExorCfgCofreObj o = cfg.estructura_objetos.Get(i);
				if (o && o.classname == t)
					return true;
			}
		}
		return false;
	}

	bool BoxInsideAnyZone(vector pos, ExorCfgCofre cfg)
	{
		if (!cfg.lugares)
			return false;
		int i;
		for (i = 0; i < cfg.lugares.Count(); i++)
		{
			ExorCfgCofreLugar l = cfg.lugares.Get(i);
			if (!l || !l.posicion)
				continue;
			vector zp = Vector(l.posicion.x, l.posicion.y, l.posicion.z);
			if (zp[1] <= 0)
				zp[1] = GetGame().SurfaceY(zp[0], zp[2]);
			if (ExorCofre.HorizDist(pos, zp) <= l.rango_efecto)
				return true;
		}
		return false;
	}

	void Tick()
	{
		if (!GetGame().IsServer())
			return;
		ExorCfgCofre cfg = GetExorConfig().cofre;
		if (!cfg.activado)
			return;

		int minOfDay;
		int weekday;
		int dayKey;
		ExorCofre.NowLocal(cfg.offset_horas, minOfDay, weekday, dayKey);
		string today = ExorCofre.DayName(weekday);

		int i;
		for (i = 0; i < m_Zones.Count(); i++)
		{
			if (m_Zones.Get(i))
				m_Zones.Get(i).Tick(cfg, minOfDay, today, dayKey);
		}

		TickBoxes(cfg, minOfDay, today);
		TickDespawn();
	}

	// Despawn por DEADLINE de fin de evento (cada tick, barato: solo compara timestamps):
	//  - CAJA CERRADA o COFRE ABIERTO con loot: cuando pasa su deadline (reloj real, persiste).
	// (Los cofres abiertos VACIOS los limpia ScanTablesEmpty cada minuto.)
	void TickDespawn()
	{
		int nowRealMin = ExorTimeUtil.NowMinutes();
		int i;
		for (i = m_Boxes.Count() - 1; i >= 0; i--)
		{
			Exor_Cofre_Packed_Base b = m_Boxes.Get(i);
			if (b && b.m_ExorDespawnAtMin > 0 && nowRealMin >= b.m_ExorDespawnAtMin)
			{
				Print(string.Format("%1 COFRE: caja despawneada (fin de evento) en %2", ExorStorageConstants.LOG, b.GetPosition()));
				b.Delete();
			}
		}
		for (i = m_Chests.Count() - 1; i >= 0; i--)
		{
			Exor_Cofre_Base c = m_Chests.Get(i);
			if (c && c.m_ExorDespawnAtMin > 0 && nowRealMin >= c.m_ExorDespawnAtMin)
			{
				Print(string.Format("%1 COFRE: cofre abierto con loot despawneado (fin de evento) en %2", ExorStorageConstants.LOG, c.GetPosition()));
				ReleaseChestSlot(c);
				c.Delete();
			}
		}
	}

	// Scan de la MESA cada minuto: despawnea los cofres ABIERTOS y VACIOS (sin items). Barato
	// (solo itera la lista de cofres, que es chica) -> sin lag aunque haya 50 players.
	void ScanTablesEmpty()
	{
		if (!GetGame().IsServer())
			return;
		if (!GetExorConfig().cofre.activado)
			return;
		int removed = 0;
		int i;
		for (i = m_Chests.Count() - 1; i >= 0; i--)
		{
			Exor_Cofre_Base c = m_Chests.Get(i);
			if (!c)
				continue;
			if (c.ExorCargoCount() == 0)	// abierto y sin loot -> despawnear + liberar el slot
			{
				ReleaseChestSlot(c);
				c.Delete();
				removed++;
			}
		}
		if (removed > 0)
			Print(string.Format("%1 COFRE: %2 cofres vacios despawneados (scan de mesa)", ExorStorageConstants.LOG, removed));
	}

	// libera el slot de la mesa que ocupaba el cofre abierto (si tenia)
	void ReleaseChestSlot(Exor_Cofre_Base c)
	{
		if (!c || c.m_ExorSlotIdx < 0)
			return;
		if (c.m_ExorSlotZone >= 0 && c.m_ExorSlotZone < m_Zones.Count() && m_Zones.Get(c.m_ExorSlotZone))
			m_Zones.Get(c.m_ExorSlotZone).FreeSlot(c.m_ExorSlotIdx);
		c.m_ExorSlotIdx = -1;
		c.m_ExorSlotZone = -1;
	}

	// Al vencer la gracia: borra TODAS las cajas/cofres dentro de la zona.
	void DeleteCofresInZone(int zoneIdx)
	{
		if (zoneIdx < 0 || zoneIdx >= m_Zones.Count())
			return;
		ExorCofreZone z = m_Zones.Get(zoneIdx);
		if (!z)
			return;
		ExorCfgCofreLugar l = z.Cfg();
		if (!l)
			return;
		vector zp = z.ResolvePos(l);
		int removed = 0;
		int i;
		for (i = m_Boxes.Count() - 1; i >= 0; i--)
		{
			Exor_Cofre_Packed_Base b = m_Boxes.Get(i);
			if (b && ExorCofre.HorizDist(b.GetPosition(), zp) <= l.rango_efecto)
			{
				b.Delete();
				removed++;
			}
		}
		for (i = m_Chests.Count() - 1; i >= 0; i--)
		{
			Exor_Cofre_Base c = m_Chests.Get(i);
			if (c && ExorCofre.HorizDist(c.GetPosition(), zp) <= l.rango_efecto)
			{
				ReleaseChestSlot(c);
				c.Delete();
				removed++;
			}
		}
		if (removed > 0)
			Print(string.Format("%1 COFRE: %2 cajas/cofres despawneados al vencer la gracia", ExorStorageConstants.LOG, removed));
	}

	// Coloca las cajas sobre la MESA (hasta mesa_max_cajas) y corre su timer. Cuando una
	// caja esta en una zona abierta con mesa: se asigna a un slot libre, se fija sobre la
	// mesa (re-pin cada tick) y cuenta; al cumplirse el tiempo se transforma en el slot.
	// Si no hay mesa (evento_estructura=false), cae al comportamiento viejo (soltar en piso).
	void TickBoxes(ExorCfgCofre cfg, int minOfDay, string today)
	{
		int nowMs = GetGame().GetTime();
		int openMs = cfg.tiempo_abrir_un_cofre_minutos * 60 * 1000;
		array<Exor_Cofre_Packed_Base> toOpen = new array<Exor_Cofre_Packed_Base>;
		int i;
		for (i = 0; i < m_Boxes.Count(); i++)
		{
			Exor_Cofre_Packed_Base b = m_Boxes.Get(i);
			if (!b)
				continue;
			bool grounded = (b.GetHierarchyParent() == null);
			int zi = -1;
			if (grounded)
				zi = ActiveZoneIndex(b.GetPosition(), cfg, minOfDay, today, nowMs);

			// fuera de zona activa (o en mochila/manos): liberar slot y reiniciar contador
			if (zi < 0)
			{
				ReleaseBoxSlot(b);
				b.m_ExorDeployStartMs = 0;
				continue;
			}

			ExorCofreZone z = m_Zones.Get(zi);
			bool usarMesa = (cfg.evento_estructura && z.HasTable());

			// asignar un slot de la mesa si corresponde y todavia no tiene
			if (usarMesa && b.m_ExorSlotIdx < 0)
			{
				// caja NUEVA: solo se acepta si la zona esta ABIERTA (no durante la gracia)
				if (!z.IsOpenNow(cfg, minOfDay, today))
				{
					b.m_ExorDeployStartMs = 0;
					continue;
				}
				int slot = z.FreeSlotIndex(cfg);
				if (slot < 0)
				{
					b.m_ExorDeployStartMs = 0;	// mesa llena -> no cuenta
					continue;
				}
				b.m_ExorSlotIdx = slot;
				b.m_ExorSlotZone = zi;
				z.SetSlot(slot, b);
				b.ExorSetOnTable(true);	// ya no se puede tomar
				b.m_ExorDeployStartMs = nowMs;
			}

			// fijar la caja sobre la mesa (re-pin: el item dropeado tiende a caerse)
			if (b.m_ExorSlotIdx >= 0)
			{
				b.SetPosition(z.SlotPos(b.m_ExorSlotIdx, cfg));
				b.SetOrientation(Vector(0, 0, 0));
			}

			// contar
			if (b.m_ExorDeployStartMs == 0)
				b.m_ExorDeployStartMs = nowMs;
			else if (openMs <= 0 || nowMs - b.m_ExorDeployStartMs >= openMs)
				toOpen.Insert(b);
		}
		for (i = 0; i < toOpen.Count(); i++)
			OpenBox(toOpen.Get(i), cfg);
	}

	// libera el slot de la mesa que ocupaba la caja (si tenia)
	void ReleaseBoxSlot(Exor_Cofre_Packed_Base b)
	{
		if (b.m_ExorSlotIdx < 0)
			return;
		if (b.m_ExorSlotZone >= 0 && b.m_ExorSlotZone < m_Zones.Count() && m_Zones.Get(b.m_ExorSlotZone))
			m_Zones.Get(b.m_ExorSlotZone).FreeSlot(b.m_ExorSlotIdx);
		b.m_ExorSlotIdx = -1;
		b.m_ExorSlotZone = -1;
		b.ExorSetOnTable(false);	// se puede volver a tomar
	}

	// indice de la zona ACTIVA (abierta O en gracia) que contiene 'pos' (radio horizontal), o -1.
	int ActiveZoneIndex(vector pos, ExorCfgCofre cfg, int minOfDay, string today, int nowMs)
	{
		int i;
		for (i = 0; i < m_Zones.Count(); i++)
		{
			ExorCofreZone z = m_Zones.Get(i);
			if (!z)
				continue;
			if (!z.IsOpenNow(cfg, minOfDay, today) && !z.IsGraceActive(nowMs))
				continue;
			ExorCfgCofreLugar l = z.Cfg();
			if (!l)
				continue;
			if (ExorCofre.HorizDist(pos, z.ResolvePos(l)) <= l.rango_efecto)
				return i;
		}
		return -1;
	}

	// Transforma la caja cerrada en el cofre abierto (en el mismo slot de la mesa) y lo
	// rellena con un bundle random.
	void OpenBox(Exor_Cofre_Packed_Base b, ExorCfgCofre cfg)
	{
		if (!b)
			return;
		string openType = b.ExorGetOpenType();
		string color = b.ExorGetColor();
		int slot = b.m_ExorSlotIdx;
		int zi = b.m_ExorSlotZone;
		vector pos = b.GetPosition();
		if (slot >= 0 && zi >= 0 && zi < m_Zones.Count() && m_Zones.Get(zi))
			pos = m_Zones.Get(zi).SlotPos(slot, cfg);
		if (openType == "")
			return;

		EntityAI chest = EntityAI.Cast(GetGame().CreateObjectEx(openType, pos, ECE_PLACE_ON_SURFACE));
		if (!chest)
		{
			Print(string.Format("%1 COFRE: no se pudo crear %2", ExorStorageConstants.LOG, openType));
			return;
		}
		chest.SetPosition(pos);	// exacto en el slot (sobre la mesa)
		// el contenedor cerrado rechaza el cargo -> abrir antes de rellenar (igual que el KothCrate)
		Barrel_ColorBase brl = Barrel_ColorBase.Cast(chest);
		if (brl)
			brl.Open();

		int puestos = FillLoot(chest, color);

		// el cofre abierto hereda el slot de la caja
		Exor_Cofre_Base ec = Exor_Cofre_Base.Cast(chest);
		if (ec && slot >= 0 && zi >= 0 && zi < m_Zones.Count() && m_Zones.Get(zi))
		{
			ec.m_ExorSlotIdx = slot;
			ec.m_ExorSlotZone = zi;
			m_Zones.Get(zi).SetSlot(slot, ec);
		}

		b.Delete();
		Print(string.Format("%1 COFRE abierto (%2) en %3 con %4 items", ExorStorageConstants.LOG, color, pos, puestos));
	}

	// Elige UN bundle al azar de la tabla del color y spawnea todos sus classnames.
	int FillLoot(EntityAI chest, string color)
	{
		ExorCfgCofre cfg = GetExorConfig().cofre;
		ExorCfgCofreDef def = FindDef(cfg, color);
		if (!def || !def.items || def.items.Count() == 0)
			return 0;
		int nb = def.items.Count();
		// pick random + contador rotativo: DayZ Math.RandomInt puede devolver el mismo valor
		// varias veces en el MISMO frame (varias cajas abriendo en el mismo tick) -> el contador
		// asegura que aperturas consecutivas caigan en bundles distintos.
		int pick = (Math.RandomInt(0, nb) + ExorCofre.s_LootCounter) % nb;
		ExorCofre.s_LootCounter++;
		if (pick < 0)
			pick = 0;
		if (pick >= nb)
			pick = nb - 1;
		TStringArray bundle = def.items.GetElement(pick);
		if (!bundle)
			return 0;
		int puestos = 0;
		int i;
		for (i = 0; i < bundle.Count(); i++)
		{
			string entry = bundle.Get(i);
			if (entry == "")
				continue;
			// formato "classname" (1) o "classname:cantidad" (ej. "M67Grenade:20")
			string cls = entry;
			int count = 1;
			int cc = entry.IndexOf(":");
			if (cc >= 0)
			{
				cls = entry.Substring(0, cc);
				count = entry.Substring(cc + 1, entry.Length() - (cc + 1)).ToInt();
				if (count < 1)
					count = 1;
			}
			int n;
			for (n = 0; n < count; n++)
			{
				EntityAI e = chest.GetInventory().CreateInInventory(cls);
				if (e)
					puestos++;
				else
				{
					Print(string.Format("%1 COFRE: item NO entro (classname invalido / mod no cargado / lleno): %2", ExorStorageConstants.LOG, cls));
					break;	// no seguir repitiendo el mismo item si no entra (lleno/invalido)
				}
			}
		}
		return puestos;
	}

	ExorCfgCofreDef FindDef(ExorCfgCofre cfg, string color)
	{
		if (!cfg.cofres)
			return null;
		int i;
		for (i = 0; i < cfg.cofres.Count(); i++)
		{
			ExorCfgCofreDef d = cfg.cofres.Get(i);
			if (d && d.color == color)
				return d;
		}
		return null;
	}

	void SyncMarkersToPlayer(PlayerBase pb)
	{
		int i;
		for (i = 0; i < m_Zones.Count(); i++)
		{
			if (m_Zones.Get(i))
				m_Zones.Get(i).SyncMarkerToPlayer(pb);
		}
	}

	// ------------------------- helpers estaticos -------------------------
	// Reloj local del server ajustado por offset_horas: minuto-del-dia (0..1439),
	// dia de la semana (0=domingo..6=sabado) y una clave de dia monotona.
	static void NowLocal(int offsetHoras, out int minOfDay, out int weekday, out int dayKey)
	{
		int y, mo, d;
		GetYearMonthDay(y, mo, d);
		int h, mi, s;
		GetHourMinuteSecond(h, mi, s);
		int total = (h * 60) + mi + (offsetHoras * 60);
		int dayShift = 0;
		while (total < 0)
		{
			total = total + 1440;
			dayShift = dayShift - 1;
		}
		while (total >= 1440)
		{
			total = total - 1440;
			dayShift = dayShift + 1;
		}
		minOfDay = total;
		int dow = ExorCofre.Sakamoto(y, mo, d) + dayShift;
		dow = dow % 7;
		if (dow < 0)
			dow = dow + 7;
		weekday = dow;
		dayKey = ExorTimeUtil.DayNumber(y, mo, d) + dayShift;
	}

	// dia de la semana (0=domingo..6=sabado). Algoritmo de Sakamoto.
	static int Sakamoto(int y, int m, int d)
	{
		array<int> t = new array<int>;
		t.Insert(0); t.Insert(3); t.Insert(2); t.Insert(5); t.Insert(0); t.Insert(3);
		t.Insert(5); t.Insert(1); t.Insert(4); t.Insert(6); t.Insert(2); t.Insert(4);
		int yy = y;
		if (m < 3)
			yy = yy - 1;
		int r = (yy + (yy / 4) - (yy / 100) + (yy / 400) + t.Get(m - 1) + d) % 7;
		if (r < 0)
			r = r + 7;
		return r;
	}

	static string DayName(int weekday)
	{
		if (weekday == 0) return "domingo";
		if (weekday == 1) return "lunes";
		if (weekday == 2) return "martes";
		if (weekday == 3) return "miercoles";
		if (weekday == 4) return "jueves";
		if (weekday == 5) return "viernes";
		return "sabado";
	}

	// "HH:MM" -> minutos desde medianoche. -1 si esta mal formado.
	static int ParseHHMM(string hhmm)
	{
		int c = hhmm.IndexOf(":");
		if (c < 0)
			return -1;
		string hh = hhmm.Substring(0, c);
		string mm = hhmm.Substring(c + 1, hhmm.Length() - (c + 1));
		return (hh.ToInt() * 60) + mm.ToInt();
	}

	// distancia HORIZONTAL (plano XZ, ignora altura)
	static float HorizDist(vector a, vector b)
	{
		vector d = a - b;
		d[1] = 0;
		return d.Length();
	}

	static int CountPlayersNear(vector pos, float radius)
	{
		int cnt = 0;
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase pb = PlayerBase.Cast(players.Get(i));
			if (!pb || !pb.IsAlive())
				continue;
			if (vector.Distance(pb.GetPosition(), pos) <= radius)
				cnt++;
		}
		return cnt;
	}

	// aviso killfeed a TODOS (reusa el DTO/RPC del killfeed; 1er palabra en color argb)
	static void Alert(string msg, int durSec, int argb)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		ExorKfDTO dto = new ExorKfDTO();
		dto.msg = msg;
		dto.msgargb = argb;
		dto.dur = durSec;
		dto.max = 5;
		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(dto, false, data);
		Param1<string> p = new Param1<string>(data);
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase pb = PlayerBase.Cast(players.Get(i));
			if (pb && pb.GetIdentity())
				pb.RPCSingleParam(ExorRPC.KILLFEED, p, true, pb.GetIdentity());
		}
		Print(string.Format("%1 COFRE: %2", ExorStorageConstants.LOG, msg));
	}

	static void BroadcastMarker(ExorKothMarkerDTO d)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(d, false, data);
		Param1<string> p = new Param1<string>(data);
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase pb = PlayerBase.Cast(players.Get(i));
			if (pb && pb.GetIdentity())
				pb.RPCSingleParam(ExorRPC.KOTH_SYNC, p, true, pb.GetIdentity());
		}
	}
}
