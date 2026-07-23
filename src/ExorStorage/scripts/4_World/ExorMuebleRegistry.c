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
	// Cuantas veces el self-heal tuvo que recrear ESTE mueble. Si sube y sube, el mueble esta
	// mal puesto (clipeado en una pared, en una pos que el motor rechaza) y el server lo va a
	// seguir borrando: hay que avisarle al player que lo corra de lugar. Los registros viejos
	// no traen el campo y se leen como 0 -> compatible con lo que ya esta en disco.
	int		heals;
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
		if (!fur)
			return;
		RegisterEntity(fur.ExorGetID(), fur);
	}

	// Igual pero para BARRILES. Se agrego porque una cuarentena de persistencia se llevo 38
	// barriles puestos y no habia forma de recrearlos: su JSON guarda el CONTENIDO pero no la
	// POSICION. Con el registro, el self-heal los devuelve en su lugar y con todo adentro.
	static void RegisterBarrel(Exor_Barrel_Base b)
	{
		if (!b)
			return;
		// SOLO barriles PUESTOS EN EL PISO. Un barril ATADO (slot de barril de un auto, o
		// dentro de un cargo) NO se registra: cuando el auto se virtualiza, ese barril se
		// guarda DENTRO del JSON del auto y su entidad se borra -> si el self-heal lo
		// recreara en el piso quedarian DOS (el del JSON del auto + el recreado) = dupe.
		// Al soltarlo en el piso queda sin padre y el backfill lo toma solo.
		if (b.GetHierarchyParent())
			return;
		RegisterEntity(b.ExorGetID(), b);
	}

	// true si ya hay registro para ese id (chequeo BARATO: no lee ni parsea el JSON). Lo usa
	// el backfill para no reescribir 700 archivos en cada arranque.
	static bool TieneRegistro(string id)
	{
		if (id == "")
			return false;
		return FileExist(PathFor(id));
	}

	static void RegisterEntity(string id, EntityAI e)
	{
		if (!GetGame().IsServer() || !e || id == "")
			return;

		// CONSERVAR el contador de reparaciones: si no, cada re-registro (los muebles se
		// re-registran al cargar) lo volveria a 0 y nunca detectariamos al mueble problematico,
		// que es justo el que se recrea una y otra vez ENTRE reinicios.
		int healsPrevios = 0;
		string path = PathFor(id);
		if (FileExist(path))
		{
			ExorMuebleRegFile viejo = new ExorMuebleRegFile();
			JsonFileLoader<ExorMuebleRegFile>.JsonLoadFile(path, viejo);
			healsPrevios = viejo.heals;
		}

		ExorMuebleRegFile f = new ExorMuebleRegFile();
		f.heals = healsPrevios;
		f.id = id;
		f.type = e.GetType();
		vector pos = e.GetPosition();
		vector ori = e.GetOrientation();
		f.x = pos[0];  f.y = pos[1];  f.z = pos[2];
		f.yaw = ori[0];  f.pitch = ori[1];  f.roll = ori[2];
		f.health = e.GetHealth01("", "");
		EnsureDir();
		JsonFileLoader<ExorMuebleRegFile>.JsonSaveFile(PathFor(id), f);
		// El cache se ACTUALIZA, no se tira. Tirarlo aca lo volvia inutil justo cuando hace
		// falta: en el arranque, CADA mueble llama a Register, asi que el proximo mueble sin id
		// tendria que releer los ~700 archivos del registro otra vez (con 9 neveras salteadas
		// eran ~6.300 lecturas de JSON = segundos de arranque de mas, y un arranque lento es
		// justo lo que no queremos).
		ActualizarCache(f);
	}

	// mete o pisa la entrada del cache (sin releer el directorio)
	static void ActualizarCache(ExorMuebleRegFile f)
	{
		if (!s_Cache || !f)
			return;
		int i;
		for (i = 0; i < s_Cache.Count(); i++)
		{
			ExorMuebleRegFile c = s_Cache.Get(i);
			if (c && c.id == f.id)
			{
				s_Cache.Set(i, f);
				return;
			}
		}
		s_Cache.Insert(f);
	}

	// saca la entrada del cache (sin releer el directorio)
	static void QuitarDelCache(string id)
	{
		if (!s_Cache || id == "")
			return;
		int i;
		for (i = s_Cache.Count() - 1; i >= 0; i--)
		{
			ExorMuebleRegFile c = s_Cache.Get(i);
			if (c && c.id == id)
				s_Cache.Remove(i);
		}
	}

	// El id tiene contenido virtualizado guardado en disco? Es LA pregunta antes de dar de baja
	// cualquier registro: el JSON del contenido se llama por el id, asi que borrar el registro de
	// un id que todavia tiene contenido deja ese contenido inalcanzable para siempre.
	static bool TieneContenido(string id)
	{
		if (id == "")
			return false;
		return FileExist(string.Format("%1\\%2.json", ExorStorageConstants.STORAGE_DIR, id));
	}

	// ----------------------------------------------------------------------------
	// ADOPCION: busca un registro HUERFANO (no hay ningun mueble vivo con ese id) del mismo
	// tipo y practicamente en la misma posicion. Lo usa AfterStoreLoad cuando un mueble llego
	// sin id, para recuperar el suyo en vez de inventarse uno nuevo. Ver el comentario alli.
	//
	// PERF: el registro son ~700 archivos y esto se llama una vez por mueble-sin-id (normalmente
	// CERO; en el peor arranque visto, 9). Para no releer el directorio 9 veces se cachea la
	// lista por CACHE_MS. El cache es solo una foto de id/tipo/pos, no toca el contenido.
	// ----------------------------------------------------------------------------
	static const int CACHE_MS = 30000;
	static ref array<ref ExorMuebleRegFile> s_Cache;
	static int s_CacheMs = 0;

	static array<ref ExorMuebleRegFile> LeerRegistro()
	{
		int now = GetGame().GetTime();
		if (s_Cache && now - s_CacheMs < CACHE_MS)
			return s_Cache;

		s_Cache = new array<ref ExorMuebleRegFile>;
		s_CacheMs = now;
		if (!FileExist(ExorStorageConstants.MUEBLES_REG_DIR))
			return s_Cache;

		string pattern = string.Format("%1\\*.json", ExorStorageConstants.MUEBLES_REG_DIR);
		string fileName;
		FileAttr attr;
		FindFileHandle h = FindFile(pattern, fileName, attr, FindFileFlags.ALL);
		if (!h)
			return s_Cache;
		bool more = true;
		while (more)
		{
			if (fileName != "")
			{
				ExorMuebleRegFile f = new ExorMuebleRegFile();
				JsonFileLoader<ExorMuebleRegFile>.JsonLoadFile(string.Format("%1\\%2", ExorStorageConstants.MUEBLES_REG_DIR, fileName), f);
				if (f.id != "" && f.type != "")
					s_Cache.Insert(f);
			}
			more = FindNextFile(h, fileName, attr);
		}
		CloseFindFile(h);
		return s_Cache;
	}

	// invalida el cache (tras dar de alta/baja registros)
	static void InvalidarCache()
	{
		s_CacheMs = 0;
	}

	static string BuscarHuerfanoEnPos(string tipo, vector pos)
	{
		if (tipo == "")
			return "";
		array<ref ExorMuebleRegFile> regs = LeerRegistro();
		int i;
		for (i = 0; i < regs.Count(); i++)
		{
			ExorMuebleRegFile f = regs.Get(i);
			if (!f || f.type != tipo)
				continue;
			if (vector.Distance(Vector(f.x, f.y, f.z), pos) > EXOR_DUPE_M)
				continue;
			// tiene que estar HUERFANO: si hay un mueble vivo con ese id, el registro es de otro
			// y adoptarlo dejaria a dos muebles compartiendo id (= comparten contenido = dupe).
			if (ExorIdVivo(f.id))
				continue;
			return f.id;
		}
		return "";
	}

	// hay algun mueble/barril VIVO con ese id?
	static bool ExorIdVivo(string id)
	{
		ExorVO_Manager vo = ExorVO_Manager.Get();
		if (!vo || id == "")
			return false;
		int i;
		if (vo.m_Openables)
		{
			for (i = 0; i < vo.m_Openables.Count(); i++)
			{
				Exor_OpenableStorage o = vo.m_Openables.Get(i);
				if (o && o.ExorGetID() == id)
					return true;
			}
		}
		if (vo.m_Barrels)
		{
			for (i = 0; i < vo.m_Barrels.Count(); i++)
			{
				Exor_Barrel_Base b = vo.m_Barrels.Get(i);
				if (b && b.ExorGetID() == id)
					return true;
			}
		}
		return false;
	}

	// baja del registro: SOLO la llama la accion de empaque (player saca el mueble a proposito).
	static void Unregister(string id)
	{
		if (id == "")
			return;
		string path = PathFor(id);
		if (FileExist(path))
			DeleteFile(path);
		QuitarDelCache(id);
	}

	// SELF-HEAL: recorre el registro; recrea los muebles que no estan vivos. Devuelve cuantos
	// recreo. Corre diferido al arrancar + cada X horas (ver el manager). Guard anti-dupe.
	static int HealScan()
	{
		if (!GetGame().IsServer())
			return 0;
		if (!FileExist(ExorStorageConstants.MUEBLES_REG_DIR))
			return 0;

		// set de IDs vivos (1 pasada por los openables + barriles registrados en el manager)
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
		if (vo && vo.m_Barrels)
		{
			int b;
			for (b = 0; b < vo.m_Barrels.Count(); b++)
			{
				Exor_Barrel_Base br = vo.m_Barrels.Get(b);
				if (br)
					alive.Set(br.ExorGetID(), true);
			}
		}

		// Instrumentacion: cuanto tarda el scan de verdad. Con 700 barriles + 700 muebles esto
		// recorre ~1400 archivos, asi que hay que poder VERLO y no suponerlo. Solo loguea si se
		// pasa del umbral -> en operacion normal no escribe nada.
		int tScanStart = GetGame().GetTime();
		int revisados = 0;

		int healed = 0;
		int saltados = 0;
		int fallidos = 0;
		int pendientes = 0;
		int resueltos = 0;	// registros duplicados limpiados en este pase
		// duplicados detectados durante la enumeracion, para resolverlos DESPUES de cerrarla
		ref array<ref ExorMuebleRegFile> dupF = new array<ref ExorMuebleRegFile>;
		ref array<EntityAI> dupVivo = new array<EntityAI>;
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
				revisados++;
				// ATAJO DE PERF (clave al sumar los barriles: el registro paso de ~114 a ~670
				// archivos): el NOMBRE del archivo ES el id ("<id>.json"), asi que se puede
				// descartar a los que estan VIVOS sin abrir ni parsear el JSON. En operacion
				// normal estan vivos TODOS menos un par -> el scan se vuelve casi gratis y
				// solo se leen de disco los pocos que realmente faltan.
				string idDelNombre = fileName;
				int punto = idDelNombre.LastIndexOf(".");
				if (punto > 0)
					idDelNombre = idDelNombre.Substring(0, punto);
				if (idDelNombre != "" && alive.Get(idDelNombre))
				{
					more = FindNextFile(h, fileName, attr);
					continue;
				}

				ExorMuebleRegFile f = new ExorMuebleRegFile();
				JsonFileLoader<ExorMuebleRegFile>.JsonLoadFile(string.Format("%1\\%2", ExorStorageConstants.MUEBLES_REG_DIR, fileName), f);
				if (f.id != "" && f.type != "" && !alive.Get(f.id))
				{
					vector pos = Vector(f.x, f.y, f.z);
					// DIAGNOSTICO: antes, un mueble que no volvia lo hacia EN SILENCIO y no
					// habia forma de saber por que (players reportaron muebles que no
					// reaparecian tras reiniciar). Ahora cada caso deja rastro en el RPT.
					// GUARD ANTI-DUPE. Antes miraba CUALQUIER mueble/barril vivo a 1.5m, y eso
					// se comia el caso normal: en una base los muebles van PEGADOS entre si, asi
					// que un locker despawneado nunca volvia porque su vecino estaba a menos de
					// 1.5m. Era la causa del "hay muebles que no reaparecen tras el reinicio".
					// Ahora solo bloquea si hay algo DEL MISMO TIPO practicamente encima (0.5m),
					// que es el unico caso que seria un duplicado de verdad.
					EntityAI vivoEncima = ExorMuebleEnPos(pos, EXOR_DUPE_M, f.type);
					if (vivoEncima)
					{
						saltados++;
						// El registro apunta a un lugar donde YA hay un mueble vivo del mismo tipo.
						// Sin resolverlo, esto se repite en cada pase (cada 30 min) para siempre y
						// el registro se llena de huerfanos. Ver ResolverDuplicado.
						// NO se resuelve ACA: resolver da de baja archivos .json de ESTE directorio
						// y estamos en medio de su enumeracion con FindFile. Se anota y se procesa
						// despues del CloseFindFile.
						dupF.Insert(f);
						dupVivo.Insert(vivoEncima);
					}
					else if (healed >= EXOR_HEAL_MAX_POR_PASE)
					{
						// TOPE POR PASE. Recrear una entidad con fisica es LO CARO de todo esto.
						// Si faltan muchas a la vez (tipico despues de una cuarentena de
						// persistencia: se cayo una celda entera con 30+ muebles) crearlas todas
						// en el mismo frame es un pico de verdad -medido: 22 recreaciones se
						// fueron a ~117ms-. Se hacen de a poco: las que no entran esperan al
						// proximo pase (30 min). Nada se pierde, solo tarda un poco mas en
						// volver, y el server no se clava.
						pendientes++;
					}
					else if (ExorRecreate(f))
					{
						healed++;
						ExorContarReparacion(f);
					}
					else
					{
						fallidos++;
						Print(string.Format("%1 SELF-HEAL: FALLO al recrear '%2' (%3) en %4 -> revisar el classname", ExorStorageConstants.LOG, f.id, f.type, pos.ToString()));
					}
				}
			}
			more = FindNextFile(h, fileName, attr);
		}
		CloseFindFile(h);

		// Ya cerrada la enumeracion, es seguro dar de baja archivos del directorio.
		int d;
		for (d = 0; d < dupF.Count(); d++)
		{
			ExorMuebleRegFile df = dupF.Get(d);
			EntityAI dv = dupVivo.Get(d);
			if (!df || !dv)
				continue;
			if (ResolverDuplicado(df, dv))
				resueltos++;
			else
				Print(string.Format("%1 SELF-HEAL: '%2' (%3) NO se recrea: ya hay otro %3 vivo encima de %4", ExorStorageConstants.LOG, df.id, df.type, Vector(df.x, df.y, df.z).ToString()));
		}

		int dur = GetGame().GetTime() - tScanStart;
		if (dur >= 25)
			Print(string.Format("%1 SELF-HEAL LENTO: %2 ms revisando %3 registros (recreados %4). Si esto crece, hay que repartir el scan en varios ticks.", ExorStorageConstants.LOG, dur, revisados, healed));

		if (healed > 0)
			Print(string.Format("%1 SELF-HEAL: %2 mueble(s)/barril(es) recreados (despawneados por el motor)", ExorStorageConstants.LOG, healed));
		if (pendientes > 0)
			Print(string.Format("%1 SELF-HEAL: %2 quedaron PENDIENTES por el tope de %3 por pase -> vuelven en el proximo pase (30 min). Nada se pierde.", ExorStorageConstants.LOG, pendientes, EXOR_HEAL_MAX_POR_PASE));
		if (resueltos > 0)
			Print(string.Format("%1 SELF-HEAL: %2 registro(s) duplicados resueltos -> dejan de repetirse en cada pase", ExorStorageConstants.LOG, resueltos));
		if (saltados > 0 || fallidos > 0)
			Print(string.Format("%1 SELF-HEAL: %2 salteados (ya habia algo en su lugar) y %3 fallidos. Si un player se queja de un mueble que no vuelve, la razon esta en las lineas de arriba.", ExorStorageConstants.LOG, saltados, fallidos));
		return healed;
	}

	// A partir de cuantas reparaciones se considera que el mueble esta MAL PUESTO (el motor lo
	// borra una y otra vez). 3 = ya no es casualidad.
	static const int EXOR_HEALS_AVISO = 3;

	// Maximo de muebles/barriles que se RECREAN en un mismo pase. Recrear una entidad con
	// fisica es lo caro; el resto del scan es casi gratis. Los que no entran vuelven en el
	// proximo pase (30 min). Acota el pico cuando falta media base de golpe.
	static const int EXOR_HEAL_MAX_POR_PASE = 8;

	// Suma una reparacion al registro del mueble y, si ya lleva varias, AVISA AL CLAN dueño del
	// territorio por el chat del mod. El self-heal lo devuelve siempre, asi que el player no
	// pierde nada, pero la causa (mueble clipeado en una pared / en una pos que el motor
	// rechaza) solo la puede arreglar el que lo puso, moviendolo de lugar.
	static void ExorContarReparacion(ExorMuebleRegFile f)
	{
		if (!f || f.id == "")
			return;
		f.heals = f.heals + 1;
		JsonFileLoader<ExorMuebleRegFile>.JsonSaveFile(PathFor(f.id), f);

		if (f.heals < EXOR_HEALS_AVISO)
			return;

		vector pos = Vector(f.x, f.y, f.z);
		Print(string.Format("%1 SELF-HEAL: OJO -> '%2' (%3) ya se recreo %4 veces en %5. Ese mueble esta MAL PUESTO (el motor lo borra siempre); hay que moverlo de lugar.", ExorStorageConstants.LOG, f.id, f.type, f.heals, pos.ToString()));

		ExorAvisarAlClan(f, pos);
	}

	// Manda el aviso por el chat del mod a los miembros CONECTADOS del clan dueño de esa base.
	// Si no hay nadie online, no pasa nada: el proximo pase (cada 30 min) lo vuelve a intentar.
	static void ExorAvisarAlClan(ExorMuebleRegFile f, vector pos)
	{
		ExorTerritoryManager tm = ExorTerritoryManager.Get();
		if (!tm)
			return;
		string gid = tm.GroupAtPos(pos);
		if (gid == "")
			return;		// el mueble no esta en ningun territorio -> no hay a quien avisarle

		ExorGroupManager gm = ExorGroupManager.Get();
		if (!gm)
			return;
		ExorGroup g = gm.FindById(gid);
		if (!g || !g.members)
			return;

		string texto = string.Format("Uno de tus muebles se sigue despawneando solo (ya %1 veces). Esta muy pegado a una pared u objeto: sacalo y volvelo a poner un poco mas separado. Por ahora te lo devolvemos con todo adentro.", f.heals);

		int i;
		for (i = 0; i < g.members.Count(); i++)
		{
			ExorGroupMember m = g.members.Get(i);
			if (!m || m.steamid == "")
				continue;
			PlayerBase p = gm.FindOnline(m.steamid);
			if (p)
				ExorMuebleRules.SendMsg(p, texto, 2);	// 2 = rojo (algo que hay que atender)
		}
	}

	// Radio del guard anti-dupe. Chico A PROPOSITO: en una base los muebles van pegados, asi
	// que un radio grande impide devolver el que falta. A 0.5m del mismo tipo ya estarian
	// literalmente uno adentro del otro = duplicado real.
	static const float EXOR_DUPE_M = 0.5;

	// Hay algo vivo DEL MISMO TIPO a <=radius de pos? (anti-dupe del self-heal). El tipo
	// importa: un barril al lado de un locker no tiene que impedir que el locker vuelva.
	// tipo == "" -> mira cualquier mueble/barril (comportamiento viejo, por si hace falta).
	static bool ExorMuebleAtPos(vector pos, float radius, string tipo)
	{
		return ExorMuebleEnPos(pos, radius, tipo) != null;
	}

	// idem pero devuelve la ENTIDAD, para poder mirarle el id (lo necesita ResolverDuplicado).
	static EntityAI ExorMuebleEnPos(vector pos, float radius, string tipo)
	{
		ExorVO_Manager vo = ExorVO_Manager.Get();
		if (!vo)
			return null;
		int i;
		if (vo.m_Openables)
		{
			for (i = 0; i < vo.m_Openables.Count(); i++)
			{
				Exor_OpenableStorage o = vo.m_Openables.Get(i);
				if (!o)
					continue;
				if (tipo != "" && o.GetType() != tipo)
					continue;
				if (vector.Distance(o.GetPosition(), pos) <= radius)
					return o;
			}
		}
		if (vo.m_Barrels)
		{
			for (i = 0; i < vo.m_Barrels.Count(); i++)
			{
				Exor_Barrel_Base b = vo.m_Barrels.Get(i);
				if (!b)
					continue;
				if (tipo != "" && b.GetType() != tipo)
					continue;
				if (vector.Distance(b.GetPosition(), pos) <= radius)
					return b;
			}
		}
		return null;
	}

	// ----------------------------------------------------------------------------
	// RESOLVER DUPLICADO del registro.
	//
	// De donde salen: un mueble cuyo OnStoreLoad devolvio false (nevera salteada por BootRepair,
	// stream corrupto) llega SIN id; AfterStoreLoad le inventaba uno nuevo y lo registraba, y el
	// registro viejo quedaba huerfano apuntando a la misma posicion. La adopcion de AfterStoreLoad
	// ya lo evita de aca en adelante; esto limpia los que YA estan en disco (12 en el server del
	// amigo, repitiendo la misma linea cada 30 min desde hace dias).
	//
	// REGLA DE ORO (regla del user: no borrar loot): un registro NO se borra si detras tiene
	// contenido guardado, porque el JSON del contenido se llama por el id -> sin registro, ese
	// contenido queda inalcanzable. Los tres casos:
	//   a) el huerfano TIENE contenido y el vivo NO -> transferirle el id al vivo. El mueble
	//      recupera lo que tenia adentro y el duplicado desaparece. Es el caso tipico.
	//   b) el huerfano NO tiene contenido -> no hay nada en juego, se borra el registro.
	//   c) los DOS tienen contenido -> NO se toca nada y se avisa fuerte. Elegir cual sobrevive
	//      es tirar el loot de alguien, y eso lo decide una persona, no el mod.
	// Devuelve true si el duplicado quedo resuelto (ya no va a repetirse).
	// ----------------------------------------------------------------------------
	static bool ResolverDuplicado(ExorMuebleRegFile f, EntityAI vivo)
	{
		if (!f || !vivo || f.id == "")
			return false;

		string idVivo = "";
		Exor_OpenableStorage fur = Exor_OpenableStorage.Cast(vivo);
		Exor_Barrel_Base bar;
		if (fur)
			idVivo = fur.ExorGetID();
		else
		{
			bar = Exor_Barrel_Base.Cast(vivo);
			if (bar)
				idVivo = bar.ExorGetID();
		}
		if (idVivo == "" || idVivo == f.id)
			return false;	// no hay nada que resolver

		bool huerfanoConLoot = TieneContenido(f.id);
		bool vivoConLoot = TieneContenido(idVivo);

		// caso c) los dos con contenido -> decision humana
		if (huerfanoConLoot && vivoConLoot)
		{
			Print(string.Format("%1 SELF-HEAL: OJO -> DOS registros con contenido en %2 ('%3' huerfano y '%4' vivo, %5). NO se toca ninguno: hay que decidir a mano cual conservar.", ExorStorageConstants.LOG, Vector(f.x, f.y, f.z).ToString(), f.id, idVivo, f.type));
			return false;
		}

		// caso a) el contenido esta del lado del huerfano -> el mueble vivo adopta ese id
		if (huerfanoConLoot)
		{
			if (fur)
				fur.ExorSetIDForHeal(f.id);
			else if (bar)
				bar.ExorSetIDForHeal(f.id);
			else
				return false;
			Unregister(idVivo);			// el registro del id nuevo (vacio) ya no sirve
			RegisterEntity(f.id, vivo);	// el registro huerfano pasa a ser el del mueble vivo
			// (Unregister y RegisterEntity ya mantienen el cache al dia; tirarlo aca obligaria a
			// releer los ~700 archivos por CADA duplicado resuelto -21 en el test local-.)
			Print(string.Format("%1 SELF-HEAL: registro duplicado RESUELTO en %2 -> el %3 vivo adopta '%4' y recupera su contenido (se dio de baja el id vacio '%5')", ExorStorageConstants.LOG, Vector(f.x, f.y, f.z).ToString(), f.type, f.id, idVivo));
			return true;
		}

		// caso b) el huerfano no tiene nada detras -> se puede dar de baja sin riesgo
		Unregister(f.id);
		Print(string.Format("%1 SELF-HEAL: registro duplicado '%2' dado de baja en %3 (sin contenido detras; el %4 vivo es '%5')", ExorStorageConstants.LOG, f.id, Vector(f.x, f.y, f.z).ToString(), f.type, idVivo));
		return true;
	}

	// recrea el mueble/barril ESTATICO en su pos/orient y le re-liga su id (para recuperar
	// contenido: con el id viejo, el reconcile/restore encuentra su JSON y devuelve todo).
	static bool ExorRecreate(ExorMuebleRegFile f)
	{
		vector pos = Vector(f.x, f.y, f.z);
		// BUG QUE ESTABA ACA: se creaba con CreateObject(..., create_physics=FALSE) y sin
		// congelar el cuerpo, o sea DISTINTO a como lo coloca un player. El propio comentario
		// del camino de colocacion lo advierte: sin ECE_CREATEPHYSICS la colision no se
		// registra hasta el proximo guardado/recarga -> el mueble recreado quedaba
		// ATRAVESABLE, y sin dBodyDynamic(false) tampoco quedaba anclado (se podia hundir o
		// caer, que es justo de lo que se quejaban los players con los muebles pegados a
		// paredes). Ahora se recrea EXACTAMENTE igual que cuando lo pone un jugador.
		Object o = GetGame().CreateObjectEx(f.type, pos, ECE_CREATEPHYSICS);
		if (!o)
			return false;

		Exor_OpenableStorage fur = Exor_OpenableStorage.Cast(o);
		if (fur)
		{
			fur.SetPosition(pos);
			fur.SetOrientation(Vector(f.yaw, f.pitch, f.roll));
			dBodyDynamic(fur, false);	// cuerpo ESTATICO: solido, no se simula ni se hunde
			fur.ExorSetIDForHeal(f.id);	// re-ligar el id -> ExorReconcileOnLoad recupera su JSON
			fur.SetHealth01("", "", f.health);
			return true;
		}

		Exor_Barrel_Base bar = Exor_Barrel_Base.Cast(o);
		if (bar)
		{
			bar.SetPosition(pos);
			bar.SetOrientation(Vector(f.yaw, f.pitch, f.roll));
			bar.ExorSetIDForHeal(f.id);	// idem: su JSON de contenido sigue en el profile
			bar.SetHealth01("", "", f.health);
			return true;
		}

		GetGame().ObjectDelete(o);
		return false;
	}
}
