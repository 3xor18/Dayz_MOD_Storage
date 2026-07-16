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

	void Exor_BodyBag()
	{
		RegisterNetSyncVariableBool("m_ExorVirtualizedSync");
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

	override void OnStoreSave(ParamsWriteContext ctx)
	{
		super.OnStoreSave(ctx);
		ctx.Write(m_ExorID);
		ctx.Write(m_ExorSpawnMin);
	}

	override bool OnStoreLoad(ParamsReadContext ctx, int version)
	{
		if (!super.OnStoreLoad(ctx, version))
			return false;
		string id;
		if (!ctx.Read(id))
			return false;
		m_ExorID = id;
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

	string ExorGetID()
	{
		if (m_ExorID == "")
			m_ExorID = ExorVO_Serializer.GenerateId();
		return m_ExorID;
	}

	string ExorGetStoragePath()
	{
		return string.Format("%1\\%2.json", ExorStorageConstants.BODYBAG_DIR, ExorGetID());
	}

	bool ExorIsVirtualized()
	{
		return FileExist(ExorGetStoragePath());
	}

	void ExorStampSpawn()
	{
		m_ExorSpawnMin = ExorTimeUtil.NowMinutes();
		m_ExorLastNearMs = GetGame().GetTime();
	}

	// items reales que tiene la bolsa AHORA = ropa en los slots de equipo (attachments) +
	// cargo directo. La mayor parte del loot del muerto vive en los slots (vest/back/body/...),
	// NO en el cargo, asi que contar solo el cargo daba 0 y rompia el guard de virtualizar.
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
	void ExorBagTick(int nowMs, array<Man> players)
	{
		ExorCfgBodyCadaver cfg = GetExorConfig().bodycadaver;

		// TTL: borrar la bolsa pasados duracion_minutos (sobrevive reinicio)
		if (m_ExorSpawnMin > 0 && cfg.duracion_minutos > 0)
		{
			int age = ExorTimeUtil.NowMinutes() - m_ExorSpawnMin;
			if (age >= cfg.duracion_minutos)
			{
				if (ExorIsVirtualized())
					DeleteFile(ExorGetStoragePath());
				Print(string.Format("%1 BodyBag %2 expirada (%3 min) -> borrada", ExorStorageConstants.LOG, ExorGetID(), age));
				GetGame().ObjectDelete(this);
				return;
			}
		}

		// YA virtualizada: restaurar si un player vivo se acerca (backup del restore-on-open).
		if (ExorIsVirtualized())
		{
			if (ExorPlayerNear(players, cfg.acercar_metros))
				ExorRestore();
			return;
		}

		// NO virtualizada: virtualizar si esta activo y hace virtualizar_minutos que no hay
		// nadie vivo a <alejar_metros. Solo si tiene loot real (si no, no hay nada que sacar).
		if (cfg.virtualizar_minutos > 0)
		{
			bool near = ExorPlayerNear(players, cfg.alejar_metros);
			if (near)
				m_ExorLastNearMs = nowMs;
			else if (nowMs - m_ExorLastNearMs >= cfg.virtualizar_minutos * 60000 && ExorContentCount() > 0)
				ExorVirtualize();
		}
	}

	// hay algun player VIVO dentro de 'radius' de la tumba?
	bool ExorPlayerNear(array<Man> players, float radius)
	{
		if (!players)
			return false;
		vector p = GetPosition();
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase pb = PlayerBase.Cast(players.Get(i));
			if (pb && pb.IsAlive() && vector.Distance(pb.GetPosition(), p) <= radius)
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
		ExorVO_ContainerFile f = new ExorVO_ContainerFile();
		f.id = ExorGetID();
		f.owner_type = GetType();

		GameInventory inv = GetInventory();
		array<EntityAI> toDelete = new array<EntityAI>;
		int i;

		// 1) SLOTS DE EQUIPO (ropa: vest/back/body/legs/feet/headgear...). Aca vive la mayor
		//    parte del loot del muerto; cada prenda con su cargo anidado (CaptureItem recursa).
		for (i = 0; i < inv.AttachmentCount(); i++)
		{
			EntityAI att = inv.GetAttachmentFromIndex(i);
			if (att)
			{
				f.items.Insert(ExorVO_Serializer.CaptureItem(att));
				toDelete.Insert(att);
			}
		}

		// 2) CARGO directo de la bolsa (armas movidas al morir / sobrante reubicado).
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

		if (f.items.Count() == 0)
			return;	// nada real que virtualizar

		// El dir bodybags\ DEBE existir o JsonSaveFile falla en SILENCIO (este era el bug
		// historico: se borraba el loot sin haberlo guardado -> tumbas vacias, 0 restauraciones).
		if (!FileExist(ExorStorageConstants.BODYBAG_DIR))
			MakeDirectory(ExorStorageConstants.BODYBAG_DIR);

		// ANTI-DUPE + ANTI-PERDIDA: escribir el JSON ANTES de borrar, y borrar SOLO si el
		// archivo quedo realmente escrito. Si el save fallo por lo que sea, dejamos el loot
		// como entidades reales (no se pierde nada) y no marcamos virtualizado.
		string path = ExorGetStoragePath();
		JsonFileLoader<ExorVO_ContainerFile>.JsonSaveFile(path, f);
		if (!FileExist(path))
		{
			Print(string.Format("%1 ERROR: BodyBag %2 no se pudo escribir el JSON -> NO virtualizo (loot intacto como entidades)", ExorStorageConstants.LOG, ExorGetID()));
			return;
		}
		for (i = 0; i < toDelete.Count(); i++)
			GetGame().ObjectDelete(toDelete.Get(i));

		m_ExorVirtualizedSync = true;
		SetSynchDirty();
		Print(string.Format("%1 BodyBag %2 virtualizada: %3 items a disco", ExorStorageConstants.LOG, ExorGetID(), f.items.Count()));
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
			m_ExorVirtualizedSync = false;
			SetSynchDirty();
			Print(string.Format("%1 BodyBag %2: tenia %3 items reales -> JSON descartado (anti-dupe)", ExorStorageConstants.LOG, ExorGetID(), real));
			return;
		}

		ExorVO_ContainerFile f = new ExorVO_ContainerFile();
		JsonFileLoader<ExorVO_ContainerFile>.JsonLoadFile(path, f);
		// armar el loot EN LA POSICION de la bolsa (NO bajo tierra: a -1000m el motor lo limpia
		// por out-of-bounds antes de que entre al cargo -> se perdia el loot) y moverlo a la bolsa
		vector hidden = GetPosition();
		int i;
		for (i = 0; i < f.items.Count(); i++)
			ExorVO_Serializer.RestoreItem(f.items.Get(i), this, hidden);

		DeleteFile(path);	// consumido tras restaurar (anti-dupe)
		m_ExorVirtualizedSync = false;
		SetSynchDirty();
		Print(string.Format("%1 BodyBag %2 restaurada: %3 items", ExorStorageConstants.LOG, ExorGetID(), f.items.Count()));
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

		bag.ExorStampSpawn();
		Print(string.Format("%1 BodyBag %2 creada en %3 (loot %4/%5)", ExorStorageConstants.LOG, bag.ExorGetID(), pos.ToString(), restored, captured));
		return bag;
	}
}
