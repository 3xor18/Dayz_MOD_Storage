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
	static const int ETAPA_MAX = 7;

	// true = este arranque NO carga el cargo de ninguna nevera (lo consulta Exor_Fridge).
	// Se prende en la etapa 3 cuando el RPT del arranque muerto acusa a una nevera.
	static bool s_SaltearNeveras = false;
	static bool SaltearNeveras()
	{
		return s_SaltearNeveras;
	}

	// build del mod que dejo el marcador (para detectar que se actualizo el mod)
	static string s_BuildDelMarcador = "";

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
		string buildLine = "";
		FGets(fh, buildLine);		// 2da linea: build del mod que dejo el marcador
		CloseFile(fh);
		buildLine.Trim();
		s_BuildDelMarcador = buildLine;
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
		FPrintln(fh, ExorStorageConstants.MOD_BUILD);	// para detectar que se actualizo el mod
		CloseFile(fh);
	}

	// Reintentos de MarcarArranqueOk cuando la ventana se cumplio pero el CE TODAVIA no
	// termino de restaurar. Ver el comentario de MarcarArranqueOk.
	static const int OK_RECHECK_MS = 60000;
	static const int OK_MAX_REINTENTOS = 10;	// 10 min extra de gracia como maximo
	static int s_OkReintentos = 0;

	// El server sobrevivio la ventana => la persistencia cargo entera. Se limpia el
	// marcador para que el proximo arranque empiece de cero.
	//
	// BUG CORREGIDO (produccion 23-jul, arranque 01:25): la ventana de 3 min se cumplia y el
	// marcador se limpiaba aunque el server siguiera atascado cargando. Ese arranque se comio
	// 20s en un solo dynamic_*.bin por una nevera corrupta, se fue a 21 GB de RAM y lo mataron
	// a los 9 minutos... con el marcador YA borrado. Resultado: la escalada volvia siempre a
	// etapa 1 y NUNCA llegaba a las etapas que arreglan de verdad (respaldos / saltear neveras
	// / cuarentena). En todos los logs del 22 y 23 BootRepair aparece solo en etapa 1.
	// Ahora, antes de dar el arranque por bueno, se exige evidencia de que el CE TERMINO de
	// restaurar; si no, se re-chequea en vez de limpiar.
	static void MarcarArranqueOk()
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		string path = PathFor();
		if (!FileExist(path))
			return;

		// Solo se difiere el limpiado cuando hay evidencia POSITIVA de que la carga sigue a
		// medias. El caso "no se pudo determinar" cuenta como OK a proposito: ver CargaEstado.
		if (CargaEstado() == CARGA_EN_CURSO)
		{
			s_OkReintentos++;
			if (s_OkReintentos <= OK_MAX_REINTENTOS)
			{
				Print(string.Format("%1 BootRepair: la persistencia TODAVIA se esta cargando -> no limpio el marcador (reintento %2/%3)", ExorStorageConstants.LOG, s_OkReintentos, OK_MAX_REINTENTOS));
				GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(MarcarArranqueOk, OK_RECHECK_MS, false);
				return;
			}
			Print(string.Format("%1 BootRepair: la carga nunca dio por terminada -> el marcador QUEDA (el proximo arranque escala)", ExorStorageConstants.LOG));
			return;
		}

		DeleteFile(path);
		Print(string.Format("%1 BootRepair: persistencia cargada OK -> marcador limpiado", ExorStorageConstants.LOG));
	}

	static const int CARGA_INDETERMINADA = 0;
	static const int CARGA_EN_CURSO      = 1;
	static const int CARGA_TERMINADA     = 2;

	// En que estado esta la carga de la persistencia de ESTE arranque, segun el RPT propio?
	// El CE loguea "Restoring file X" y despues "N items loaded" cuando lo TERMINO; si el
	// ultimo Restoring no tiene su items loaded, ese archivo quedo a medias.
	//
	// SESGO DELIBERADO HACIA "TERMINADA": lo unico que hace el llamador con EN_CURSO es NO
	// limpiar el marcador, y no limpiarlo significa que el proximo arranque ESCALA. La etapa 2
	// restaura los respaldos .002, o sea le retrocede un ciclo de guardado a TODO el mapa
	// (bases de todo el mundo incluidas). Equivocarse para ese lado es MUCHO peor que no
	// escalar: la causa conocida de los crashes ya esta tapada en origen (guards de nevera y
	// de cargador). Por eso, si no se puede determinar -no hay RPT, no se puede abrir, el
	// hosting los escribe en otro lado, cambio el formato del log-, se devuelve TERMINADA y el
	// marcador se limpia. Solo se frena con evidencia POSITIVA de una carga a medias.
	static int CargaEstado()
	{
		array<string> rpts = ListarArchivos("$profile:", "*.RPT");
		if (rpts.Count() == 0)
			return CARGA_INDETERMINADA;
		OrdenarDesc(rpts);
		string actual = "$profile:" + rpts.Get(0);
		if (!TieneCargaDePersistencia(actual))
			return CARGA_INDETERMINADA;	// no hay lineas de carga: puede que aun no empezo o que
										// el RPT no sea el nuestro -> no bloquear el limpiado
		if (UltimoRestoringFile(actual) != "")
			return CARGA_EN_CURSO;		// hay un archivo abierto sin su "items loaded"
		return CARGA_TERMINADA;
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

		// EL MOD SE ACTUALIZO: el marcador lo dejo otra build. Una build nueva puede traer
		// justo el arreglo que faltaba, asi que la escalada arranca DE CERO en vez de quedar
		// clavada en "agotado" por lo que no pudo la version vieja. (Paso en produccion: el
		// server llego a etapa 4 con la build anterior y la nueva -que ya tenia cuarentena-
		// entro directo al mensaje de rendirse sin llegar a usarla nunca.)
		if (etapa > 0 && s_BuildDelMarcador != "" && s_BuildDelMarcador != ExorStorageConstants.MOD_BUILD)
		{
			Print(string.Format("%1 BootRepair: el mod se actualizo (%2 -> %3) -> la escalada empieza de nuevo", ExorStorageConstants.LOG, s_BuildDelMarcador, ExorStorageConstants.MOD_BUILD));
			etapa = 1;
		}

		if (etapa == 0)
		{
			// DETECCION TEMPRANA (23-jul): no esperar a la etapa 3 para saltear las neveras.
			// Una nevera corrupta NO siempre mata el arranque de una: el del 23-jul 01:25
			// "termino" de cargar (con 1 item failed y el stream de dynamic_007 partido), se
			// fue a 21 GB de RAM y recien ahi murio. Como ese arranque no cuenta como "murio
			// cargando", la escalada no avanzaba y al siguiente reinicio volvia a pasar lo
			// mismo. Si el arranque ANTERIOR dejo el rastro de una nevera corrupta en el RPT,
			// este arranque no carga el cargo de las neveras y listo.
			// Dura UN SOLO arranque: el RPT de este arranque lleva la marca "nevera SALTEADA",
			// que ContieneNeveraCorrupta reconoce como autoinfligida -> el proximo arranque las
			// carga normal, ya con la persistencia reescrita limpia por el guardado de este.
			// Y el contenido no se pierde: el mueble vive virtualizado en su JSON, que no pasa
			// por el stream del engine.
			// MEDIDO (test local 23-jul, con la MISMA nevera corrupta que produccion, en
			// [8111.2, 7742.4]): arranque cargando neveras -> 3 "Serious stream damage",
			// 1 "String CORRUPTED", 1 item failed. Mismo arranque salteandolas -> 0 y 0.
			if (UltimoArranqueAcusaNevera())
			{
				s_SaltearNeveras = true;
				Print("[3xorVO] ============================================================");
				Print(string.Format("%1 BootRepair: el arranque anterior logueo una NEVERA corrupta -> este arranque NO carga el cargo de las neveras", ExorStorageConstants.LOG));
				Print(string.Format("%1 BootRepair: no se toca ningun archivo de persistencia (bases, carpas y loot intactos) y el contenido sigue en el JSON de cada mueble", ExorStorageConstants.LOG));
				Print("[3xorVO] ============================================================");
			}
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
			// PASO BARATO ANTES DE PERDER UNA CELDA DEL MAPA: si el que revienta es un mueble
			// del mod (nevera), no hace falta tirar el archivo entero -donde ademas viven
			// bases, carpas y loot de otra gente-. Alcanza con no cargar el cargo de las
			// neveras en ESTE arranque: el server levanta y solo se sacrifica lo que hubiera
			// adentro de las neveras. Si el RPT no acusa a una nevera, se pasa de largo.
			if (AcusaNevera())
			{
				s_SaltearNeveras = true;
				Print(string.Format("%1 BootRepair: el RPT acusa a una NEVERA corrupta -> este arranque NO carga el contenido de las neveras", ExorStorageConstants.LOG));
				Print(string.Format("%1 BootRepair: se pierde lo que hubiera DENTRO de las neveras, pero NO se toca ningun archivo (bases, carpas y loot intactos)", ExorStorageConstants.LOG));
			}
			else
			{
				Print(string.Format("%1 BootRepair: el RPT no acusa a una nevera -> paso directo a la cuarentena del archivo", ExorStorageConstants.LOG));
				Cuarentena();
			}
		}
		else if (etapa <= 6)
		{
			// Cuarentena, hasta 3 archivos: cada pasada aparta el que mata el arranque. Si tras
			// sacar uno muere en otro, la siguiente pasada saca ese. Con tope, para no ir
			// comiendose el mapa entero en un bucle.
			Cuarentena();
		}
		else
		{
			Print(string.Format("%1 BootRepair: AGOTADO. Ni los respaldos, ni saltear las neveras, ni la cuarentena levantaron el server.", ExorStorageConstants.LOG));
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

	// El arranque que murio, acuso a una NEVERA? El engine loguea la entidad concreta cuando
	// no puede deserializar su inventario:
	//    !!! Corrupted inventory "Exor_Fridge:19765"
	//    Scripted variables corrupted upon "Exor_Fridge".
	// Si aparece, sabemos que el problema es una nevera y alcanza con no cargar su contenido:
	// no hay que sacrificar el archivo de persistencia entero.
	static bool AcusaNevera()
	{
		array<string> rpts = ListarArchivos("$profile:", "*.RPT");
		if (rpts.Count() == 0)
			return false;
		OrdenarDesc(rpts);

		int mirados = 0;
		int i;
		for (i = 0; i < rpts.Count(); i++)
		{
			if (mirados >= 4)
				break;
			// solo miran los RPT que tienen carga de persistencia (el del arranque actual no)
			if (UltimoRestoringFile("$profile:" + rpts.Get(i)) == "")
				continue;
			mirados++;
			if (ContieneNeveraCorrupta("$profile:" + rpts.Get(i)))
			{
				Print(string.Format("%1 BootRepair: segun '%2', el que revienta es una nevera", ExorStorageConstants.LOG, rpts.Get(i)));
				return true;
			}
			return false;	// ese arranque murio pero NO por una nevera
		}
		return false;
	}

	// El arranque ANTERIOR (haya sobrevivido o no) dejo rastro de una nevera corrupta?
	// Se diferencia de AcusaNevera() en que NO exige que ese arranque haya muerto cargando:
	// justamente el caso que se nos escapaba era una nevera que rompia el stream sin matar
	// el arranque en el acto. Se mira UN solo RPT: el mas nuevo que ya haya cargado
	// persistencia (el del arranque actual todavia no cargo, asi que se descarta solo).
	static bool UltimoArranqueAcusaNevera()
	{
		array<string> rpts = ListarArchivos("$profile:", "*.RPT");
		if (rpts.Count() == 0)
			return false;
		OrdenarDesc(rpts);
		int i;
		for (i = 0; i < rpts.Count(); i++)
		{
			if (i >= 4)		// no escanear el historial entero
				break;
			string p = "$profile:" + rpts.Get(i);
			if (!TieneCargaDePersistencia(p))
				continue;
			if (ContieneNeveraCorrupta(p))
			{
				Print(string.Format("%1 BootRepair: '%2' tiene rastro de nevera corrupta", ExorStorageConstants.LOG, rpts.Get(i)));
				return true;
			}
			return false;	// el arranque anterior esta limpio
		}
		return false;
	}

	// el RPT llego a restaurar persistencia? (sirve para descartar el RPT del arranque actual,
	// que en el momento de OnInit todavia no tiene ninguna linea de carga)
	static bool TieneCargaDePersistencia(string rptPath)
	{
		FileHandle fh = OpenFile(rptPath, FileMode.READ);
		if (fh == 0)
			return false;
		bool found = false;
		string linea;
		while (FGets(fh, linea) >= 0)
		{
			if (linea.IndexOf("Restoring file") >= 0)
			{
				found = true;
				break;
			}
		}
		CloseFile(fh);
		return found;
	}

	// OJO CON EL BUCLE (visto en el test local 23-jul): cuando NOSOTROS salteamos las neveras,
	// el OnStoreLoad devuelve false sin consumir el stream y el motor loguea igual su
	// "Scripted variables corrupted upon Exor_Fridge" por CADA nevera salteada. O sea: el
	// arranque en el que aplicamos la cura deja el MISMO rastro que el arranque enfermo. Si no
	// se distingue, el arranque siguiente lo lee como corrupcion nueva, vuelve a saltear, y las
	// neveras no cargan NUNCA MAS.
	// Por eso, si el RPT tiene nuestra linea de "nevera SALTEADA", sus lineas de corrupcion son
	// autoinfligidas -> ese arranque NO acusa a nadie y el proximo carga las neveras normal
	// (que es justo lo que queremos: probar de nuevo con la persistencia ya reescrita limpia).
	static bool ContieneNeveraCorrupta(string rptPath)
	{
		FileHandle fh = OpenFile(rptPath, FileMode.READ);
		if (fh == 0)
			return false;
		bool found = false;
		bool salteadas = false;
		string linea;
		while (FGets(fh, linea) >= 0)
		{
			if (linea.IndexOf("nevera SALTEADA") >= 0)
			{
				salteadas = true;
				break;		// arranque con la cura puesta -> no sirve como acusacion
			}
			if (linea.IndexOf("Exor_Fridge") < 0)
				continue;
			if (linea.IndexOf("Corrupted inventory") >= 0)
			{
				found = true;
				continue;	// seguir leyendo: puede aparecer despues un "nevera SALTEADA"
			}
			if (linea.IndexOf("corrupted upon") >= 0)
				found = true;
		}
		CloseFile(fh);
		if (salteadas)
			return false;
		return found;
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
