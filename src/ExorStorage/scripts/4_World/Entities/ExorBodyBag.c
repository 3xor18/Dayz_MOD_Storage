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

	int ExorCargoCount()
	{
		GameInventory inv = GetInventory();
		if (!inv)
			return 0;
		CargoBase cargo = inv.GetCargo();
		if (!cargo)
			return 0;
		return cargo.GetItemCount();
	}

	// ------------------------- tick lento (5s): TTL -------------------------
	// IMPORTANTE: las TUMBAS ya NO se virtualizan. Virtualizar sacaba el loot a disco
	// tras 5 min sin nadie a <10m y el camino de restauracion no lo devolvia a tiempo
	// (0 restauraciones en produccion sobre 270 virtualizaciones) -> las tumbas quedaban
	// VACIAS y se perdian las armas/gear. Una tumba vive pocos minutos (duracion_minutos)
	// y guarda el loot de UN jugador: el costo de tenerlo real es trivial y evita el
	// round-trip fragil que ademas degrada las armas. El loot queda SIEMPRE como
	// entidades reales durante toda la vida de la tumba.
	void ExorBagTick(int nowMs)
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

		// RECUPERACION: una tumba que quedo virtualizada por una version anterior (JSON en
		// disco) se realiza AHORA, sin depender de que un player se acerque -> devuelve el
		// loot que estaba atrapado en disco.
		if (ExorIsVirtualized())
			ExorRestore();
	}

	// ------------------------- tick rapido (5s): recuperar loot legacy -------------------------
	// Las tumbas ya no duermen/virtualizan. Solo sirve para recuperar de inmediato una
	// tumba que quedo virtualizada por una version anterior (devuelve su loot a mundo).
	void ExorBagWake()
	{
		if (ExorIsVirtualized())
			ExorRestore();
	}

	void ExorVirtualize()
	{
		ExorVO_ContainerFile f = new ExorVO_ContainerFile();
		f.id = ExorGetID();
		f.owner_type = GetType();

		CargoBase cargo = GetInventory().GetCargo();
		array<EntityAI> toDelete = new array<EntityAI>;
		int i;
		for (i = 0; i < cargo.GetItemCount(); i++)
		{
			EntityAI it = cargo.GetItem(i);
			if (it)
			{
				f.items.Insert(ExorVO_Serializer.CaptureItem(it));
				toDelete.Insert(it);
			}
		}

		// ANTI-DUPE: escribir el JSON ANTES de borrar (crash-safe)
		JsonFileLoader<ExorVO_ContainerFile>.JsonSaveFile(ExorGetStoragePath(), f);
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

		// ANTI-DUPE: si la bolsa YA tiene items reales en el cargo (la persistencia los
		// cargo tras un crash/reinicio con un save previo a la virtualizacion), esos son
		// la verdad -> DESCARTAR el JSON, no restaurar encima (si no, se duplican: cargo
		// real + lo que el engine tiro al piso por "invalid location").
		CargoBase rc = null;
		if (GetInventory())
			rc = GetInventory().GetCargo();
		if (rc && rc.GetItemCount() > 0)
		{
			DeleteFile(path);
			m_ExorVirtualizedSync = false;
			SetSynchDirty();
			Print(string.Format("%1 BodyBag %2: tenia %3 items reales -> JSON descartado (anti-dupe)", ExorStorageConstants.LOG, ExorGetID(), rc.GetItemCount()));
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
