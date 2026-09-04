// ============================================================================
// 3xor_Vanilla_Optimization - BANCO DE PRUEBAS AUTOMATICO (SOLO server, admin)
// ============================================================================
// POR QUE EXISTE
// ----------------------------------------------------------------------------
// Dos preguntas que hasta ahora solo se podian contestar entrando al juego y
// mirando:
//
//   1) "Se pierde loot al abrir / cerrar / virtualizar / restaurar?"
//      Se contesta comparando el arbol de contenido ANTES y DESPUES de un ciclo
//      completo, item por item, incluyendo el TIPO DE CADA BALA (que es donde
//      se perdian las perforantes) y el contenido anidado de las mochilas.
//
//   2) "Esta optimizacion sirvio o no?"
//      Se contesta cronometrando la MISMA operacion N veces antes y despues del
//      cambio. Sin esto se optimiza por intuicion, y en este mod la intuicion ya
//      se equivoco dos veces (la teoria del 5/5 y la causa del lag de raid).
//
// Corre SIN JUGADOR: si existe el marcador, el server monta la prueba solo a los
// 90 s de arrancar y escribe el informe. Asi el ciclo "cambiar -> medir" no
// depende de que alguien entre al juego.
//
// MARCADOR: profiles/3xorVanillaOptimization/autotest.txt
//   linea 1: <items> <iteraciones> <x> <z>
//   ejemplo: 300 20 1914 7887
// El marcador SE BORRA al leerlo (corre una sola vez).
//
// INFORME: profiles/3xorVanillaOptimization/autotest_report.txt
// ============================================================================

class ExorSelfTest
{
	static string PathInforme()
	{
		return ExorStorageConstants.CONFIG_DIR + "\\autotest_report.txt";
	}

	static ref array<string> s_Lineas;

	static void Reset()
	{
		s_Lineas = new array<string>;
	}

	static void Log(string s)
	{
		if (!s_Lineas)
			s_Lineas = new array<string>;
		s_Lineas.Insert(s);
		Print(string.Format("%1 AUTOTEST | %2", ExorStorageConstants.LOG, s));
	}

	static void Volcar()
	{
		if (!s_Lineas)
			return;
		FileHandle fh = OpenFile(PathInforme(), FileMode.WRITE);
		if (fh == 0)
		{
			Print(string.Format("%1 AUTOTEST: no se pudo escribir el informe", ExorStorageConstants.LOG));
			return;
		}
		int i;
		for (i = 0; i < s_Lineas.Count(); i++)
			FPrintln(fh, s_Lineas.Get(i));
		CloseFile(fh);
		Print(string.Format("%1 AUTOTEST: informe escrito en %2", ExorStorageConstants.LOG, PathInforme()));
	}

	// ------------------------------------------------------------------------
	//  HUELLA DEL CONTENIDO (lo que se compara antes/despues)
	// ------------------------------------------------------------------------
	// Un descriptor por entidad del arbol, con TODO lo que el mod dice guardar. Si un
	// campo se persiste y no esta aca, una perdida de ese campo pasaria desapercibida.
	// Se junta en una lista PLANA y ordenada: asi la comparacion no depende del orden en
	// que el motor devuelve el cargo (que no es estable) y las diferencias se pueden
	// listar una por una en vez de decir solo "no coincide".
	static string Descriptor(EntityAI e, int prof)
	{
		if (!e)
			return "(null)";
		int vida = (int)Math.Round(e.GetHealth01("", "") * 1000);
		int cant = -1;
		ItemBase ib = ItemBase.Cast(e);
		if (ib && ib.HasQuantity())
			cant = (int)Math.Round(ib.GetQuantity());
		int balas = -1;
		Magazine mag = Magazine.Cast(e);
		if (mag)
			balas = mag.GetAmmoCount();
		int comida = -1;
		Edible_Base ed = Edible_Base.Cast(e);
		if (ed && ed.GetFoodStage())
			comida = ed.GetFoodStage().GetFoodStageType();

		// BALAS: el tipo real de cada cartucho, no solo cuantos. Es el campo que fallaba
		// (perforantes que volvian normales, escopetas que volvian vacias), asi que es
		// justamente el que no puede faltar en la huella.
		ExorVO_ItemData tmp = new ExorVO_ItemData();
		ExorVO_Serializer.CaptureAmmo(e, tmp);
		string detalle = "";
		if (tmp.ammo)
		{
			int a;
			for (a = 0; a < tmp.ammo.Count(); a++)
			{
				// El DAÑO del cartucho se cuantiza a centesimas antes de comparar. El motor
				// devuelve 0.001 donde se le guardo 0.000 (redondeo suyo al almacenar un
				// cartucho a mano), y esa milesima hacia "fallar" el ciclo sin que se hubiera
				// perdido nada. A centesimas sigue detectando una bala realmente dañada.
				string kind;
				int nBalas;
				float dmg;
				string ctype;
				if (ExorVO_Serializer.ExorAmmoDec(tmp.ammo.Get(a), kind, nBalas, dmg, ctype))
					detalle = detalle + string.Format("%1|%2|%3|%4;", kind, nBalas, (int)Math.Round(dmg * 100), ctype);
				else
					detalle = detalle + tmp.ammo.Get(a) + ";";
			}
		}
		return string.Format("d%1 %2 vida=%3 cant=%4 balas=%5 comida=%6 [%7]", prof, e.GetType(), vida, cant, balas, comida, detalle);
	}

	static void Recolectar(EntityAI e, int prof, array<string> dest)
	{
		if (!e)
			return;
		dest.Insert(Descriptor(e, prof));
		GameInventory inv = e.GetInventory();
		if (!inv)
			return;
		int i;
		for (i = 0; i < inv.AttachmentCount(); i++)
			Recolectar(inv.GetAttachmentFromIndex(i), prof + 1, dest);
		CargoBase cg = inv.GetCargo();
		if (cg)
		{
			for (i = 0; i < cg.GetItemCount(); i++)
				Recolectar(cg.GetItem(i), prof + 1, dest);
		}
	}

	// Huella del CONTENIDO de un contenedor (no del contenedor en si).
	static array<string> Huella(EntityAI cont)
	{
		array<string> res = new array<string>;
		if (!cont)
			return res;
		GameInventory inv = cont.GetInventory();
		if (!inv)
			return res;
		int i;
		for (i = 0; i < inv.AttachmentCount(); i++)
			Recolectar(inv.GetAttachmentFromIndex(i), 0, res);
		CargoBase cg = inv.GetCargo();
		if (cg)
		{
			for (i = 0; i < cg.GetItemCount(); i++)
				Recolectar(cg.GetItem(i), 0, res);
		}
		return res;
	}

	// Cuenta cuantas veces aparece cada descriptor (multiconjunto). Se compara asi y no
	// item por item en orden porque el motor NO devuelve el cargo en un orden estable:
	// ordenar por indice daria falsos positivos. Y no se ordenan las listas porque en
	// Enforce comparar strings con '<' no es confiable.
	static map<string, int> Contar(array<string> l)
	{
		map<string, int> m = new map<string, int>;
		int i;
		for (i = 0; i < l.Count(); i++)
		{
			string k = l.Get(i);
			int prev = 0;
			m.Find(k, prev);
			m.Set(k, prev + 1);
		}
		return m;
	}

	// Compara dos huellas y deja en el informe las diferencias CONCRETAS. Devuelve la
	// cantidad de diferencias (0 = el ciclo no toco nada).
	static int Comparar(string titulo, array<string> antes, array<string> despues)
	{
		map<string, int> a = Contar(antes);
		map<string, int> b = Contar(despues);
		int faltan = 0;
		int sobran = 0;
		int mostrados = 0;
		int i;

		for (i = 0; i < a.Count(); i++)
		{
			string ka = a.GetKey(i);
			int na = a.GetElement(i);
			int nb = 0;
			b.Find(ka, nb);
			if (na > nb)
			{
				faltan += na - nb;
				if (mostrados < 15)
				{
					Log(string.Format("   [-] FALTAN %1: %2", na - nb, ka));
					mostrados++;
				}
			}
		}
		for (i = 0; i < b.Count(); i++)
		{
			string kb = b.GetKey(i);
			int nb2 = b.GetElement(i);
			int na2 = 0;
			a.Find(kb, na2);
			if (nb2 > na2)
			{
				sobran += nb2 - na2;
				if (mostrados < 15)
				{
					Log(string.Format("   [+] SOBRAN %1: %2", nb2 - na2, kb));
					mostrados++;
				}
			}
		}

		int dif = faltan + sobran;
		if (dif == 0)
			Log(string.Format("  OK  %1: %2 entidades identicas antes y despues", titulo, antes.Count()));
		else
			Log(string.Format("  FALLA  %1: %2 diferencias (%3 perdidas, %4 aparecidas) sobre %5 entidades", titulo, dif, faltan, sobran, antes.Count()));
		return dif;
	}

	// ------------------------------------------------------------------------
	//  CONTENIDO DE PRUEBA
	// ------------------------------------------------------------------------
	// Se elige a mano para cubrir los casos que YA fallaron alguna vez:
	//   - pilas de municion (el caso comun, y el que mide el costo)
	//   - un cargador con balas TRAZADORAS/PERFORANTES mezcladas (volvian normales)
	//   - un arma de cargador INTERNO (escopeta/Mosin: volvian vacias)
	//   - un arma con cargador puesto (el cargador no calzaba y quedaba suelto)
	//   - una mochila CON COSAS ADENTRO (el anidado que DayZ tira al piso al cargar)
	//   - comida (etapa de coccion, la nevera)
	// Todo lo que no exista en los configs de este server se saltea sin romper nada.
	static bool Existe(string type)
	{
		return ExorVO_Serializer.ExorTypeExiste(type);
	}

	static string PrimeroQueExista(array<string> candidatos)
	{
		int i;
		for (i = 0; i < candidatos.Count(); i++)
		{
			if (Existe(candidatos.Get(i)))
				return candidatos.Get(i);
		}
		return "";
	}

	// Munición del CALIBRE correcto para 'contenedorType' (un cargador o un cartucho de
	// referencia). Se saca de los configs y no de una lista fija a proposito: meterle a un
	// cargador una bala de otro calibre deja el cargador ilegible para el motor y el
	// siguiente GetCartridgeAtIndex CRASHEA el server (pasó armando esta prueba). Ademas
	// asi la prueba sirve igual en un server con mods de armas.
	// 'especial' = preferir trazadora/perforante, que es justo el caso que fallaba
	// (volvian convertidas en balas normales al restaurar).
	static string BalaDeCalibre(string contenedorType, bool especial)
	{
		string cal = ExorVO_Serializer.ExorCalibreDe(contenedorType);
		if (cal == "")
			return "";
		string normal = "";
		int n = GetGame().ConfigGetChildrenCount("CfgMagazines");
		int i;
		for (i = 0; i < n; i++)
		{
			string nom;
			GetGame().ConfigGetChildName("CfgMagazines", i, nom);
			if (nom.IndexOf("Ammo_") != 0)
				continue;	// los cartuchos sueltos se llaman Ammo_*; los cargadores Mag_*
			if (ExorVO_Serializer.ExorCalibreDe(nom) != cal)
				continue;
			string bajo = nom;
			bajo.ToLower();
			if (especial && (bajo.Contains("tracer") || bajo.Contains("ap")))
				return nom;
			if (normal == "")
				normal = nom;
		}
		return normal;
	}

	// Munición que ESTA ARMA puede chambrear (para las de cargador interno: escopeta, Mosin).
	static string BalaDeArma(string wpnType, bool especial)
	{
		TStringArray de = new TStringArray;
		GetGame().ConfigGetTextArray("CfgWeapons " + wpnType + " chamberableFrom", de);
		int i;
		for (i = 0; i < de.Count(); i++)
		{
			string b = BalaDeCalibre(de.Get(i), especial);
			if (b != "")
				return b;
		}
		return "";
	}

	// Llena 'cont' con 'items' pilas de municion + el set de casos raros. Devuelve
	// cuantas entidades de nivel superior entraron.
	// Cada fase deja su linea en el log ANTES de ejecutarse: si el motor se cae armando
	// alguna, el informe dice exactamente cual (asi se encontro el crash del calibre).
	static int Llenar(EntityAI cont, int items)
	{
		if (!cont || !cont.GetInventory())
			return 0;
		int n = 0;
		int i;

		// --- 1) el grueso: pilas de municion (lo que hace pesado al contenedor) ---
		string pila = PrimeroQueExista(ArrUno("Ammo_556x45"));
		if (pila != "")
		{
			for (i = 0; i < items; i++)
			{
				EntityAI it = cont.GetInventory().CreateInInventory(pila);
				if (!it)
					break;
				ItemBase ib = ItemBase.Cast(it);
				// cantidades distintas: si el guardado perdiera la cantidad, con pilas
				// todas iguales no se notaria.
				if (ib && ib.HasQuantity())
					ib.SetQuantity(5 + (i - ((i / 17) * 17)));
				n++;
			}
		}

		// --- 2) cargador con municion ESPECIAL (el bug de las perforantes) ---
		string magType = PrimeroQueExista(ArrTres("Mag_STANAG_30Rnd", "Mag_AKM_30Rnd", "Mag_CMAG_20Rnd"));
		if (magType != "")
		{
			// MUNICION DE FABRICA, sin tocar los cartuchos a mano. Rellenarlos con
			// ServerSetAmmoCount(0) + ServerStoreCartridge deja el cargador en un estado que
			// el motor no puede releer y CRASHEA en GetCartridgeAtIndex (reproducido dos
			// veces). Ese camino se investiga aparte, en la SONDA del final: aca lo unico
			// que interesa es que el ciclo de guardado no pierda la municion.
			Log(string.Format("  fase 2: cargador %1 con su municion de fabrica", magType));
			if (cont.GetInventory().CreateInInventory(magType))
				n++;
		}

		// --- 3) arma de cargador INTERNO (escopeta / Mosin: volvian vacias) ---
		string wInt = PrimeroQueExista(ArrTres("Mosin9130", "Izh43Shotgun", "Izh18Shotgun"));
		if (wInt != "")
		{
			string balaInt = BalaDeArma(wInt, true);
			Log(string.Format("  fase 3: arma de cargador interno %1 con %2", wInt, balaInt));
			if (balaInt != "")
			{
				Weapon_Base w = Weapon_Base.Cast(cont.GetInventory().CreateInInventory(wInt));
				if (w)
				{
					int mz;
					for (mz = 0; mz < w.GetMuzzleCount(); mz++)
					{
						int c;
						for (c = 0; c < 3; c++)
							w.PushCartridgeToInternalMagazine(mz, 0.0, balaInt);
					}
					n++;
				}
			}
		}

		// --- 4) arma con cargador puesto (el que no calzaba y quedaba suelto) ---
		string wMag = PrimeroQueExista(ArrTres("M4A1", "AKM", "FAL"));
		if (wMag != "")
		{
			string magDeArma = ExorMagDe(wMag);
			Log(string.Format("  fase 4: arma %1 con cargador %2 puesto", wMag, magDeArma));
			EntityAI w2 = cont.GetInventory().CreateInInventory(wMag);
			Weapon_Base wb = Weapon_Base.Cast(w2);
			if (wb && magDeArma != "")
			{
				// se deja la municion DE FABRICA a proposito: manipular los cartuchos de un
				// cargador ya enganchado al arma es justo el camino que rompio el motor.
				wb.SpawnAttachedMagazine(magDeArma);
				n++;
			}
		}

		// --- 5) mochila CON COSAS ADENTRO (el anidado) ---
		string bolso = PrimeroQueExista(ArrTres("TaloonBag_Blue", "MountainBag_Blue", "HuntingBag"));
		if (bolso != "" && pila != "")
		{
			Log(string.Format("  fase 5: mochila %1 con 6 pilas adentro", bolso));
			EntityAI bp = cont.GetInventory().CreateInInventory(bolso);
			if (bp && bp.GetInventory())
			{
				int b;
				for (b = 0; b < 6; b++)
				{
					EntityAI sub = bp.GetInventory().CreateInInventory(pila);
					ItemBase sib = ItemBase.Cast(sub);
					if (sib && sib.HasQuantity())
						sib.SetQuantity(11 + b);
				}
				n++;
			}
		}

		// --- 6) comida (etapa / pudricion: lo de la nevera) ---
		string comida = PrimeroQueExista(ArrTres("Apple", "TacticalBaconCan", "SardinesCan"));
		if (comida != "")
		{
			Log(string.Format("  fase 6: comida %1", comida));
			if (cont.GetInventory().CreateInInventory(comida))
				n++;
		}
		Log("  fases completas");
		return n;
	}

	// Ropa PUESTA con cosas en los bolsillos. Es la forma real del loot de una tumba: la
	// mayor parte no esta en el cargo sino en los SLOTS DE EQUIPO, cada prenda con su propio
	// contenido anidado. Justamente el caso que DayZ tira al piso al recargar el mundo y el
	// que hizo que las tumbas volvieran vacias.
	static int LlenarRopa(EntityAI cont)
	{
		if (!cont || !cont.GetInventory())
			return 0;
		int n = 0;
		array<string> prendas = new array<string>;
		prendas.Insert("PlateCarrierVest");
		prendas.Insert("HuntingJacket_Brown");
		prendas.Insert("CargoPants_Blue");
		prendas.Insert("MilitaryBoots_Black");
		prendas.Insert("TaloonBag_Blue");
		string pila = PrimeroQueExista(ArrUno("Ammo_556x45"));

		int i;
		for (i = 0; i < prendas.Count(); i++)
		{
			string tipo = prendas.Get(i);
			if (!Existe(tipo))
				continue;
			EntityAI pr = cont.GetInventory().CreateInInventory(tipo);
			if (!pr)
				continue;
			n++;
			// bolsillos con contenido: lo anidado es lo que hay que probar
			if (pila != "" && pr.GetInventory())
			{
				int k;
				for (k = 0; k < 3; k++)
				{
					EntityAI sub = pr.GetInventory().CreateInInventory(pila);
					ItemBase sib = ItemBase.Cast(sub);
					if (sib && sib.HasQuantity())
						sib.SetQuantity(7 + k + i);
				}
			}
		}
		return n;
	}

	// Solo COMIDA: la nevera rechaza todo lo demas (ExorCanStore). Sirve para probar que el
	// ciclo no le cambia la etapa de coccion ni le destroza la comida al restaurarla, que es
	// lo especifico de la nevera (su contenido envejece por tiempo transcurrido, no por tick).
	static int LlenarComida(EntityAI cont, int cuantos)
	{
		if (!cont || !cont.GetInventory())
			return 0;
		array<string> comidas = new array<string>;
		comidas.Insert("Apple");
		comidas.Insert("TacticalBaconCan");
		comidas.Insert("SardinesCan");
		comidas.Insert("Pear");
		comidas.Insert("Plum");
		int n = 0;
		int i;
		for (i = 0; i < cuantos; i++)
		{
			string tipo = comidas.Get(i - ((i / comidas.Count()) * comidas.Count()));
			if (!Existe(tipo))
				continue;
			if (cont.GetInventory().CreateInInventory(tipo))
				n++;
		}
		return n;
	}

	// Helpers: Enforce no tiene literales de array, y armar uno a mano en cada llamada
	// ensucia el codigo de arriba.
	static array<string> ArrUno(string a)
	{
		array<string> r = new array<string>;
		r.Insert(a);
		return r;
	}

	static array<string> ArrTres(string a, string b, string c)
	{
		array<string> r = new array<string>;
		r.Insert(a);
		r.Insert(b);
		r.Insert(c);
		return r;
	}

	// cargador que declara el arma en su config (el primero de magazines[])
	static string ExorMagDe(string wpn)
	{
		TStringArray mags = new TStringArray;
		GetGame().ConfigGetTextArray("CfgWeapons " + wpn + " magazines", mags);
		if (mags.Count() > 0)
			return mags.Get(0);
		return "";
	}
}

// ============================================================================
//  ORQUESTADOR: monta la prueba, la corre por pasos y escribe el informe
// ============================================================================
// Va por pasos con CallLater porque el ciclo real NO es sincrono: el restore se
// reparte en frames y hay un cooldown de reapertura anti-dupe. Forzar todo en un
// frame mediria un camino que en produccion no existe.
class ExorAutoTest
{
	static string PathMarcador()
	{
		return ExorStorageConstants.CONFIG_DIR + "\\autotest.txt";
	}

	static int s_Items = 300;
	static int s_Iters = 20;
	static float s_X;
	static float s_Z;

	static Exor_OpenableStorage s_Cont;
	static Exor_Barrel_Base s_Barril;
	static Exor_BodyBag s_Tumba;
	static Exor_OpenableStorage s_Nevera;
	static ref array<string> s_HuellaAntes;
	static int s_Fallas;

	// La llama MissionServer.OnInit.
	static void Init()
	{
		if (!GetGame().IsServer())
			return;
		string path = PathMarcador();
		if (!FileExist(path))
			return;

		FileHandle fh = OpenFile(path, FileMode.READ);
		if (fh == 0)
			return;
		string linea = "";
		FGets(fh, linea);
		CloseFile(fh);
		DeleteFile(path);	// una sola vez

		array<string> a = new array<string>;
		linea.Trim();
		linea.Split(" ", a);
		if (a.Count() > 0) s_Items = a.Get(0).ToInt();
		if (a.Count() > 1) s_Iters = a.Get(1).ToInt();
		if (a.Count() > 2) s_X = a.Get(2).ToFloat();
		if (a.Count() > 3) s_Z = a.Get(3).ToFloat();
		if (s_Items < 1) s_Items = 1;
		if (s_Iters < 1) s_Iters = 1;

		Print(string.Format("%1 AUTOTEST: programado -> %2 items, %3 iteraciones, en <%4, %5>",
			ExorStorageConstants.LOG, s_Items, s_Iters, s_X, s_Z));

		// 75 s: despues de que el CE termino de restaurar la persistencia. Medir durante
		// el arranque mide el arranque, no el regimen normal.
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorAutoTest.Paso1, 75000, false);
	}

	// ---------------- PASO 1: montar el contenedor y llenarlo ----------------
	static void Paso1()
	{
		ExorSelfTest.Reset();
		ExorSelfTest.Log("==========================================================");
		ExorSelfTest.Log("  AUTOTEST - " + ExorRaidLog.TimeStamp());
		ExorSelfTest.Log("  build " + ExorStorageConstants.MOD_BUILD + " | version " + ExorStorageConstants.MOD_VERSION);
		ExorSelfTest.Log(string.Format("  %1 items por contenedor, %2 iteraciones de cronometro", s_Items, s_Iters));
		ExorSelfTest.Log("==========================================================");
		s_Fallas = 0;

		vector pos = Vector(s_X, 0, s_Z);
		pos[1] = GetGame().SurfaceY(s_X, s_Z);

		s_Cont = Exor_OpenableStorage.Cast(GetGame().CreateObjectEx("Exor_Locker", pos, ECE_PLACE_ON_SURFACE));
		if (!s_Cont)
		{
			ExorSelfTest.Log("ERROR: no se pudo crear el locker de prueba");
			ExorSelfTest.Volcar();
			return;
		}
		s_Cont.Open();
		int t0 = GetGame().GetTime();
		int n = ExorSelfTest.Llenar(s_Cont, s_Items);
		int tLlenar = GetGame().GetTime() - t0;
		ExorSelfTest.Log("");
		ExorSelfTest.Log(string.Format("PREPARACION: %1 entradas de nivel superior creadas en %2 ms", n, tLlenar));

		s_HuellaAntes = ExorSelfTest.Huella(s_Cont);
		ExorSelfTest.Log(string.Format("PREPARACION: la huella del contenido tiene %1 entidades (incluye anidado)", s_HuellaAntes.Count()));

		// ---------------- CRONOMETRO ----------------
		Cronometrar();

		// ---------------- INTEGRIDAD: ciclo real ----------------
		ExorSelfTest.Log("");
		ExorSelfTest.Log("--- INTEGRIDAD DEL LOOT (ciclo real: cerrar -> virtualizar -> abrir) ---");
		s_Cont.Close();
		s_Cont.ExorVirtualize();
		int reales = ExorContainerOps.CargoCount(s_Cont);
		ExorSelfTest.Log(string.Format("  virtualizado: quedan %1 items reales en el mundo (tiene que ser 0)", reales));
		if (reales != 0)
			s_Fallas++;

		// 4 s: mas que el cooldown de reapertura anti-dupe (3 s por default)
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorAutoTest.Paso2, 4000, false);
	}

	// ---------------- CRONOMETRO de las operaciones caras ----------------
	// Se mide con el contenedor ABIERTO y lleno, que es el estado en el que el server
	// paga estas operaciones de verdad.
	static void Cronometrar()
	{
		int i;
		int t0;
		int ms;

		ExorSelfTest.Log("");
		ExorSelfTest.Log("--- CRONOMETRO (ms por operacion, promedio de " + s_Iters.ToString() + ") ---");

		// 1) firma del inventario vivo (se paga en CADA intento de guardado)
		t0 = GetGame().GetTime();
		for (i = 0; i < s_Iters; i++)
			ExorContainerOps.FirmaViva(s_Cont, true);
		ms = GetGame().GetTime() - t0;
		Fila("FirmaViva (decidir si hay que escribir)", ms);

		// 2) armado del DTO
		t0 = GetGame().GetTime();
		for (i = 0; i < s_Iters; i++)
			ExorContainerOps.ArmarSnapshot(s_Cont, "bench", true);
		ms = GetGame().GetTime() - t0;
		Fila("ArmarSnapshot (DTO en memoria)", ms);

		// 3) volcado a disco del DTO ya armado (CAMINO VIEJO: DTO + JsonSerializer por item)
		ExorVO_ContainerFile f = ExorContainerOps.ArmarSnapshot(s_Cont, "bench", true);
		string pruebaPath = ExorStorageConstants.STORAGE_DIR + "\\bench.json";
		t0 = GetGame().GetTime();
		for (i = 0; i < s_Iters; i++)
			ExorContainerOps.GuardarJL(pruebaPath, f);
		ms = GetGame().GetTime() - t0;
		Fila("GuardarJL (serializar + escribir el archivo)", ms);
		int bytesViejo = TamanoArchivo(pruebaPath);

		ExorSelfTest.Log(string.Format("  tamano del archivo de contenido: %1 bytes", bytesViejo));

		// 4) la operacion COMPLETA de guardado, que es la que corre en produccion
		t0 = GetGame().GetTime();
		for (i = 0; i < s_Iters; i++)
			s_Cont.ExorWriteSnapshot(true);
		ms = GetGame().GetTime() - t0;
		Fila("ExorWriteSnapshot(forzado) = OPERACION REAL", ms);

		// 5) lectura del archivo (se paga en cada apertura)
		t0 = GetGame().GetTime();
		for (i = 0; i < s_Iters; i++)
			ExorContainerOps.LeerJL(pruebaPath);
		ms = GetGame().GetTime() - t0;
		Fila("LeerJL (lectura del archivo)", ms);

		if (FileExist(pruebaPath))
			DeleteFile(pruebaPath);

		// 7) el ciclo entero virtualizar + restaurar (lo mas caro del mod)
		int iterCiclo = 3;
		if (s_Iters < 3)
			iterCiclo = s_Iters;
		int tVirt = 0;
		int tRest = 0;
		for (i = 0; i < iterCiclo; i++)
		{
			t0 = GetGame().GetTime();
			s_Cont.ExorVirtualize();
			tVirt += GetGame().GetTime() - t0;
			t0 = GetGame().GetTime();
			s_Cont.ExorDoRestore();
			s_Cont.ExorRestoreDrain();
			tRest += GetGame().GetTime() - t0;
		}
		FilaN("ExorVirtualize (guardar + sacar del mundo)", tVirt, iterCiclo);
		FilaN("Restore completo (recrear todo el arbol)", tRest, iterCiclo);
	}

	static void Fila(string nombre, int msTotal)
	{
		FilaN(nombre, msTotal, s_Iters);
	}

	static void FilaN(string nombre, int msTotal, int iters)
	{
		if (iters < 1)
			iters = 1;
		// Una cifra decimal sin usar floats en el formato (Enforce los imprime largos).
		// El valor se arma en su propia variable: un string.Format ANIDADO dentro de otro
		// sale VACIO en este compilador (la primera corrida del banco imprimio "  ms" sin
		// numero, que es justo el dato que se estaba midiendo).
		int decimas = (msTotal * 10) / iters;
		int ent = decimas / 10;
		int dec = decimas - (ent * 10);
		string val = ent.ToString() + "." + dec.ToString();
		ExorSelfTest.Log(string.Format("  %1 ms   %2   (total %3 ms / %4)", val, nombre, msTotal, iters));
	}

	static int TamanoArchivo(string path)
	{
		FileHandle fh = OpenFile(path, FileMode.READ);
		if (fh == 0)
			return 0;
		int total = 0;
		string l;
		while (FGets(fh, l) >= 0)
			total += l.Length() + 1;
		CloseFile(fh);
		return total;
	}

	// ---------------- PASO 2: reabrir y comparar ----------------
	static void Paso2()
	{
		if (!s_Cont)
			return;
		s_Cont.Open();
		// el restore se reparte en frames y puede diferirse por el espaciado global:
		// se le da tiempo y despues se termina lo que quede.
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorAutoTest.Paso3, 3000, false);
	}

	static void Paso3()
	{
		if (!s_Cont)
			return;
		Asegurar(s_Cont);
		array<string> despues = ExorSelfTest.Huella(s_Cont);
		s_Fallas += ExorSelfTest.Comparar("locker: cerrar -> virtualizar -> abrir", s_HuellaAntes, despues);

		// ---------------- BARRIL: el mismo ciclo ----------------
		ExorSelfTest.Log("");
		ExorSelfTest.Log("--- INTEGRIDAD DEL LOOT: BARRIL ---");
		vector pos = s_Cont.GetPosition();
		pos[0] = pos[0] + 4.0;
		pos[1] = GetGame().SurfaceY(pos[0], pos[2]);
		s_Barril = Exor_Barrel_Base.Cast(GetGame().CreateObjectEx("Exor_Barrel_500", pos, ECE_PLACE_ON_SURFACE));
		if (!s_Barril)
		{
			ExorSelfTest.Log("  (no se pudo crear el barril de prueba: se saltea)");
			Fin();
			return;
		}
		s_Barril.Open();
		int chicos = s_Items / 4;
		if (chicos < 10)
			chicos = 10;
		ExorSelfTest.Llenar(s_Barril, chicos);
		s_HuellaAntes = ExorSelfTest.Huella(s_Barril);
		ExorSelfTest.Log(string.Format("  contenido de prueba: %1 entidades", s_HuellaAntes.Count()));
		s_Barril.Close();
		s_Barril.ExorVirtualize();
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorAutoTest.Paso4, 4000, false);
	}

	static void Paso4()
	{
		if (!s_Barril)
		{
			Fin();
			return;
		}
		s_Barril.Open();
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(ExorAutoTest.Paso5, 3000, false);
	}

	static void Paso5()
	{
		if (s_Barril)
		{
			AsegurarBarril(s_Barril);
			array<string> despues = ExorSelfTest.Huella(s_Barril);
			s_Fallas += ExorSelfTest.Comparar("barril: cerrar -> virtualizar -> abrir", s_HuellaAntes, despues);
		}
		// ---------------- TUMBA: el mismo ciclo ----------------
		ExorSelfTest.Log("");
		ExorSelfTest.Log("--- INTEGRIDAD DEL LOOT: TUMBA (bolsa de cadaver) ---");
		vector tpos = Vector(s_X, 0, s_Z + 6);
		tpos[1] = GetGame().SurfaceY(tpos[0], tpos[2]);
		s_Tumba = Exor_BodyBag.Cast(GetGame().CreateObjectEx("Exor_BodyBag", tpos, ECE_PLACE_ON_SURFACE));
		if (!s_Tumba)
		{
			ExorSelfTest.Log("  (no se pudo crear la tumba de prueba: se saltea)");
			Fin();
			return;
		}
		int ropas = ExorSelfTest.LlenarRopa(s_Tumba);
		int sueltos = ExorSelfTest.Llenar(s_Tumba, 40);
		s_HuellaAntes = ExorSelfTest.Huella(s_Tumba);
		ExorSelfTest.Log(string.Format("  %1 prenda(s) en slots + %2 entrada(s) en el cargo -> %3 entidades", ropas, sueltos, s_HuellaAntes.Count()));

		int tv0 = GetGame().GetTime();
		s_Tumba.ExorVirtualize();
		int tv = GetGame().GetTime() - tv0;
		int quedan = s_Tumba.ExorContentCount();
		ExorSelfTest.Log(string.Format("  virtualizada en %1 ms; quedan %2 items reales (tiene que ser 0)", tv, quedan));
		if (quedan != 0)
			s_Fallas++;

		int tr0 = GetGame().GetTime();
		s_Tumba.ExorRestore();
		int tr = GetGame().GetTime() - tr0;
		ExorSelfTest.Log(string.Format("  restaurada en %1 ms", tr));
		array<string> despTumba = ExorSelfTest.Huella(s_Tumba);
		s_Fallas += ExorSelfTest.Comparar("tumba: virtualizar -> restaurar", s_HuellaAntes, despTumba);

		// ---------------- NEVERA: el mismo ciclo, con comida ----------------
		ExorSelfTest.Log("");
		ExorSelfTest.Log("--- INTEGRIDAD DEL LOOT: NEVERA ---");
		vector npos = Vector(s_X + 4, 0, s_Z + 6);
		npos[1] = GetGame().SurfaceY(npos[0], npos[2]);
		s_Nevera = Exor_OpenableStorage.Cast(GetGame().CreateObjectEx("Exor_Fridge", npos, ECE_PLACE_ON_SURFACE));
		if (!s_Nevera)
		{
			ExorSelfTest.Log("  (no se pudo crear la nevera de prueba: se saltea)");
			Fin();
			return;
		}
		s_Nevera.Open();
		int comidas = ExorSelfTest.LlenarComida(s_Nevera, 30);
		s_HuellaAntes = ExorSelfTest.Huella(s_Nevera);
		ExorSelfTest.Log(string.Format("  %1 comida(s) adentro -> %2 entidades", comidas, s_HuellaAntes.Count()));
		s_Nevera.Close();
		s_Nevera.ExorVirtualize();
		int quedanN = ExorContainerOps.CargoCount(s_Nevera);
		ExorSelfTest.Log(string.Format("  virtualizada: quedan %1 items reales (tiene que ser 0)", quedanN));
		if (quedanN != 0)
			s_Fallas++;
		s_Nevera.ExorDoRestore();
		s_Nevera.ExorRestoreDrain();
		array<string> despN = ExorSelfTest.Huella(s_Nevera);
		s_Fallas += ExorSelfTest.Comparar("nevera: cerrar -> virtualizar -> restaurar", s_HuellaAntes, despN);

		Fin();
	}

	// Deja el contenedor RESTAURADO antes de sacarle la huella, pase lo que pase entre medio.
	// Hace falta porque el tick del mod sigue corriendo durante la prueba y puede volver a
	// virtualizar el contenedor por su cuenta: el pase PRE-REINICIO (que cierra y guarda todo
	// unos minutos antes de una hora de reinicio programado) lo hizo justo entre el Open() y
	// la comparacion, y la prueba reporto "305 items perdidos" cuando en realidad estaban
	// sanos y salvos en el JSON. Un banco de pruebas que falla segun la hora no sirve.
	static void Asegurar(Exor_OpenableStorage c)
	{
		c.ExorRestoreDrain();
		if (c.ExorIsVirtualized())
		{
			ExorSelfTest.Log("  (el contenedor se habia vuelto a virtualizar solo -tick del mod-; se restaura antes de comparar)");
			c.Open();
			c.ExorDoRestore();
			c.ExorRestoreDrain();
		}
	}

	static void AsegurarBarril(Exor_Barrel_Base b)
	{
		b.ExorRestoreDrain();
		if (b.ExorIsVirtualized())
		{
			ExorSelfTest.Log("  (el barril se habia vuelto a virtualizar solo; se restaura antes de comparar)");
			b.Open();
			b.ExorDoRestore();
			b.ExorRestoreDrain();
		}
	}

	static void Fin()
	{
		ExorSelfTest.Log("");
		ExorSelfTest.Log("--- ENTIDADES DEL MOD AL TERMINAR ---");
		ExorSelfTest.Log(ExorProfiler.ResumenEntidades());
		ExorSelfTest.Log("");
		if (s_Fallas == 0)
			ExorSelfTest.Log("RESULTADO: OK - ninguna diferencia de contenido en el ciclo completo");
		else
			ExorSelfTest.Log(string.Format("RESULTADO: %1 DIFERENCIA(S) - revisar las lineas [-] y [+] de arriba", s_Fallas));
		ExorSelfTest.Volcar();

		// limpiar lo de la prueba: si quedara, el proximo arranque lo tomaria como
		// contenido de verdad y ademas ensucia las mediciones siguientes.
		if (s_Cont)
			GetGame().ObjectDelete(s_Cont);
		if (s_Barril)
			GetGame().ObjectDelete(s_Barril);
		if (s_Tumba)
			GetGame().ObjectDelete(s_Tumba);
		if (s_Nevera)
			GetGame().ObjectDelete(s_Nevera);
		s_Cont = null;
		s_Barril = null;
		s_Tumba = null;
		s_Nevera = null;
	}

	// ========================================================================
	//  LO QUE ENCONTRO LA SONDA (ya no hace falta correrla)
	// ========================================================================
	// Se armo una sonda que leia cartuchos paso a paso dejando el indice en el RPT ANTES de
	// cada lectura, porque un ACCESS_VIOLATION adentro de una llamada nativa no se puede
	// atrapar desde script y la ultima linea escrita es la unica pista. Dio DOS resultados,
	// los dos reproducidos varias veces:
	//
	//   1) Con los parametros de salida SIN INICIALIZAR ("float d; string t;"),
	//      GetCartridgeAtIndex mata el server leyendo un cargador recien creado, intacto.
	//      Inicializandolos (float d = 0; string t = vacio) lee sin problema.
	//      ESTE era el que rompia produccion: ese codigo corre al guardar cualquier
	//      contenedor o tumba que tenga un cargador adentro.
	//
	//   2) Un cargador SUELTO EN EL MUNDO (no dentro de un inventario) al que se le hace
	//      ServerSetAmmoCount(0) y despues ServerStoreCartridge queda en un estado que
	//      GetCartridgeAtIndex tampoco puede leer, aun con los parametros inicializados.
	//      El mod NO cae ahi: ApplyAmmo hace esa misma secuencia mientras el item esta
	//      suelto, pero recien lo LEE cuando ya esta adentro del contenedor, y asi anda
	//      (lo confirma la prueba de integridad, que restaura cargadores y los vuelve a
	//      leer sin caerse). Queda anotado para no inventar por ahi una lectura sobre un
	//      cargador suelto recien rellenado.
	//
	// La sonda se saco del banco a proposito: para probar el punto 2 hay que MATAR el
	// server, asi que dejarla puesta significaba terminar cada corrida con un crash.
}
