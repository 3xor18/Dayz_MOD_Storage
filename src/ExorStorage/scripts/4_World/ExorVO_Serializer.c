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
	// Posicion EXACTA en el cargo del padre (para restaurar sin desordenar la grilla).
	// row = -1 -> no habia posicion guardada (item viejo o no estaba en cargo) -> al 1er hueco.
	int cargo_idx = 0;
	int cargo_row = -1;
	int cargo_col = -1;
	bool cargo_flip = false;
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

		// guardar la posicion del item en el cargo de su padre (si esta en un cargo),
		// para restaurarlo en el MISMO lugar y no desordenar la grilla
		InventoryLocation loc = new InventoryLocation();
		if (e.GetInventory() && e.GetInventory().GetCurrentInventoryLocation(loc) && loc.GetType() == InventoryLocationType.CARGO)
		{
			data.cargo_idx = loc.GetIdx();
			data.cargo_row = loc.GetRow();
			data.cargo_col = loc.GetCol();
			data.cargo_flip = loc.GetFlip();
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
	// CLAVE: un contenedor que YA esta dentro de otro contenedor (mochila en
	// barril) NO acepta que le creen items adentro. Por eso cada item se ARMA
	// COMPLETO en el piso (objeto del mundo = acepta cargo) y recien lleno se
	// MUEVE entero adentro del parent (mover un contenedor lleno si esta
	// permitido). Asi funciona el anidado arbitrario (mochila con cosas, etc).
	// 'root' = el contenedor de mas arriba (barril / bodybag) al que mandar el
	// sobrante que no entre en su parent ANIDADO, en vez de tirarlo al piso (asi el
	// loot no se pierde). En la llamada de nivel superior se pasa null -> el parent
	// ES la raiz; en la recursion se propaga hacia abajo.
	// asAttachment = true si este item iba ATADO al parent (cargador en arma, mira,
	// ropa en slot) en vez de en su CARGO. Los attachments se enganchan distinto
	// (TakeEntityAsAttachment) que los items de cargo (posicion exacta / hueco).
	static EntityAI RestoreItem(ExorVO_ItemData data, EntityAI parent, vector groundPos, EntityAI root = null, bool asAttachment = false)
	{
		if (!root)
			root = parent;

		// ECE_KEEPHEIGHT (no traza a la superficie): los callers pasan una posicion BAJO
		// TIERRA, asi el item se arma oculto y se mueve al contenedor sin que el player lo
		// vea aparecer en el piso (evita el parpadeo y cualquier duda de dupeo). El item se
		// arma+mueve en el MISMO frame del server, asi que nunca queda looteable.
		EntityAI e = EntityAI.Cast(GetGame().CreateObjectEx(data.type, groundPos, ECE_KEEPHEIGHT));
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

		// Llenar 'e' MIENTRAS esta en el piso (objeto del mundo, acepta cargo).
		int i;
		for (i = 0; i < data.attachments.Count(); i++)
		{
			RestoreItem(data.attachments.Get(i), e, groundPos, root, true);	// ATTACHMENT (cargador/mira/ropa)
		}
		// CARGO anidado (lo que esta DENTRO de 'e', ej. una mochila): los items SIMPLES
		// van a su CASILLA EXACTA del cargo de 'e' (CreateEntityInCargoEx), igual que en el
		// barril -> vuelven a su slot tal cual estaban. Los complejos (con hijos) se arman y
		// se mueven con la red de seguridad (root).
		for (i = 0; i < data.cargo.Count(); i++)
		{
			ExorVO_ItemData cd = data.cargo.Get(i);
			bool csimple = (cd.attachments.Count() == 0 && cd.cargo.Count() == 0);
			if (csimple && cd.cargo_row >= 0 && cd.cargo_col >= 0 && e.GetInventory())
			{
				EntityAI cex = e.GetInventory().CreateEntityInCargoEx(cd.type, cd.cargo_idx, cd.cargo_row, cd.cargo_col, cd.cargo_flip);
				if (cex)
				{
					ApplyScalars(cex, cd);
					continue;
				}
			}
			RestoreItem(cd, e, groundPos, root, false);	// complejo o fallo -> camino normal
		}

		// Mover 'e' (ya lleno) adentro del parent.
		if (parent)
		{
			bool moved = false;

			if (asAttachment)
			{
				// CARGADOR -> ARMA: en DayZ los mags NO se enganchan con
				// TakeEntityAsAttachment ni ANY (por eso caian sueltos al barril y lo
				// rebalsaban). La forma correcta es SpawnAttachedMagazine en el arma (lo
				// que usan todos los loadouts vanilla). Creamos el mag DENTRO del arma y
				// borramos el suelto que se habia armado en el piso.
				Weapon_Base wpn = Weapon_Base.Cast(parent);
				Magazine emag = Magazine.Cast(e);
				if (wpn && emag)
				{
					Magazine nm = wpn.SpawnAttachedMagazine(data.type);
					if (nm)
					{
						nm.SetHealth01("", "", data.health);
						if (data.ammoCount >= 0)
							nm.ServerSetAmmoCount(data.ammoCount);
						GetGame().ObjectDelete(e);
						moved = true;
					}
				}

				// resto de attachments (miras, supresores, ropa en slot) -> attachment normal
				if (!moved)
					moved = parent.GetInventory().TakeEntityAsAttachment(InventoryMode.SERVER, e);
			}
			else
			{
				// CARGO: al primer hueco. (Se probo la POSICION EXACTA con
				// InventoryLocation + LocationAddEntity, pero NO mueve de forma confiable
				// una entidad ya creada en el piso -> dejaba items en el SUELO aunque
				// dijera que si. TakeEntityToInventory SI los mueve; el orden se mantiene
				// razonable porque se restauran en el MISMO orden en que se guardaron.
				// ANY -no solo CARGO- para que el bodybag meta la ropa del muerto en sus
				// slots de prenda; un item suelto del barril igual termina en cargo.)
				moved = parent.GetInventory().TakeEntityToInventory(InventoryMode.SERVER, FindInventoryLocationType.ANY, e);
			}

			// 3) RED DE SEGURIDAD: no entro en su parent anidado (cargador->arma, arma->
			// chaleco) -> en vez del PISO, mandarlo al contenedor RAIZ (barril/bodybag).
			if (!moved && root && root != parent && root.GetInventory())
			{
				moved = root.GetInventory().TakeEntityToInventory(InventoryMode.SERVER, FindInventoryLocationType.ANY, e);
				if (moved)
					Print(string.Format("%1 '%2' no entro en '%3' -> reubicado en la raiz '%4'", ExorStorageConstants.LOG, data.type, parent.GetType(), root.GetType()));
			}

			if (!moved)
			{
				// muy raro: ni en el parent ni en la raiz. Subirlo a la SUPERFICIE de la
				// raiz para que sea visible/recuperable (no perderlo bajo tierra).
				if (root)
					e.SetPosition(root.GetPosition());
				Print(string.Format("%1 AVISO: '%2' no entro en '%3' ni en la raiz (lleno) -> queda en el piso", ExorStorageConstants.LOG, data.type, parent.GetType()));
			}
		}

		return e;
	}

	// Restaura una lista de items de nivel TOP en un contenedor (barril/bodybag)
	// GRANDES PRIMERO: arma cada item en el piso (sin meterlo al contenedor todavia),
	// los ordena por footprint (ancho*alto en la grilla) descendente y recien ahi los
	// mete. Asi los items grandes (rifles) agarran su bloque CONTIGUO en la grilla
	// vacia y los chicos rellenan los huecos -> entra todo en el MISMO espacio que uso
	// el player, sin que un item grande quede afuera por fragmentacion.
	static void RestoreItemsBigFirst(array<ref ExorVO_ItemData> items, EntityAI container, vector groundPos)
	{
		array<EntityAI> complex = new array<EntityAI>;	// contenedores/con hijos -> best-effort
		int i;
		for (i = 0; i < items.Count(); i++)
		{
			ExorVO_ItemData d = items.Get(i);

			// SIMPLE (sin attachments ni cargo) con posicion guardada -> crear en su CASILLA
			// EXACTA del barril (CreateEntityInCargoEx respeta row/col/flip). Asi logs/ruedas/
			// latas/cargadores-sueltos vuelven a su lugar y no se rebalsa por fragmentacion.
			bool simple = (d.attachments.Count() == 0 && d.cargo.Count() == 0);
			if (simple && d.cargo_row >= 0 && d.cargo_col >= 0)
			{
				EntityAI ex = container.GetInventory().CreateEntityInCargoEx(d.type, d.cargo_idx, d.cargo_row, d.cargo_col, d.cargo_flip);
				if (ex)
				{
					ApplyScalars(ex, d);
					continue;
				}
				// fallo (casilla ocupada/invalida) -> al camino normal de abajo
			}

			// contenedor / con hijos / fallo del exacto: armar BAJO TIERRA, mover despues
			EntityAI ge = RestoreItem(d, null, groundPos, container);
			if (ge)
				complex.Insert(ge);
		}

		// ordenar los complejos por footprint DESC (grandes primero) y meterlos best-effort
		int n = complex.Count();
		int a, b;
		for (a = 0; a < n - 1; a++)
		{
			for (b = 0; b < n - 1 - a; b++)
			{
				if (ItemFootprint(complex.Get(b + 1)) > ItemFootprint(complex.Get(b)))
				{
					EntityAI tmp = complex.Get(b);
					complex.Set(b, complex.Get(b + 1));
					complex.Set(b + 1, tmp);
				}
			}
		}

		for (i = 0; i < complex.Count(); i++)
		{
			EntityAI it = complex.Get(i);
			if (!container.GetInventory().TakeEntityToInventory(InventoryMode.SERVER, FindInventoryLocationType.ANY, it))
			{
				// caso raro: subirlo a la SUPERFICIE del contenedor (visible/recuperable)
				it.SetPosition(container.GetPosition());
				Print(string.Format("%1 AVISO: '%2' no entro en '%3' (lleno) -> queda en el piso", ExorStorageConstants.LOG, it.GetType(), container.GetType()));
			}
		}
	}

	// aplica los datos escalares (vida, cantidad, balas, etapa de comida) a un item ya creado
	static void ApplyScalars(EntityAI e, ExorVO_ItemData d)
	{
		e.SetHealth01("", "", d.health);
		ItemBase ib = ItemBase.Cast(e);
		if (ib && d.quantity >= 0)
			ib.SetQuantity(d.quantity);
		Magazine mag = Magazine.Cast(e);
		if (mag && d.ammoCount >= 0)
			mag.ServerSetAmmoCount(d.ammoCount);
		if (d.foodStage > 0)
		{
			Edible_Base ed = Edible_Base.Cast(e);
			if (ed)
				ed.ChangeFoodStage(d.foodStage);
		}
	}

	// footprint (ancho*alto en celdas) del item, para ordenar grandes-primero
	static int ItemFootprint(EntityAI e)
	{
		InventoryItem ii = InventoryItem.Cast(e);
		if (!ii)
			return 1;
		int w = 1;
		int h = 1;
		GetGame().GetInventoryItemSize(ii, w, h);
		return w * h;
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
