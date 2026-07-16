// ============================================================================
// 3xor_Vanilla_Optimization - Mensajes del SERVER en el chat (SOLO server)
// Manda mensajes automaticos al chat de los players (verde, sin remitente):
//   - REPETIBLES: cada X minutos, arrancando N min despues del reinicio.
//   - AGENDADOS: por dia de semana + ventana horaria (hora_inicio..hora_fin),
//     con placeholder (ej. "##") que se reemplaza por los minutos que faltan
//     hasta hora_fin (cuenta regresiva). Se repiten cada 'repetir_en_minutos'.
// La hora usa off_set_horas (server UTC -> hora local) reusando ExorCofre.NowLocal.
// Config: mensajes.json en el profile. Se crea con defaults al 1er arranque y NO
// se sobreescribe (se puede editar con el server parado; al reiniciar lo toma).
// ============================================================================

// ---- structs de config (nombres de campo = llaves del JSON, tal cual) ----
class ExorMsgRepetible
{
	string texto = "";
}

class ExorMsgAgendado
{
	string texto = "";
	string placeholder = "";
	ref TStringArray dias_semana_repetir;
	string hora_inicio = "";
	string hora_fin = "";
	bool   repetible = true;
	int    repetir_en_minutos = 5;

	void ExorMsgAgendado()
	{
		dias_semana_repetir = new TStringArray;
	}
}

class ExorCfgMensajes
{
	bool activado = true;
	int  off_set_horas = -4;
	int  off_set_dias = 0;
	int  segundos_entre_mensajes = 4;   // separacion MINIMA entre un mensaje y el siguiente (no salen todos de golpe)
	int  enviar_mensajes_repetibles_minutos_despues_de_reinicar_server = 5;
	int  tomar_y_enviar_un_mensaje_repetible_cada_tantos_minutos = 60;  // cada X min agarra UNO random del array de repetibles y lo manda
	bool enviar_mensajes_repetibles = true;
	bool enviar_mensajes_agendados = true;
	ref array<ref ExorMsgRepetible> mensajes_repetibles;
	ref array<ref ExorMsgAgendado> mensajes_agendados;

	void ExorCfgMensajes()
	{
		mensajes_repetibles = new array<ref ExorMsgRepetible>;
		mensajes_agendados = new array<ref ExorMsgAgendado>;
	}

	ExorMsgRepetible AddRep(string texto)
	{
		ExorMsgRepetible r = new ExorMsgRepetible();
		r.texto = texto;
		mensajes_repetibles.Insert(r);
		return r;
	}

	ExorMsgAgendado AddAge(string texto, string ph, string ini, string fin, int cadaMin)
	{
		ExorMsgAgendado a = new ExorMsgAgendado();
		a.texto = texto;
		a.placeholder = ph;
		a.hora_inicio = ini;
		a.hora_fin = fin;
		a.repetible = true;
		a.repetir_en_minutos = cadaMin;
		mensajes_agendados.Insert(a);
		return a;
	}

	void CreateDefault()
	{
		activado = true;
		off_set_horas = -4;
		off_set_dias = 0;
		segundos_entre_mensajes = 4;
		enviar_mensajes_repetibles_minutos_despues_de_reinicar_server = 5;
		tomar_y_enviar_un_mensaje_repetible_cada_tantos_minutos = 60;
		enviar_mensajes_repetibles = true;
		enviar_mensajes_agendados = true;

		AddRep("Bienvenido al server");
		AddRep("Hacer Alianzas es Baneable");

		ExorMsgAgendado a1 = AddAge("Sever se reinicia en ## minutos", "##", "11:50", "12:00", 5);
		a1.dias_semana_repetir.Insert("lunes"); a1.dias_semana_repetir.Insert("martes");
		a1.dias_semana_repetir.Insert("miercoles"); a1.dias_semana_repetir.Insert("jueves");
		a1.dias_semana_repetir.Insert("viernes"); a1.dias_semana_repetir.Insert("sabado");
		a1.dias_semana_repetir.Insert("domingo");

		// countdown al INICIO del raid (sabado, cuenta hasta las 20:00)
		ExorMsgAgendado a2 = AddAge("Horari de Raids inicia en ## minutos", "##", "19:29", "20:00", 5);
		a2.dias_semana_repetir.Insert("sabado");

		// RAID ACTIVO: una sola ventana que CRUZA MEDIANOCHE -> sabado 20:00 hasta domingo 02:40
		ExorMsgAgendado a3 = AddAge("Raid Activo, recordad: Sin alianzas, solo entrar a bases raidenado puertas y sin saltar muros, no puedes destruir items que no te lleves o dejar items en el piso en la base raideada", "", "20:00", "02:40", 10);
		a3.dias_semana_repetir.Insert("sabado");

		// countdown al FIN del raid (domingo de madrugada, cuenta hasta las 03:00)
		ExorMsgAgendado a4 = AddAge("Horari de Raids Finaliza en ## minutos", "##", "02:40", "03:00", 5);
		a4.dias_semana_repetir.Insert("domingo");

		// RAID FINALIZADO (domingo de madrugada, 03:00 a 06:00)
		ExorMsgAgendado a5 = AddAge("Horario de Raid Finalizado, salirse de la base q esten raidenado y no matar a lso miembros d la base para permitir que construyan", "", "03:00", "06:00", 10);
		a5.dias_semana_repetir.Insert("domingo");
	}
}

class ExorServerMsg
{
	static ref ExorCfgMensajes s_Cfg;
	static int s_RepLastMs;              // ultimo envio (ms uptime) de UN repetible (timer global, no por-mensaje)
	static ref array<int> s_AgeLastMs;   // ultimo envio (ms uptime) por mensaje agendado
	static ref array<int> s_AgeStartM;   // hora_inicio ya parseada a minutos (1 sola vez al cargar, no en cada tick)
	static ref array<int> s_AgeEndM;     // hora_fin ya parseada a minutos
	static ref array<string> s_Queue;    // cola de salida: los mensajes se mandan de a uno, espaciados
	static bool s_Draining;              // hay un drenaje de cola en curso
	static const int TICK_MS = 20000;    // chequeo cada 20s (resolucion buena para minutos)

	static void Start()
	{
		if (!GetGame() || !GetGame().IsServer())
			return;

		Load();

		if (!s_Cfg.activado)
		{
			Print(string.Format("%1 Mensajes del server: DESACTIVADO (mensajes.json activado=false)", ExorStorageConstants.LOG));
			return;
		}

		s_RepLastMs = -1;
		int i;
		s_AgeLastMs = new array<int>;
		s_AgeStartM = new array<int>;
		s_AgeEndM = new array<int>;
		for (i = 0; i < s_Cfg.mensajes_agendados.Count(); i++)
		{
			s_AgeLastMs.Insert(-1);
			ExorMsgAgendado a = s_Cfg.mensajes_agendados.Get(i);
			int sm = ExorCofre.ParseHHMM(a.hora_inicio);
			int em = ExorCofre.ParseHHMM(a.hora_fin);
			if (sm >= 1440) sm = sm - 1440;   // "24:00" -> 0
			if (em >= 1440) em = em - 1440;
			s_AgeStartM.Insert(sm);
			s_AgeEndM.Insert(em);
		}

		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Tick, TICK_MS, true);
		Print(string.Format("%1 Mensajes del server iniciado: %2 repetibles, %3 agendados (offset %4h)",
			ExorStorageConstants.LOG, s_Cfg.mensajes_repetibles.Count(), s_Cfg.mensajes_agendados.Count(), s_Cfg.off_set_horas));
	}

	// Carga mensajes.json. Si no existe, lo crea con defaults. Si existe, respeta lo
	// que haya (no lo sobreescribe con defaults; solo re-guarda para agregar llaves nuevas).
	static void Load()
	{
		s_Cfg = new ExorCfgMensajes();
		if (FileExist(ExorStorageConstants.CFG_MENSAJES))
			JsonFileLoader<ExorCfgMensajes>.JsonLoadFile(ExorStorageConstants.CFG_MENSAJES, s_Cfg);
		else
			s_Cfg.CreateDefault();
		JsonFileLoader<ExorCfgMensajes>.JsonSaveFile(ExorStorageConstants.CFG_MENSAJES, s_Cfg);
	}

	static void Tick()
	{
		if (!GetGame() || !GetGame().IsServer() || !s_Cfg || !s_Cfg.activado)
			return;

		// sin players online no hay a quien mandar: se saltea todo el procesamiento.
		// (los repetibles arrancan de nuevo cuando entra alguien; los agendados se
		//  reevaluan al toque en el 1er tick con gente).
		if (ExorVO_Manager.s_PopCount <= 0)
			return;

		int uptime = GetGame().GetTime();
		int i;
		int interval;   // Enforce = scope de FUNCION: se declaran 1 sola vez y se reusan en ambos loops
		int last;

		// ------------------- REPETIBLES: uno RANDOM del array cada X min -------------------
		if (s_Cfg.enviar_mensajes_repetibles && s_Cfg.mensajes_repetibles.Count() > 0)
		{
			int startDelay = s_Cfg.enviar_mensajes_repetibles_minutos_despues_de_reinicar_server * 60000;
			interval = s_Cfg.tomar_y_enviar_un_mensaje_repetible_cada_tantos_minutos * 60000;
			if (interval <= 0)
				interval = 60000;
			if (uptime >= startDelay && (s_RepLastMs < 0 || uptime - s_RepLastMs >= interval))
			{
				int idx = Math.RandomInt(0, s_Cfg.mensajes_repetibles.Count());   // 0..count-1
				Enqueue(s_Cfg.mensajes_repetibles.Get(idx).texto);
				s_RepLastMs = uptime;
			}
		}

		// ------------------- AGENDADOS -------------------
		if (s_Cfg.enviar_mensajes_agendados && s_Cfg.mensajes_agendados.Count() > 0)
		{
			int minOfDay, weekday, dayKey;
			ExorCofre.NowLocal(s_Cfg.off_set_horas, minOfDay, weekday, dayKey);
			int wd = (weekday + s_Cfg.off_set_dias) % 7;
			if (wd < 0)
				wd = wd + 7;
			int wdYest = (wd - 1 + 7) % 7;	// dia de AYER (para ventanas que cruzan medianoche)
			string todayName = ExorCofre.DayName(wd);
			string yestName = ExorCofre.DayName(wdYest);

			for (i = 0; i < s_Cfg.mensajes_agendados.Count(); i++)
			{
				ExorMsgAgendado a = s_Cfg.mensajes_agendados.Get(i);

				bool dayToday = (a.dias_semana_repetir && a.dias_semana_repetir.Find(todayName) >= 0);
				bool dayYest  = (a.dias_semana_repetir && a.dias_semana_repetir.Find(yestName) >= 0);
				int startM = s_AgeStartM.Get(i);   // ya parseado en Start() (no en cada tick)
				int endM = s_AgeEndM.Get(i);
				if (startM < 0 || endM < 0)
					continue;

				// minsToEnd = minutos que faltan hasta hora_fin (para el placeholder ##).
				bool inWin = false;
				int minsToEnd = 0;
				if (startM < endM)
				{
					// ventana NORMAL: empieza y termina el MISMO dia
					if (dayToday && minOfDay >= startM && minOfDay < endM)
					{
						inWin = true;
						minsToEnd = endM - minOfDay;
					}
				}
				else
				{
					// ventana que CRUZA MEDIANOCHE (ej. sabado 20:00 -> 03:00): el dia
					// configurado es el de INICIO; termina en la madrugada del dia siguiente.
					if (dayToday && minOfDay >= startM)
					{
						inWin = true;                        // parte 1: la noche del dia de inicio
						minsToEnd = (1440 - minOfDay) + endM;
					}
					else if (dayYest && minOfDay < endM)
					{
						inWin = true;                        // parte 2: madrugada del dia siguiente
						minsToEnd = endM - minOfDay;
					}
				}

				if (!inWin)
				{
					s_AgeLastMs.Set(i, -1);   // fuera de ventana: reset (reingreso -> manda de una)
					continue;
				}

				interval = a.repetir_en_minutos * 60000;
				if (interval <= 0)
					interval = 60000;
				last = s_AgeLastMs.Get(i);

				bool due = false;
				if (last < 0)
					due = true;                                         // 1er tick dentro de la ventana
				else if (a.repetible && (uptime - last >= interval))
					due = true;                                         // repeticion dentro de la ventana

				if (due)
				{
					string txt = a.texto;
					if (a.placeholder != "")
					{
						if (minsToEnd < 0)
							minsToEnd = 0;
						txt.Replace(a.placeholder, minsToEnd.ToString());
					}
					Enqueue(txt);
					s_AgeLastMs.Set(i, uptime);
				}
			}
		}
	}

	// Encola un mensaje para salir espaciado (no todos de golpe). Si no hay un drenaje
	// en curso, arranca uno (manda el 1ro al instante y programa los siguientes).
	static void Enqueue(string text)
	{
		if (text == "")
			return;
		if (!s_Queue)
			s_Queue = new array<string>;
		s_Queue.Insert(text);
		if (!s_Draining)
		{
			s_Draining = true;
			Drain();
		}
	}

	// Saca UNO de la cola, lo manda, y programa el siguiente a 'segundos_entre_mensajes'.
	static void Drain()
	{
		if (!s_Queue || s_Queue.Count() == 0)
		{
			s_Draining = false;
			return;
		}
		string t = s_Queue.Get(0);
		s_Queue.Remove(0);
		Broadcast(t);

		int gap = 4000;
		if (s_Cfg)
			gap = s_Cfg.segundos_entre_mensajes * 1000;
		if (gap < 500)
			gap = 500;
		GetGame().GetCallQueue(CALL_CATEGORY_SYSTEM).CallLater(Drain, gap, false);
	}

	// Manda 'text' al chat de TODOS como mensaje del SERVER (verde, sin remitente).
	static void Broadcast(string text)
	{
		if (text == "")
			return;

		ExorCfgChat chatCfg = GetExorConfig().chat;

		ExorChatMsg msg = new ExorChatMsg();
		msg.name = "";
		msg.text = text;
		msg.channel = 0;
		msg.kind = 1;	// SERVER: verde, sin remitente
		msg.dur = chatCfg.duracion_segundos;
		if (msg.dur <= 0)
			msg.dur = 20;
		msg.max = chatCfg.max_lineas;
		msg.chars = chatCfg.max_caracteres_por_linea;
		msg.maxlin = chatCfg.max_lineas_por_mensaje;

		JsonSerializer js = new JsonSerializer();
		string data;
		js.WriteToString(msg, false, data);
		Param1<string> p = new Param1<string>(data);

		array<Man> players = new array<Man>;
		GetGame().GetPlayers(players);
		int i;
		for (i = 0; i < players.Count(); i++)
		{
			PlayerBase pb = PlayerBase.Cast(players.Get(i));
			if (pb && pb.GetIdentity())
				pb.RPCSingleParam(ExorRPC.CHAT_MSG, p, true, pb.GetIdentity());
		}

		Print(string.Format("%1 SERVERMSG: %2", ExorStorageConstants.LOG, text));
	}
}
