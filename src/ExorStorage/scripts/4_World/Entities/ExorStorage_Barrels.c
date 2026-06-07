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
			ExorStorageSettings settings = GetExorStorageSettings();
			int now = GetGame().GetTime();
			int cdMs = settings.cooldown_abrir_segundos * 1000;
			if (cdMs > 0 && m_ExorLastCloseMs > 0 && now - m_ExorLastCloseMs < cdMs)
			{
				// Anti-dupe: cooldown de reapertura activo
				return;
			}
			ExorRestoreIfNeeded();
			m_ExorLastInteractMs = now;
		}
		super.Open();
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
	void ExorTick(int now, ExorStorageSettings settings)
	{
		// Auto-cierre de barril dejado abierto
		if (IsOpen())
		{
			if (settings.auto_cerrar_minutos > 0 && now - m_ExorLastInteractMs > settings.auto_cerrar_minutos * 60000)
			{
				Close();
				Print(string.Format("%1 Barril %2 auto-cerrado por inactividad", ExorStorageConstants.LOG, ExorGetID()));
			}
			return;
		}

		// Virtualizacion: cerrado, con contenido, sin interaccion por X min
		if (settings.virtualizar_minutos <= 0)
			return;
		if (ExorIsVirtualized())
			return;
		if (now - m_ExorLastInteractMs < settings.virtualizar_minutos * 60000)
			return;
		if (ExorCargoCount() == 0)
			return;

		ExorVirtualize();
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

		Print(string.Format("%1 Barril %2 virtualizado: %3 items a disco", ExorStorageConstants.LOG, ExorGetID(), f.items.Count()));
	}

	void ExorRestoreIfNeeded()
	{
		string path = ExorGetStoragePath();
		if (!FileExist(path))
			return;

		ExorVO_ContainerFile f = new ExorVO_ContainerFile();
		JsonFileLoader<ExorVO_ContainerFile>.JsonLoadFile(path, f);

		int restored = 0;
		int i;
		for (i = 0; i < f.items.Count(); i++)
		{
			EntityAI e = ExorVO_Serializer.RestoreItem(f.items.Get(i), this, GetPosition());
			if (e)
				restored++;
		}

		// ANTI-DUPE: el JSON se elimina tras restaurar (una caja dupeada
		// encontraria el archivo ya consumido -> barril vacio)
		DeleteFile(path);

		Print(string.Format("%1 Barril %2 restaurado: %3/%4 items", ExorStorageConstants.LOG, ExorGetID(), restored, f.items.Count()));
	}

	// ------------------------- reglas de guardado (Fase 3) -------------------------
	override bool CanReceiveItemIntoCargo(EntityAI item)
	{
		if (GetGame().IsServer() && item)
		{
			ExorStorageSettings settings = GetExorStorageSettings();
			if (settings.blacklist.Find(item.GetType()) != -1)
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
		// disco; empaquetarlo perderia el contenido
		if (ExorIsVirtualized())
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
