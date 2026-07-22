// ============================================================================
// 3xor_Vanilla_Optimization - AUTO-REPARACION de la persistencia (SOLO server)
// ============================================================================
// PROBLEMA (produccion 22-jul-2026): un item quedo a MEDIO ARMAR en la persistencia
// (un cargador que no calzaba en el arma rompio su creacion) y quedo escrito en un
// dynamic_XXX.bin. Desde ahi el server NO volvio a arrancar: moria SIEMPRE en el mismo
// punto de "[CE][Storage] Restoring file .../dynamic_007.bin", con un crash NATIVO
// (Unhandled exception / Reason: Unknown) y el RPT cortado a mitad de linea. Sin error
// de script: no hay nada que atrapar desde el mod.
//
// La causa ya esta tapada por los guards de ExorVO_Serializer, pero eso solo sirve
// HACIA ADELANTE. Un server que YA tiene el .bin podrido queda muerto y el admin tiene
// que meter mano a mpmissions/<mision>/storage_1/data/ por SSH/FTP. Esto lo automatiza:
// el mod se repara solo, sin que nadie toque la carpeta de la mision.
//
// COMO: DayZ ya guarda 2 respaldos rotativos de cada archivo de persistencia (.001 y
// .002, los mismos que se ven en el log "[CE][Storage] ver:26 stamp:..."). El repair
// solo los usa. Escalado por etapas, guardado en el profile:
//
//   etapa 0  arranque normal.
//   etapa 1  el arranque anterior murio cargando -> REINTENTAR tal cual (un crash
//            puede ser transitorio; no se toca la data por un solo episodio).
//   etapa 2  volvio a morir -> restaurar TODOS los dynamic_*.bin desde su .002
//            (= retroceder un ciclo de guardado, unos minutos de juego).
//   etapa 3  idem desde .001 (el respaldo mas viejo).
//   etapa 4  se agoto lo automatico -> log claro para el admin. NO se borra nada solo:
//            borrar un dynamic_XXX.bin sin respaldo se lleva puesto todo lo que hubiera
//            en esa celda del mapa (bases incluidas) y eso lo decide una persona.
//
// El .bin que se pisa NUNCA se pierde: se guarda al lado como .exorbad antes de copiar.
//
// COMO SE DETECTA EL CRASH: en OnInit (que corre ANTES de que el CE cargue la
// persistencia) se escribe el marcador con la etapa. Si el server sigue vivo pasados
// VENTANA_MS, la carga TERMINO -> se borra el marcador. Si el server muere durante la
// carga, el marcador queda -> el proximo arranque sube de etapa y repara. Un crash
// posterior (a las 5 horas de juego) NO cuenta: el marcador ya se habia borrado.
// ============================================================================
class ExorBootRepair
{
	// La carga del CE tarda ~30s en un server poblado. 3 min da margen de sobra sin
	// tragarse un crash tardio (que no es problema de persistencia).
	static const int VENTANA_MS = 180000;
	static const int ETAPA_MAX = 4;

	static string PathFor()
	{
		return ExorStorageConstants.CONFIG_DIR + "\\boot_repair.txt";
	}

	// etapa pendiente del arranque anterior (0 = el anterior arranco bien)
	static int LeerEtapa()
	{
		string path = PathFor();
		if (!FileExist(path))
			return 0;
		FileHandle fh = OpenFile(path, FileMode.READ);
		if (fh == 0)
			return 0;
		string line = "";
		FGets(fh, line);
		CloseFile(fh);
		line.Trim();
		if (line == "")
			return 0;
		// tolerante: si el admin edito el archivo a mano con un editor que le mete BOM u
		// otra basura, quedarse solo con el primer numero en vez de leer 0 en silencio.
		string soloDigitos = "";
		int i;
		for (i = 0; i < line.Length(); i++)
		{
			string ch = line.Substring(i, 1);
			if (ch.ToInt() > 0 || ch == "0")
				soloDigitos = soloDigitos + ch;
			else if (soloDigitos != "")
				break;
		}
		if (soloDigitos == "")
			return 0;
		return soloDigitos.ToInt();
	}

	static void EscribirEtapa(int etapa)
	{
		if (!FileExist(ExorStorageConstants.CONFIG_DIR))
			MakeDirectory(ExorStorageConstants.CONFIG_DIR);
		FileHandle fh = OpenFile(PathFor(), FileMode.WRITE);
		if (fh == 0)
			return;
		FPrintln(fh, etapa.ToString());
		CloseFile(fh);
	}

	// El server sobrevivio la ventana => la persistencia cargo entera. Se limpia el
	// marcador para que el proximo arranque empiece de cero.
	static void MarcarArranqueOk()
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		string path = PathFor();
		if (FileExist(path))
		{
			DeleteFile(path);
			Print(string.Format("%1 BootRepair: persistencia cargada OK -> marcador limpiado", ExorStorageConstants.LOG));
		}
	}

	// ----------------------------------------------------------------------------
	// Punto de entrada. Se llama al PRINCIPIO de MissionServer.OnInit, que corre ANTES
	// de que el CE restaure los dynamic_*.bin -> todavia estamos a tiempo de repararlos.
	// ----------------------------------------------------------------------------
	static void Run()
	{
		if (!GetGame() || !GetGame().IsServer())
			return;

		int etapa = LeerEtapa();

		if (etapa == 0)
		{
			// arranque normal: armar el marcador por las dudas y seguir
			EscribirEtapa(1);
			GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(MarcarArranqueOk, VENTANA_MS, false);
			return;
		}

		Print("[3xorVO] ============================================================");
		Print(string.Format("%1 BootRepair: el arranque ANTERIOR murio cargando la persistencia (etapa %2)", ExorStorageConstants.LOG, etapa));

		if (etapa == 1)
		{
			// un solo episodio puede ser transitorio -> reintentar sin tocar nada
			Print(string.Format("%1 BootRepair: reintento sin tocar la data. Si vuelve a morir, restauro los respaldos del engine.", ExorStorageConstants.LOG));
		}
		else if (etapa == 2)
		{
			RestaurarRespaldos("002");
		}
		else if (etapa == 3)
		{
			Cuarentena();
		}
		else
		{
			Print(string.Format("%1 BootRepair: AGOTADO. Ni los respaldos del engine ni la cuarentena levantaron el server.", ExorStorageConstants.LOG));
			Print(string.Format("%1 BootRepair: hace falta decision manual (restaurar un backup del hosting). Todo lo apartado quedo como *.exorbad / *.exorquarantine al lado, nada se borro.", ExorStorageConstants.LOG));
		}
		Print("[3xorVO] ============================================================");

		int siguiente = etapa + 1;
		if (siguiente > ETAPA_MAX)
			siguiente = ETAPA_MAX;
		EscribirEtapa(siguiente);
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(MarcarArranqueOk, VENTANA_MS, false);
	}

	// ----------------------------------------------------------------------------
	// Copia <dir>/dynamic_XXX.<ext> sobre <dir>/dynamic_XXX.bin para cada archivo de
	// persistencia, guardando antes el .bin actual como .exorbad (nada se pierde).
	// ----------------------------------------------------------------------------
	static void RestaurarRespaldos(string ext)
	{
		string dataDir = BuscarDataDir();
		if (dataDir == "")
		{
			Print(string.Format("%1 BootRepair: NO se encontro la carpeta de persistencia (storage_X\\data) -> no puedo auto-reparar", ExorStorageConstants.LOG));
			return;
		}

		Print(string.Format("%1 BootRepair: restaurando respaldos .%2 en '%3'", ExorStorageConstants.LOG, ext, dataDir));

		int copiados = 0;
		int sinRespaldo = 0;
		string listaSinRespaldo = "";
		// TODOS los .bin de la carpeta, no solo los dynamic_*: types/events/building/vehicles
		// tambien pueden estar podridos. Los unicos que el engine NO respalda son animals.bin
		// y zombies.bin (se regeneran solos), asi que esos se saltean por no tener de donde.
		array<string> bins = ListarArchivos(dataDir, "*.bin");
		int i;
		for (i = 0; i < bins.Count(); i++)
		{
			string nombre = bins.Get(i);				// ej "dynamic_007.bin"
			int dot = nombre.LastIndexOf(".");
			if (dot <= 0)
				continue;
			string baseName = nombre.Substring(0, dot);	// "dynamic_007"

			string binPath = dataDir + "\\" + nombre;
			string bakPath = dataDir + "\\" + baseName + "." + ext;
			if (!FileExist(bakPath))
			{
				sinRespaldo++;
				listaSinRespaldo = listaSinRespaldo + " " + nombre;
				continue;
			}

			// Resguardo del .bin ORIGINAL antes de pisarlo (auditable / reversible a mano).
			// Si ya existe NO se pisa: en una escalada de etapas, el .exorbad de la primera
			// pasada es el .bin autentico del server; sobrescribirlo en la segunda pasada lo
			// reemplazaria por un respaldo ya restaurado y se perderia el original.
			string malPath = dataDir + "\\" + baseName + ".exorbad";
			if (!FileExist(malPath))
				CopyFile(binPath, malPath);

			if (CopyFile(bakPath, binPath))
			{
				copiados++;
				Print(string.Format("%1 BootRepair: '%2' <- '%3.%4' (el anterior quedo como %3.exorbad)", ExorStorageConstants.LOG, nombre, baseName, ext));
			}
			else
			{
				Print(string.Format("%1 BootRepair: FALLO copiando '%2.%3' sobre '%4'", ExorStorageConstants.LOG, baseName, ext, nombre));
			}
		}

		Print(string.Format("%1 BootRepair: %2 archivo(s) restaurados", ExorStorageConstants.LOG, copiados));
		if (sinRespaldo > 0)
			Print(string.Format("%1 BootRepair: %2 sin respaldo .%3 (el engine no los respalda, se regeneran solos):%4", ExorStorageConstants.LOG, sinRespaldo, ext, listaSinRespaldo));
		if (copiados == 0)
			Print(string.Format("%1 BootRepair: no habia respaldos utilizables -> el proximo arranque escala de etapa", ExorStorageConstants.LOG));
	}

	// ----------------------------------------------------------------------------
	// CUARENTENA (ultimo recurso automatico).
	//
	// Aprendido en produccion (22-jul): restaurar los respaldos NO siempre alcanza. Si el
	// item podrido lleva varios ciclos de guardado escrito, esta en el .bin Y en el .001 Y
	// en el .002 -> los tres matan el arranque. Ademas el engine elige solo el archivo con
	// el stamp mas nuevo de los tres, asi que pisar el .bin con uno mas viejo no fuerza
	// nada (se vio "Restoring file .../dynamic_007.002" despues de copiar el .001 encima).
	//
	// Entonces: se APARTA el archivo entero (sus 3 copias) para que el CE se saltee esa
	// celda del mapa y el server levante. Se pierde lo que hubiera ahi (loot y bases de esa
	// celda), pero NADA se borra: las 3 copias quedan como *.exorquarantine y un admin
	// puede devolverlas si consigue repararlas.
	//
	// COMO SE SABE CUAL: del RPT del arranque anterior. La ultima linea
	// "[CE][Storage] Restoring file ".../dynamic_XXX.bin"" es justo el archivo que estaba
	// cargando cuando murio. Se busca el RPT mas nuevo que TENGA esa linea (el del arranque
	// actual todavia no llego a cargar la persistencia, asi que se descarta solo).
	// ----------------------------------------------------------------------------
	static void Cuarentena()
	{
		string dataDir = BuscarDataDir();
		if (dataDir == "")
		{
			Print(string.Format("%1 BootRepair: NO se encontro la carpeta de persistencia -> no puedo poner en cuarentena", ExorStorageConstants.LOG));
			return;
		}

		string baseName = DetectarArchivoQueMata();
		if (baseName == "")
		{
			Print(string.Format("%1 BootRepair: no pude identificar en el RPT que archivo mata el arranque -> sin cuarentena automatica", ExorStorageConstants.LOG));
			return;
		}

		Print(string.Format("%1 BootRepair: CUARENTENA de '%2' (es el archivo que estaba cargando cuando murio)", ExorStorageConstants.LOG, baseName));
		Print(string.Format("%1 BootRepair: se pierde lo que hubiera en esa celda del mapa, pero las copias quedan como %2.*.exorquarantine", ExorStorageConstants.LOG, baseName));

		int apartados = 0;
		apartados += ApartarUno(dataDir, baseName, "bin");
		apartados += ApartarUno(dataDir, baseName, "001");
		apartados += ApartarUno(dataDir, baseName, "002");
		Print(string.Format("%1 BootRepair: %2 copia(s) de '%3' apartadas", ExorStorageConstants.LOG, apartados, baseName));
	}

	// mueve <dir>\<base>.<ext> a <dir>\<base>.<ext>.exorquarantine. Devuelve 1 si lo hizo.
	static int ApartarUno(string dir, string baseName, string ext)
	{
		string src = dir + "\\" + baseName + "." + ext;
		if (!FileExist(src))
			return 0;
		string dst = src + ".exorquarantine";
		if (FileExist(dst))
			DeleteFile(dst);
		if (!CopyFile(src, dst))
		{
			Print(string.Format("%1 BootRepair: FALLO copiando '%2' a cuarentena -> NO se borra el original", ExorStorageConstants.LOG, src));
			return 0;
		}
		DeleteFile(src);	// solo despues de tener la copia: nunca se pierde data
		return 1;
	}

	// nombre base (ej "dynamic_007") del ultimo archivo que el CE estaba restaurando en el
	// arranque que murio, leido del RPT. "" si no se pudo determinar.
	static string DetectarArchivoQueMata()
	{
		array<string> rpts = ListarArchivos("$profile:", "*.RPT");
		if (rpts.Count() == 0)
			return "";

		// los nombres llevan fecha-hora (DayZServer_2026-07-22_12-00-36.RPT) -> el orden
		// alfabetico DESCENDENTE es el cronologico inverso. Se mira del mas nuevo al mas
		// viejo y se corta en el primero que tenga lineas de carga de persistencia.
		OrdenarDesc(rpts);

		int mirados = 0;
		int i;
		for (i = 0; i < rpts.Count(); i++)
		{
			if (mirados >= 4)	// no escanear el historial entero: los 4 mas nuevos alcanzan
				break;
			mirados++;
			string encontrado = UltimoRestoringFile("$profile:" + rpts.Get(i));
			if (encontrado != "")
			{
				Print(string.Format("%1 BootRepair: segun '%2', murio cargando '%3'", ExorStorageConstants.LOG, rpts.Get(i), encontrado));
				return encontrado;
			}
		}
		return "";
	}

	// Lee el RPT y devuelve el archivo cuya carga QUEDO A MEDIAS, o "" si la carga
	// termino bien (ese arranque no murio cargando).
	//
	// CLAVE (bug detectado en el test local): no alcanza con el ultimo "Restoring file".
	// En un arranque EXITOSO tambien hay uno ultimo -simplemente el ultimo de la secuencia-
	// y quedaria acusado un archivo sano. El CE loguea "Restoring file X" y despues
	// "N items loaded" cuando lo TERMINO. Si el server murio adentro, el "items loaded"
	// nunca sale -> ese archivo quedo pendiente y ES el culpable.
	static string UltimoRestoringFile(string rptPath)
	{
		FileHandle fh = OpenFile(rptPath, FileMode.READ);
		if (fh == 0)
			return "";
		string pendiente = "";
		string linea;
		while (FGets(fh, linea) >= 0)
		{
			if (linea.IndexOf("Restoring file") >= 0)
			{
				string b = BaseNameDeLinea(linea);
				if (b != "")
					pendiente = b;
				continue;
			}
			if (linea.IndexOf("items loaded") >= 0)
				pendiente = "";		// ese archivo termino de cargar OK
		}
		CloseFile(fh);
		return pendiente;
	}

	// De: [CE][Storage] Restoring file "/ruta/storage_1/data/dynamic_007.bin" 941 items.
	// saca: dynamic_007
	static string BaseNameDeLinea(string linea)
	{
		int q1 = linea.IndexOf("\"");
		if (q1 < 0)
			return "";
		string resto = linea.Substring(q1 + 1, linea.Length() - q1 - 1);
		int q2 = resto.IndexOf("\"");
		if (q2 <= 0)
			return "";
		string ruta = resto.Substring(0, q2);

		// quedarse con el nombre de archivo (la ruta puede venir con / o con \)
		int corte = ruta.LastIndexOf("/");
		int corte2 = ruta.LastIndexOf("\\");
		if (corte2 > corte)
			corte = corte2;
		string nombre = ruta;
		if (corte >= 0)
			nombre = ruta.Substring(corte + 1, ruta.Length() - corte - 1);

		// sacarle la extension (.bin / .001 / .002)
		int dot = nombre.LastIndexOf(".");
		if (dot <= 0)
			return "";
		return nombre.Substring(0, dot);
	}

	// orden alfabetico DESCENDENTE (burbuja: son pocos archivos)
	static void OrdenarDesc(array<string> a)
	{
		int i;
		int j;
		for (i = 0; i < a.Count(); i++)
		{
			for (j = i + 1; j < a.Count(); j++)
			{
				if (a.Get(j) > a.Get(i))
				{
					string tmp = a.Get(i);
					a.Set(i, a.Get(j));
					a.Set(j, tmp);
				}
			}
		}
	}

	// ----------------------------------------------------------------------------
	// Encuentra "$mission:storage_X\data". El numero depende del instanceId del
	// serverDZ.cfg, asi que se busca en vez de asumir storage_1.
	// ----------------------------------------------------------------------------
	static string BuscarDataDir()
	{
		array<string> dirs = ListarDirectorios("$mission:", "storage_*");
		int i;
		for (i = 0; i < dirs.Count(); i++)
		{
			string cand = "$mission:" + dirs.Get(i) + "\\data";
			if (FileExist(cand))
				return cand;
		}
		return "";
	}

	// nombres de archivo (sin ruta) que matchean 'patron' dentro de 'dir'
	static array<string> ListarArchivos(string dir, string patron)
	{
		array<string> salida = new array<string>;
		string fileName;
		FileAttr attr;
		FindFileHandle h = FindFile(dir + "\\" + patron, fileName, attr, FindFileFlags.ALL);
		if (!h)
			return salida;
		if (fileName != "")
			salida.Insert(fileName);
		while (FindNextFile(h, fileName, attr))
		{
			if (fileName != "")
				salida.Insert(fileName);
		}
		CloseFindFile(h);
		return salida;
	}

	// nombres de subcarpetas que matchean 'patron' dentro de 'dir'
	static array<string> ListarDirectorios(string dir, string patron)
	{
		array<string> salida = new array<string>;
		string fileName;
		FileAttr attr;
		FindFileHandle h = FindFile(dir + patron, fileName, attr, FindFileFlags.DIRECTORIES);
		if (!h)
			return salida;
		if (fileName != "")
			salida.Insert(fileName);
		while (FindNextFile(h, fileName, attr))
		{
			if (fileName != "")
				salida.Insert(fileName);
		}
		CloseFindFile(h);
		return salida;
	}
}
