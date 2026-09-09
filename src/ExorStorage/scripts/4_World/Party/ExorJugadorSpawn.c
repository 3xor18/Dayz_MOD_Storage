// ============================================================================
// 3xor_Vanilla_Optimization - Estado de spawn por jugador (SOLO server)
// ============================================================================
// Guarda DOS cosas por steamid, las dos usadas en el momento en que el MOTOR crea
// el personaje (MissionServer.CreateCharacter):
//
//   genero  el sexo que el jugador eligio en la pantalla de muerte (-1 = nunca eligio,
//           se respeta el personaje que le toque).
//   visto   si este steamid ya jugo alguna vez en este server. Sirve para distinguir
//           "primer login de mi vida" de "reaparicion despues de morir": las dos pasan
//           por OnClientNewEvent y el motor no las diferencia. Al que entra por primera
//           vez se lo manda derecho a un punto de spawns.json, SIN pantalla de eleccion
//           (no murio, no vio nada, y abrirle un menu en su primer login es justo donde
//           mas cosas pueden salir mal).
//
// POR QUE EL SEXO SE APLICA ACA Y NO CAMBIANDO EL PERSONAJE (9-sep-2026): antes el hub
// de spawn cambiaba de sexo REEMPLAZANDO la entidad (CreatePlayer + SelectPlayer + borrar
// el cuerpo viejo). Andaba toda la sesion, pero el motor liga el slot de personaje de la
// base de datos a la entidad que EL devuelve desde OnClientNewEvent: el personaje creado
// a mano nunca entra en ese vinculo, asi que el guardado que cubre a los CONECTADOS
// seguia apuntando al cuerpo viejo (ya borrado) y no escribia nada valido. El que estaba
// online cuando el server reiniciaba volvia como freshie en el spawn y perdia todo
// (3 jugadores en el reinicio del 9-sep 01:00). La unica forma sana es que el personaje
// nazca ya del sexo pedido, y por eso la eleccion se pide ANTES, mientras esta muerto.
class ExorJugadorSpawnRow
{
	string steamid;
	int genero;	// 0 = hombre, 1 = mujer, -1 = sin preferencia
	bool visto;	// ya jugo alguna vez en este server
}

class ExorJugadorSpawnFile
{
	ref array<ref ExorJugadorSpawnRow> rows;
	void ExorJugadorSpawnFile() { rows = new array<ref ExorJugadorSpawnRow>; }
}

class ExorJugadorSpawn
{
	static ref map<string, ref ExorJugadorSpawnRow> s_Rows;
	static bool s_Loaded;

	static void EnsureLoaded()
	{
		if (s_Loaded)
			return;
		s_Loaded = true;
		s_Rows = new map<string, ref ExorJugadorSpawnRow>;
		if (!FileExist(ExorStorageConstants.JUGADORES_FILE))
			return;
		ExorJugadorSpawnFile f = new ExorJugadorSpawnFile();
		JsonFileLoader<ExorJugadorSpawnFile>.JsonLoadFile(ExorStorageConstants.JUGADORES_FILE, f);
		if (!f.rows)
			return;
		int i;
		for (i = 0; i < f.rows.Count(); i++)
		{
			ExorJugadorSpawnRow r = f.rows.Get(i);
			if (r && r.steamid != "")
				s_Rows.Set(r.steamid, r);
		}
	}

	static void Save()
	{
		if (!s_Loaded)
			return;
		ExorJugadorSpawnFile f = new ExorJugadorSpawnFile();
		foreach (string k, ExorJugadorSpawnRow r : s_Rows)
			f.rows.Insert(r);
		JsonFileLoader<ExorJugadorSpawnFile>.JsonSaveFile(ExorStorageConstants.JUGADORES_FILE, f);
	}

	static ExorJugadorSpawnRow Fila(string sid, bool crear)
	{
		EnsureLoaded();
		ExorJugadorSpawnRow r;
		if (s_Rows.Find(sid, r) && r)
			return r;
		if (!crear)
			return null;
		r = new ExorJugadorSpawnRow();
		r.steamid = sid;
		r.genero = -1;
		r.visto = false;
		s_Rows.Set(sid, r);
		return r;
	}

	// true = este steamid nunca jugo aca. Se consulta ANTES de marcarlo (ver el orden en
	// MissionServer.OnClientNewEvent).
	static bool EsPrimerLogin(string sid)
	{
		if (sid == "")
			return false;	// sin identidad no se decide nada raro: se trata como habitual
		ExorJugadorSpawnRow r = Fila(sid, false);
		return !r || !r.visto;
	}

	// Marca que ya jugo. Se llama tanto cuando entra con un personaje existente
	// (OnClientReadyEvent) como despues de crearle el primero.
	static void MarcarVisto(string sid)
	{
		if (sid == "")
			return;
		ExorJugadorSpawnRow r = Fila(sid, true);
		if (r.visto)
			return;	// ya estaba: no reescribir el JSON en cada login
		r.visto = true;
		Save();
	}

	// -1 = no tiene preferencia guardada (se respeta el personaje que le toque).
	static int LeerGenero(string sid)
	{
		if (sid == "")
			return -1;
		ExorJugadorSpawnRow r = Fila(sid, false);
		if (!r)
			return -1;
		if (r.genero == 1)
			return 1;
		if (r.genero == 0)
			return 0;
		return -1;
	}

	// Guarda la eleccion de la pantalla de muerte. Escribe a disco solo si cambio.
	static void GuardarGenero(string sid, int genero)
	{
		if (sid == "" || genero < 0 || genero > 1)
			return;
		ExorJugadorSpawnRow r = Fila(sid, true);
		if (r.genero == genero)
			return;
		r.genero = genero;
		Save();
	}

	// Tipo de personaje con el que el MOTOR tiene que crear a este jugador.
	//
	// 'pedidoCliente' es el sexo que el CLIENTE mando en su login data (0/1, -1 = nada).
	// Ese es el canal bueno: DayZ serializa el personaje elegido y lo manda SOLO al
	// reaparecer, asi que llega justo antes de que se cree el cuerpo. Un RPC no sirve para
	// esto: mientras el jugador esta muerto su entidad ya no vale y el mensaje se pierde
	// (probado el 9-sep: la pantalla se veia y el click nunca llegaba al server).
	//
	// Si el cliente no mando nada, se cae a lo ultimo que se le aplico (por si acaso).
	static string TipoParaLogin(string sid, string tipoOriginal, int pedidoCliente)
	{
		if (!GetExorConfig().spawns.elegir_genero)
			return tipoOriginal;

		int quiere = pedidoCliente;
		if (quiere < 0)
			quiere = LeerGenero(sid);
		if (quiere < 0)
			return tipoOriginal;

		GuardarGenero(sid, quiere);
		if (quiere == ExorSpawn.GeneroDeTipo(tipoOriginal))
			return tipoOriginal;

		string nuevo = GetExorConfig().spawns.TipoAlAzar(quiere == 1);
		if (nuevo == "")
			return tipoOriginal;	// sin tipos usables en spawns.json: no romper el login

		string etiqueta = "hombre";
		if (quiere == 1)
			etiqueta = "mujer";
		Print(string.Format("%1 SPAWN: %2 aparece como %3 por su eleccion (%4 -> %5)", ExorStorageConstants.LOG, sid, etiqueta, tipoOriginal, nuevo));
		return nuevo;
	}

}

// Cache del CLIENTE con el sexo que se va a usar al reaparecer. Se llena leyendo el
// propio personaje que el cliente tiene elegido y lo cambia el click de la pantalla de
// muerte. -1 = todavia sin resolver.
class ExorGeneroClient
{
	static int s_Sel = -1;
}

// Eleccion que el jugador hizo EN LA PANTALLA DE MUERTE y que todavia no se pudo mandar.
// El sexo viaja solo (va en el login data), pero el punto de spawn y el equipo VIP si
// necesitan un RPC, y estando muerto no hay entidad valida para mandarlo. Entonces se
// guarda aca y se manda apenas el personaje nuevo esta vivo (ver MissionGameplay.OnUpdate).
class ExorSpawnPend
{
	static int s_Idx = -1;		// indice REAL del punto elegido (-1 = no eligio)
	static bool s_Equip = false;	// pidio el equipamiento VIP
	static bool s_Enviado = false;	// ya se mando al server para esta vida

	static void Reset()
	{
		s_Idx = -1;
		s_Equip = false;
		s_Enviado = false;
	}

	// Arranque de una muerte NUEVA: limpia lo de la vida anterior y deja marcada una zona
	// por defecto, para que el que aprieta REAPARECER sin tocar nada igual caiga en una
	// zona del server. Se elige la primera SIN cooldown; si todas estan en cooldown, la
	// primera igual (el server la va a rechazar y avisar, pero no se pierde la eleccion).
	static void NuevaMuerte()
	{
		Reset();
		ExorSpawnMenuDTO dto = ExorSpawnClient.s_DTO;
		if (!dto || !dto.nombres)
			return;
		int i;
		int primero = -1;
		for (i = 0; i < dto.nombres.Count(); i++)
		{
			int real = i;
			if (dto.punto_idx && i < dto.punto_idx.Count())
				real = dto.punto_idx.Get(i);
			if (primero < 0)
				primero = real;
			int cd = 0;
			if (dto.punto_cd_seg && i < dto.punto_cd_seg.Count())
				cd = dto.punto_cd_seg.Get(i);
			if (cd <= 0)
			{
				s_Idx = real;
				return;
			}
		}
		s_Idx = primero;
	}
}
