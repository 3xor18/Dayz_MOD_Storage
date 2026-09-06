// ============================================================================
// 3xor_Vanilla_Optimization - Bolsa de cadaver (SOLO server la maneja)
// Al morir un jugador, ~delay despues el cuerpo se convierte en esta bolsa
// (contenedor) con TODO su loot. Hereda de SeaChest (carriable en manos + cargo
// + persistencia). Reusa el serializer/virtualizacion del barril.
//   - TTL: se borra pasados duracion_minutos (sobrevive reinicio via m_ExorSpawnMin).
//   - Virtualizacion: saca el loot del mundo si no hay player vivo cerca por X min;
//     lo restaura cuando un player se acerca (radios en bodycadaver.json).
// ============================================================================
// extends Container_Base: es la script-class de SeaChest (contenedor carriable).
// Container_Base extends ItemBase, asi que todos los metodos de item siguen.
class Exor_BodyBag extends Container_Base
{
	protected string m_ExorID;          // id persistente -> liga con su JSON de virtualizacion
	protected int m_ExorSpawnMin;       // minuto-numero (reloj host) de cuando aparecio -> TTL
	protected int m_ExorLastNearMs;     // uptime ms de la ultima vez que hubo un player vivo cerca
	protected bool m_ExorVirtualizedSync;
	protected bool m_ExorExpirada;      // TTL cumplido: ObjectDelete ya pedido (borrado diferido)

	void Exor_BodyBag()
	{
		RegisterNetSyncVariableBool("m_ExorVirtualizedSync");
	}

	// Log de rutina, apagado por defecto (mismo gate que el barril). El virtualizar/restaurar
	// de tumbas era ~460 lineas por sesion de 8h de I/O sincrona por evento normal. Lo que
	// SI sigue en Print son los eventos con valor forense: creacion (liga muerte->bolsa),
	// expiracion por TTL (explica el "perdi mi loot") y el descarte anti-dupe.
	void ExorDbg(string ev)
	{
		if (ExorStorageConstants.DEBUG_BARRELS)
			Print(string.Format("%1 [dbg] BodyBag %2: %3", ExorStorageConstants.LOG, ExorGetID(), ev));
	}

	override void EEInit()
	{
		super.EEInit();

		// Quitar la colision del modelo (la lapida bloquearia el paso) -> se camina
		// a traves. Corre en server Y cliente (la colision con el player es client-side).
		dBodyDestroy(this);

		if (GetGame().IsServer())
		{
			SetAllowDamage(false);	// el loot no se pierde
			m_ExorLastNearMs = GetGame().GetTime();
			ExorVO_Manager.RegisterBodyBag(this);
		}
	}

	override void EEDelete(EntityAI parent)
	{
		if (GetGame().IsServer())
			ExorVO_Manager.UnregisterBodyBag(this);
		super.EEDelete(parent);
	}

	// PERSISTENCIA: solo ENTEROS en el stream (magico + id + minuto de spawn). Ningun string:
	// un ctx.Read(string) sobre un stream corrido tira una Virtual Machine Exception que mata
	// el arranque del server y no se puede atrapar desde script. Ver ExorPid.
	override void OnStoreSave(ParamsWriteContext ctx)
	{
		super.OnStoreSave(ctx);
		ctx.Write(ExorPid.EXOR_MAGIC);
		ctx.Write(ExorGetPid());
		ctx.Write(m_ExorSpawnMin);
	}

	override bool OnStoreLoad(ParamsReadContext ctx, int version)
	{
		if (!super.OnStoreLoad(ctx, version))
			return false;

		int magic;
		if (!ctx.Read(magic))
			return false;
		if (magic != ExorPid.EXOR_MAGIC)
		{
			// Stream corrido. Se descarta la tumba en vez de arriesgarse a cargar con el id de
			// OTRA: dos tumbas con el mismo id comparten su JSON, o sea su loot.
			Print(string.Format("%1 GUARD: stream de tumba DESALINEADO (magico %2) -> tumba descartada", ExorStorageConstants.LOG, magic));
			return false;
		}
		int pid;
		if (!ctx.Read(pid))
			return false;
		if (!ExorPid.Plausible(pid))
			return false;
		m_ExorPid = pid;
		m_ExorID = pid.ToString();

		int sm;
		if (!ctx.Read(sm))
			return false;
		m_ExorSpawnMin = sm;
		return true;
	}

	override void AfterStoreLoad()
	{
		super.AfterStoreLoad();
		if (GetGame().IsServer())
		{
			m_ExorLastNearMs = GetGame().GetTime();
			m_ExorVirtualizedSync = ExorIsVirtualized();
			SetSynchDirty();
		}
	}

	// Id NUMERICO persistente (ver ExorPid): unica identidad de la tumba. El string de las
	// rutas es su representacion decimal.
	protected int m_ExorPid;

	int ExorGetPid()
	{
		if (m_ExorPid <= 0)
		{
			m_ExorPid = ExorPid.Nuevo();
			m_ExorID = m_ExorPid.ToString();
		}
		return m_ExorPid;
	}

	string ExorGetID()
	{
		if (m_ExorID == "")
			m_ExorID = ExorGetPid().ToString();
		return m_ExorID;
	}

	string ExorGetStoragePath()
	{
		return string.Format("%1\\%2.json", ExorStorageConstants.BODYBAG_DIR, ExorGetID());
	}

	// ------------------------- purga de JSON huerfanos -------------------------
	// Al arrancar, borra los JSON de bolsas que ya no existen. Se acumulaban porque el TTL
	// solo borraba el archivo si la bolsa estaba virtualizada, y porque un crash/reinicio
	// deja archivos sin dueño. En produccion habia 273 acumulados (4 dias) con tumbas que
	// duran 30 minutos.
	// Es seguro correrlo DESPUES de que la persistencia cargo las bolsas vivas: cada bolsa
	// viva se registra en el manager, asi que lo que no este registrado es basura.
	static void PurgarHuerfanos()
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (!FileExist(ExorStorageConstants.BODYBAG_DIR))
			return;

		// ids de las bolsas VIVAS (las que la persistencia acaba de cargar)
		set<string> vivas = new set<string>;
		array<Exor_BodyBag> bags = ExorVO_Manager.Get().m_BodyBags;
		int i;
		for (i = 0; i < bags.Count(); i++)
		{
			if (bags.Get(i))
				vivas.Insert(bags.Get(i).ExorGetID());
		}

		int borrados = 0;
		string pattern = string.Format("%1\\*.json", ExorStorageConstants.BODYBAG_DIR);
		string fileName;
		FileAttr attr;
		FindFileHandle h = FindFile(pattern, fileName, attr, FindFileFlags.ALL);
		if (h)
		{
			if (fileName != "" && PurgarUno(fileName, vivas))
				borrados++;
			while (FindNextFile(h, fileName, attr))
			{
				if (fileName != "" && PurgarUno(fileName, vivas))
					borrados++;
			}
			CloseFindFile(h);
		}
		if (borrados > 0)
			Print(string.Format("%1 BodyBags: %2 JSON huerfanos borrados al arrancar", ExorStorageConstants.LOG, borrados));
	}

	// true si borro el archivo. fileName viene SIN ruta (ej "123_45_67.json").
	static bool PurgarUno(string fileName, set<string> vivas)
	{
		int dot = fileName.IndexOf(".");
		if (dot <= 0)
			return false;
		string id = fileName.Substring(0, dot);
		if (vivas.Find(id) != -1)
			return false;	// tiene bolsa viva: es su virtualizacion, NO tocar
		DeleteFile(string.Format("%1\\%2", ExorStorageConstants.BODYBAG_DIR, fileName));
		return true;
	}

	// ESTADO VIRTUALIZADO, CACHEADO.
	// Era un FileExist -o sea una llamada al sistema de archivos- y lo consulta el tick de
	// proximidad de CADA tumba: con 250 tumbas vivas eso son cientos de stats por tick, en
	// el mismo bloque que ya habia sido el peor del server (pico medido de 656 ms).
	// El archivo lo escribe y lo borra SOLO este mod, y siempre por los tres caminos de
	// abajo, asi que la cache no se puede desincronizar sola. Se resuelve perezosamente la
	// primera vez (ahi si vale el FileExist: es cuando de verdad no se sabe).
	protected bool m_ExorVirtResuelto;
	protected bool m_ExorVirtCache;

	bool ExorIsVirtualized()
	{
		if (!m_ExorVirtResuelto)
		{
			m_ExorVirtResuelto = true;
			m_ExorVirtCache = FileExist(ExorGetStoragePath());
		}
		return m_ExorVirtCache;
	}

	// Unico punto donde se mueve la cache: que quede claro que va SIEMPRE junto con la
	// escritura o el borrado del archivo.
	void ExorSetVirtualizada(bool v)
	{
		m_ExorVirtResuelto = true;
		m_ExorVirtCache = v;
		m_ExorVirtualizedSync = v;
	}

	void ExorStampSpawn()
	{
		m_ExorSpawnMin = ExorTimeUtil.NowMinutes();
		m_ExorLastNearMs = GetGame().GetTime();
	}

	// items reales que tiene la bolsa AHORA = ropa en los slots de equipo (attachments) +
	// cargo directo. La mayor parte del loot del muerto vive en los slots (vest/back/body/...),
	// NO en el cargo, asi que contar solo el cargo daba 0 y rompia el guard de virtualizar.
	// ------------------- armas colgadas de una mochila: DESCOLGARLAS -------------------
	// Las mochilas de mods con slots propios de arma ("Shoulder" / "Melee") se NIEGAN a
	// salir de un contenedor mientras tengan algo colgado: el mod que las trae overridea
	// CanPutIntoHands() y CanPutInCargo() para devolver false si HasAttachments()
	// (ej. STAG_Clothing_ArmyBackpack_Base en Alteria). En un cadaver vanilla eso se
	// resuelve descolgando el rifle primero, pero DENTRO de la tumba el jugador la ve y no
	// la puede sacar de ninguna forma -> la mochila quedaba trabada, con todo su contenido.
	//
	// Al armar la tumba las descolgamos y las dejamos SUELTAS en el cargo: no se pierde
	// nada, la mochila sale como cualquier otra prenda y el arma queda a un click. Es el
	// mismo criterio que HoistRuinedContents (sacar del nudo lo que el motor no deja mover).
	//
	// OJO: se saltea la tumba MISMA. Ella declara "Shoulder"/"Melee" en sus attachments,
	// que es donde caen las armas que el muerto llevaba encima; esas salen sin problema y
	// tienen que quedarse en su slot.
	void ExorDescolgarArmas()
	{
		array<EntityAI> pend = new array<EntityAI>;
		ExorHijosDirectos(this, pend);	// nivel 1: la tumba MISMA no se toca

		int guard = 0;
		while (pend.Count() > 0 && guard < 4096)	// tope anti-ciclo; el arbol real de una tumba son ~50 nodos
		{
			guard++;
			EntityAI e = pend.Get(0);
			pend.Remove(0);
			if (!e)
				continue;
			ExorHijosDirectos(e, pend);	// seguir bajando (mochila dentro de mochila)
			ExorDescolgarSlot(e, "Shoulder");
			ExorDescolgarSlot(e, "Melee");
		}
	}

	// Si 'e' tiene algo colgado en 'slot', lo pasa al cargo de la tumba.
	void ExorDescolgarSlot(EntityAI e, string slot)
	{
		EntityAI arma = e.FindAttachmentBySlotName(slot);
		if (!arma)
			return;
		// MOVER la entidad REAL (nunca copiar: perderia cargador y miras)
		if (GetInventory().TakeEntityToCargo(InventoryMode.SERVER, arma))
		{
			Print(string.Format("%1 BodyBag %2: %3 descolgada de %4 -> al cargo de la tumba", ExorStorageConstants.LOG, ExorGetID(), arma.GetType(), e.GetType()));
			return;
		}
		// no entro en el cargo (300 slots: practicamente imposible) -> al piso al lado de la
		// tumba antes que dejarla trabada adentro. Nunca se pierde.
		arma.SetPosition(GetPosition());
		arma.SetOrientation("0 0 0");
		ItemBase armaib = ItemBase.Cast(arma);
		if (armaib)
			armaib.PlaceOnSurface();
		Print(string.Format("%1 AVISO: BodyBag %2: %3 no entro en el cargo -> dejada en el piso al lado", ExorStorageConstants.LOG, ExorGetID(), arma.GetType()));
	}

	// Agrega a 'dest' los attachments + el cargo DIRECTO de 'e' (un solo nivel).
	static void ExorHijosDirectos(EntityAI e, array<EntityAI> dest)
	{
		GameInventory inv = e.GetInventory();
		if (!inv)
			return;
		int i;
		for (i = 0; i < inv.AttachmentCount(); i++)
		{
			EntityAI att = inv.GetAttachmentFromIndex(i);
			if (att)
				dest.Insert(att);
		}
		CargoBase cg = inv.GetCargo();
		if (cg)
		{
			for (i = 0; i < cg.GetItemCount(); i++)
			{
				EntityAI it = cg.GetItem(i);
				if (it)
					dest.Insert(it);
			}
		}
	}

	int ExorContentCount()
	{
		GameInventory inv = GetInventory();
		if (!inv)
			return 0;
		int n = inv.AttachmentCount();
		CargoBase cargo = inv.GetCargo();
		if (cargo)
			n += cargo.GetItemCount();
		return n;
	}

	// ------------------------- tick (5s): TTL + virtualizar/restaurar por distancia -------------------------
	// Baja entidades sin perder loot: virtualiza (loot a disco) cuando NADIE vivo esta a
	// <alejar_metros por virtualizar_minutos, y restaura al acercarse un player a <acercar_
	// metros (backup del restore-on-open, que es lo confiable). El fix del bug historico
	// (0 restauraciones -> tumbas vacias) es el DOBLE trigger: proximidad (aca) + Open()
	// SINCRONO al abrir. acercar < alejar = histeresis para no thrashear.
	// ---- PASE 1 (todas las bolsas, todos los ticks): TTL ----
	// Es una resta de enteros: no vale la pena repartirlo y diferirlo haria que las tumbas
	// duren de mas. Devuelve true si la bolsa se borro (ya no existe).
	bool ExorBagTTLTick()
	{
		ExorCfgBodyCadaver cfg = GetExorConfig().bodycadaver;
		if (m_ExorSpawnMin <= 0 || cfg.duracion_minutos <= 0)
			return false;

		int age = ExorTimeUtil.NowMinutes() - m_ExorSpawnMin;
		if (age < cfg.duracion_minutos)
			return false;

		// Borrar el JSON SIEMPRE, no solo si estaba virtualizada. Si la bolsa expiraba
		// con items reales (habia alguien cerca, asi que nunca virtualizo) su archivo
		// quedaba huerfano para siempre: se encontraron 273 JSON acumulados desde
		// hacia 4 dias, con tumbas que duran 30 minutos.
		DeleteFile(ExorGetStoragePath());
		ExorSetVirtualizada(false);
		Print(string.Format("%1 BodyBag %2 expirada (%3 min) -> borrada", ExorStorageConstants.LOG, ExorGetID(), age));
		// ObjectDelete es DIFERIDO (el puntero sigue vivo en el array hasta fin de frame), asi
		// que se marca: el pase 2 de este mismo tick tiene que saltearla en vez de ponerse a
		// virtualizar (escribir un JSON) una bolsa que ya esta condenada.
		m_ExorExpirada = true;
		GetGame().ObjectDelete(this);
		return true;
	}

	// ---- PASE 2 (con cupo y cursor): virtualizar/restaurar por distancia ----
	// 'alivePos' viene YA resuelto por el manager (posiciones de los players vivos, calculadas
	// una sola vez por tick). Antes cada bolsa hacia su propio Cast+IsAlive+Distance por cada
	// player: con 250 tumbas y 50 players eran 12.500 casts por tick (pico medido: 656ms).
	// Diferir esto es seguro: el restore CONFIABLE es sincrono al abrir la tumba (Open()), esto
	// es solo el backup por proximidad.
	// 'permitirOps' = queda cupo en este tick para hacer el trabajo PESADO (virtualizar o
	// restaurar). Devuelve true si LO HIZO, para que el manager descuente ese cupo.
	//
	// POR QUE HACE FALTA EL CUPO. El chequeo de distancia es barato; virtualizar o restaurar
	// no (medido: ~4 ms y ~12 ms para una tumba de 5 prendas llenas + 45 items). Sin cupo,
	// una oleada -el final de un raid, con decenas de muertos a la vez- llega al limite de
	// tumbas revisadas por tick y hace las 40 operaciones en el MISMO frame. El cupo las
	// reparte; nada se pierde: la que no entro se hace en el siguiente tick, el cursor
	// rotativo le garantiza el turno, y abrir la tumba la restaura en el acto igual.
	bool ExorBagProximityTick(int nowMs, array<vector> alivePos, bool permitirOps)
	{
		if (m_ExorExpirada)
			return false;	// ya la borro el pase de TTL en este mismo tick
		ExorCfgBodyCadaver cfg = GetExorConfig().bodycadaver;

		// YA virtualizada: restaurar si un player vivo se acerca (backup del restore-on-open).
		if (ExorIsVirtualized())
		{
			if (!ExorPlayerNear(alivePos, cfg.acercar_metros))
				return false;
			if (!permitirOps)
				return false;	// sin cupo: el proximo tick. Abrir la tumba restaura igual, en el acto.
			ExorRestore();
			return true;
		}

		// NO virtualizada: virtualizar si esta activo y hace virtualizar_minutos que no hay
		// nadie vivo a <alejar_metros. Solo si tiene loot real (si no, no hay nada que sacar).
		if (cfg.virtualizar_minutos <= 0)
			return false;
		if (ExorPlayerNear(alivePos, cfg.alejar_metros))
		{
			m_ExorLastNearMs = nowMs;
			return false;
		}
		if (nowMs - m_ExorLastNearMs < cfg.virtualizar_minutos * 60000)
			return false;
		if (ExorContentCount() == 0)
			return false;
		if (!permitirOps)
			return false;
		ExorVirtualize();
		return true;
	}

	// hay algun player VIVO dentro de 'radius' de la tumba?
	// Compara distancias al CUADRADO: evita una raiz cuadrada por player y por bolsa, que en
	// el peor tick medido se ejecutaba miles de veces.
	bool ExorPlayerNear(array<vector> alivePos, float radius)
	{
		if (!alivePos)
			return false;
		vector p = GetPosition();
		float r2 = radius * radius;
		int i;
		for (i = 0; i < alivePos.Count(); i++)
		{
			if (vector.DistanceSq(alivePos.Get(i), p) <= r2)
				return true;
		}
		return false;
	}

	// RESTORE-ON-OPEN (confiable, sincrono): cuando un player abre la tumba, recrea el loot
	// del JSON ANTES de que vea el cargo -> nunca ve la tumba vacia (mismo patron del barril).
	override void Open()
	{
		super.Open();
		if (GetGame() && GetGame().IsServer() && ExorIsVirtualized())
			ExorRestore();
	}

	void ExorVirtualize()
	{
		GameInventory inv = GetInventory();
		if (!inv)
			return;

		// Lo que hay que sacar del mundo: la ropa de los SLOTS DE EQUIPO (donde vive la mayor
		// parte del loot del muerto, cada prenda con su cargo anidado) mas el CARGO directo
		// de la bolsa (armas movidas al morir / sobrante reubicado).
		// La ropa se guarda como ITEM y no como attachment a proposito: al restaurarla se la
		// deja caer en su slot con FindInventoryLocationType.ANY, y este restore solo mira la
		// lista de items. Guardarla en la otra lista seria perder toda la ropa del muerto.
		ExorVO_ContainerFile f = ExorContainerOps.CabeceraSnapshot(this, ExorGetID());
		array<EntityAI> toDelete = new array<EntityAI>;
		int i;
		for (i = 0; i < inv.AttachmentCount(); i++)
		{
			EntityAI att = inv.GetAttachmentFromIndex(i);
			if (att)
			{
				f.items.Insert(ExorVO_Serializer.CaptureItem(att));
				toDelete.Insert(att);
			}
		}
		CargoBase cargo = inv.GetCargo();
		if (cargo)
		{
			for (i = 0; i < cargo.GetItemCount(); i++)
			{
				EntityAI it = cargo.GetItem(i);
				if (it)
				{
					f.items.Insert(ExorVO_Serializer.CaptureItem(it));
					toDelete.Insert(it);
				}
			}
		}
		if (toDelete.Count() == 0)
			return;	// nada real que virtualizar

		// El dir bodybags\ DEBE existir o la escritura falla en SILENCIO (este era el bug
		// historico: se borraba el loot sin haberlo guardado -> tumbas vacias, 0 restauraciones).
		if (!FileExist(ExorStorageConstants.BODYBAG_DIR))
			MakeDirectory(ExorStorageConstants.BODYBAG_DIR);

		// ANTI-DUPE + ANTI-PERDIDA: escribir ANTES de borrar, y borrar SOLO si el archivo
		// quedo realmente escrito. Si la escritura fallo, el loot se queda como entidades
		// reales (no se pierde nada) y la tumba no se marca virtualizada.
		string path = ExorGetStoragePath();
		ExorContainerOps.GuardarJL(path, f);
		if (!FileExist(path))
		{
			Print(string.Format("%1 ERROR: BodyBag %2 no se pudo escribir el JSON -> NO virtualizo (loot intacto como entidades)", ExorStorageConstants.LOG, ExorGetID()));
			return;
		}
		for (i = 0; i < toDelete.Count(); i++)
			GetGame().ObjectDelete(toDelete.Get(i));

		ExorSetVirtualizada(true);
		SetSynchDirty();
		ExorDbg(string.Format("virtualizada: %1 items a disco", toDelete.Count()));	// rutina -> a debug
	}

	void ExorRestore()
	{
		string path = ExorGetStoragePath();
		if (!FileExist(path))
			return;

		// ANTI-DUPE: si la bolsa YA tiene items reales (ropa en slots + cargo) porque la
		// persistencia los cargo tras un crash/reinicio con un save previo a la virtualizacion,
		// esos son la verdad -> DESCARTAR el JSON, no restaurar encima (si no, se duplican).
		// Cuenta attachments+cargo (la ropa vive en los slots, no en el cargo).
		int real = ExorContentCount();
		if (real > 0)
		{
			DeleteFile(path);
			ExorSetVirtualizada(false);
			SetSynchDirty();
			Print(string.Format("%1 BodyBag %2: tenia %3 items reales -> JSON descartado (anti-dupe)", ExorStorageConstants.LOG, ExorGetID(), real));
			return;
		}

		ExorVO_ContainerFile f = new ExorVO_ContainerFile();
		ExorVO_ContainerFile jl = ExorContainerOps.LeerJL(path);
		if (jl)
			f = jl;

		// GUARD DE TUMBA (1): JSON ilegible/vacio -> la tumba no tiene nada que devolver y
		// quedaria de adorno. Se borra el archivo Y la tumba (no dejar fantasmas colgados).
		if (!f || !f.items)
		{
			DeleteFile(path);
			ExorSetVirtualizada(false);
			Print(string.Format("%1 GUARD: BodyBag %2 con JSON corrupto/ilegible -> tumba ELIMINADA", ExorStorageConstants.LOG, ExorGetID()));
			GetGame().ObjectDelete(this);
			return;
		}

		// GUARD DE TUMBA (2): podar de la data guardada lo imposible de restaurar (classnames
		// que ya no existen, cargadores que no calzan en su arma). Es la limpieza de la data
		// YA MALA: sin esto, restaurarla dejaba entidades a medio armar que se persistian y
		// tumbaban el arranque del server.
		int podados = ExorVO_Serializer.Sanitize(f.items, "", false);
		if (podados > 0)
			Print(string.Format("%1 GUARD: BodyBag %2 -> %3 item(s) corruptos descartados antes de restaurar", ExorStorageConstants.LOG, ExorGetID(), podados));

		// armar el loot EN LA POSICION de la bolsa (NO bajo tierra: a -1000m el motor lo limpia
		// por out-of-bounds antes de que entre al cargo -> se perdia el loot) y moverlo a la bolsa
		vector hidden = GetPosition();
		int pedidos = f.items.Count();
		int ok = 0;
		int i;
		for (i = 0; i < pedidos; i++)
		{
			if (ExorVO_Serializer.RestoreItem(f.items.Get(i), this, hidden))
				ok++;
		}

		// El restore ubica cada item con FindInventoryLocationType.ANY -> una mochila con
		// slots de arma se puede volver a quedar el rifle. Descolgar aca tambien.
		ExorDescolgarArmas();

		DeleteFile(path);	// consumido tras restaurar (anti-dupe)
		ExorSetVirtualizada(false);
		SetSynchDirty();

		// GUARD DE TUMBA (3): tenia loot y no se pudo restaurar NADA -> la tumba queda vacia
		// y sin sentido. Eliminarla en vez de dejar una lapida vacia para siempre.
		if (pedidos > 0 && ok == 0)
		{
			Print(string.Format("%1 GUARD: BodyBag %2 no pudo restaurar ninguno de sus %3 items -> tumba ELIMINADA", ExorStorageConstants.LOG, ExorGetID(), pedidos));
			GetGame().ObjectDelete(this);
			return;
		}
		if (ok < pedidos)
			Print(string.Format("%1 AVISO: BodyBag %2 restauro %3/%4 items (el resto era data invalida)", ExorStorageConstants.LOG, ExorGetID(), ok, pedidos));

		ExorDbg(string.Format("restaurada: %1 items", ok));	// rutina -> a debug
	}

	// NO MOVIBLE: no se puede levantar a las manos ni meter en otro contenedor.
	override bool IsTakeable()
	{
		return false;
	}

	override bool CanPutInCargo(EntityAI parent)
	{
		return false;
	}

	override bool CanPutIntoHands(EntityAI parent)
	{
		return false;
	}

	// No lockeable (igual que el barril)
	override bool CanReceiveAttachment(EntityAI attachment, int slotId)
	{
		if (attachment)
		{
			string type = attachment.GetType();
			type.ToLower();
			if (type.Contains("lock"))
				return false;
		}
		return super.CanReceiveAttachment(attachment, slotId);
	}

	// ------------------------- creacion desde el loot capturado (server) -------------------------
	// El loot se captura en PlayerBase.EEKilled (cuando el cuerpo esta INTACTO) y se
	// recrea aca dentro de la bolsa. Asi no depende de leer el cuerpo 1s despues
	// (cuando el motor ya pudo dropear/mover cosas).
	static Exor_BodyBag SpawnFromLoot(vector pos, array<ref ExorVO_ItemData> loot, array<EntityAI> moveItems)
	{
		if (!GetGame() || !GetGame().IsServer())
			return null;

		// GUARD PREVENTIVO: podar el loot capturado ANTES de crear la tumba. Asi nunca se
		// escribe en la persistencia una combinacion imposible (cargador que no calza en el
		// arma, classname fantasma) que despues rompe el arranque del server.
		int podados = ExorVO_Serializer.Sanitize(loot, "", false);
		if (podados > 0)
			Print(string.Format("%1 GUARD: %2 item(s) invalidos descartados del loot ANTES de crear la tumba", ExorStorageConstants.LOG, podados));

		Exor_BodyBag bag = Exor_BodyBag.Cast(GetGame().CreateObjectEx("Exor_BodyBag", pos, ECE_PLACE_ON_SURFACE));
		if (!bag)
		{
			Print(string.Format("%1 ERROR: no se pudo crear Exor_BodyBag", ExorStorageConstants.LOG));
			return null;
		}
		// PARAR la lapida vertical (sin esto queda acostada) + anclar al piso
		bag.SetOrientation("0 0 0");
		bag.SetPosition(pos);
		// BUGFIX (no deja coger items): si el cuerpo cayo hundido en el piso/un objeto, la
		// bolsa quedaba clippeada y la interaccion (mirar/tab) no la detectaba. Asentarla
		// limpiamente SOBRE la superficie la deja siempre accesible.
		bag.PlaceOnSurface();

		// PRENDAS DESTRUIDAS (ruined): su contenido no se puede abrir mientras la prenda
		// rota esta anidada/atada -> antes quedaba ATRAPADO y habia que tirar la prenda al
		// piso. Lo sacamos al nivel superior para que caiga directo en el cargo de la tumba.
		ExorVO_Serializer.HoistRuinedContents(loot);

		// el loot se ARMA en la posicion de la bolsa (NO bajo tierra: a -1000m el motor lo limpia
		// por out-of-bounds antes de que entre al cargo) y se mueve a la bolsa
		vector hidden = pos;

		int captured = 0;
		int restored = 0;
		if (loot)
		{
			int i;
			for (i = 0; i < loot.Count(); i++)
			{
				if (!loot.Get(i))
					continue;
				captured++;
				if (ExorVO_Serializer.RestoreItem(loot.Get(i), bag, hidden))	// cada prenda cae en su slot (ANY)
					restored++;
			}
		}

		// armas (mano + espalda/hombro) y lo que tenga en manos: MOVER la entidad
		// REAL (preserva cargador + miras). Copiar/recrear un arma pierde el mag.
		if (moveItems)
		{
			int w;
			for (w = 0; w < moveItems.Count(); w++)
			{
				EntityAI it = moveItems.Get(w);
				if (!it)
					continue;
				if (bag.GetInventory().TakeEntityToInventory(InventoryMode.SERVER, FindInventoryLocationType.ANY, it))
				{
					Print(string.Format("%1 BodyBag: %2 movida a la bolsa (real)", ExorStorageConstants.LOG, it.GetType()));
					continue;
				}
				// fallback: no entro en la bolsa (ej. arma larga sin hueco). NUNCA copiar+borrar
				// (eso perdia el cargador y, si la copia no se ubicaba, perdia el arma entera).
				// En su lugar dejamos el ARMA REAL en el piso al lado de la bolsa: conserva
				// cargador + miras y queda lootable. Nunca se pierde.
				it.SetPosition(pos);
				it.SetOrientation("0 0 0");
				ItemBase itib = ItemBase.Cast(it);
				if (itib)
					itib.PlaceOnSurface();
				Print(string.Format("%1 AVISO: %2 no entro en la bolsa -> dejada en el piso al lado (real, intacta)", ExorStorageConstants.LOG, it.GetType()));
			}
		}

		// VA AL FINAL a proposito: el loop de moveItems usa FindInventoryLocationType.ANY,
		// asi que el propio motor puede colgar el rifle del slot "Shoulder" de una mochila
		// que ya esta adentro. Descolgar antes no serviria de nada.
		bag.ExorDescolgarArmas();

		bag.ExorStampSpawn();
		Print(string.Format("%1 BodyBag %2 creada en %3 (loot %4/%5)", ExorStorageConstants.LOG, bag.ExorGetID(), pos.ToString(), restored, captured));
		return bag;
	}
}
