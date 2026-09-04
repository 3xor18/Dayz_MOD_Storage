// ============================================================================
// 3xor_Vanilla_Optimization - Chat custom: ruteo server-side (SOLO server)
// Recibe el mensaje del emisor + canal y lo manda a los destinatarios:
//   GLOBAL (0) -> todos los players.
//   ZONA   (1) -> solo los players dentro de radio_zona_metros del emisor.
// Cada mensaje lleva dur/max para que el cliente no necesite la config.
// ============================================================================
class ExorChatServer
{
	// Anti-spam (en memoria, se reinicia con el server): por steamid el ultimo
	// momento que escribio + el ultimo texto (para bloquear repetidos).
	static ref map<string, int> s_LastMs;
	static ref map<string, string> s_LastText;

	// PURGA. Estos dos mapas crecen con CADA jugador distinto que escribio alguna vez y no
	// se limpiaban nunca: en un server con rotacion alta son miles de entradas vivas hasta
	// el reinicio. Como la entrada solo sirve para el cooldown y el anti-repetido (segundos),
	// todo lo mas viejo que 'ttlMs' es basura garantizada. La llama ExorHousekeeping.
	static void PurgarViejos(int now, int ttlMs)
	{
		if (!s_LastMs)
			return;
		array<string> muertas = new array<string>;
		foreach (string sid, int ms : s_LastMs)
		{
			if (now - ms > ttlMs)
				muertas.Insert(sid);
		}
		int i;
		for (i = 0; i < muertas.Count(); i++)
		{
			s_LastMs.Remove(muertas.Get(i));
			if (s_LastText)
				s_LastText.Remove(muertas.Get(i));
		}
	}

	static void Ensure()
	{
		if (!s_LastMs)
			s_LastMs = new map<string, int>;
		if (!s_LastText)
			s_LastText = new map<string, string>;
	}

	// ------------------------------------------------------------------------
	//  COMANDOS DE ADMIN POR CHAT
	// ------------------------------------------------------------------------
	// Se eligio el chat como disparador porque ya existe, ya llega al server autenticado
	// con la identidad del jugador, y no hace falta ninguna UI nueva. La autorizacion es la
	// lista de admins del mod, la misma que usa el candado de autos.
	static void Comando(PlayerBase quien, string linea)
	{
		if (!quien || !quien.GetIdentity())
			return;
		string sid = quien.GetIdentity().GetPlainId();
		bool esAdmin = GetExorConfig().carlock.ExorEsAdmin(sid);
		if (!esAdmin)
		{
			ExorCfgStorage st = GetExorConfig().storage;
			if (st && st.bypass_lootear_steamids && st.bypass_lootear_steamids.Find(sid) >= 0)
				esAdmin = true;
		}
		if (!esAdmin)
		{
			ExorMuebleRules.SendRed(quien, "Ese comando es solo para admins.");
			Print(string.Format("%1 SEGURIDAD: %2 intento el comando '%3' sin ser admin", ExorStorageConstants.LOG, sid, linea));
			return;
		}

		array<string> arg = new array<string>;
		linea.Split(" ", arg);
		string cmd = arg.Get(0);
		cmd.ToLower();

		if (cmd == "/perf")
		{
			int frames = 300;
			if (arg.Count() > 1)
				frames = arg.Get(1).ToInt();
			ExorProfiler.Iniciar(frames, quien);
			return;
		}
		if (cmd == "/entidades")
		{
			ExorMuebleRules.SendVerde(quien, ExorProfiler.ResumenEntidades());
			return;
		}
		if (cmd == "/carga")
		{
			int cuantos = 20;
			int items = 200;
			if (arg.Count() > 1)
				cuantos = arg.Get(1).ToInt();
			if (arg.Count() > 2)
				items = arg.Get(2).ToInt();
			ExorLoadTest.Generar(quien, cuantos, items);
			return;
		}
		if (cmd == "/limpiarcarga")
		{
			ExorLoadTest.Limpiar(quien);
			return;
		}
		ExorMuebleRules.SendVerde(quien, "Comandos: /perf [frames] | /carga [cant] [items] | /limpiarcarga | /entidades");
	}

	static void Handle(PlayerBase sender, string text, int channel)
	{
		if (!GetGame() || !GetGame().IsServer())
			return;
		if (!sender || !sender.GetIdentity())
			return;

		ExorCfgChat cfg = GetExorConfig().chat;
		if (!cfg.habilitado)
			return;

		// Sanitizar el texto (largo maximo; vacio se descarta)
		string msgTxt = text;
		if (msgTxt == "")
			return;
		if (msgTxt.Length() > 120)
			msgTxt = msgTxt.Substring(0, 120);

		// ----------------- COMANDOS DE ADMIN -----------------
		// Un mensaje que empieza con "/" no es chat: es un comando. Se resuelve ANTES del
		// anti-spam (un admin midiendo no tiene por que esperar el cooldown) y NUNCA se
		// reenvia a los demas jugadores.
		// Solo para los steamids de admin del mod: un comando que perfila el server o crea
		// 200 contenedores no puede quedar al alcance de cualquiera.
		if (msgTxt.IndexOf("/") == 0)
		{
			ExorChatServer.Comando(sender, msgTxt);
			return;
		}

		// ----------------- anti-spam -----------------
		Ensure();
		string sid = sender.GetIdentity().GetPlainId();
		int now = GetGame().GetTime();

		int last;
		if (cfg.cooldown_segundos > 0 && s_LastMs.Find(sid, last))
		{
			if (now - last < cfg.cooldown_segundos * 1000)
			{
				sender.MessageImportant("Esperá un momento antes de volver a escribir.");
				return;
			}
		}

		string lastTxt;
		if (cfg.bloquear_repetidos && s_LastText.Find(sid, lastTxt) && lastTxt == msgTxt)
		{
			sender.MessageImportant("No repitas el mismo mensaje.");
			return;
		}

		s_LastMs.Set(sid, now);
		s_LastText.Set(sid, msgTxt);

		int ch = channel;
		if (ch != 1)
			ch = 0;

		ExorChatMsg msg = new ExorChatMsg();
		msg.name = sender.GetIdentity().GetName();
		msg.text = msgTxt;
		msg.channel = ch;
		msg.dur = cfg.duracion_segundos;
		msg.max = cfg.max_lineas;
		msg.chars = cfg.max_caracteres_por_linea;
		msg.maxlin = cfg.max_lineas_por_mensaje;

		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(msg, false, data);
		Param1<string> p = new Param1<string>(data);

		vector spos = sender.GetPosition();
		float radio = cfg.radio_zona_metros;

		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase pb = PlayerBase.Cast(players.Get(i));
			if (!pb || !pb.GetIdentity())
				continue;
			if (ch == 1 && vector.Distance(pb.GetPosition(), spos) > radio)
				continue;	// zona: fuera del radio
			pb.RPCSingleParam(ExorRPC.CHAT_MSG, p, true, pb.GetIdentity());
		}

		// Eco al log del server (util para moderacion)
		Print(string.Format("%1 CHAT[%2] %3: %4", ExorStorageConstants.LOG, ExorChatChannelTag(ch), msg.name, msgTxt));
	}

	static string ExorChatChannelTag(int ch)
	{
		if (ch == 1)
			return "ZONA";
		return "GLOBAL";
	}
}
