// ============================================================================
// 3xor_Vanilla_Optimization - CANARY de carga de neveras (auto-recuperacion)
// ============================================================================
// Una nevera con inventario CORRUPTO crashea el server al cargar su cargo (el motor
// intenta reservar memoria basura -> pico -> "Out of memory" enganoso; la RAM real
// usada era 3.5/12 GB). No se puede detectar cual es la corrupta desde script (los
// scripted vars cargan bien; el cargo lo lee el MOTOR despues de OnStoreLoad).
//
// TECNICA (auto-repara sin intervencion, sirve en cualquier server):
//   - En OnStoreLoad de CADA nevera, ANTES de que el motor cargue su cargo, se escribe
//     el id de esa nevera a un archivo "canary".
//   - Si el server sobrevive la carga (llega al 1er tick), se BORRA el canary.
//   - Si el server CRASHEA cargando el cargo de una nevera, el canary queda con SU id.
//   - Al proximo arranque, esa nevera ve su id en el canary -> se DESCARTA (return false)
//     -> el motor NO carga su cargo corrupto -> el server ARRANCA. El self-heal la recrea
//     vacia en su posicion. Las demas neveras cargan normal (con su contenido).
// Aisla SOLO la nevera corrupta, automaticamente. SOLO server.
// ============================================================================
class ExorFridgeCanary
{
	static string PathFor()
	{
		return ExorStorageConstants.CONFIG_DIR + "\\fridge_loading.txt";
	}

	// clave por POSICION (x_z redondeados): identifica una nevera de forma estable entre
	// arranques (la posicion persiste igual). "" si la posicion es invalida (0,0).
	static string PosKey(vector p)
	{
		if (p[0] == 0 && p[2] == 0)
			return "";
		return Math.Round(p[0]).ToString() + "_" + Math.Round(p[2]).ToString();
	}

	// id de la nevera cuyo cargo se esta cargando ahora (o "" si ninguna / se borro).
	static string Read()
	{
		string path = PathFor();
		if (!FileExist(path))
			return "";
		FileHandle fh = OpenFile(path, FileMode.READ);
		if (fh == 0)
			return "";
		string line = "";
		FGets(fh, line);
		CloseFile(fh);
		line.Trim();
		return line;
	}

	static void Write(string id)
	{
		if (id == "")
			return;
		if (!FileExist(ExorStorageConstants.CONFIG_DIR))
			MakeDirectory(ExorStorageConstants.CONFIG_DIR);
		FileHandle fh = OpenFile(PathFor(), FileMode.WRITE);
		if (fh == 0)
			return;
		FPrintln(fh, id);
		CloseFile(fh);
	}

	static void Clear()
	{
		string path = PathFor();
		if (FileExist(path))
			DeleteFile(path);
	}
}
