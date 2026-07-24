// ============================================================================
// 3xor_Vanilla_Optimization - ESTADO del candado de autos (SOLO server)
// ============================================================================
// El candado de un auto se guarda en un JSON APARTE, keyed por un id estable del
// auto (CarLocks/<id>.json). NO se toca el stream de persistencia del auto salvo
// por UN ancla (el id), que se escribe con lectura OPCIONAL -> retro-compatible:
// un auto guardado por una version sin candado simplemente no tiene ese id, se le
// genera uno la 1ra vez que se le pone candado, y nada se corrompe.
//
// Por que aparte y no en el stream del auto: agregar/quitar campos del stream de
// una entidad rompe la carga de lo guardado por otra version ("String CORRUPTED")
// -lo aprendimos con la nevera-. Un JSON aparte no tiene ese problema: leer de mas
// o de menos es imposible, cada campo es por nombre.
//
// El "desbloqueado por" (quien ya metio la clave) NO se guarda aca: es runtime
// (se resetea al reiniciar, cada miembro re-ingresa una vez), igual que los lockers.
//
// ANCLA = POSICION, no el stream del auto. Aprendido HOY en el test local: agregar aunque
// sea UN campo al OnStoreSave/OnStoreLoad de CarScript corrompe los autos ya guardados
// ("Scripted variables corrupted upon FordRaptor...") -exactamente el bug de la nevera-.
// Por eso NO se toca el stream. El auto SIEMPRE carga en la posicion en que se guardo, asi
// que la clave del archivo es tipo+posicion redondeada: se escribe en OnStoreSave (posicion
// actual) y se lee en AfterStoreLoad (misma posicion). El archivo lleva el id estable del
// auto para que la virtualizacion siga ligada. Colision (2 autos del mismo tipo a <1m) es
// practicamente imposible para autos de clan estacionados.
// ============================================================================
class ExorCarLockFile
{
	string	car_id;			// id estable del auto (continuidad con la virtualizacion)
	string	clave;			// codigo del candado ("" = sin candado)
	string	setter_sid;		// steamid del que puso la clave (para avisos / kick-cleanup)
	string	group_id;		// clan dueño al momento de poner la clave (para permisos)
	// steamids que YA ingresaron la clave. Se persiste para NO re-pedirla en cada reconexion /
	// reinicio (una vez que la metiste, el auto te recuerda). Al conectarte, el server te
	// devuelve el acceso (grant) de estos autos.
	ref TStringArray unlocked_by;

	void ExorCarLockFile()
	{
		unlocked_by = new TStringArray;
	}
}

class ExorCarLockStore
{
	static void EnsureDir()
	{
		if (!FileExist(ExorStorageConstants.CARLOCK_DIR))
			MakeDirectory(ExorStorageConstants.CARLOCK_DIR);
	}

	// clave de archivo = tipo + posicion redondeada a metros. El auto carga donde se guardo,
	// asi que la misma pos da la misma clave en save y en load.
	static string KeyFor(string type, vector pos)
	{
		return string.Format("%1_%2_%3", type, Math.Round(pos[0]), Math.Round(pos[2]));
	}

	static string PathFor(string key)
	{
		return string.Format("%1\\%2.json", ExorStorageConstants.CARLOCK_DIR, key);
	}

	static bool Existe(string key)
	{
		if (key == "")
			return false;
		return FileExist(PathFor(key));
	}

	// carga el estado del candado en esa clave; null si no hay candado ahi.
	static ExorCarLockFile Load(string key)
	{
		if (key == "" || !FileExist(PathFor(key)))
			return null;
		ExorCarLockFile f = new ExorCarLockFile();
		JsonFileLoader<ExorCarLockFile>.JsonLoadFile(PathFor(key), f);
		if (f.clave == "")
			return null;	// archivo vacio = sin candado
		return f;
	}

	static void Save(string key, string carId, string clave, string setterSid, string groupId, TStringArray unlockedBy)
	{
		if (key == "")
			return;
		EnsureDir();
		ExorCarLockFile f = new ExorCarLockFile();
		f.car_id = carId;
		f.clave = clave;
		f.setter_sid = setterSid;
		f.group_id = groupId;
		if (unlockedBy)
		{
			int i;
			for (i = 0; i < unlockedBy.Count(); i++)
				f.unlocked_by.Insert(unlockedBy.Get(i));
		}
		JsonFileLoader<ExorCarLockFile>.JsonSaveFile(PathFor(key), f);
	}

	// saca el candado (raid exitoso, o el dueño lo quita): borra el archivo.
	static void Clear(string key)
	{
		if (key == "")
			return;
		string p = PathFor(key);
		if (FileExist(p))
			DeleteFile(p);
	}
}
