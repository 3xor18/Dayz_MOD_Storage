// ============================================================================
//  AVISOS AL JUGADOR -> TODOS AL CHAT DEL MOD
// ----------------------------------------------------------------------------
//  Antes el mod hablaba por DOS canales distintos y el jugador tenia que mirar a dos
//  lugares: el chat del mod (mensajes del server, reglas de muebles, chat de la gente) y
//  el aviso vanilla 'MessageImportant', que sale en otra esquina, con otra tipografia y
//  otra duracion. Cosas del mismo tipo -"no podes construir aca", "party lleno", "ese
//  punto esta en cooldown"- caian en uno o en otro segun donde se hubiera escrito el
//  codigo, sin ningun criterio.
//
//  Este helper es el unico punto de salida: todo va al chat del mod, en verde si es
//  informativo y en rojo si es una negativa.
//
//  SIRVE DE LOS DOS LADOS, que es el motivo de que exista y no sea una linea suelta:
//   - en el SERVER manda el RPC CHAT_MSG al jugador;
//   - en el CLIENTE encola directo en ExorChatQueue, porque ahi no hay a quien mandarle
//     un RPC (el aviso ya esta en la maquina que lo tiene que ver).
//  Asi el mismo llamado funciona desde 4_World y desde 5_Mission sin pensarlo.
// ============================================================================
class ExorAviso
{
	static const int VERDE = 1;   // informativo
	static const int ROJO  = 2;   // negativa / error

	// Punto de entrada normal: el color lo decide el propio texto.
	//
	// La alternativa era clasificar a mano los ~60 avisos repartidos por diez archivos, y
	// eso se desincroniza en cuanto alguien agrega uno nuevo y se olvida. Con una sola
	// regla, en un solo lugar, un aviso nuevo sale bien sin que nadie se acuerde de nada.
	// El color es cosmetico: si algun caso raro sale del color equivocado no rompe nada,
	// y se corrige aca o forzandolo con Info()/Error().
	static void Enviar(PlayerBase p, string texto)
	{
		Mostrar(p, texto, EsNegativa(texto));
	}

	// Aviso informativo (verde), forzado.
	static void Info(PlayerBase p, string texto)
	{
		Mostrar(p, texto, VERDE);
	}

	// Negativa o error (rojo), forzado.
	static void Error(PlayerBase p, string texto)
	{
		Mostrar(p, texto, ROJO);
	}

	// "Te estoy diciendo que NO": arranca con una negacion, o con un "Solo ..." que en
	// este mod siempre introduce una restriccion ("Solo el lider puede invitar").
	static int EsNegativa(string texto)
	{
		string t = texto;
		t.ToLower();
		if (t.IndexOf("no ") == 0 || t.IndexOf("solo ") == 0 || t.IndexOf("espera") == 0)
			return ROJO;
		if (t.IndexOf(" no pod") >= 0 || t.IndexOf(" no se puede") >= 0 || t.IndexOf(" no ten") >= 0)
			return ROJO;
		if (t.IndexOf("prohib") >= 0 || t.IndexOf("lleno") >= 0 || t.IndexOf("cooldown") >= 0)
			return ROJO;
		return VERDE;
	}

	static void Mostrar(PlayerBase p, string texto, int kind)
	{
		if (!p || texto == "")
			return;

		ExorCfgChat cfg = GetExorConfig().chat;
		ExorChatMsg m = new ExorChatMsg();
		m.name = "";
		m.text = texto;
		m.channel = 0;
		m.dur = 8;
		m.max = cfg.max_lineas;
		m.kind = kind;
		m.chars = cfg.max_caracteres_por_linea;
		m.maxlin = cfg.max_lineas_por_mensaje;

		if (GetGame().IsServer())
		{
			if (!p.GetIdentity())
				return;
			JsonSerializer js = new JsonSerializer();
			string data;
			js.WriteToString(m, false, data);
			p.RPCSingleParam(ExorRPC.CHAT_MSG, new Param1<string>(data), true, p.GetIdentity());
			return;
		}

		// cliente: el aviso ya esta donde tiene que verse, no hay red de por medio
		ExorChatQueue.Enqueue(m);
	}
}
