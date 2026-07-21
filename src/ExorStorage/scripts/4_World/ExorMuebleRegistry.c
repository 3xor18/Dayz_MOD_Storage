// ============================================================================
// 3xor_Vanilla_Optimization - REGISTRO de muebles + SELF-HEAL
// ============================================================================
// Cada mueble colocado registra un JSON {id,tipo,pos,orient}. El JSON se BORRA SOLO cuando
// el player EMPAQUETA el mueble (accion deliberada). Si el motor lo despawnea (bug de pared,
// CE, invalid-location, etc.) el JSON QUEDA -> el self-heal, al comparar registro vs muebles
// vivos, ve que falta y lo RECREA (con el mismo id -> recupera su contenido virtualizado).
//
// Distincion clave "player lo saco" vs "el server lo despawneo": la baja del registro pasa
// SOLO en la accion de empaque; cualquier otra desaparicion deja el registro -> se recrea.
//
// PERF: el chequeo corre 1 vez al arrancar (diferido, tras cargar la persistencia) y luego
// cada X horas. Recorrer cientos de registros 1 vez = milisegundos. No es por-frame ni por-
// player. Guard anti-dupe: antes de recrear se chequea que no haya ya un mueble en esa pos.
// ============================================================================
class ExorMuebleRegFile
{
	string	id;
	string	type;
	float	x;
	float	y;
	float	z;
	float	yaw;
	float	pitch;
	float	roll;
	float	health;
}

class ExorMuebleRegistry
{
	static void EnsureDir()
	{
		if (!FileExist(ExorStorageConstants.MUEBLES_REG_DIR))
			MakeDirectory(ExorStorageConstants.MUEBLES_REG_DIR);
	}

	static string PathFor(string id)
	{
		return string.Format("%1\\%2.json", ExorStorageConstants.MUEBLES_REG_DIR, id);
	}

	// alta/actualizacion del registro (idempotente: se llama al colocar y al cargar).
	static void Register(Exor_OpenableStorage fur)
	{
		if (!GetGame().IsServer() || !fur)
			return;
		string id = fur.ExorGetID();
		if (id == "")
			return;
		ExorMuebleRegFile f = new ExorMuebleRegFile();
		f.id = id;
		f.type = fur.GetType();
		vector pos = fur.GetPosition();
		vector ori = fur.GetOrientation();
		f.x = pos[0];  f.y = pos[1];  f.z = pos[2];
		f.yaw = ori[0];  f.pitch = ori[1];  f.roll = ori[2];
		f.health = fur.GetHealth01("", "");
		EnsureDir();
		JsonFileLoader<ExorMuebleRegFile>.JsonSaveFile(PathFor(id), f);
	}

	// baja del registro: SOLO la llama la accion de empaque (player saca el mueble a proposito).
	static void Unregister(string id)
	{
		if (id == "")
			return;
		string path = PathFor(id);
		if (FileExist(path))
			DeleteFile(path);
	}

	// SELF-HEAL: recorre el registro; recrea los muebles que no estan vivos. Devuelve cuantos
	// recreo. Corre diferido al arrancar + cada X horas (ver el manager). Guard anti-dupe.
	static int HealScan()
	{
		if (!GetGame().IsServer())
			return 0;
		if (!FileExist(ExorStorageConstants.MUEBLES_REG_DIR))
			return 0;

		// set de IDs vivos (1 pasada por los openables registrados en el manager)
		ExorVO_Manager vo = ExorVO_Manager.Get();
		ref map<string, bool> alive = new map<string, bool>;
		if (vo && vo.m_Openables)
		{
			int a;
			for (a = 0; a < vo.m_Openables.Count(); a++)
			{
				Exor_OpenableStorage o = vo.m_Openables.Get(a);
				if (o)
					alive.Set(o.ExorGetID(), true);
			}
		}

		int healed = 0;
		string pattern = string.Format("%1\\*.json", ExorStorageConstants.MUEBLES_REG_DIR);
		string fileName;
		FileAttr attr;
		FindFileHandle h = FindFile(pattern, fileName, attr, FindFileFlags.ALL);
		if (!h)
			return 0;
		bool more = true;
		while (more)
		{
			if (fileName != "")
			{
				ExorMuebleRegFile f = new ExorMuebleRegFile();
				JsonFileLoader<ExorMuebleRegFile>.JsonLoadFile(string.Format("%1\\%2", ExorStorageConstants.MUEBLES_REG_DIR, fileName), f);
				if (f.id != "" && f.type != "" && !alive.Get(f.id))
				{
					vector pos = Vector(f.x, f.y, f.z);
					if (!ExorMuebleAtPos(pos, 1.5))	// guard anti-dupe: nada vivo en esa pos
					{
						if (ExorRecreate(f))
							healed++;
					}
				}
			}
			more = FindNextFile(h, fileName, attr);
		}
		CloseFindFile(h);
		if (healed > 0)
			Print(string.Format("%1 SELF-HEAL: %2 mueble(s) recreados (despawneados por el motor)", ExorStorageConstants.LOG, healed));
		return healed;
	}

	// hay algun mueble (openable) vivo a <=radius de pos? (anti-dupe del self-heal)
	static bool ExorMuebleAtPos(vector pos, float radius)
	{
		ExorVO_Manager vo = ExorVO_Manager.Get();
		if (!vo || !vo.m_Openables)
			return false;
		int i;
		for (i = 0; i < vo.m_Openables.Count(); i++)
		{
			Exor_OpenableStorage o = vo.m_Openables.Get(i);
			if (o && vector.Distance(o.GetPosition(), pos) <= radius)
				return true;
		}
		return false;
	}

	// recrea el mueble ESTATICO en su pos/orient y le re-liga su id (para recuperar contenido).
	static bool ExorRecreate(ExorMuebleRegFile f)
	{
		vector pos = Vector(f.x, f.y, f.z);
		Object o = GetGame().CreateObject(f.type, pos, false, false, false);
		Exor_OpenableStorage fur = Exor_OpenableStorage.Cast(o);
		if (!fur)
		{
			if (o)
				GetGame().ObjectDelete(o);
			return false;
		}
		fur.SetPosition(pos);
		fur.SetOrientation(Vector(f.yaw, f.pitch, f.roll));
		fur.ExorSetIDForHeal(f.id);	// re-ligar el id -> ExorReconcileOnLoad recupera su JSON
		fur.SetHealth01("", "", f.health);
		return true;
	}
}
