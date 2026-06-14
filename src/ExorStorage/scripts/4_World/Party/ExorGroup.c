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

class ExorGroup
{
	string id;
	string owner_id;     // steamid del lider
	string mast_id;      // id del mastil que reclamo el territorio (Fase C)
	float mast_x;        // posicion del mastil (Fase C) - floats sueltos: JSON-safe
	float mast_y;
	float mast_z;
	ref array<ref ExorGroupMember> members;

	void ExorGroup()
	{
		members = new array<ref ExorGroupMember>;
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
