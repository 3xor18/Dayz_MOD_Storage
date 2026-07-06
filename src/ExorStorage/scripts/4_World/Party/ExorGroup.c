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

class ExorGroup
{
	string id;
	string owner_id;     // steamid del lider
	string mast_id;      // id del mastil que reclamo el territorio (Fase C)
	float mast_x;        // posicion del mastil (Fase C) - floats sueltos: JSON-safe
	float mast_y;
	float mast_z;
	string mast_flag;    // classname de la bandera colgada (para restaurar el color al recrear el mastil tras crash)
	ref array<ref ExorGroupMember> members;
	ref array<ref ExorExMember> former_members;   // historial de los que salieron (para baneo de clan)
	int inactivity_alert_day;                      // ultimo dia que se aviso inactividad (0 = nunca); se resetea al conectarse alguien

	void ExorGroup()
	{
		members = new array<ref ExorGroupMember>;
		former_members = new array<ref ExorExMember>;
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
				members.Remove(i);
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
