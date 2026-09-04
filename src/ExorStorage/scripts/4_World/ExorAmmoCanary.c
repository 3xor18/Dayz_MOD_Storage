// ============================================================================
// 3xor_Vanilla_Optimization - CANARIO DE LECTURA DE CARTUCHOS (SOLO server)
// ============================================================================
// QUE PROBLEMA CUBRE
// ----------------------------------------------------------------------------
// Para no perder la municion REAL de un cargador (que las perforantes vuelvan
// perforantes y no normales), el mod lee sus cartuchos uno por uno con
// Magazine.GetCartridgeAtIndex. Esa llamada es NATIVA, y en el banco de pruebas se
// la vio tirar ACCESS_VIOLATION -o sea matar el server entero- leyendo un cargador
// recien creado. Una excepcion adentro de una llamada nativa NO se puede atrapar
// desde script: no hay try/catch que valga, el proceso se muere.
//
// Como no se puede atrapar, se detecta DESPUES: se deja una marca en disco justo
// antes de la primera lectura de la sesion y se borra apenas vuelve. Si al arrancar
// la marca sigue ahi, la sesion anterior murio exactamente ahi -> en ESTE arranque
// no se leen cartuchos.
//
// QUE SE PIERDE CON EL MODO DEGRADADO: los cargadores se guardan solo con su
// CANTIDAD de balas, asi que al restaurarlos vuelven con la municion por defecto de
// su classname (una perforante puede volver normal). NO se pierde ningun item, ni
// una sola bala de cantidad. Es exactamente el comportamiento que el mod tenia
// antes de guardar el detalle. Un server degradado y andando es mejor que uno que
// se cae en cada guardado.
//
// La marca se borra sola al apagar limpio, asi que el modo degradado dura UN
// arranque: si fue una casualidad, el siguiente vuelve a intentar. Si se repite, el
// admin lo va a ver en el RPT ("MODO DEGRADADO") y queda el rastro para investigar.
// Es el mismo patron que ya se usa con las neveras (ver ExorFridgeCanary).
// ============================================================================
class ExorAmmoCanary
{
	static bool s_Deshabilitado;	// este arranque NO lee cartuchos
	static bool s_YaProbado;		// ya se hizo (y sobrevivio) la primera lectura de la sesion

	static string Path()
	{
		return ExorStorageConstants.CONFIG_DIR + "\\ammo_canary.txt";
	}

	// La llama el arranque, ANTES de que se guarde o restaure ningun contenedor.
	static void Init()
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		s_YaProbado = false;
		if (FileExist(Path()))
		{
			s_Deshabilitado = true;
			DeleteFile(Path());	// se limpia: el proximo arranque vuelve a intentar
			Print(string.Format("%1 MODO DEGRADADO DE MUNICION: el arranque anterior murio leyendo los cartuchos de un cargador.", ExorStorageConstants.LOG));
			Print(string.Format("%1   -> en esta sesion los cargadores se guardan solo con su CANTIDAD de balas.", ExorStorageConstants.LOG));
			Print(string.Format("%1   -> no se pierde ningun item; lo unico que puede cambiar es el TIPO de bala al restaurar.", ExorStorageConstants.LOG));
			return;
		}
		s_Deshabilitado = false;
	}

	// true = se pueden leer los cartuchos. La primera vez de la sesion deja la marca en
	// disco: si el motor se cae adentro de la lectura, la marca queda y el proximo arranque
	// entra en modo degradado.
	static bool PuedeLeer()
	{
		if (s_Deshabilitado)
			return false;
		if (!s_YaProbado)
		{
			s_YaProbado = true;
			FileHandle fh = OpenFile(Path(), FileMode.WRITE);
			if (fh != 0)
			{
				FPrintln(fh, "leyendo cartuchos por primera vez en esta sesion");
				CloseFile(fh);
			}
		}
		return true;
	}

	static bool s_Confirmado;

	// La llama el serializer apenas la primera lectura VOLVIO viva. Toca disco UNA sola vez
	// por sesion: esto corre por cada cargador de cada contenedor que se guarda, asi que un
	// FileExist aca seria I/O en pleno camino caliente.
	static void MarcarOk()
	{
		if (s_Confirmado)
			return;
		s_Confirmado = true;
		if (FileExist(Path()))
			DeleteFile(Path());
	}
}
