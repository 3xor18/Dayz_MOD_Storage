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
			RestaurarRespaldos("001");
		}
		else
		{
			Print(string.Format("%1 BootRepair: AGOTADO. Los respaldos del engine (.002 y .001) tampoco arrancan.", ExorStorageConstants.LOG));
			Print(string.Format("%1 BootRepair: hace falta decision manual: borrar el dynamic_XXX.bin que muere (se pierde lo que haya en esa celda del mapa) o restaurar un backup del hosting.", ExorStorageConstants.LOG));
			Print(string.Format("%1 BootRepair: los .bin originales quedaron guardados como *.exorbad al lado.", ExorStorageConstants.LOG));
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
				continue;
			}

			// resguardo del .bin actual ANTES de pisarlo (auditable / reversible a mano)
			string malPath = dataDir + "\\" + baseName + ".exorbad";
			if (FileExist(malPath))
				DeleteFile(malPath);
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

		Print(string.Format("%1 BootRepair: %2 archivo(s) restaurados, %3 sin respaldo .%4", ExorStorageConstants.LOG, copiados, sinRespaldo, ext));
		if (copiados == 0)
			Print(string.Format("%1 BootRepair: no habia respaldos utilizables -> el proximo arranque escala de etapa", ExorStorageConstants.LOG));
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
