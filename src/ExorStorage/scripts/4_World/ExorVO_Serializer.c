// ============================================================================
// 3xor_Vanilla_Optimization - Serializador recursivo de entidades
// Captura/restaura items con: vida, cantidad, balas de cargadores, etapa de
// comida, attachments y cargo anidado (mochilas con cosas adentro, etc.)
// ============================================================================

class ExorVO_ItemData
{
	string type;
	float health = 1.0;
	float quantity = -1;
	int ammoCount = -1;
	int foodStage = -1;
	ref array<ref ExorVO_ItemData> attachments;
	ref array<ref ExorVO_ItemData> cargo;

	void ExorVO_ItemData()
	{
		attachments = new array<ref ExorVO_ItemData>;
		cargo = new array<ref ExorVO_ItemData>;
	}
}

// Archivo de un barril virtualizado
class ExorVO_ContainerFile
{
	int version = 1;
	string id;
	string owner_type;
	ref array<ref ExorVO_ItemData> items;

	void ExorVO_ContainerFile()
	{
		items = new array<ref ExorVO_ItemData>;
	}
}

// Trabajo de relleno diferido: llenar un contenedor recien creado un instante
// despues, cuando su inventario ya esta inicializado
class ExorVO_RestoreJob
{
	EntityAI parent;
	vector fallbackPos;
	ref array<ref ExorVO_ItemData> children;

	void ExorVO_RestoreJob()
	{
		children = new array<ref ExorVO_ItemData>;
	}
}

class ExorVO_Serializer
{
	// ------------------------- CAPTURA -------------------------
	static ExorVO_ItemData CaptureItem(EntityAI e)
	{
		ExorVO_ItemData data = new ExorVO_ItemData();
		data.type = e.GetType();
		data.health = e.GetHealth01("", "");

		ItemBase ib = ItemBase.Cast(e);
		if (ib && ib.HasQuantity())
		{
			data.quantity = ib.GetQuantity();
		}

		Magazine mag = Magazine.Cast(e);
		if (mag)
		{
			data.ammoCount = mag.GetAmmoCount();
		}

		Edible_Base ed = Edible_Base.Cast(e);
		if (ed && ed.GetFoodStage())
		{
			data.foodStage = ed.GetFoodStage().GetFoodStageType();
		}

		CaptureChildren(e, data);
		return data;
	}

	static void CaptureChildren(EntityAI e, ExorVO_ItemData data)
	{
		GameInventory inv = e.GetInventory();
		if (!inv)
			return;

		int i;
		for (i = 0; i < inv.AttachmentCount(); i++)
		{
			EntityAI att = inv.GetAttachmentFromIndex(i);
			if (att)
			{
				data.attachments.Insert(CaptureItem(att));
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
					data.cargo.Insert(CaptureItem(it));
				}
			}
		}
	}

	// ------------------------- RESTAURACION -------------------------
	// Mantiene vivos los trabajos de relleno diferido hasta que se procesan
	static ref array<ref ExorVO_RestoreJob> s_PendingJobs = new array<ref ExorVO_RestoreJob>;

	static EntityAI RestoreItem(ExorVO_ItemData data, EntityAI parent, vector fallbackPos)
	{
		EntityAI e;

		if (parent)
		{
			e = parent.GetInventory().CreateInInventory(data.type);
		}
		if (!e)
		{
			// No entro (inventario lleno o sin parent): al piso como fallback
			e = EntityAI.Cast(GetGame().CreateObjectEx(data.type, fallbackPos, ECE_PLACE_ON_SURFACE));
		}
		if (!e)
		{
			Print(string.Format("%1 ERROR restaurando item tipo '%2' (clase desconocida?)", ExorStorageConstants.LOG, data.type));
			return null;
		}

		e.SetHealth01("", "", data.health);

		ItemBase ib = ItemBase.Cast(e);
		if (ib && data.quantity >= 0)
		{
			ib.SetQuantity(data.quantity);
		}

		Magazine mag = Magazine.Cast(e);
		if (mag && data.ammoCount >= 0)
		{
			mag.ServerSetAmmoCount(data.ammoCount);
		}

		if (data.foodStage > 0)
		{
			Edible_Base ed = Edible_Base.Cast(e);
			if (ed)
			{
				ed.ChangeFoodStage(data.foodStage);
			}
		}

		// Hijos (attachments + cargo anidado): DIFERIDOS. Un contenedor recien
		// creado no tiene su inventario listo en el mismo frame; si llenamos ya,
		// el contenido cae al piso. CreateInInventory elige solo slot o cargo.
		if (data.attachments.Count() > 0 || data.cargo.Count() > 0)
		{
			ExorVO_RestoreJob job = new ExorVO_RestoreJob();
			job.parent = e;
			job.fallbackPos = fallbackPos;
			int i;
			for (i = 0; i < data.attachments.Count(); i++)
			{
				job.children.Insert(data.attachments.Get(i));
			}
			for (i = 0; i < data.cargo.Count(); i++)
			{
				job.children.Insert(data.cargo.Get(i));
			}
			s_PendingJobs.Insert(job);
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorVO_Serializer.RunRestoreJob, 200, false, job);
		}

		return e;
	}

	static void RunRestoreJob(ExorVO_RestoreJob job)
	{
		if (job && job.parent)
		{
			int i;
			for (i = 0; i < job.children.Count(); i++)
			{
				RestoreItem(job.children.Get(i), job.parent, job.fallbackPos);
			}
		}
		int idx = s_PendingJobs.Find(job);
		if (idx != -1)
		{
			s_PendingJobs.Remove(idx);
		}
	}

	// ------------------------- UTILIDADES -------------------------
	static string GenerateId()
	{
		int t = GetGame().GetTime();
		int r1 = Math.RandomInt(0, 99999);
		int r2 = Math.RandomInt(0, 99999);
		return string.Format("%1_%2_%3", t, r1, r2);
	}

	static void EnsureDirs()
	{
		if (!FileExist(ExorStorageConstants.CONFIG_DIR))
			MakeDirectory(ExorStorageConstants.CONFIG_DIR);
		if (!FileExist(ExorStorageConstants.STORAGE_DIR))
			MakeDirectory(ExorStorageConstants.STORAGE_DIR);
	}
}
