// ============================================================================
// 3xor_Vanilla_Optimization - CLAVE de lockers: persistencia en JSON aparte
// ============================================================================
// La clave (+ quien la puso + quienes la desbloquearon) NO se guarda en el stream de
// persistencia del mueble (OnStoreSave/Load) porque agregar campos ahi rompe la carga de
// muebles guardados por una version previa (String CORRUPTED -> resetea el ID). En su lugar
// se guarda un JSON por locker, keyed por el ID del mueble (el mismo que usa la
// virtualizacion). Se lee en AfterStoreLoad y se reescribe en cada cambio. SOLO server.
// ============================================================================
class ExorLockKeyFile
{
	string key;						// "" = sin clave
	string setter;					// steamid del que la puso
	ref TStringArray unlocked;		// steamids que ya la ingresaron

	void ExorLockKeyFile()
	{
		unlocked = new TStringArray;
	}
}

class ExorLockKeyStore
{
	static string PathFor(string id)
	{
		return string.Format("%1\\lockkey_%2.json", ExorStorageConstants.STORAGE_DIR, id);
	}

	static void EnsureDir()
	{
		if (!FileExist(ExorStorageConstants.STORAGE_DIR))
			MakeDirectory(ExorStorageConstants.STORAGE_DIR);
	}

	// carga la clave de un locker (por su ID). Devuelve null si no tiene archivo (= sin clave).
	static ExorLockKeyFile Load(string id)
	{
		if (id == "")
			return null;
		string path = PathFor(id);
		if (!FileExist(path))
			return null;
		ExorLockKeyFile f = new ExorLockKeyFile();
		JsonFileLoader<ExorLockKeyFile>.JsonLoadFile(path, f);
		if (!f.unlocked)
			f.unlocked = new TStringArray;
		return f;
	}

	static void Save(string id, ExorLockKeyFile f)
	{
		if (id == "" || !f)
			return;
		EnsureDir();
		JsonFileLoader<ExorLockKeyFile>.JsonSaveFile(PathFor(id), f);
	}

	static void Delete(string id)
	{
		if (id == "")
			return;
		string path = PathFor(id);
		if (FileExist(path))
			DeleteFile(path);
	}
}
