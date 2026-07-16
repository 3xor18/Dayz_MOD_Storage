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

	static void Ensure()
	{
		if (!s_LastMs)
			s_LastMs = new map<string, int>;
		if (!s_LastText)
			s_LastText = new map<string, string>;
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
