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
	// Junta TODOS los classnames del arbol (item + attachments + cargo, recursivo) en
	// 'out'. Lo usa la limpieza del piso del barril (saber que tipos le pertenecen).
	static void CollectTypes(array<ref ExorVO_ItemData> items, TStringArray dest)
	{
		if (!items)
			return;
		int i;
		for (i = 0; i < items.Count(); i++)
		{
			ExorVO_ItemData d = items.Get(i);
			if (!d)
				continue;
			if (dest.Find(d.type) < 0)
				dest.Insert(d.type);
			CollectTypes(d.attachments, dest);
			CollectTypes(d.cargo, dest);
		}
	}

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

		ApplyScalars(e, data);

		// 1) ATTACHMENTS mientras 'e' esta SUELTO (cargador/mira/ropa solo enganchan suelto).
		int i;
		for (i = 0; i < data.attachments.Count(); i++)
			RestoreItem(data.attachments.Get(i), e, groundPos, root, true);

		// 2) Mover 'e' adentro del parent SIN su cargo todavia. CLAVE anti-dupe: mover un
		// contenedor VACIO no duplica; mover uno LLENO duplica el contenido (el engine hace
		// LocationSyncMoveEntity+SendServerMove y el contenido se replica al piso) -> por eso
		// el cargo se llena DESPUES (paso 3), ya con 'e' en su lugar.
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
				// CARGO: mover el item (ya armado/lleno, suelto) a su CASILLA EXACTA con su
				// ROTACION (flip). TakeEntityToCargoEx no lleva flip -> usamos TakeToDst con un
				// InventoryLocation.SetCargo(parent,e,idx,row,col,FLIP). Asi la mochila vuelve a
				// su bloque Y orientada igual (acostada/parada) que como la guardo el player ->
				// no fragmenta ni choca con vecinos. Si la casilla esta ocupada/invalida o es
				// legacy (sin posicion), cae al primer hueco (ANY).
				if (data.cargo_row >= 0 && data.cargo_col >= 0)
				{
					InventoryLocation src = new InventoryLocation();
					if (e.GetInventory() && e.GetInventory().GetCurrentInventoryLocation(src))
					{
						InventoryLocation dst = new InventoryLocation();
						dst.SetCargo(parent, e, data.cargo_idx, data.cargo_row, data.cargo_col, data.cargo_flip);
						moved = parent.GetInventory().TakeToDst(InventoryMode.SERVER, src, dst);
					}
				}
				if (!moved)
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

		// 3) CARGO: llenar 'e' AHORA en su lugar. Se MUEVEN los items adentro (no se crean):
		// 'e' puede estar nested y crear dentro de un contenedor-en-cargo no anda, pero MOVER
		// si. Recursivo: un bolso-en-bolso tambien se mueve VACIO y se llena en su lugar.
		for (i = 0; i < data.cargo.Count(); i++)
			RestoreItem(data.cargo.Get(i), e, groundPos, root, false);

		return e;
	}

	// Llena un item YA CREADO (e) con sus attachments + cargo anidado, en POSICION EXACTA
	// y recursivo (mochila dentro de mochila vuelve a su casilla). Los attachments van por
	// el camino de RestoreItem (cargador->arma con SpawnAttachedMagazine, etc.).
	static void FillItem(EntityAI e, ExorVO_ItemData data, vector groundPos, EntityAI root)
	{
		int i;
		for (i = 0; i < data.attachments.Count(); i++)
			RestoreItem(data.attachments.Get(i), e, groundPos, root, true);

		for (i = 0; i < data.cargo.Count(); i++)
		{
			ExorVO_ItemData cd = data.cargo.Get(i);
			// 'e' esta SUELTO (recien creado, todavia no movido a su parent), asi que SI se
			// puede crear adentro. POSICION EXACTA solo para items realmente sueltos (sin cargo
			// ni attachments). Un bolso-en-bolso o un arma se arma aparte (RestoreItem) y se
			// mueve LLENO adentro de 'e' -> porque crear DENTRO de un contenedor que ya esta en
			// cargo no funciona, y los attachments no enganchan estando en cargo.
			if (cd.attachments.Count() == 0 && cd.cargo.Count() == 0 && cd.cargo_row >= 0 && cd.cargo_col >= 0 && e.GetInventory())
			{
				EntityAI cex = e.GetInventory().CreateEntityInCargoEx(cd.type, cd.cargo_idx, cd.cargo_row, cd.cargo_col, cd.cargo_flip);
				if (cex)
				{
					ApplyScalars(cex, cd);
					continue;
				}
			}
			// bolso/arma anidado, sin posicion (legacy) o fallo -> armar suelto + mover lleno
			RestoreItem(cd, e, groundPos, root, false);
		}
	}

	// Restaura una lista de items de nivel TOP en un contenedor (barril/bodybag). Cada cosa
	// vuelve a su CASILLA EXACTA (no hay fragmentacion):
	//  - item suelto (sin cargo ni attachments) -> CreateEntityInCargoEx directo en su casilla.
	//  - mochila/arma -> se arma SUELTA y llena (RestoreItem) y se mueve a su casilla exacta
	//    con TakeEntityToCargoEx. Asi una mochila grande vuelve a su bloque, no a "el 1er hueco".
	static void RestoreItemsBigFirst(array<ref ExorVO_ItemData> items, EntityAI container, vector groundPos)
	{
		int i;
		for (i = 0; i < items.Count(); i++)
		{
			ExorVO_ItemData d = items.Get(i);

			// item REALMENTE SUELTO (sin cargo NI attachments) -> directo a su casilla
			if (d.attachments.Count() == 0 && d.cargo.Count() == 0 && d.cargo_row >= 0 && d.cargo_col >= 0 && container.GetInventory())
			{
				EntityAI ex = container.GetInventory().CreateEntityInCargoEx(d.type, d.cargo_idx, d.cargo_row, d.cargo_col, d.cargo_flip);
				if (ex)
				{
					ApplyScalars(ex, d);
					continue;
				}
				// fallo (casilla ocupada/invalida) -> al camino de abajo
			}

			// mochila/arma (o sin posicion / fallo): armar SUELTO y lleno, y moverlo a su
			// casilla exacta (RestoreItem hace el TakeEntityToCargoEx adentro).
			RestoreItem(d, container, groundPos, container);
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

	// ------------------------- RUINED: liberar el contenido -------------------------
	// Una prenda/contenedor DESTRUIDO (ruined, health01==0) no se puede ATAR a un slot
	// ni abrir estando anidado -> al lootear un cuerpo el loot quedaba ATRAPADO dentro
	// de la prenda rota y habia que tirarla al piso para verlo. Para la TUMBA: vaciamos
	// los contenedores ruined y subimos su contenido al nivel superior (= cargo de la
	// tumba), accesible de una. El cascaron ruined queda VACIO (para que se siga viendo
	// que la prenda estaba destruida). Recursivo: tambien vacia ruined anidados.
	static bool IsRuined(ExorVO_ItemData d)
	{
		return d && d.health <= 0.001;
	}

	// Sube a 'top' el contenido (cargo) de cualquier item RUINED del arbol de cada
	// elemento de 'top'. Worklist: lo subido se vuelve a procesar (por si era a su vez
	// un contenedor ruined). El cascaron ruined queda vacio.
	static void HoistRuinedContents(array<ref ExorVO_ItemData> top)
	{
		if (!top)
			return;
		int i = 0;
		while (i < top.Count())
		{
			DrainRuinedTree(top.Get(i), top);
			i++;
		}
	}

	// Para 'd' y sus hijos (cargo + attachments): si estan ruined, mueve su cargo a 'top'.
	static void DrainRuinedTree(ExorVO_ItemData d, array<ref ExorVO_ItemData> top)
	{
		if (!d)
			return;

		// primero recursion en hijos NO-ruined (sus ruined internos tambien se vacian)
		int j;
		if (d.attachments)
			for (j = 0; j < d.attachments.Count(); j++)
				DrainRuinedTree(d.attachments.Get(j), top);
		if (d.cargo)
			for (j = 0; j < d.cargo.Count(); j++)
				DrainRuinedTree(d.cargo.Get(j), top);

		// si 'd' esta ruined y tiene cargo -> sacarlo al nivel superior
		if (IsRuined(d) && d.cargo && d.cargo.Count() > 0)
		{
			int k;
			for (k = 0; k < d.cargo.Count(); k++)
			{
				ExorVO_ItemData c = d.cargo.Get(k);
				if (!c)
					continue;
				// que caiga en el 1er hueco libre de la tumba (la casilla original era
				// de la grilla de la prenda rota y podria chocar)
				c.cargo_row = -1;
				c.cargo_col = -1;
				top.Insert(c);
			}
			d.cargo.Clear();
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
