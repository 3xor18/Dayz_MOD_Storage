// ============================================================================
// 3xor_Vanilla_Optimization - Party: manager de grupos (SOLO server, Fase B)
// Crea/borra grupos, persiste a groups/<id>.json, maneja invitar/aceptar/
// rechazar/salir/kick, sincroniza el roster a los clientes y aplica auto-kick.
// El lider (owner) es el dueno del territorio: si sale, el party se disuelve.
// ============================================================================
class ExorGroupManager
{
	static ref ExorGroupManager s_Instance;

	ref array<ref ExorGroup> m_Groups;
	ref map<string, ref ExorPendingInvite> m_PendingInvites;	// key = steamid del invitado

	void ExorGroupManager()
	{
		m_Groups = new array<ref ExorGroup>;
		m_PendingInvites = new map<string, ref ExorPendingInvite>;
	}

	static ExorGroupManager Get()
	{
		if (!s_Instance)
			s_Instance = new ExorGroupManager();
		return s_Instance;
	}

	static void Init()
	{
		if (!GetGame().IsServer())
			return;
		Get().LoadAll();
	}

	// ------------------------- helpers de identidad -------------------------
	static string SteamId(PlayerBase p)
	{
		if (p && p.GetIdentity())
			return p.GetIdentity().GetPlainId();
		return "";
	}

	static string PlayerName(PlayerBase p)
	{
		if (p && p.GetIdentity())
			return p.GetIdentity().GetName();
		return "";
	}

	PlayerBase FindOnline(string sid)
	{
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase pb = PlayerBase.Cast(players.Get(i));
			if (pb && pb.GetIdentity() && pb.GetIdentity().GetPlainId() == sid)
				return pb;
		}
		return null;
	}

	// Indice steamid->PlayerBase de TODOS los online, armado de UNA pasada. Para los
	// caminos calientes (ej. ExorPartyLive.Tick a 4 Hz) que si no llamarian FindOnline
	// -> GetPlayers() decenas de veces por tick. Se arma 1 vez y se consulta O(1).
	void BuildOnlineIndex(map<string, PlayerBase> idx)
	{
		if (!idx)
			return;
		idx.Clear();
		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase pb = PlayerBase.Cast(players.Get(i));
			if (pb && pb.GetIdentity())
				idx.Set(pb.GetIdentity().GetPlainId(), pb);
		}
	}

	// ------------------------- busqueda -------------------------
	ExorGroup FindByPlayer(string sid)
	{
		int i;
		for (i = 0; i < m_Groups.Count(); i++)
		{
			if (m_Groups.Get(i).HasMember(sid))
				return m_Groups.Get(i);
		}
		return null;
	}

	ExorGroup FindById(string id)
	{
		int i;
		for (i = 0; i < m_Groups.Count(); i++)
		{
			if (m_Groups.Get(i).id == id)
				return m_Groups.Get(i);
		}
		return null;
	}

	// true si ambos steamids estan en el MISMO grupo (clan/party/territorio = mismo
	// mastil). Un solo lookup, solo se llama al morir alguien -> sin costo en performance.
	// Lo usa el Score para NO contar teamkills (kill del asesino / death de la victima),
	// aunque el kill SI se muestre en el killfeed.
	bool SameParty(string sidA, string sidB)
	{
		if (sidA == "" || sidB == "")
			return false;
		ExorGroup g = FindByPlayer(sidA);
		return g != null && g.HasMember(sidB);
	}

	// ------------------------- persistencia -------------------------
	string GroupPath(string id)
	{
		return string.Format("%1\\%2.json", ExorStorageConstants.GROUPS_DIR, id);
	}

	void SaveGroup(ExorGroup g)
	{
		if (!FileExist(ExorStorageConstants.GROUPS_DIR))
			MakeDirectory(ExorStorageConstants.GROUPS_DIR);
		JsonFileLoader<ExorGroup>.JsonSaveFile(GroupPath(g.id), g);
	}

	void LoadAll()
	{
		if (!FileExist(ExorStorageConstants.GROUPS_DIR))
		{
			MakeDirectory(ExorStorageConstants.GROUPS_DIR);
			return;
		}

		string pattern = string.Format("%1\\*.json", ExorStorageConstants.GROUPS_DIR);
		string fileName;
		FileAttr attr;
		FindFileHandle h = FindFile(pattern, fileName, attr, FindFileFlags.ALL);
		if (h)
		{
			if (fileName != "")
				LoadGroupFile(fileName);
			while (FindNextFile(h, fileName, attr))
			{
				if (fileName != "")
					LoadGroupFile(fileName);
			}
			CloseFindFile(h);
		}
		Print(string.Format("%1 Party: %2 grupos cargados de disco", ExorStorageConstants.LOG, m_Groups.Count()));
	}

	void LoadGroupFile(string fileName)
	{
		string full = string.Format("%1\\%2", ExorStorageConstants.GROUPS_DIR, fileName);
		ExorGroup g = new ExorGroup();
		JsonFileLoader<ExorGroup>.JsonLoadFile(full, g);
		if (g.id == "" || g.owner_id == "")
			return;
		// Grupo marcado como borrado (disuelto/eliminado): NO se carga como activo -> sus miembros
		// quedan libres y su mastil NO se recrea. El FILE se conserva en disco (baneo de clan).
		if (g.borrado == 1)
			return;
		m_Groups.Insert(g);
	}

	void DeleteGroup(ExorGroup g)
	{
		int idx = m_Groups.Find(g);
		if (idx != -1)
			m_Groups.Remove(idx);
		string path = GroupPath(g.id);
		if (FileExist(path))
			DeleteFile(path);
	}

	// ------------------------- sincronizacion al cliente -------------------------
	void SyncToPlayer(PlayerBase p, ExorGroup g)
	{
		if (!p || !p.GetIdentity())
			return;

		ExorRosterDTO dto = new ExorRosterDTO();
		if (g)
		{
			dto.group_id = g.id;
			dto.owner_id = g.owner_id;
			dto.you_are_owner = (g.owner_id == SteamId(p));
			int i;
			for (i = 0; i < g.members.Count(); i++)
			{
				ExorGroupMember src = g.members.Get(i);
				ExorGroupMember cp = new ExorGroupMember();
				cp.steamid = src.steamid;
				cp.name = src.name;
				cp.last_seen_day = src.last_seen_day;
				dto.members.Insert(cp);
			}
		}

		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(dto, false, data);
		ExorNetChunk.Send(p, p.GetIdentity(), ExorRPC.ROSTER_SYNC, data);
	}

	void SyncGroup(ExorGroup g)
	{
		if (!g)
			return;
		int i;
		for (i = 0; i < g.members.Count(); i++)
		{
			PlayerBase pb = FindOnline(g.members.Get(i).steamid);
			if (pb)
				SyncToPlayer(pb, g);
		}
	}

	// ------------------------- ciclo de vida del grupo -------------------------
	ExorGroup CreateGroup(PlayerBase owner)
	{
		if (!owner || !owner.GetIdentity())
			return null;
		string sid = SteamId(owner);

		ExorGroup existing = FindByPlayer(sid);
		if (existing)
			return existing;

		ExorGroup g = new ExorGroup();
		g.id = ExorVO_Serializer.GenerateId();
		g.owner_id = sid;
		g.AddOrUpdate(sid, PlayerName(owner), ExorTimeUtil.TodayNumber());
		m_Groups.Insert(g);

		owner.ExorSetGroupId(g.id);
		SaveGroup(g);
		SyncToPlayer(owner, g);
		Print(string.Format("%1 Party: grupo %2 creado por %3", ExorStorageConstants.LOG, g.id, PlayerName(owner)));
		return g;
	}

	void DisbandGroup(ExorGroup g, string reason, string bySteamId = "")
	{
		// Volcar el roster al log forense (miembros/ex-miembros: sirve para banear un clan).
		ExorLogDisband(g, reason);
		int i;
		for (i = 0; i < g.members.Count(); i++)
		{
			PlayerBase pb = FindOnline(g.members.Get(i).steamid);
			if (pb)
			{
				pb.ExorSetGroupId("");
				SyncToPlayer(pb, null);
				ExorPartyLive.Get().ClearForPlayer(pb);	// limpiar HUD/nameplates/marcas al disolverse
				pb.MessageImportant(reason);
			}
		}
		// NO se borra el file: se marca borrado=1 (se conserva el roster para baneo de clan).
		MarkGroupDeleted(g, bySteamId);
		// Hook para la Fase C: despawnear el mastil del territorio.
		ExorOnGroupDisbanded(g);
	}

	// Marca un grupo como disuelto/eliminado SIN borrar el file: borrado=1 + quien + cuando,
	// lo guarda y lo saca de la lista ACTIVA (deja de existir para miembros/mastil/zonas). El
	// file queda en disco para poder banear a un clan entero despues (miembros + ex-miembros).
	void MarkGroupDeleted(ExorGroup g, string bySteamId)
	{
		if (!g)
			return;
		g.borrado = 1;
		g.borrado_por = bySteamId;
		g.borrado_fecha = ExorRaidLog.TimeStamp();
		SaveGroup(g);	// persiste el file con la marca (NO se borra del disco)
		int idx = m_Groups.Find(g);
		if (idx != -1)
			m_Groups.Remove(idx);
		Print(string.Format("%1 Party: grupo %2 marcado BORRADO por %3 (%4) -> file conservado", ExorStorageConstants.LOG, g.id, bySteamId, g.borrado_fecha));
	}

	// Fase C: al disolverse el grupo, despawnear su mastil (si quedo alguno).
	void ExorOnGroupDisbanded(ExorGroup g)
	{
		if (!GetGame().IsServer() || !g)
			return;
		ExorTerritoryManager.Get().DespawnMastForGroup(g.id);
	}

	// El OBJETO-bandera desaparecio SIN que el owner disolviera el party (bug/CE/crash:
	// el TerritoryFlag se borro solo, normalmente sin nadie en la base). A diferencia de
	// DisbandGroup, esto NO borra el grupo: el party se CONSERVA y el mastil se auto-restaura
	// (al reconectar un miembro, o diferido si hay alguno online). Solo se pierde temporalmente
	// la proteccion de territorio hasta que el mastil se recrea.
	void MarkMastLost(ExorGroup g, string reason)
	{
		if (!GetGame().IsServer() || !g)
			return;
		g.mast_lost = 1;
		SaveGroup(g);

		// log forense (evento propio, NO CLAN_DISUELTO: el clan NO se disolvio)
		string ownerName = "?";
		ExorGroupMember owner = g.FindMember(g.owner_id);
		if (owner)
			ownerName = owner.name;
		vector basePos = Vector(g.mast_x, g.mast_y, g.mast_z);
		ExorRaidLog.Write("MASTIL_PERDIDO", g.owner_id, ownerName,
			basePos, string.Format("clan %1 (lider %2): %3 | el party se CONSERVA, el mastil se auto-restaura", g.id, ownerName, reason));

		// avisar a los miembros online (sin limpiar su party)
		int i;
		for (i = 0; i < g.members.Count(); i++)
		{
			PlayerBase pb = FindOnline(g.members.Get(i).steamid);
			if (pb)
				pb.MessageImportant("Tu mástil desapareció (bug del servidor). Tu party sigue activo; el mástil se restaurará solo (reconectá si no aparece).");
		}

		Print(string.Format("%1 Party: MASTIL PERDIDO del grupo %2 -> party conservado, pendiente de auto-restaurar (%3)", ExorStorageConstants.LOG, g.id, reason));

		// intento de restauracion diferido: si hay algun miembro online sirve de "builder".
		// Diferido (no en el mismo frame del borrado) para no chocar con la pasada del CE
		// que pudo haber borrado la bandera.
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(TryRestoreGroupMast, 5000, false, g.id);
	}

	// Intenta recrear el mastil perdido de un grupo usando cualquier miembro online como
	// builder. Si no hay ninguno online, no hace nada (se restaura al reconectar un miembro).
	void TryRestoreGroupMast(string groupId)
	{
		if (!GetGame().IsServer())
			return;
		ExorGroup g = FindById(groupId);
		if (!g || g.mast_lost != 1)
			return;
		int i;
		for (i = 0; i < g.members.Count(); i++)
		{
			PlayerBase pb = FindOnline(g.members.Get(i).steamid);
			if (pb)
			{
				ExorTerritoryManager.Get().HealGroupMast(g, pb);
				return;
			}
		}
	}

	// ------------------------- invitaciones -------------------------
	bool Invite(PlayerBase leader, PlayerBase target)
	{
		if (!GetGame().IsServer())
			return false;
		if (!leader || !target || !target.GetIdentity())
			return false;

		ExorGroup g = FindByPlayer(SteamId(leader));
		if (!g)
		{
			leader.MessageImportant("No tenes un party. Reclama territorio con un mastil primero.");
			return false;
		}
		if (!g.IsOwner(SteamId(leader)))
		{
			leader.MessageImportant("Solo el lider del party puede invitar.");
			return false;
		}

		string tsid = SteamId(target);
		if (g.HasMember(tsid))
		{
			leader.MessageImportant("Esa persona ya esta en tu party.");
			return false;
		}
		int max = GetExorConfig().party.grupo.max_miembros;
		if (g.members.Count() >= max)
		{
			leader.MessageImportant(string.Format("Party lleno (max %1).", max));
			return false;
		}

		ExorPendingInvite inv = new ExorPendingInvite();
		inv.group_id = g.id;
		inv.inviter_name = PlayerName(leader);
		inv.created_ms = GetGame().GetTime();
		m_PendingInvites.Set(tsid, inv);

		target.RPCSingleParam(ExorRPC.INVITE, new Param2<string, string>(g.id, inv.inviter_name), true, target.GetIdentity());
		target.MessageImportant(string.Format("%1 te invito a su party. Abri el menu (P) para aceptar.", inv.inviter_name));
		leader.MessageImportant(string.Format("Invitacion enviada a %1.", PlayerName(target)));
		return true;
	}

	bool AcceptInvite(PlayerBase target)
	{
		if (!GetGame().IsServer() || !target)
			return false;
		string tsid = SteamId(target);

		ExorPendingInvite inv;
		if (!m_PendingInvites.Find(tsid, inv))
		{
			target.MessageImportant("No tenes invitaciones pendientes.");
			return false;
		}
		if (GetGame().GetTime() - inv.created_ms > 120000)
		{
			m_PendingInvites.Remove(tsid);
			target.MessageImportant("La invitacion expiro.");
			return false;
		}

		if (FindByPlayer(tsid))
		{
			target.MessageImportant("Sali de tu party actual antes de aceptar otro.");
			return false;
		}

		ExorGroup g = FindById(inv.group_id);
		if (!g)
		{
			m_PendingInvites.Remove(tsid);
			target.MessageImportant("El party ya no existe.");
			return false;
		}
		int max = GetExorConfig().party.grupo.max_miembros;
		if (g.members.Count() >= max)
		{
			m_PendingInvites.Remove(tsid);
			target.MessageImportant("El party esta lleno.");
			return false;
		}

		g.AddOrUpdate(tsid, PlayerName(target), ExorTimeUtil.TodayNumber());
		target.ExorSetGroupId(g.id);
		m_PendingInvites.Remove(tsid);
		SaveGroup(g);
		SyncGroup(g);

		target.MessageImportant("Te uniste al party.");
		PlayerBase leader = FindOnline(g.owner_id);
		if (leader)
			leader.MessageImportant(string.Format("%1 acepto y se unio al party.", PlayerName(target)));
		return true;
	}

	void DeclineInvite(PlayerBase target)
	{
		if (!GetGame().IsServer() || !target)
			return;
		string tsid = SteamId(target);
		ExorPendingInvite inv;
		if (m_PendingInvites.Find(tsid, inv))
		{
			m_PendingInvites.Remove(tsid);
			PlayerBase leader = FindOnline(ExorFindLeaderOfGroup(inv.group_id));
			if (leader)
				leader.MessageImportant(string.Format("%1 rechazo la invitacion.", PlayerName(target)));
		}
		target.MessageImportant("Invitacion rechazada.");
	}

	string ExorFindLeaderOfGroup(string groupId)
	{
		ExorGroup g = FindById(groupId);
		if (g)
			return g.owner_id;
		return "";
	}

	// ------------------------- invitacion abierta (en el mastil) -------------------------
	// Un MIEMBRO abre la invitacion mirando el mastil. Mientras esta abierta (10 min
	// o hasta que entre alguien), un no-miembro puede 'Unirse al grupo' en el mastil.
	void OpenInviteAtMast(PlayerBase member, TerritoryFlag mast)
	{
		if (!GetGame().IsServer() || !member || !mast)
			return;
		Print(string.Format("%1 OpenInviteAtMast: actor=%2 mast_group=%3", ExorStorageConstants.LOG, SteamId(member), mast.ExorGetGroupId()));
		ExorGroup g = FindById(mast.ExorGetGroupId());
		if (!g || !g.HasMember(SteamId(member)))
		{
			member.MessageImportant("No sos miembro de este party.");
			return;
		}
		int max = GetExorConfig().party.grupo.max_miembros;
		if (g.members.Count() >= max)
		{
			member.MessageImportant(string.Format("Party lleno (max %1).", max));
			return;
		}
		mast.ExorOpenInvite();
		member.MessageImportant("Invitacion abierta (10 min). Otro jugador puede 'Unirse al grupo' mirando el mastil.");
	}

	void CancelInviteAtMast(PlayerBase member, TerritoryFlag mast)
	{
		if (!GetGame().IsServer() || !member || !mast)
			return;
		ExorGroup g = FindById(mast.ExorGetGroupId());
		if (!g || !g.HasMember(SteamId(member)))
			return;
		mast.ExorCloseInvite();
		member.MessageImportant("Invitacion cancelada.");
	}

	void JoinAtMast(PlayerBase joiner, TerritoryFlag mast)
	{
		if (!GetGame().IsServer() || !joiner || !mast)
			return;
		if (!mast.ExorIsInviteOpen())
		{
			joiner.MessageImportant("No hay invitacion abierta en este mastil.");
			return;
		}
		string jsid = SteamId(joiner);
		if (FindByPlayer(jsid))
		{
			joiner.MessageImportant("Ya estas en un party. Sali primero.");
			return;
		}
		ExorGroup g = FindById(mast.ExorGetGroupId());
		if (!g)
		{
			joiner.MessageImportant("El party ya no existe.");
			return;
		}
		int max = GetExorConfig().party.grupo.max_miembros;
		if (g.members.Count() >= max)
		{
			joiner.MessageImportant("El party esta lleno.");
			return;
		}
		g.AddOrUpdate(jsid, PlayerName(joiner), ExorTimeUtil.TodayNumber());
		joiner.ExorSetGroupId(g.id);
		mast.ExorCloseInvite();	// una invitacion = un ingreso; reabrir para invitar a otro
		SaveGroup(g);
		SyncGroup(g);
		joiner.MessageImportant("Te uniste al party.");
		PlayerBase owner = FindOnline(g.owner_id);
		if (owner && owner != joiner)
			owner.MessageImportant(string.Format("%1 se unio al party.", PlayerName(joiner)));
	}

	// ------------------------- salir / kick -------------------------
	void Leave(PlayerBase player)
	{
		if (!GetGame().IsServer() || !player)
			return;
		string sid = SteamId(player);
		ExorGroup g = FindByPlayer(sid);
		if (!g)
		{
			player.MessageImportant("No estas en ningun party.");
			return;
		}

		if (g.IsOwner(sid))
		{
			DisbandGroup(g, "El lider disolvio el party.", sid);
			return;
		}

		g.AddFormer(sid, PlayerName(player), ExorTimeUtil.TodayNumber(), "salio");
		g.RemoveMember(sid);
		player.ExorSetGroupId("");
		SyncToPlayer(player, null);
		ExorPartyLive.Get().ClearForPlayer(player);	// limpiar HUD/nameplates/marcas del que sale
		player.MessageImportant("Saliste del party.");
		SaveGroup(g);
		SyncGroup(g);
	}

	bool Kick(PlayerBase leader, string targetSid)
	{
		if (!GetGame().IsServer() || !leader)
			return false;
		string asid = SteamId(leader);
		ExorGroup g = FindByPlayer(asid);
		if (!g || !g.HasMember(asid))
		{
			leader.MessageImportant("No estas en ningun party.");
			return false;
		}
		ExorGroupMember mm = g.FindMember(targetSid);
		if (!mm)
		{
			leader.MessageImportant("Esa persona no esta en tu party.");
			return false;
		}
		// Expulsar al DUENO = disolver el party + borrar el mastil
		if (targetSid == g.owner_id)
		{
			DisbandGroup(g, "El party fue disuelto (se expulso al lider).", asid);
			return true;
		}

		string nm = mm.name;
		g.AddFormer(targetSid, nm, ExorTimeUtil.TodayNumber(), "expulsado");
		g.RemoveMember(targetSid);
		SaveGroup(g);
		SyncGroup(g);

		PlayerBase kicked = FindOnline(targetSid);
		if (kicked)
		{
			kicked.ExorSetGroupId("");
			SyncToPlayer(kicked, null);
			ExorPartyLive.Get().ClearForPlayer(kicked);	// limpiar HUD/nameplates/marcas del expulsado
			kicked.MessageImportant("Fuiste sacado del party.");
		}
		leader.MessageImportant(string.Format("%1 fue sacado del party.", nm));
		return true;
	}

	// ------------------------- conexion / auto-kick -------------------------
	void OnPlayerConnected(PlayerBase player)
	{
		if (!GetGame().IsServer() || !player || !player.GetIdentity())
			return;
		string sid = SteamId(player);
		ExorGroup g = FindByPlayer(sid);
		if (!g)
		{
			SyncToPlayer(player, null);	// limpia el roster del cliente
			return;
		}

		// Actualizar nombre + ultimo login
		g.AddOrUpdate(sid, PlayerName(player), ExorTimeUtil.TodayNumber());
		g.inactivity_alert_day = 0;	// alguien se conecto: re-armar el aviso de inactividad
		player.ExorSetGroupId(g.id);
		AutoKickScan(g);
		SaveGroup(g);
		SyncGroup(g);
		// (el self-heal del mastil perdido ya lo dispara ExorAfterClientSpawned -> HealGroupMast a los 8s)
	}

	void AutoKickScan(ExorGroup g)
	{
		int days = GetExorConfig().party.grupo.auto_kick_dias;
		if (days <= 0)
			return;
		int today = ExorTimeUtil.TodayNumber();
		int i;
		for (i = g.members.Count() - 1; i >= 0; i--)
		{
			ExorGroupMember mm = g.members.Get(i);
			if (mm.steamid == g.owner_id)
				continue;	// nunca al lider
			if (today - mm.last_seen_day >= days)
			{
				Print(string.Format("%1 Party: auto-kick de %2 (%3 dias sin login)", ExorStorageConstants.LOG, mm.name, today - mm.last_seen_day));
				g.AddFormer(mm.steamid, mm.name, today, "auto-kick inactividad");
				g.RemoveMember(mm.steamid);
			}
		}
	}

	// ------------------------- aviso de clanes inactivos -------------------------
	// Recorre todos los grupos y, si NINGUN miembro actual se conecto en los ultimos
	// 'aviso_tiempo_inactividad_dias', escribe un aviso al log forense diario (ExorRaidLog) con las
	// coordenadas de la base. Dedup: 1 aviso por clan por dia (inactivity_alert_day),
	// re-armado cuando alguien se conecta (ver OnPlayerConnected). Se llama en
	// MissionServer.OnInit tras cargar los grupos; como el server reinicia seguido,
	// eso alcanza para avisar al menos 1 vez por dia mientras el clan siga inactivo.
	void ScanInactiveClans()
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		ExorCfgPartyProteccion p = GetExorConfig().party.proteccion;
		if (!p.aviso_clan_inactivo)
			return;
		int umbral = p.aviso_tiempo_inactividad_dias;
		if (umbral <= 0)
			return;

		int today = ExorTimeUtil.TodayNumber();
		int avisados = 0;
		int i;
		for (i = 0; i < m_Groups.Count(); i++)
		{
			ExorGroup g = m_Groups.Get(i);
			if (g.members.Count() == 0)
				continue;
			int last = g.MostRecentSeenDay();
			if (last < 0)
				continue;
			int dias = today - last;
			if (dias < umbral)
				continue;
			if (g.inactivity_alert_day == today)
				continue;	// ya se aviso hoy de este clan

			string ownerName = "?";
			ExorGroupMember owner = g.FindMember(g.owner_id);
			if (owner)
				ownerName = owner.name;
			vector basePos = Vector(g.mast_x, g.mast_y, g.mast_z);
			string detalle = string.Format("clan %1 (lider %2, %3 miembros) sin conexion hace %4 dias (umbral %5)",
				g.id, ownerName, g.members.Count(), dias, umbral);
			ExorRaidLog.Write("CLAN_INACTIVO", g.owner_id, ownerName, basePos, detalle);

			g.inactivity_alert_day = today;
			SaveGroup(g);
			avisados++;
		}
		if (avisados > 0)
			Print(string.Format("%1 Party: %2 clan(es) inactivo(s) avisados al raidlog (umbral %3 dias)", ExorStorageConstants.LOG, avisados, umbral));
	}

	// Vuelca al log forense el roster completo (miembros + ex-miembros) de un grupo
	// que se esta por disolver, para conservar la info de quien estuvo en el clan.
	void ExorLogDisband(ExorGroup g, string reason)
	{
		if (!g)
			return;
		string ids = "";
		int i;
		for (i = 0; i < g.members.Count(); i++)
		{
			if (ids != "")
				ids += ", ";
			ids += g.members.Get(i).name + "=" + g.members.Get(i).steamid;
		}
		string exids = "";
		for (i = 0; i < g.former_members.Count(); i++)
		{
			if (exids != "")
				exids += ", ";
			exids += g.former_members.Get(i).name + "=" + g.former_members.Get(i).steamid;
		}
		string ownerName = "?";
		ExorGroupMember owner = g.FindMember(g.owner_id);
		if (owner)
			ownerName = owner.name;
		vector basePos = Vector(g.mast_x, g.mast_y, g.mast_z);
		string detalle = string.Format("clan %1 (lider %2) disuelto: %3 | miembros: [%4] | ex-miembros: [%5]",
			g.id, ownerName, reason, ids, exids);
		ExorRaidLog.Write("CLAN_DISUELTO", g.owner_id, ownerName, basePos, detalle);
	}
}
