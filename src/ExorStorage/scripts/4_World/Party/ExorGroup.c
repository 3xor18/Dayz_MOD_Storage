// VERSION DEL ROSTER (global). La bumpea CUALQUIER cambio de composicion de
// cualquier grupo: altas/bajas de miembros y altas/bajas de grupos. El indice
// steamid->grupo de ExorGroupManager guarda con que version se construyo y se
// reconstruye solo cuando no coincide.
// Es el patron de "cache con sello de version": el que escribe no tiene que saber
// quien cachea, y es imposible que un cache quede sirviendo datos viejos (a lo sumo
// se reconstruye de mas, que es barato y raro).
class ExorRosterVersion
{
	static int s_V = 1;
	static void Bump() { s_V = s_V + 1; }
	static int Get()   { return s_V; }
}

// ============================================================================
// 3xor_Vanilla_Optimization - Party: modelo de datos del grupo (Fase B)
// Un grupo = 1 lider (owner) + N miembros. Se persiste a groups/<id>.json.
// Mas adelante (Fase C) el grupo guarda tambien la posicion/id del mastil.
// ============================================================================

class ExorGroupMember
{
	string steamid;
	string name;
	int last_seen_day;   // numero de dia del ultimo login (auto-kick por inactividad)
}

// Ex-miembro: alguien que ESTUVO en el grupo y salio / fue expulsado / auto-kickeado.
// Se conserva en el grupo para poder rastrear o banear a un clan completo aunque
// algunos integrantes se hayan ido antes. Persiste en groups/<id>.json.
class ExorExMember
{
	string steamid;
	string name;
	int left_day;     // numero de dia (ExorTimeUtil) en que salio
	string motivo;    // "salio" | "expulsado" | "auto-kick inactividad"
}

// Clan del que provino el FUNDADOR de este grupo (era ex-miembro de otro clan
// cuando fundo este territorio). Sirve para rastrear que un jugador expulsado
// se armo su propia base. Persiste en groups/<id>.json del grupo NUEVO.
class ExorPriorClan
{
	string group_id;   // id del clan anterior
	string owner_id;   // lider (steamid) del clan anterior
	int    left_day;   // dia en que salio/lo expulsaron de ese clan
	string motivo;     // "salio" | "expulsado" | "auto-kick inactividad"
}

class ExorGroup
{
	string id;
	string owner_id;     // steamid del lider
	string mast_id;      // id del mastil que reclamo el territorio (Fase C)
	float mast_x;        // posicion del mastil (Fase C) - floats sueltos: JSON-safe
	float mast_y;
	float mast_z;
	string mast_flag;    // classname de la bandera colgada (para restaurar el color al recrear el mastil tras crash)
	int mast_lost;       // 1 = el objeto-bandera desaparecio (bug/CE/crash); el PARTY se conserva y el mastil se auto-restaura (no se disuelve el grupo)
	int claim_min;       // minuto de reclamo actual del territorio (para heredar la ventana de bandera blanca al reconstruir/reparar el mastil)
	int last_build_min;  // minuto del ultimo build/mudanza de mastil (cooldown de 1 dia para reconstruir)
	int borrado;         // 1 = grupo disuelto/eliminado; el FILE NO se borra (se conserva para baneo de clan). default 0 (files viejos/nuevos)
	string borrado_por;  // steamID de quien lo disolvio/elimino
	string borrado_fecha;// fecha+hora de la eliminacion ("YYYY-MM-DD HH:MM:SS")
	ref array<ref ExorGroupMember> members;
	ref array<ref ExorExMember> former_members;   // historial de los que salieron (para baneo de clan)
	ref array<ref ExorPriorClan> founder_prior_clans; // clan(es) de los que el fundador era ex-miembro al crear este grupo
	int inactivity_alert_day;                      // ultimo dia que se aviso inactividad (0 = nunca); se resetea al conectarse alguien

	void ExorGroup()
	{
		members = new array<ref ExorGroupMember>;
		former_members = new array<ref ExorExMember>;
		founder_prior_clans = new array<ref ExorPriorClan>;
	}

	// Deja constancia (en el file de ESTE grupo nuevo) de que su fundador venia
	// de otro clan como ex-miembro. 1 entrada por group_id anterior.
	void AddPriorClan(string prevGroupId, string prevOwnerId, int leftDay, string motivo)
	{
		if (prevGroupId == "")
			return;
		int i;
		for (i = 0; i < founder_prior_clans.Count(); i++)
		{
			if (founder_prior_clans.Get(i).group_id == prevGroupId)
				return;   // ya registrado
		}
		ExorPriorClan pc = new ExorPriorClan();
		pc.group_id = prevGroupId;
		pc.owner_id = prevOwnerId;
		pc.left_day = leftDay;
		pc.motivo   = motivo;
		founder_prior_clans.Insert(pc);
	}

	ExorGroupMember FindMember(string sid)
	{
		int i;
		for (i = 0; i < members.Count(); i++)
		{
			if (members.Get(i).steamid == sid)
				return members.Get(i);
		}
		return null;
	}

	bool HasMember(string sid)
	{
		return FindMember(sid) != null;
	}

	bool IsOwner(string sid)
	{
		return owner_id == sid;
	}

	void AddOrUpdate(string sid, string nm, int day)
	{
		ExorGroupMember mm = FindMember(sid);
		if (!mm)
		{
			mm = new ExorGroupMember();
			mm.steamid = sid;
			members.Insert(mm);
			ExorRosterVersion.Bump();	// entro alguien -> el indice steamid->grupo caduca
		}
		mm.name = nm;
		mm.last_seen_day = day;
		RemoveFormer(sid);   // si volvio a entrar, ya no cuenta como "ex-miembro"
	}

	void RemoveMember(string sid)
	{
		int i;
		for (i = members.Count() - 1; i >= 0; i--)
		{
			if (members.Get(i).steamid == sid)
			{
				members.Remove(i);
				ExorRosterVersion.Bump();	// salio alguien -> el indice caduca
			}
		}
	}

	// ------------------------- ex-miembros -------------------------
	ExorExMember FindFormer(string sid)
	{
		int i;
		for (i = 0; i < former_members.Count(); i++)
		{
			if (former_members.Get(i).steamid == sid)
				return former_members.Get(i);
		}
		return null;
	}

	// Registra (o actualiza) a un ex-miembro. Mantiene 1 entrada por steamid.
	void AddFormer(string sid, string nm, int day, string motivo)
	{
		if (sid == "")
			return;
		ExorExMember ex = FindFormer(sid);
		if (!ex)
		{
			ex = new ExorExMember();
			ex.steamid = sid;
			former_members.Insert(ex);
		}
		ex.name = nm;
		ex.left_day = day;
		ex.motivo = motivo;
	}

	void RemoveFormer(string sid)
	{
		int i;
		for (i = former_members.Count() - 1; i >= 0; i--)
		{
			if (former_members.Get(i).steamid == sid)
				former_members.Remove(i);
		}
	}

	// Dia mas reciente en que se vio CONECTADO a algun miembro actual (-1 si no hay).
	int MostRecentSeenDay()
	{
		int best = -1;
		int i;
		for (i = 0; i < members.Count(); i++)
		{
			if (members.Get(i).last_seen_day > best)
				best = members.Get(i).last_seen_day;
		}
		return best;
	}
}

// DTO que el server serializa y manda al cliente (roster para menu P / HUD).
class ExorRosterDTO
{
	string group_id;
	string owner_id;
	bool you_are_owner;   // si el que recibe este roster es el lider del grupo
	ref array<ref ExorGroupMember> members;

	void ExorRosterDTO()
	{
		members = new array<ref ExorGroupMember>;
	}
}

// Invitacion pendiente (vive en el server hasta aceptar/rechazar/expirar).
class ExorPendingInvite
{
	string group_id;
	string inviter_name;
	int created_ms;
}
