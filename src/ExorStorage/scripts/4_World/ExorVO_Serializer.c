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
	// MUNICION REAL (que BALA, no solo cuantas). 'ammoCount' arriba solo guarda la CANTIDAD, y
	// restaurarla con ServerSetAmmoCount rellena el cargador con la bala POR DEFECTO del
	// classname -> las perforantes/trazadoras volvian convertidas en balas normales. Ademas
	// las armas de cargador INTERNO (escopetas, Mosin) no guardan su municion en ningun
	// Magazine: vive dentro del arma, asi que volvian VACIAS.
	// Formato compacto, una entrada por tramo (ver ExorAmmoEnc):
	//   "M|<cantidad>|<daño x10000>|<classname>"  cargador/pila: tramo de balas iguales, en orden
	//   "I|<boca>|<daño x10000>|<classname>"      una bala del cargador INTERNO del arma
	//   "C|<boca>|<daño x10000>|<classname>"      la bala de la RECAMARA
	// Vacio en la inmensa mayoria de los items -> no engorda el JSON.
	ref TStringArray ammo;
	ref array<ref ExorVO_ItemData> attachments;
	ref array<ref ExorVO_ItemData> cargo;

	void ExorVO_ItemData()
	{
		attachments = new array<ref ExorVO_ItemData>;
		cargo = new array<ref ExorVO_ItemData>;
		// 'ammo' NO se crea aca a proposito: queda null salvo que el item de verdad tenga
		// municion que guardar (un cargador o un arma). Es la inmensa mayoria de los items,
		// y un array vacio igual se serializa ("ammo":[]) -> con 500 items por locker eran
		// varios KB por archivo, en archivos que se reescriben todo el tiempo. Ver AmmoList().
	}

	// Devuelve la lista creandola la primera vez (lazy). Solo la llama la captura cuando
	// realmente encontro cartuchos.
	TStringArray AmmoList()
	{
		if (!ammo)
			ammo = new TStringArray;
		return ammo;
	}
}

// Archivo de un barril virtualizado
class ExorVO_ContainerFile
{
	int version = 1;
	string id;
	string owner_type;
	// Momento en que se escribio este archivo, en minutos absolutos de reloj real
	// (ExorTimeUtil.NowMinutes). 0 = archivo viejo, sin sello.
	// Lo usa la NEVERA para envejecer la comida por el tiempo que estuvo fuera del mundo:
	// virtualizado un item no existe, asi que el motor no le puede correr la pudricion.
	int vmin;
	// 1 = el contenedor tenia ENERGIA cuando se virtualizo (nevera con bateria) -> su
	// contenido NO envejece mientras le dure esa carga. 0 = sin energia.
	int vpow;
	// MINUTOS de carga que le quedaban a la bateria al guardar. Con esto el envejecimiento
	// se resuelve SIN TICKEAR: al restaurar se sabe cuanto del tiempo transcurrido estuvo
	// realmente refrigerado (los primeros vbat minutos) y cuanto no (el resto).
	int vbat;
	ref array<ref ExorVO_ItemData> items;
	ref array<ref ExorVO_ItemData> att;	// attachments virtualizados (ej armas en slots del mueble)

	void ExorVO_ContainerFile()
	{
		items = new array<ref ExorVO_ItemData>;
		att = new array<ref ExorVO_ItemData>;
	}
}

// Cabecera del archivo de contenido en formato JSON-Lines: los metadatos del contenedor,
// sin los items (que van uno por linea). Ver ExorContainerOps.GuardarJL.
class ExorVO_ContainerHead
{
	int version = 1;
	string id;
	string owner_type;
	int vmin;
	int vpow;
	int vbat;
}

class ExorVO_Serializer
{
	// ------------------------- RESTORE INCOMPLETO -------------------------
	// Contador de items que NO se pudieron meter en el contenedor al restaurar (ni en su
	// parent anidado ni en la raiz: quedaron sueltos en el piso). Lo usa el contenedor para
	// decidir si puede re-capturar su contenido al virtualizar: si el ultimo restore quedo
	// incompleto, RE-capturar achicaria el JSON y ese loot se perderia de verdad -> en ese
	// caso se conserva el JSON viejo (que sigue teniendo todo) y se reintenta la proxima vez.
	static int s_FallosUbicacion;

	static void ResetFallosUbicacion() { s_FallosUbicacion = 0; }
	static int  FallosUbicacion()      { return s_FallosUbicacion; }

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

		// tipo REAL de cada bala (cargador/pila) + recamara y cargador interno del arma
		CaptureAmmo(e, data);

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

	// ------------------------- MUNICION REAL (tipo de bala) -------------------------
	// PROBLEMA REPORTADO: un arma guardada en un locker con balas PERFORANTES volvia con
	// balas normales, y las escopetas / Mosin volvian VACIAS.
	// Dos causas distintas:
	//   1) CARGADORES: se guardaba solo GetAmmoCount() y se restauraba con
	//      ServerSetAmmoCount(n), que rellena el cargador con la bala POR DEFECTO del
	//      classname. En DayZ cada cartucho de un Magazine tiene su PROPIO tipo y daño
	//      (GetCartridgeAtIndex) -> hay que guardarlos uno por uno.
	//   2) ARMAS DE CARGADOR INTERNO (escopeta, Mosin, Blaze...): su municion NO vive en
	//      ningun Magazine, vive DENTRO del arma (cargador interno + recamara). Como el
	//      serializador solo miraba attachments y cargo, esa municion no se guardaba nunca.
	// Se guarda con RLE (un tramo por corrida de balas iguales): un cargador de 30 tiros
	// del mismo tipo son 1 sola entrada, no 30.

	// codifica una entrada. dmg va como entero (x10000) para no depender de como
	// formatea floats el compilador ni perder precision en el JSON.
	static string ExorAmmoEnc(string kind, int a, float dmg, string type)
	{
		int d = (int)Math.Round(dmg * 10000);
		if (d < 0)
			d = 0;
		return string.Format("%1|%2|%3|%4", kind, a, d, type);
	}

	// decodifica una entrada. false = entrada ilegible -> se ignora (nunca revienta nada).
	static bool ExorAmmoDec(string s, out string kind, out int a, out float dmg, out string type)
	{
		array<string> p = new array<string>;
		s.Split("|", p);
		if (p.Count() != 4)
			return false;
		kind = p.Get(0);
		a = p.Get(1).ToInt();
		dmg = p.Get(2).ToInt() / 10000.0;
		type = p.Get(3);
		if (type == "")
			return false;
		return true;
	}

	static void CaptureAmmo(EntityAI e, ExorVO_ItemData data)
	{
		int i;
		float d;
		string t;

		// --- CARGADOR ---
		// Las PILAS SUELTAS de balas (Ammunition_Base) quedan afuera A PROPOSITO: su classname
		// YA dice que bala es ("Ammo_762x54Tracer" son trazadoras y punto), asi que el
		// ServerSetAmmoCount de siempre las restaura bien. Recorrerlas cartucho por cartucho
		// seria el grueso del costo (un barril con 500 items y pilas de 100 balas = decenas de
		// miles de llamadas por snapshot) sin ganar nada. El problema son los CARGADORES, que
		// aceptan mezcla y cuyo classname no dice nada de la municion.
		Magazine mag = Magazine.Cast(e);
		if (mag && e.IsInherited(Ammunition_Base))
			return;
		if (mag)
		{
			int n = mag.GetAmmoCount();
			string curT = "";
			float curD = 0;
			int run = 0;
			for (i = 0; i < n; i++)
			{
				if (!mag.GetCartridgeAtIndex(i, d, t) || t == "")
					continue;
				if (run > 0 && t == curT && Math.AbsFloat(d - curD) < 0.0001)
				{
					run++;
					continue;
				}
				if (run > 0)
					data.AmmoList().Insert(ExorAmmoEnc("M", run, curD, curT));
				curT = t;
				curD = d;
				run = 1;
			}
			if (run > 0)
				data.AmmoList().Insert(ExorAmmoEnc("M", run, curD, curT));
			return;		// un cargador no es un arma: no hay recamara que mirar
		}

		// --- ARMA: recamara + cargador interno, por boca ---
		Weapon_Base w = Weapon_Base.Cast(e);
		if (!w)
			return;
		int muzzles = w.GetMuzzleCount();
		int m;
		int c;
		for (m = 0; m < muzzles; m++)
		{
			if (!w.IsChamberEmpty(m) && w.GetCartridgeInfo(m, d, t) && t != "")
				data.AmmoList().Insert(ExorAmmoEnc("C", m, d, t));
			int ic = w.GetInternalMagazineCartridgeCount(m);
			for (c = 0; c < ic; c++)
			{
				if (w.GetInternalMagazineCartridgeInfo(m, c, d, t) && t != "")
					data.AmmoList().Insert(ExorAmmoEnc("I", m, d, t));
			}
		}
	}

	// Restaura la municion guardada. Devuelve true si escribio cartuchos en un CARGADOR
	// (asi el llamador sabe que NO tiene que usar el ServerSetAmmoCount legacy y pisarlos).
	static bool ApplyAmmo(EntityAI e, ExorVO_ItemData d)
	{
		if (!d || !d.ammo || d.ammo.Count() == 0)
			return false;

		string kind;
		int a;
		float dmg;
		string type;
		int i;
		int k;

		// --- CARGADOR / PILA ---
		Magazine mag = Magazine.Cast(e);
		if (mag)
		{
			// contar primero: si el JSON es viejo (sin tramos "M") no se toca nada y el
			// llamador sigue con el camino legacy.
			bool hayM = false;
			for (i = 0; i < d.ammo.Count(); i++)
			{
				if (ExorAmmoDec(d.ammo.Get(i), kind, a, dmg, type) && kind == "M")
				{
					hayM = true;
					break;
				}
			}
			if (!hayM)
				return false;

			mag.ServerSetAmmoCount(0);	// vaciar lo que traiga de fabrica
			for (i = 0; i < d.ammo.Count(); i++)
			{
				if (!ExorAmmoDec(d.ammo.Get(i), kind, a, dmg, type) || kind != "M")
					continue;
				if (a < 0)
					continue;
				for (k = 0; k < a; k++)
				{
					if (!mag.ServerStoreCartridge(dmg, type))
						break;	// lleno o tipo que no calza -> cortar ese tramo, no insistir
				}
			}
			return true;
		}

		// --- ARMA: vaciar lo de fabrica y reponer lo guardado ---
		Weapon_Base w = Weapon_Base.Cast(e);
		if (!w)
			return false;

		int muzzles = w.GetMuzzleCount();
		int m;
		float fd;
		string ft;
		// Un arma recien creada puede nacer con municion (varias armas -sobre todo de mods-
		// traen la recamara/cargador interno cargados por defecto). Sin vaciar, lo guardado
		// se SUMARIA a lo de fabrica = municion duplicada.
		int guard;
		for (m = 0; m < muzzles; m++)
		{
			guard = 0;
			while (w.GetInternalMagazineCartridgeCount(m) > 0 && guard < 100)
			{
				if (!w.PopCartridgeFromInternalMagazine(m, fd, ft))
					break;
				guard++;
			}
			if (!w.IsChamberEmpty(m))
				w.PopCartridgeFromChamber(m, fd, ft);
		}

		bool toco = false;
		// PRIMERO el cargador interno y DESPUES la recamara: es el orden con el que llena
		// vanilla (Weapon_Base.FillInnerMagazine) y el unico que deja el arma consistente.
		for (i = 0; i < d.ammo.Count(); i++)
		{
			if (!ExorAmmoDec(d.ammo.Get(i), kind, a, dmg, type) || kind != "I")
				continue;
			if (a < 0 || a >= muzzles)
				continue;
			if (w.PushCartridgeToInternalMagazine(a, dmg, type))
				toco = true;
		}
		for (i = 0; i < d.ammo.Count(); i++)
		{
			if (!ExorAmmoDec(d.ammo.Get(i), kind, a, dmg, type) || kind != "C")
				continue;
			if (a < 0 || a >= muzzles)
				continue;
			if (w.IsChamberFull(a))
				continue;
			if (w.PushCartridgeToChamber(a, dmg, type))
				toco = true;
		}
		if (toco)
		{
			// La maquina de estados del arma tiene que enterarse de que ahora hay bala
			// (si no, el arma se ve/comporta como descargada hasta que la manipulen).
			w.RandomizeFSMState();
			w.Synchronize();
		}
		return false;	// no es un cargador: el ammoCount legacy no aplica
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

	// ------------------------- GUARDS ANTI DATA CORRUPTA -------------------------
	// PROBLEMA REAL (22-jul): al restaurar una tumba, un cargador que NO calza en el arma
	// hizo `SpawnAttachedMagazine` -> "Failed to create and attach null to P1" (ERROR de VM
	// de vanilla). El arma quedaba a MEDIO ARMAR y ese estado se guardaba en la persistencia
	// -> el siguiente arranque moria cargando ese dynamic_XXX.bin (crash NATIVO, sin log).
	// REGLA: lo que no se puede restaurar SE DESCARTA. Se pierde ese item, nunca el server.

	// el classname existe en los configs? (data vieja de un mod que ya no esta, o basura)
	static bool ExorTypeExiste(string type)
	{
		if (type == "")
			return false;
		if (GetGame().ConfigIsExisting("CfgVehicles " + type))
			return true;
		if (GetGame().ConfigIsExisting("CfgWeapons " + type))
			return true;
		if (GetGame().ConfigIsExisting("CfgMagazines " + type))
			return true;
		return false;
	}

	// el cargador 'magType' esta declarado en el arma 'wpnType'? (magazines[] del config).
	// Hay que chequearlo ANTES de SpawnAttachedMagazine: llamarlo con un mag que no calza
	// tira el ERROR de VM y deja el arma a medias. Sin magazines[] (arma de recamara, o
	// arma de mod raro) -> false: cae al camino de attachment normal, que es inofensivo.
	static bool ExorMagCalza(string wpnType, string magType)
	{
		if (wpnType == "" || magType == "")
			return false;
		TStringArray mags = new TStringArray;
		GetGame().ConfigGetTextArray("CfgWeapons " + wpnType + " magazines", mags);
		if (mags.Count() == 0)
			return false;
		string mt = magType;
		mt.ToLower();
		int i;
		for (i = 0; i < mags.Count(); i++)
		{
			string m = mags.Get(i);
			m.ToLower();
			if (m == mt)
				return true;
		}
		return false;
	}

	// el hijo 'magType' es un CARGADOR que NO calza en el arma 'wpnType'? (= hay que
	// descartarlo). Falso si el padre no es un arma o el hijo no es un cargador.
	static bool ExorEsMagIncompatible(string wpnType, string magType)
	{
		if (wpnType == "")
			return false;
		if (!GetGame().ConfigIsExisting("CfgWeapons " + wpnType))
			return false;
		if (!GetGame().ConfigIsExisting("CfgMagazines " + magType))
			return false;
		return !ExorMagCalza(wpnType, magType);
	}

	// LIMPIEZA DE DATA YA MALA: poda un arbol guardado (JSON de tumba/barril/mueble/auto)
	// sacando lo que es imposible de restaurar, ANTES de tocar el mundo:
	//   - entradas nulas o con classname inexistente
	//   - cargadores que no calzan en su arma (el caso que rompio el server)
	// Devuelve cuantas entradas descarto. Recursivo (mochila dentro de mochila).
	//
	// RED DE SEGURIDAD (importante): se marca primero y se borra despues. Si el chequeo
	// diera por malo TODO el contenido de un nivel con varios items, es mucho mas probable
	// que el chequeo este roto (un ConfigIsExisting que falla) a que el jugador tuviera
	// TODO corrupto -> se ABORTA la poda de ese nivel y no se borra nada. Vale mas dejar
	// pasar data dudosa (RestoreItem tiene sus propios guards por item) que borrarle el
	// barril entero a alguien por un bug mio.
	static int Sanitize(array<ref ExorVO_ItemData> items, string parentType, bool sonAttachments)
	{
		if (!items)
			return 0;
		int total = items.Count();
		if (total == 0)
			return 0;

		array<int> aBorrar = new array<int>;
		array<string> motivos = new array<string>;
		int i;
		for (i = total - 1; i >= 0; i--)
		{
			ExorVO_ItemData d = items.Get(i);
			if (!d)
			{
				aBorrar.Insert(i);
				motivos.Insert("(null) [entrada vacia]");
				continue;
			}
			if (!ExorTypeExiste(d.type))
			{
				aBorrar.Insert(i);
				motivos.Insert(d.type + " [classname inexistente]");
				continue;
			}
			if (sonAttachments && ExorEsMagIncompatible(parentType, d.type))
			{
				aBorrar.Insert(i);
				motivos.Insert(d.type + " [cargador que no calza en el arma]");
				continue;
			}
		}

		// ABORTAR: descartaria TODO un nivel con varios items -> sospecha de bug propio.
		if (aBorrar.Count() == total && total > 1)
		{
			Print(string.Format("%1 GUARD ABORTADO: la poda queria descartar los %2 items de '%3' -> NO se borra nada (posible falso positivo)", ExorStorageConstants.LOG, total, parentType));
			return 0;
		}

		int quitados = 0;
		for (i = 0; i < aBorrar.Count(); i++)
		{
			Print(string.Format("%1 GUARD: %2 DESCARTADO de la data guardada (padre '%3')", ExorStorageConstants.LOG, motivos.Get(i), parentType));
			items.Remove(aBorrar.Get(i));	// indices en orden DESCENDENTE -> borrar no corre los siguientes
			quitados++;
		}

		// recursion sobre lo que quedo
		for (i = 0; i < items.Count(); i++)
		{
			ExorVO_ItemData k = items.Get(i);
			if (!k)
				continue;
			quitados += Sanitize(k.attachments, k.type, true);
			quitados += Sanitize(k.cargo, k.type, false);
		}
		return quitados;
	}

	static EntityAI RestoreItem(ExorVO_ItemData data, EntityAI parent, vector groundPos, EntityAI root = null, bool asAttachment = false)
	{
		if (!root)
			root = parent;

		// GUARD: sin data o con un classname que no existe -> no crear nada (CreateObjectEx
		// con basura devuelve null y el resto del arbol quedaba a medias).
		if (!data || !ExorTypeExiste(data.type))
		{
			if (data)
				Print(string.Format("%1 GUARD: item '%2' DESCARTADO (classname inexistente)", ExorStorageConstants.LOG, data.type));
			return null;
		}

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

		// CAUSA RAIZ del loot que llegaba DESARMADO a la tumba (235 casos en 3 dias en el
		// server real: 'M4_MPHndgrd_Black' no entro en 'M4A1', 'PlateCarrierPouches' no entro
		// en 'PlateCarrierVest', 'Battery9V' no entro en 'Headtorch_Grey'...). Vanilla, en
		// ItemBase:
		//     CanPutInCargo(parent)      -> false si parent.IsRuined()
		//     CanPutAsAttachment(parent) -> false si IsRuined() || parent.IsRuined()
		// o sea que un contenedor/arma DESTRUIDO rechaza TODO lo que le quieras meter, y un
		// item destruido no se ata a ningun lado. Como la vida se aplicaba ACA -antes de
		// colgarle los attachments y el cargo-, el item quedaba ruined en el instante mismo
		// del armado y despues rebotaba a la raiz todo lo suyo. Por eso fallaban los CINCO
		// hijos del mismo chaleco: no era capacidad ni slots, era el padre ya destruido.
		// Tambien es el origen de la VME "[X::SpawnAttachedMagazine] Failed to create and
		// attach null": el arma ruined no aceptaba el cargador.
		// ARREGLO: se arma TODO a vida llena y la vida va al FINAL (ver el cierre de esta
		// funcion). El resto de escalares (cantidad, balas, etapa de comida) no bloquean nada,
		// asi que se aplican ya.
		ApplyScalarsSinVida(e, data);

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
				// GUARD (crash 22-jul): SpawnAttachedMagazine con un mag que NO calza en el arma
				// tira ERROR de VM ("Failed to create and attach null") y deja el arma a medio
				// armar -> eso se persiste y el server no vuelve a arrancar. Si no calza,
				// el cargador se DESCARTA y el arma se restaura entera e intacta.
				if (wpn && emag && !ExorMagCalza(wpn.GetType(), data.type))
				{
					Print(string.Format("%1 GUARD: cargador '%2' no calza en '%3' -> DESCARTADO (el arma se restaura igual)", ExorStorageConstants.LOG, data.type, wpn.GetType()));
					GetGame().ObjectDelete(e);
					return null;
				}
				if (wpn && emag)
				{
					// GUARD 2 (VME x3 el 22-jul, en UMP45, SCARH_Black y M4A1 al restaurar
					// tumbas): el cargador CALZA, pero el arma YA TIENE UNO PUESTO. Varias armas
					// -sobre todo de mods- nacen con su cargador por defecto al crearlas, asi que
					// al llegar el nuestro, CreateAttachment no tiene slot libre y devuelve null;
					// vanilla loguea "Failed to create and attach null" y el arma queda a medio
					// armar (que es justo lo que despues rompe la persistencia).
					// No se descarta el cargador guardado: se saca el de fabrica y se pone el
					// nuestro, para no perderle al player la municion que tenia.
					Magazine yaPuesto = wpn.GetMagazine(0);
					if (yaPuesto)
					{
						Print(string.Format("%1 GUARD: '%2' ya venia con cargador de fabrica -> se reemplaza por el guardado ('%3')", ExorStorageConstants.LOG, wpn.GetType(), data.type));
						GetGame().ObjectDelete(yaPuesto);
					}

					Magazine nm = wpn.SpawnAttachedMagazine(data.type);
					if (nm)
					{
						nm.SetHealth01("", "", data.health);
						// mismo criterio que ApplyScalarsSinVida: el detalle real manda; el
						// ServerSetAmmoCount solo entra si el JSON es viejo y no lo trae.
						if (!ApplyAmmo(nm, data) && data.ammoCount >= 0)
							nm.ServerSetAmmoCount(data.ammoCount);
						// 'e' (el cargador suelto que se habia armado en el piso) se descarta: el
						// bueno es 'nm', ya adentro del arma. Se RETORNA aca: seguir de largo
						// dejaria tocando una entidad borrada (SetHealth01 al final de la funcion)
						// y un cargador no tiene cargo que restaurar.
						GetGame().ObjectDelete(e);
						return nm;
					}
					else
					{
						// Si igual fallo, que quede DICHO en el log: antes esto caia en silencio
						// al camino de attachment normal y el cargador terminaba suelto.
						Print(string.Format("%1 GUARD: SpawnAttachedMagazine('%2') fallo en '%3' -> el cargador va como attachment normal", ExorStorageConstants.LOG, data.type, wpn.GetType()));
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
				// PlaceOnSurface ademas lo deja como un objeto de MUNDO en regla. Un item que
				// quedo a mitad de camino de un movimiento de inventario es justo el que el
				// motor despues reporta como "CEStorageElement::Save X parent problem,
				// hierParent:0" en cada ciclo de guardado, y esos no persisten bien.
				if (root)
				{
					e.SetPosition(root.GetPosition());
					e.PlaceOnSurface();
				}
				// marcar el restore como INCOMPLETO: este item existe en el mundo pero NO
				// esta adentro del contenedor -> re-capturar ahora lo borraria del JSON.
				s_FallosUbicacion++;
				Print(string.Format("%1 AVISO: '%2' no entro en '%3' ni en la raiz (lleno) -> queda en el piso", ExorStorageConstants.LOG, data.type, parent.GetType()));
			}
		}

		// 3) CARGO: llenar 'e' AHORA en su lugar. Se MUEVEN los items adentro (no se crean):
		// 'e' puede estar nested y crear dentro de un contenedor-en-cargo no anda, pero MOVER
		// si. Recursivo: un bolso-en-bolso tambien se mueve VACIO y se llena en su lugar.
		for (i = 0; i < data.cargo.Count(); i++)
			RestoreItem(data.cargo.Get(i), e, groundPos, root, false);

		// 4) VIDA AL FINAL. Ahora 'e' ya esta en su lugar y ya tiene adentro todo lo suyo, asi
		// que recien aca se le pone el daño guardado. Al reves -como estaba- un item ruined
		// rechaza attachments y cargo (ItemBase.CanPutInCargo / CanPutAsAttachment) y su
		// contenido terminaba tirado en la raiz de la tumba. Ruinar DESPUES no expulsa nada:
		// vanilla convive con prendas/armas destruidas llenas todo el tiempo.
		e.SetHealth01("", "", data.health);

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
			RestoreItemTop(items.Get(i), container, groundPos);
	}

	// UNA entrada de nivel superior. Se extrajo del bucle de arriba para que el contenedor
	// pueda restaurar POR LOTES repartidos en varios frames (ver Exor_OpenableStorage.
	// ExorRestorePump) sin duplicar la logica. La entrada se restaura ENTERA: un bolso con
	// su contenido es atomico, que es lo que evita dejar arboles a medias.
	static void RestoreItemTop(ExorVO_ItemData d, EntityAI container, vector groundPos)
	{
		if (!d || !container)
			return;

		// item REALMENTE SUELTO (sin cargo NI attachments) -> directo a su casilla
		if (d.attachments.Count() == 0 && d.cargo.Count() == 0 && d.cargo_row >= 0 && d.cargo_col >= 0 && container.GetInventory())
		{
			EntityAI ex = container.GetInventory().CreateEntityInCargoEx(d.type, d.cargo_idx, d.cargo_row, d.cargo_col, d.cargo_flip);
			if (ex)
			{
				ApplyScalars(ex, d);
				return;
			}
			// fallo (casilla ocupada/invalida) -> al camino de abajo
		}

		// mochila/arma (o sin posicion / fallo): armar SUELTO y lleno, y moverlo a su
		// casilla exacta (RestoreItem hace el TakeEntityToCargoEx adentro).
		RestoreItem(d, container, groundPos, container);
	}

	// aplica los datos escalares (vida, cantidad, balas, etapa de comida) a un item ya creado.
	// Usarlo SOLO con items HOJA (sin attachments ni cargo que restaurar): si el item todavia
	// tiene que recibir cosas adentro, va ApplyScalarsSinVida + la vida al final, porque un
	// item ruined rechaza todo lo que se le quiera meter. Ver el comentario en RestoreItem.
	static void ApplyScalars(EntityAI e, ExorVO_ItemData d)
	{
		e.SetHealth01("", "", d.health);
		ApplyScalarsSinVida(e, d);
	}

	// idem pero SIN tocar la vida (queda a full hasta que el item este armado y ubicado).
	// Nada de esto bloquea el inventario: cantidad, balas y etapa de comida son inocuas.
	static void ApplyScalarsSinVida(EntityAI e, ExorVO_ItemData d)
	{
		ItemBase ib = ItemBase.Cast(e);
		if (ib && d.quantity >= 0)
			ib.SetQuantity(d.quantity);
		// MUNICION: primero el detalle real (que bala en cada posicion, recamara y cargador
		// interno del arma). Si devuelve true ya escribio los cartuchos del cargador -> NO
		// pisarlos con el ServerSetAmmoCount legacy, que los convertiria en balas normales.
		bool ammoExacta = ApplyAmmo(e, d);
		Magazine mag = Magazine.Cast(e);
		if (mag && !ammoExacta && d.ammoCount >= 0)
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

		bool ruinado = IsRuined(d);
		int k;

		// CARGO de un contenedor ruined -> al cargo de la tumba. Nadie puede abrir una prenda
		// rota anidada, asi que adentro el loot queda atrapado.
		if (ruinado && d.cargo && d.cargo.Count() > 0)
		{
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

		// ATTACHMENTS: sale SOLO el que esta destruido, mire como mire el padre.
		//
		// Un M4A1 destruido con el handguard y la culata sanos llega a la tumba CON el handguard
		// y la culata puestos; solo la mira y el cargador rotos caen sueltos. Es lo que manda
		// vanilla igual: ItemBase.CanPutAsAttachment devuelve false si el propio item esta
		// ruined, asi que atar uno roto no era una opcion.
		//
		// OJO, error que estuvo aca: tambien se sacaban los attachments SANOS cuando el padre
		// estaba roto, por miedo a que quedaran inalcanzables. Falso. EntityAI.AreChildrenAccessible
		// corta por CARGO, no por vida: a un arma metida en un contenedor no le podes sacar la
		// mira este rota o sana -sacas el arma del contenedor y ahi la desarmas-. Es DayZ normal
		// y pasa con cualquier arma en cualquier barril. El resultado del miedo era peor: un
		// handguard al 100% tirado suelto en la tumba en vez de puesto en su arma.
		//
		// Se recorre al reves y se saca uno por uno porque casi siempre es un caso MIXTO (unos
		// attachments rotos y otros sanos): no se puede vaciar la lista entera.
		if (d.attachments && d.attachments.Count() > 0)
		{
			for (k = d.attachments.Count() - 1; k >= 0; k--)
			{
				ExorVO_ItemData a = d.attachments.Get(k);
				if (!a)
					continue;
				if (!IsRuined(a))
					continue;	// sano -> se queda en su slot
				a.cargo_row = -1;
				a.cargo_col = -1;
				top.Insert(a);
				d.attachments.Remove(k);
			}
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
		if (!FileExist(ExorStorageConstants.BODYBAG_DIR))
			MakeDirectory(ExorStorageConstants.BODYBAG_DIR);	// virtualizacion de tumbas
	}
}
