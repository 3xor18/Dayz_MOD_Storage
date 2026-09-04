// ============================================================================
// 3xor_Vanilla_Optimization - IDENTIDAD PERSISTENTE POR ENTERO (SOLO server)
// ============================================================================
// EL PROBLEMA QUE MATA ESTE ARCHIVO
// ----------------------------------------------------------------------------
// Hasta ahora cada contenedor guardaba su identidad y su clave como STRINGS dentro
// del stream de persistencia del engine (OnStoreSave/OnStoreLoad). Eso es lo que
// dejaba el server sin arrancar:
//
//   1. el engine no puede deserializar el inventario de una entidad y loguea
//      'Corrupted inventory "Exor_Fridge:14235"'. No crashea, pero deja el cursor
//      del stream corrido.
//   2. nuestro OnStoreLoad hace ctx.Read(string) sobre ese stream corrido.
//   3. el engine detecta el largo absurdo, escupe "!!! String CORRUPTED - FIX
//      OnStoreLoad()" y tira una VIRTUAL MACHINE EXCEPTION.
//   4. "Failed to read modstorage for entity Type=Exor_Fridge" -> el server NO ARRANCA.
//
// Y no se puede blindar desde script: la excepcion la tira el engine DENTRO del Read.
//
// LA SOLUCION
// ----------------------------------------------------------------------------
// El stream pasa a llevar SOLO DOS ENTEROS: un numero magico y un id numerico.
// Leer un int de un stream corrido devuelve basura, pero NUNCA lanza una excepcion.
// Y el magico permite darse cuenta: si no coincide, la entidad se descarta limpiamente
// (la recrea el self-heal) en vez de cargar con el id de OTRO contenedor, que seria
// mucho peor -dos contenedores compartiendo id = compartiendo contenido = duplicacion-.
//
// Todo lo demas -la clave del locker, quien la puso, quienes ya la ingresaron- se va a
// un JSON lateral indexado por ese id. Ahi puede crecer y cambiar de forma sin ningun
// riesgo, porque no pasa por el stream del engine.
//
// Esto elimina la familia de bugs entera, no un caso particular.
// ============================================================================

// ----------------------------------------------------------------------------
//  GENERADOR DE IDS
// ----------------------------------------------------------------------------
// Contador persistido en disco. Se prefiere un contador y no un random porque el id
// tiene que ser UNICO de verdad: dos contenedores con el mismo id comparten el archivo
// de contenido, o sea que uno se lleva el loot del otro.
class ExorPid
{
	// Numero magico al frente del stream. Es un valor arbitrario pero improbable: si lo
	// que leemos no es esto, el stream esta corrido y no hay que confiar en lo que sigue.
	static const int EXOR_MAGIC = 0x3E07A1;

	static int s_Next;
	static bool s_Cargado;

	static string PathContador()
	{
		return ExorStorageConstants.CONFIG_DIR + "\\next_pid.txt";
	}

	static void Cargar()
	{
		if (s_Cargado)
			return;
		s_Cargado = true;
		s_Next = 1;
		string path = PathContador();
		if (!FileExist(path))
			return;
		FileHandle fh = OpenFile(path, FileMode.READ);
		if (fh == 0)
			return;
		string line = "";
		FGets(fh, line);
		CloseFile(fh);
		line.Trim();
		int v = line.ToInt();
		if (v > 0)
			s_Next = v;
	}

	static void Guardar()
	{
		if (!FileExist(ExorStorageConstants.CONFIG_DIR))
			MakeDirectory(ExorStorageConstants.CONFIG_DIR);
		FileHandle fh = OpenFile(PathContador(), FileMode.WRITE);
		if (fh == 0)
			return;
		FPrintln(fh, s_Next.ToString());
		CloseFile(fh);
	}

	// Proximo id libre. Se persiste en el acto: si el server se cae justo despues de
	// entregar un id, el proximo arranque NO lo vuelve a entregar.
	static int Nuevo()
	{
		Cargar();
		int id = s_Next;
		s_Next = s_Next + 1;
		Guardar();
		return id;
	}

	// El pid leido del stream es creible? Un pid de 0 o negativo, o absurdamente grande,
	// solo puede venir de un stream corrido.
	static bool Plausible(int pid)
	{
		if (pid <= 0)
			return false;
		if (pid > 100000000)	// cien millones de contenedores: imposible
			return false;
		return true;
	}
}

// ----------------------------------------------------------------------------
//  ESTADO LATERAL DEL CONTENEDOR (clave del locker y quien la ingreso)
// ----------------------------------------------------------------------------
// Vive en su propio JSON, fuera del stream del engine. Agregarle campos a futuro es
// gratis y no puede romper la carga de la persistencia — que es exactamente lo que
// paso dos veces cuando estos datos estaban en el stream (v2.10.0 los agrego y rompio
// la migracion; v2.10.1 los saco y rompio la carga de lo que v2.10.0 habia guardado).
class ExorLockState
{
	string clave;			// clave del locker ("" = sin clave)
	string setter;			// steamid del que la puso
	ref TStringArray abiertos;	// steamids que ya la ingresaron (se resetea por sesion)

	void ExorLockState()
	{
		abiertos = new TStringArray;
	}
}

class ExorLockStore
{
	static string Dir()
	{
		return ExorStorageConstants.CONFIG_DIR + "\\lockstate";
	}

	static string PathFor(int pid)
	{
		return string.Format("%1\\%2.json", Dir(), pid);
	}

	static ExorLockState Cargar(int pid)
	{
		ExorLockState st = new ExorLockState();
		if (pid <= 0)
			return st;
		string path = PathFor(pid);
		if (!FileExist(path))
			return st;
		JsonFileLoader<ExorLockState>.JsonLoadFile(path, st);
		if (!st.abiertos)
			st.abiertos = new TStringArray;
		return st;
	}

	static void Guardar(int pid, ExorLockState st)
	{
		if (pid <= 0 || !st)
			return;
		// sin clave y sin nadie que la haya ingresado -> no hace falta archivo
		if (st.clave == "")
		{
			Borrar(pid);
			return;
		}
		if (!FileExist(ExorStorageConstants.CONFIG_DIR))
			MakeDirectory(ExorStorageConstants.CONFIG_DIR);
		if (!FileExist(Dir()))
			MakeDirectory(Dir());
		JsonFileLoader<ExorLockState>.JsonSaveFile(PathFor(pid), st);
	}

	static void Borrar(int pid)
	{
		string path = PathFor(pid);
		if (FileExist(path))
			DeleteFile(path);
	}
}
