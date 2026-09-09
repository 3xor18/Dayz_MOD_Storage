// ============================================================================
// 3xor_Vanilla_Optimization - Preferencia de sexo del personaje (SOLO server)
// ============================================================================
// POR QUE EXISTE ESTO (9-sep-2026): antes el hub de spawn cambiaba de sexo
// REEMPLAZANDO la entidad (CreatePlayer + SelectPlayer + borrar el cuerpo viejo).
// Andaba toda la sesion, pero el motor liga el slot de personaje de la base de datos
// a la entidad que EL devuelve desde OnClientNewEvent: el personaje creado a mano
// nunca entra en ese vinculo, asi que el guardado que cubre a los que estan CONECTADOS
// seguia apuntando al cuerpo viejo (que ademas ya se habia borrado) y no escribia nada
// valido. Resultado: el que estaba online cuando el server reiniciaba volvia como
// freshie en el spawn, con todo su loot perdido (reportado el 9-sep: 3 jugadores en
// un solo reinicio).
//
// La unica forma sana de darle otro sexo a un jugador es que el personaje lo cree el
// MOTOR con el tipo que queremos, o sea en MissionServer.CreateCharacter (ver
// ExorStorage_Mission.c). Ahi el personaje queda registrado y persiste como cualquiera.
// Como ese momento es ANTES de que el jugador vea el hub, la eleccion se GUARDA aca
// por steamid y se aplica DESDE LA PROXIMA APARICION.
class ExorGeneroPrefRow
{
	string steamid;
	int genero;	// 0 = hombre, 1 = mujer
}

class ExorGeneroPrefFile
{
	ref array<ref ExorGeneroPrefRow> rows;
	void ExorGeneroPrefFile() { rows = new array<ref ExorGeneroPrefRow>; }
}

class ExorGeneroPref
{
	static ref map<string, ref ExorGeneroPrefRow> s_Rows;
	static bool s_Loaded;

	static void EnsureLoaded()
	{
		if (s_Loaded)
			return;
		s_Loaded = true;
		s_Rows = new map<string, ref ExorGeneroPrefRow>;
		if (!FileExist(ExorStorageConstants.GENERO_FILE))
			return;
		ExorGeneroPrefFile f = new ExorGeneroPrefFile();
		JsonFileLoader<ExorGeneroPrefFile>.JsonLoadFile(ExorStorageConstants.GENERO_FILE, f);
		if (!f.rows)
			return;
		int i;
		for (i = 0; i < f.rows.Count(); i++)
		{
			ExorGeneroPrefRow r = f.rows.Get(i);
			if (r && r.steamid != "")
				s_Rows.Set(r.steamid, r);
		}
	}

	static void Save()
	{
		if (!s_Loaded)
			return;
		ExorGeneroPrefFile f = new ExorGeneroPrefFile();
		foreach (string k, ExorGeneroPrefRow r : s_Rows)
			f.rows.Insert(r);
		JsonFileLoader<ExorGeneroPrefFile>.JsonSaveFile(ExorStorageConstants.GENERO_FILE, f);
	}

	// -1 = no tiene preferencia guardada (se respeta el personaje que eligio en su cliente).
	static int Leer(string sid)
	{
		EnsureLoaded();
		ExorGeneroPrefRow r;
		if (sid != "" && s_Rows.Find(sid, r) && r)
		{
			if (r.genero == 1)
				return 1;
			return 0;
		}
		return -1;
	}

	// Guarda la eleccion del hub. Escribe a disco solo si cambio algo: esto lo llama
	// CADA spawn de CADA jugador, y el JSON no tiene por que reescribirse cada vez.
	static void Guardar(string sid, int genero)
	{
		if (sid == "" || genero < 0 || genero > 1)
			return;
		EnsureLoaded();
		ExorGeneroPrefRow r;
		if (s_Rows.Find(sid, r) && r && r.genero == genero)
			return;	// ya estaba asi
		if (!r)
		{
			r = new ExorGeneroPrefRow();
			r.steamid = sid;
			s_Rows.Set(sid, r);
		}
		r.genero = genero;
		Save();
	}

	// Tipo de personaje con el que el MOTOR tiene que crear a este jugador. Devuelve el
	// que venia (el que armo en su cliente) si la feature esta apagada, si no tiene
	// preferencia guardada o si ya coincide con lo que pidio.
	static string TipoParaLogin(PlayerIdentity identity, string tipoOriginal)
	{
		if (!identity)
			return tipoOriginal;
		if (!GetExorConfig().spawns.elegir_genero)
			return tipoOriginal;

		int quiere = Leer(identity.GetPlainId());
		if (quiere < 0)
			return tipoOriginal;
		if (quiere == ExorSpawn.GeneroDeTipo(tipoOriginal))
			return tipoOriginal;

		string nuevo = GetExorConfig().spawns.TipoAlAzar(quiere == 1);
		if (nuevo == "")
			return tipoOriginal;	// sin tipos usables en spawns.json: no romper el login

		string etiqueta = "hombre";
		if (quiere == 1)
			etiqueta = "mujer";
		Print(string.Format("%1 SPAWN: %2 aparece como %3 por su preferencia (%4 -> %5)", ExorStorageConstants.LOG, identity.GetPlainId(), etiqueta, tipoOriginal, nuevo));
		return nuevo;
	}
}
