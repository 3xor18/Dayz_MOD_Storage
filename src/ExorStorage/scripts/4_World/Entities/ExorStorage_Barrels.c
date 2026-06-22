// ============================================================================
// 3xor_Vanilla_Optimization - Barril 3xor
// Fase 1: empaquetar/desplegar, indestructible, no lockeable
// Fase 2: virtualizacion del contenido + auto-cierre
// Fase 3: cooldown anti-dupe de apertura + blacklist + ropa con items
// ============================================================================

class Exor_Barrel_Base : Barrel_ColorBase
{
	// ID persistente del barril (liga el barril con su JSON de contenido)
	protected string m_ExorID;
	// Timestamps de runtime (ms de uptime del server)
	protected int m_ExorLastInteractMs;
	protected int m_ExorLastCloseMs;
	// Sincronizado al cliente: el barril tiene contenido virtualizado en disco
	// (el cliente no puede chequear el archivo; sin esto mostraria "Empaquetar")
	protected bool m_ExorVirtualizedSync;

	void Exor_Barrel_Base()
	{
		RegisterNetSyncVariableBool("m_ExorVirtualizedSync");
	}

	// ------------------------- init / persistencia -------------------------
	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			SetAllowDamage(false);	// indestructible: el loot nunca se pierde
			m_ExorLastInteractMs = GetGame().GetTime();
			m_ExorLastCloseMs = 0;
			ExorVO_Manager.RegisterBarrel(this);
		}
	}

	override void EEDelete(EntityAI parent)
	{
		if (GetGame().IsServer())
		{
			ExorVO_Manager.UnregisterBarrel(this);
		}
		super.EEDelete(parent);
	}

	override void OnStoreSave(ParamsWriteContext ctx)
	{
		super.OnStoreSave(ctx);
		ctx.Write(m_ExorID);
	}

	override bool OnStoreLoad(ParamsReadContext ctx, int version)
	{
		if (!super.OnStoreLoad(ctx, version))
			return false;
		string id;
		if (!ctx.Read(id))
			return false;
		m_ExorID = id;
		return true;
	}

	override void AfterStoreLoad()
	{
		super.AfterStoreLoad();
		// Tras reinicio, avisar al cliente si este barril esta virtualizado
		if (GetGame().IsServer())
		{
			m_ExorVirtualizedSync = ExorIsVirtualized();
			SetSynchDirty();
		}
	}

	string ExorGetID()
	{
		if (m_ExorID == "")
		{
			m_ExorID = ExorVO_Serializer.GenerateId();
		}
		return m_ExorID;
	}

	string ExorGetStoragePath()
	{
		return string.Format("%1\\%2.json", ExorStorageConstants.STORAGE_DIR, ExorGetID());
	}

	bool ExorIsVirtualized()
	{
		return FileExist(ExorGetStoragePath());
	}

	// ------------------------- abrir / cerrar -------------------------
	override void Open()
	{
		if (GetGame().IsServer())
		{
			ExorCfgStorage settings = GetExorConfig().storage;
			int now = GetGame().GetTime();
			int cdMs = settings.cooldown_abrir_segundos * 1000;
			if (cdMs > 0 && m_ExorLastCloseMs > 0 && now - m_ExorLastCloseMs < cdMs)
			{
				// Anti-dupe: cooldown de reapertura activo
				return;
			}
		}

		// IMPORTANTE: abrir ANTES de restaurar — un barril cerrado rechaza
		// items en su cargo (regla vanilla) y todo caeria al piso
		super.Open();

		if (GetGame().IsServer())
		{
			ExorRestoreIfNeeded();
			m_ExorLastInteractMs = GetGame().GetTime();
		}
	}

	override void Close()
	{
		super.Close();
		if (GetGame().IsServer())
		{
			int now = GetGame().GetTime();
			m_ExorLastInteractMs = now;
			m_ExorLastCloseMs = now;
		}
	}

	// ------------------------- virtualizacion (Fase 2) -------------------------
	// Llamado por el manager cada tick
	// Devuelve true si virtualizo en este tick (el manager usa eso para el throttle).
	// allowVirtualize=false -> el barril igual se auto-cierra pero NO virtualiza este tick.
	bool ExorTick(int now, ExorCfgStorage settings, bool allowVirtualize)
	{
		// Umbrales en ms (config en SEGUNDOS). Recomendado: cerrar 10s, virtualizar 30s.
		int cerrarMs = settings.auto_cerrar_segundos * 1000;
		int virtMs = settings.virtualizar_segundos * 1000;

		// Auto-cierre por CERCANIA del jugador: mientras haya alguien a menos de
		// cerrar_distancia_metros, el barril sigue ABIERTO (lo esta usando, aunque solo
		// ordene su inventario sin mover items del barril). Se cierra recien cerrarMs
		// DESPUES de que el ultimo jugador se aleja. (EECargoIn/Out tambien resetean.)
		if (IsOpen())
		{
			if (ExorVO_Manager.IsAlivePlayerNear(GetPosition(), settings.cerrar_distancia_metros))
				m_ExorLastInteractMs = now;
			if (cerrarMs > 0 && now - m_ExorLastInteractMs > cerrarMs)
			{
				Close();
				Print(string.Format("%1 Barril %2 auto-cerrado (nadie cerca)", ExorStorageConstants.LOG, ExorGetID()));
			}
			return false;
		}

		if (!allowVirtualize)	// THROTTLE: sin cupo este tick -> virtualiza en el proximo
			return false;

		// Virtualizacion: cerrado, con contenido, sin interaccion por el umbral. Al
		// virtualizar se escribe el JSON (respaldo) y se sacan los items del mundo ->
		// el barril queda crash-safe (cualquier reinicio restaura del JSON) y aliviana el server.
		if (virtMs <= 0)
			return false;
		if (ExorIsVirtualized())
			return false;
		if (now - m_ExorLastInteractMs < virtMs)
			return false;
		if (ExorCargoCount() == 0)
			return false;

		ExorVirtualize();
		return true;
	}

	// reinicia el timer de inactividad cada vez que entra/sale un item del barril (asi el
	// auto-cierre de 10s mide actividad REAL, no el tiempo desde que se abrio).
	// (Nombres vanilla correctos: EECargoIn/EECargoOut, 1 solo parametro.)
	override void EECargoIn(EntityAI item)
	{
		super.EECargoIn(item);
		if (GetGame() && GetGame().IsServer())
			m_ExorLastInteractMs = GetGame().GetTime();
	}

	override void EECargoOut(EntityAI item)
	{
		super.EECargoOut(item);
		if (GetGame() && GetGame().IsServer())
			m_ExorLastInteractMs = GetGame().GetTime();
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

		// ANTI-DUPE: escribir el JSON ANTES de borrar los items (crash-safe)
		JsonFileLoader<ExorVO_ContainerFile>.JsonSaveFile(ExorGetStoragePath(), f);

		for (i = 0; i < toDelete.Count(); i++)
		{
			GetGame().ObjectDelete(toDelete.Get(i));
		}

		m_ExorVirtualizedSync = true;
		SetSynchDirty();

		Print(string.Format("%1 Barril %2 virtualizado: %3 items a disco", ExorStorageConstants.LOG, ExorGetID(), f.items.Count()));
	}

	void ExorRestoreIfNeeded()
	{
		string path = ExorGetStoragePath();
		if (!FileExist(path))
			return;

		// ANTI-DUPE CRITICO: si existe el JSON, el barril esta virtualizado y el JSON
		// es la UNICA verdad. Tras un crash/reinicio (este server reinicia muy seguido)
		// el engine puede restaurar en el cargo una COPIA VIEJA de los items (snapshot
		// previo al borrado de la virtualizacion). Si restauramos el JSON ENCIMA, se
		// DUPLICAN y, al pasar de los 500 slots, el sobrante CAE AL PISO ("no entro,
		// lleno"). Por eso vaciamos primero el cargo actual (items stale del engine).
		// Un barril virtualizado+cerrado no puede tener items legitimos en el cargo
		// (la virtualizacion los borro), asi que esto nunca borra loot real.
		CargoBase cargo = null;
		if (GetInventory())
			cargo = GetInventory().GetCargo();
		int staleN = 0;
		if (cargo)
		{
			array<EntityAI> stale = new array<EntityAI>;
			int s;
			for (s = 0; s < cargo.GetItemCount(); s++)
			{
				EntityAI se = cargo.GetItem(s);
				if (se)
					stale.Insert(se);
			}
			staleN = stale.Count();
			for (s = 0; s < staleN; s++)
				GetGame().ObjectDelete(stale.Get(s));
		}

		if (staleN > 0)
		{
			// habia copias viejas: ObjectDelete no libera los slots del cargo en el
			// acto, asi que diferimos la restauracion 1 tick para no recrear sobre un
			// cargo que el engine todavia ve "lleno" (volveria a tirar loot al piso)
			Print(string.Format("%1 Barril %2: %3 items stale del engine borrados antes de restaurar (anti-dupe)", ExorStorageConstants.LOG, ExorGetID(), staleN));
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorDoRestore, 250, false);
			return;
		}

		ExorDoRestore();
	}

	void ExorDoRestore()
	{
		string path = ExorGetStoragePath();
		if (!FileExist(path))
			return;	// idempotente: si ya se restauro (doble Open), el archivo no esta

		ExorVO_ContainerFile f = new ExorVO_ContainerFile();
		JsonFileLoader<ExorVO_ContainerFile>.JsonLoadFile(path, f);

		// restaurar GRANDES PRIMERO (evita que un rifle quede afuera por fragmentacion
		// cuando el barril esta casi lleno -> todo entra en el mismo espacio). Se arma BAJO
		// TIERRA (1000m abajo) para que no se vea el parpadeo de items en el piso.
		vector hidden = GetPosition();
		hidden[1] = hidden[1] - 1000.0;
		ExorVO_Serializer.RestoreItemsBigFirst(f.items, this, hidden);

		// ANTI-DUPE: el JSON se elimina tras restaurar (una caja dupeada
		// encontraria el archivo ya consumido -> barril vacio)
		DeleteFile(path);

		m_ExorVirtualizedSync = false;
		SetSynchDirty();

		Print(string.Format("%1 Barril %2 restaurado: %3 items", ExorStorageConstants.LOG, ExorGetID(), f.items.Count()));
	}

	// ------------------------- reglas de guardado (Fase 3) -------------------------
	override bool CanReceiveItemIntoCargo(EntityAI item)
	{
		if (GetGame().IsServer() && item)
		{
			if (GetExorConfig().storage.blacklist.Find(item.GetType()) != -1)
				return false;
		}
		return super.CanReceiveItemIntoCargo(item);
	}

	// No lockeable: bloquea CodeLock / candados de cualquier mod
	override bool CanReceiveAttachment(EntityAI attachment, int slotId)
	{
		if (attachment)
		{
			string type = attachment.GetType();
			type.ToLower();
			if (type.Contains("codelock") || type.Contains("combinationlock") || type.Contains("padlock") || type.Contains("lock"))
				return false;
		}
		return super.CanReceiveAttachment(attachment, slotId);
	}

	// ------------------------- empaquetado (Fase 1) -------------------------
	// Solo si esta cerrado, sano, sin items adentro Y SIN contenido virtualizado.
	// El liquido (agua de lluvia) NO bloquea: se descarta al empaquetar.
	bool ExorCanBePacked()
	{
		if (IsOpen())
			return false;
		if (IsRuined())
			return false;
		// CRITICO: un barril virtualizado parece vacio pero su loot esta en
		// disco; empaquetarlo perderia el contenido. La variable sincronizada
		// hace que el CLIENTE tampoco muestre la accion
		if (m_ExorVirtualizedSync)
			return false;
		if (GetGame().IsServer() && ExorIsVirtualized())
			return false;
		if (GetInventory())
		{
			if (GetInventory().AttachmentCount() > 0)
				return false;
			CargoBase cargo = GetInventory().GetCargo();
			if (cargo && cargo.GetItemCount() > 0)
				return false;
		}
		return true;
	}

	string ExorGetPackedType()
	{
		return "";
	}
}

class Exor_Barrel_500 : Exor_Barrel_Base
{
	override string ExorGetPackedType()
	{
		return "Exor_Barrel_500_Packed";
	}
}

// ---------------------------------------------------------------------------
// Barril empaquetado (item transportable, sin cargo). Se despliega con accion.
// ---------------------------------------------------------------------------
class Exor_Barrel_Packed_Base : ItemBase
{
	// La caja tambien es indestructible (consistente con el barril)
	override void EEInit()
	{
		super.EEInit();
		if (GetGame().IsServer())
		{
			SetAllowDamage(false);
		}
	}

	override void SetActions()
	{
		super.SetActions();
		AddAction(ExorActionDeployBarrel);
	}

	string ExorGetDeployedType()
	{
		return "";
	}
}

class Exor_Barrel_500_Packed : Exor_Barrel_Packed_Base
{
	override string ExorGetDeployedType()
	{
		return "Exor_Barrel_500";
	}
}
