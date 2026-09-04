// ============================================================================
// 3xor_Vanilla_Optimization - OPERACIONES COMPARTIDAS DE CONTENEDOR (SOLO server)
// ============================================================================
// POR QUE EXISTE ESTE ARCHIVO
// ----------------------------------------------------------------------------
// El barril (Exor_Barrel_Base) y los muebles (Exor_OpenableStorage) hacen lo MISMO
// -virtualizar contenido a un JSON y restaurarlo- pero heredan de clases vanilla
// distintas (Barrel_ColorBase vs Container_Base) y Enforce no tiene herencia multiple
// ni interfaces. Resultado historico: el motor entero quedo escrito DOS VECES, en dos
// archivos de mas de 1000 lineas, con 25 metodos duplicados.
//
// Eso no es un problema estetico. Es la causa directa de bugs reales:
//   - la optimizacion de "no reescribir el JSON si no cambio" se agrego a los muebles
//     y NO a los barriles -> 657 barriles siguieron reescribiendo su archivo entero
//     cada 15 s durante toda una sesion, sin que nadie se enterara;
//   - el arreglo del restore por lotes y el guard de carga segura hubo que aplicarlos
//     dos veces, a mano, el mismo dia.
// Cada arreglo futuro se paga dos veces, o queda a medias.
//
// LO QUE SE HACE ACA
// ----------------------------------------------------------------------------
// Se saca a funciones ESTATICAS todo lo que no depende del estado interno de la
// entidad: parseo del JSON, barrido del piso, conteo de cargo, armado del snapshot.
// El estado (m_ExorVirt, m_ExorSnapDirty, el job de restore...) se queda en cada
// entidad, que pasa a ser orquestacion fina sobre estas operaciones.
//
// Se eligio esto y no una clase-motor con el estado adentro por una razon concreta:
// mover el estado son ~190 reescrituras a mano en el codigo que maneja el loot de
// todos los jugadores, y el beneficio es de mantenibilidad, no de runtime. Esto saca
// el grueso de las lineas duplicadas y cada extraccion es un movimiento mecanico que
// el compilador verifica. El paso siguiente -unificar tambien el estado- queda para
// una sesion con test de perdida de loot in-game.
// ============================================================================
class ExorContainerOps
{
	// ------------------------------------------------------------------------
	//  RUTA DEL JSON DE CONTENIDO
	// ------------------------------------------------------------------------
	static string StoragePath(string id)
	{
		return string.Format("%1\\%2.json", ExorStorageConstants.STORAGE_DIR, id);
	}

	// ------------------------------------------------------------------------
	//  CONTEO DE CARGO
	// ------------------------------------------------------------------------
	static int CargoCount(EntityAI e)
	{
		if (!e)
			return 0;
		GameInventory inv = e.GetInventory();
		if (!inv)
			return 0;
		CargoBase cargo = inv.GetCargo();
		if (!cargo)
			return 0;
		return cargo.GetItemCount();
	}

	// ------------------------------------------------------------------------
	//  TIPOS GUARDADOS, LEIDOS DEL TEXTO CRUDO DEL JSON
	// ------------------------------------------------------------------------
	// Se parsea el TEXTO y no el objeto deserializado a proposito: deserializar el
	// archivo entero para sacar una lista de classnames es caro y, ademas, la
	// deserializacion solo traia el primer nivel.
	// El dedup va con un map y no con result.Find(): con cientos de items el Find
	// lineal convierte esto en O(n^2).
	static TStringArray TiposDelJson(string path)
	{
		TStringArray result = new TStringArray;
		map<string, bool> seen = new map<string, bool>;
		FileHandle fh = OpenFile(path, FileMode.READ);
		if (fh == 0)
			return result;
		string line;
		while (FGets(fh, line) >= 0)
		{
			int kp = line.IndexOf("\"type\"");
			if (kp < 0)
				continue;
			string rest = line.Substring(kp + 6, line.Length() - (kp + 6));		// despues de "type"
			int q1 = rest.IndexOf("\"");
			if (q1 < 0)
				continue;
			string rest2 = rest.Substring(q1 + 1, rest.Length() - (q1 + 1));	// despues de la 1ra comilla
			int q2 = rest2.IndexOf("\"");
			if (q2 < 0)
				continue;
			string val = rest2.Substring(0, q2);
			if (val != "" && !seen.Contains(val))
			{
				seen.Set(val, true);
				result.Insert(val);
			}
		}
		CloseFile(fh);
		return result;
	}

	// ------------------------------------------------------------------------
	//  BARRIDO DE PISO (drops de "invalid location" del arranque)
	// ------------------------------------------------------------------------
	// Al cargar el mundo, DayZ tira al piso el contenido ANIDADO de un contenedor
	// (una mochila con cosas adentro de un barril). Al restaurar del JSON esos items
	// se recrearian y quedarian DUPLICADOS. Esto borra los que estan sueltos cerca y
	// son de un tipo que ESTE contenedor guarda.
	//
	// Conservador a proposito: solo items SIN padre (sueltos de verdad), de un tipo
	// que figura en el JSON, y cerca en HORIZONTAL -la altura se ignora porque el
	// contenedor puede estar en un piso elevado y el drop cae al terreno de abajo-.
	// Distancia AL CUADRADO: es un barrido sobre todos los objetos del radio.
	static int LimpiarDropsCerca(EntityAI duenio, string jsonPath, float scanRadius, float maxHoriz)
	{
		if (!duenio || !FileExist(jsonPath))
			return 0;
		TStringArray types = TiposDelJson(jsonPath);
		if (types.Count() == 0)
			return 0;

		vector bpos = duenio.GetPosition();
		array<Object> nearby = new array<Object>;
		array<CargoBase> proxy = new array<CargoBase>;
		GetGame().GetObjectsAtPosition(bpos, scanRadius, nearby, proxy);

		float maxSq = maxHoriz * maxHoriz;
		int removed = 0;
		int i;
		for (i = 0; i < nearby.Count(); i++)
		{
			EntityAI e = EntityAI.Cast(nearby.Get(i));
			if (!e || e == duenio)
				continue;
			if (e.GetHierarchyParent())		// solo items SUELTOS (no dentro de un inventario)
				continue;
			if (!e.IsInherited(ItemBase))
				continue;
			if (types.Find(e.GetType()) < 0)	// solo tipos que este contenedor guarda
				continue;
			if (ExorMath.Dist2DSq(e.GetPosition(), bpos) > maxSq)
				continue;
			GetGame().ObjectDelete(e);
			removed++;
		}
		return removed;
	}

	// ------------------------------------------------------------------------
	//  FIRMA DEL CONTENIDO REAL, SIN ARMAR EL DTO
	// ------------------------------------------------------------------------
	// El volcado a JSON solo hace falta si el contenido CAMBIO. Hasta ahora eso se decidia
	// armando el DTO completo y firmandolo despues: para un locker de 500 items son ~1500
	// objetos creados (un ExorVO_ItemData con sus dos arrays por item, recursivo) que en el
	// caso comun -nada cambio- se tiran enteros a la basura. Y el caso comun es la enorme
	// mayoria: un contenedor abierto se re-marca sucio cada SNAP_DEBOUNCE_MS aunque nadie
	// toque nada.
	//
	// Esto recorre el inventario VIVO y calcula la misma firma sin allocar ni un DTO. Si
	// coincide con la del ultimo volcado, no se arma nada y no se escribe nada.
	// Es la UNICA formula de firma que se usa para decidir si hay que escribir: no tiene que
	// coincidir con ninguna otra, solo ser consistente consigo misma entre llamadas. Por eso
	// se saco la version que firmaba el DTO: mantener dos formulas en sincronia era una
	// trampa esperando (si divergen, o se reescribe siempre o no se reescribe nunca).
	static int FirmaViva(EntityAI duenio, bool conAttachments)
	{
		if (!duenio)
			return 0;
		GameInventory inv = duenio.GetInventory();
		if (!inv)
			return 0;
		int hItems = 17;
		int hAtt = 17;
		int i;
		CargoBase cargo = inv.GetCargo();
		if (cargo)
		{
			for (i = 0; i < cargo.GetItemCount(); i++)
				hItems = FirmaDeEntidad(cargo.GetItem(i), hItems, i);
		}
		if (conAttachments)
		{
			for (i = 0; i < inv.AttachmentCount(); i++)
				hAtt = FirmaDeEntidad(inv.GetAttachmentFromIndex(i), hAtt, -1);
		}
		return (hItems * 31) + hAtt;
	}

	// Un item y su arbol, mezclado en 'h'. Mira los mismos campos que captura
	// ExorVO_Serializer.CaptureItem: si un campo se guarda en el JSON, tiene que estar aca,
	// porque si no un cambio de ese campo no dispararia la reescritura.
	static int FirmaDeEntidad(EntityAI e, int h, int idxCargo)
	{
		if (!e)
			return h;
		h = (h * 31) + e.GetType().Hash();
		h = (h * 31) + (int)(e.GetHealth01("", "") * 10000);

		float qty = -1;
		ItemBase ib = ItemBase.Cast(e);
		if (ib && ib.HasQuantity())
			qty = ib.GetQuantity();
		h = (h * 31) + (int)qty;

		int ammo = -1;
		Magazine mag = Magazine.Cast(e);
		if (mag)
		{
			ammo = mag.GetAmmoCount();
			// Tipo de la PRIMERA bala. Sin esto, recargar un cargador con otra municion
			// (perforante -> normal) con la misma cantidad no cambiaria la firma y el JSON
			// se quedaria con la municion vieja: justo el bug que arreglamos hoy.
			// Una sola consulta por cargador, no por cartucho.
			if (ammo > 0)
			{
				float cdmg;
				string ctype;
				if (mag.GetCartridgeAtIndex(0, cdmg, ctype) && ctype != "")
					h = (h * 31) + ctype.Hash();
			}
		}
		h = (h * 31) + ammo;

		int food = -1;
		Edible_Base ed = Edible_Base.Cast(e);
		if (ed && ed.GetFoodStage())
			food = ed.GetFoodStage().GetFoodStageType();
		h = (h * 31) + food;

		// posicion en la grilla del padre (mover un item ES un cambio)
		int cidx = 0;
		int crow = -1;
		int ccol = -1;
		InventoryLocation loc = new InventoryLocation();
		if (e.GetInventory() && e.GetInventory().GetCurrentInventoryLocation(loc) && loc.GetType() == InventoryLocationType.CARGO)
		{
			cidx = loc.GetIdx();
			crow = loc.GetRow();
			ccol = loc.GetCol();
		}
		h = (h * 31) + (cidx * 7919) + (crow * 31) + ccol;

		// recursion: attachments y cargo anidado
		GameInventory inv = e.GetInventory();
		if (!inv)
			return h;
		int hAtt = 17;
		int hCar = 17;
		int i;
		for (i = 0; i < inv.AttachmentCount(); i++)
			hAtt = FirmaDeEntidad(inv.GetAttachmentFromIndex(i), hAtt, -1);
		CargoBase cg = inv.GetCargo();
		if (cg)
		{
			for (i = 0; i < cg.GetItemCount(); i++)
				hCar = FirmaDeEntidad(cg.GetItem(i), hCar, i);
		}
		h = (h * 31) + hAtt;
		h = (h * 31) + hCar;
		return h;
	}

	// ------------------------------------------------------------------------
	//  ARMADO DEL SNAPSHOT (cargo + attachments -> DTO)
	// ------------------------------------------------------------------------
	// Recorre el contenido REAL y arma el objeto que se serializa al JSON. No escribe
	// nada: el llamador decide si hace falta escribirlo (ver la firma de contenido).
	// 'conAttachments' = tambien capturar los attachments del contenedor (el mueble de
	// armas guarda las armas en slots; el barril no tiene nada de eso).
	static ExorVO_ContainerFile ArmarSnapshot(EntityAI duenio, string id, bool conAttachments)
	{
		ExorVO_ContainerFile f = new ExorVO_ContainerFile();
		if (!duenio)
			return f;
		f.id = id;
		f.owner_type = duenio.GetType();
		// SELLO DE TIEMPO: desde que el contenido se virtualiza deja de existir para el
		// motor, asi que lo que dependa del paso del tiempo (la pudricion de la comida en
		// la nevera) hay que cobrarlo al restaurar. Ver Exor_Fridge.ExorOnItemsRestored.
		f.vmin = ExorTimeUtil.NowMinutes();

		GameInventory inv = duenio.GetInventory();
		if (!inv)
			return f;
		CargoBase cargo = inv.GetCargo();
		int i;
		if (cargo)
		{
			for (i = 0; i < cargo.GetItemCount(); i++)
			{
				EntityAI it = cargo.GetItem(i);
				if (it)
					f.items.Insert(ExorVO_Serializer.CaptureItem(it));
			}
		}
		if (conAttachments)
		{
			for (i = 0; i < inv.AttachmentCount(); i++)
			{
				EntityAI att = inv.GetAttachmentFromIndex(i);
				if (att)
					f.att.Insert(ExorVO_Serializer.CaptureItem(att));
			}
		}
		return f;
	}

	// ------------------------------------------------------------------------
	//  FORMATO JSON-LINES DEL ARCHIVO DE CONTENIDO
	// ------------------------------------------------------------------------
	// El archivo pasa de ser UN objeto JSON gigante a: una linea de cabecera + UNA LINEA POR
	// ITEM de nivel superior.
	//
	//   #EXORJL1 {"id":"12","owner_type":"Exor_Locker","vmin":123,"vpow":0,"vbat":0}
	//   I {"type":"Ammo_556x45","health":1.0,...}
	//   A {"type":"M4A1",...}
	//
	// DOS GANANCIAS REALES:
	//
	// 1) AISLAMIENTO DE CORRUPCION. Con un solo objeto gigante, si el archivo se trunca o se
	//    corrompe un byte, JsonLoadFile falla y se pierde el contenedor ENTERO. Con una linea
	//    por item, una linea rota pierde ESE item y el resto se restaura igual. Para un mod
	//    cuyo historial esta lleno de incidentes de corrupcion, esto no es cosmetico.
	//
	// 2) PICO DE MEMORIA. Escribir ya no exige tener el JSON completo del contenedor armado
	//    en memoria: se serializa y se vuelca item por item. Con lockers de 500 slots y un
	//    techo de RAM de ~4 GB, el pico importa.
	//
	// El lector acepta tambien el formato viejo (objeto unico): si la primera linea no trae
	// la marca, cae a JsonFileLoader. Migrar es gratis y sin ventana de riesgo.
	static const string EXOR_JL_MAGIC = "#EXORJL1 ";
	// lineas que se juntan en memoria antes de volcarlas de una (ver GuardarJL)
	static const int EXOR_JL_LOTE = 25;

	// Cabecera: los metadatos del contenedor sin los items.
	static void GuardarJL(string path, ExorVO_ContainerFile f)
	{
		if (!f)
			return;
		FileHandle fh = OpenFile(path, FileMode.WRITE);
		if (fh == 0)
		{
			Print(string.Format("%1 ERROR: no se pudo abrir '%2' para escribir el contenido", ExorStorageConstants.LOG, path));
			return;
		}
		JsonSerializer js = new JsonSerializer();
		string linea;

		ExorVO_ContainerHead head = new ExorVO_ContainerHead();
		head.id = f.id;
		head.owner_type = f.owner_type;
		head.vmin = f.vmin;
		head.vpow = f.vpow;
		head.vbat = f.vbat;
		js.WriteToString(head, false, linea);

		// ESCRITURA POR LOTES.
		// MEDIDO: una operacion de snapshot sobre un contenedor de 300 items costaba ~18 ms, y
		// el tick del mod era el PEOR FRAME del server (36-64 ms sobre una base de ~1 ms).
		// Un FPrintln por item son 300 llamadas de I/O sincrona; antes JsonSaveFile hacia UNA
		// sola escritura nativa, asi que pasar a JSON-Lines habia empeorado la escritura.
		// Se juntan EXOR_JL_LOTE lineas en memoria y se vuelcan de una: el archivo queda
		// exactamente igual (una linea por item, con su aislamiento de corrupcion) pero con
		// 1/N de las llamadas al disco.
		// No se acumula el archivo ENTERO en un solo string a proposito: concatenar 128 KB en
		// Enforce es cuadratico y saldria peor que el problema que arregla.
		string buf = EXOR_JL_MAGIC + linea;
		int enBuf = 1;

		int i;
		if (f.items)
		{
			for (i = 0; i < f.items.Count(); i++)
			{
				if (!f.items.Get(i))
					continue;
				js.WriteToString(f.items.Get(i), false, linea);
				buf = buf + "\n" + "I " + linea;
				enBuf++;
				if (enBuf >= EXOR_JL_LOTE)
				{
					FPrintln(fh, buf);
					buf = "";
					enBuf = 0;
				}
			}
		}
		if (f.att)
		{
			for (i = 0; i < f.att.Count(); i++)
			{
				if (!f.att.Get(i))
					continue;
				js.WriteToString(f.att.Get(i), false, linea);
				if (enBuf == 0)
					buf = "A " + linea;
				else
					buf = buf + "\n" + "A " + linea;
				enBuf++;
				if (enBuf >= EXOR_JL_LOTE)
				{
					FPrintln(fh, buf);
					buf = "";
					enBuf = 0;
				}
			}
		}
		if (enBuf > 0)
			FPrintln(fh, buf);
		CloseFile(fh);
	}

	// Devuelve null si el archivo no existe. Si no tiene la marca, cae al formato viejo.
	static ExorVO_ContainerFile LeerJL(string path)
	{
		if (!FileExist(path))
			return null;
		FileHandle fh = OpenFile(path, FileMode.READ);
		if (fh == 0)
			return null;

		string primera = "";
		if (FGets(fh, primera) < 0)
		{
			CloseFile(fh);
			return null;
		}
		if (primera.IndexOf(EXOR_JL_MAGIC) != 0)
		{
			// formato viejo (un objeto JSON) -> lector de siempre
			CloseFile(fh);
			ExorVO_ContainerFile viejo = new ExorVO_ContainerFile();
			JsonFileLoader<ExorVO_ContainerFile>.JsonLoadFile(path, viejo);
			return viejo;
		}

		ExorVO_ContainerFile f = new ExorVO_ContainerFile();
		JsonSerializer js = new JsonSerializer();
		string err;

		ExorVO_ContainerHead head = new ExorVO_ContainerHead();
		string cab = primera.Substring(EXOR_JL_MAGIC.Length(), primera.Length() - EXOR_JL_MAGIC.Length());
		if (js.ReadFromString(head, cab, err))
		{
			f.id = head.id;
			f.owner_type = head.owner_type;
			f.vmin = head.vmin;
			f.vpow = head.vpow;
			f.vbat = head.vbat;
		}

		int rotas = 0;
		string linea;
		while (FGets(fh, linea) >= 0)
		{
			if (linea.Length() < 3)
				continue;
			string tipo = linea.Substring(0, 1);
			string cuerpo = linea.Substring(2, linea.Length() - 2);
			ExorVO_ItemData d = new ExorVO_ItemData();
			if (!js.ReadFromString(d, cuerpo, err))
			{
				// UNA linea rota se descarta y se sigue. Antes esto perdia el contenedor entero.
				rotas++;
				continue;
			}
			if (tipo == "A")
				f.att.Insert(d);
			else
				f.items.Insert(d);
		}
		CloseFile(fh);
		if (rotas > 0)
			Print(string.Format("%1 AVISO: '%2' tenia %3 linea(s) ilegibles -> se perdieron esos items, el resto se restaura", ExorStorageConstants.LOG, path, rotas));
		return f;
	}

	// ------------------------------------------------------------------------
	//  LECTURA + SANEO DEL JSON ANTES DE RESTAURAR
	// ------------------------------------------------------------------------
	// Poda lo imposible de restaurar (classname fantasma, cargador que no calza en su
	// arma). Sin esto se creaban entidades a medio armar que se persistian y despues
	// tumbaban el arranque del server. Devuelve null si el archivo no existe.
	static ExorVO_ContainerFile LeerYSanear(string jsonPath, string idParaLog)
	{
		ExorVO_ContainerFile f = LeerJL(jsonPath);
		if (!f)
			return null;
		int podados = ExorVO_Serializer.Sanitize(f.att, "", true) + ExorVO_Serializer.Sanitize(f.items, "", false);
		if (podados > 0)
			Print(string.Format("%1 GUARD: contenedor %2 -> %3 item(s) corruptos descartados antes de restaurar", ExorStorageConstants.LOG, idParaLog, podados));
		return f;
	}

	// ------------------------------------------------------------------------
	//  ITEMS REALES A BORRAR AL VIRTUALIZAR
	// ------------------------------------------------------------------------
	// Junta las entidades que hay que sacar del mundo. Se junta ANTES de borrar y no
	// se borra mientras se recorre: mutar el cargo durante su propia iteracion es como
	// se saltean items.
	static array<EntityAI> ContenidoReal(EntityAI duenio, bool conAttachments)
	{
		// OJO: 'out' es palabra reservada en Enforce (modificador de parametro), no se puede
		// usar como nombre de variable.
		array<EntityAI> res = new array<EntityAI>;
		if (!duenio)
			return res;
		GameInventory inv = duenio.GetInventory();
		if (!inv)
			return res;
		CargoBase cargo = inv.GetCargo();
		int i;
		if (cargo)
		{
			for (i = 0; i < cargo.GetItemCount(); i++)
			{
				EntityAI it = cargo.GetItem(i);
				if (it)
					res.Insert(it);
			}
		}
		if (conAttachments)
		{
			for (i = 0; i < inv.AttachmentCount(); i++)
			{
				EntityAI att = inv.GetAttachmentFromIndex(i);
				if (att)
					res.Insert(att);
			}
		}
		return res;
	}
}
