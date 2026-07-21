// ============================================================================
// 3xor_Vanilla_Optimization - CANARY de carga de neveras (auto-recuperacion)
// ============================================================================
// Una nevera con inventario CORRUPTO crashea el server al cargar su cargo (el motor
// intenta reservar memoria basura -> pico -> "Out of memory" enganoso; la RAM real
// usada era 3.5/12 GB). No se puede detectar cual es la corrupta desde script (los
// scripted vars cargan bien; el cargo lo lee el MOTOR dentro de super.OnStoreLoad,
// y el crash ocurre AHI DENTRO, antes de que podamos escribir nada DESPUES de super).
//
// POR QUE FALLO el canary por POSICION (v2.10.5): en una entidad PERSISTIDA la posicion
// NO esta disponible antes de super.OnStoreLoad (EEInit corre con la pos aun en 0,0,0;
// la posicion se restaura DENTRO de super). -> PosKey daba "" -> el canary nunca se armaba
// -> crasheaba SIEMPRE en la misma nevera. Confirmado con 2 arranques que murieron
// identico en "Exor_Fridge:12769".
//
// TECNICA CORREGIDA (por SECUENCIA, disponible SIEMPRE antes de super):
//   - Un contador estatico se resetea a 0 en cada arranque (se reinicia la VM de script).
//   - En OnStoreLoad de CADA nevera, ANTES de super, se pide el proximo numero (1,2,3,...)
//     -> es el N-esimo fridge en cargar. El orden de carga es DETERMINISTA entre arranques
//     mientras la data no cambie (se lee secuencial de dynamic_XXX.bin).
//   - Antes de super se escribe ese N al archivo canary. Si el server SOBREVIVE la carga
//     completa, el manager BORRA el canary al 1er tick.
//   - Si el server CRASHEA cargando el cargo de la nevera N, el canary queda con N.
//   - Al proximo arranque, la nevera que saque el numero N ve N en el canary -> se DESCARTA
//     (return false, sin llamar a super) -> el motor NO carga su cargo corrupto -> ARRANCA.
//     El self-heal la recrea vacia. Las demas neveras cargan normal (con su contenido).
// Aisla SOLO la nevera corrupta, automaticamente. SOLO server.
// ============================================================================
class ExorFridgeCanary
{
	// contador de neveras cargadas en ESTE arranque. Estatico -> se reinicia a 0 cada boot
	// (nueva VM de script). No depende de posicion (que no existe antes de super).
	static int s_Seq = 0;

	// siguiente numero de secuencia para la nevera que esta por cargar (1,2,3,...).
	static int NextSeq()
	{
		s_Seq = s_Seq + 1;
		return s_Seq;
	}

	static string PathFor()
	{
		return ExorStorageConstants.CONFIG_DIR + "\\fridge_loading.txt";
	}

	// numero de la nevera cuyo cargo se estaba cargando (o -1 si ninguno / se borro).
	static int ReadSeq()
	{
		string path = PathFor();
		if (!FileExist(path))
			return -1;
		FileHandle fh = OpenFile(path, FileMode.READ);
		if (fh == 0)
			return -1;
		string line = "";
		FGets(fh, line);
		CloseFile(fh);
		line.Trim();
		if (line == "")
			return -1;
		return line.ToInt();
	}

	static void WriteSeq(int n)
	{
		if (!FileExist(ExorStorageConstants.CONFIG_DIR))
			MakeDirectory(ExorStorageConstants.CONFIG_DIR);
		FileHandle fh = OpenFile(PathFor(), FileMode.WRITE);
		if (fh == 0)
			return;
		FPrintln(fh, n.ToString());
		CloseFile(fh);
	}

	static void Clear()
	{
		string path = PathFor();
		if (FileExist(path))
			DeleteFile(path);
	}
}
