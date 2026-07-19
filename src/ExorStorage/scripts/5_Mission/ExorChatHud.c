// ============================================================================
// 3xor_Vanilla_Optimization - Chat custom (cliente)
// Panel transparente abajo-izquierda. Mensajes de JUGADOR: "[canal] nombre: texto"
// (nombre azul, texto blanco). Mensajes del SERVER (kind=1): texto VERDE, SIN
// remitente. Los mensajes largos se PARTEN en hasta 3 lineas (misma cant. de
// caracteres por linea); las lineas de continuacion van indentadas y con un
// espacio de grupo un poco menor para que se lean como el MISMO comentario.
// El input reusa la caja vanilla (ChatInputMenu) pero el envio lo interceptamos.
// ============================================================================
class ExorChatEntry
{
	string name;
	string text;
	int channel;
	int kind;        // 0 = jugador, 1 = server (verde, sin remitente)
	int expireMs;
	ref TStringArray lines;   // el texto ya partido en <= MAX_LINES trozos
}

// Una linea VISUAL ya lista para pintar (segmentos con su color + X de arranque).
class ExorChatVisLine
{
	ref TStringArray segs;
	ref array<int> cols;
	float startX;
	bool isHeader;   // 1ra linea (arriba) de su entrada -> arriba de ella va el gap de grupo
}

class ExorChatHud
{
	protected Widget m_Root;
	protected bool m_Tried;
	protected ref array<TextWidget> m_Segs;
	protected ref array<ref ExorChatEntry> m_Entries;

	const int ROWS = 9;          // slots de linea VISUAL disponibles
	const int SEGS = 4;          // segmentos de texto por slot
	const int MAX_LINES = 3;     // fallback: maximo de lineas por mensaje (si el DTO viene sin config)
	const int MAX_CHARS = 55;    // fallback: caracteres por linea (si el DTO viene sin config)
	const float LEFT_M = 18;     // margen izquierdo
	const float CONT_INDENT = 0; // sin indent: la continuacion arranca alineada (una linea mas abajo, estilo chat)
	const float BOTTOM_M = 150;  // separacion del borde inferior
	const float LINE_H = 30;     // alto de anclaje de cada linea
	const float CONT_STEP = 27;  // separacion vertical entre lineas del MISMO mensaje (juntas)
	const float MSG_STEP = 44;   // separacion vertical entre mensajes DISTINTOS (mas aire para agrupar)

	void Create()
	{
		if (m_Tried)
			return;
		m_Tried = true;

		m_Root = GetGame().GetWorkspace().CreateWidgets("ExorStorage/gui/exor_chat.layout");
		if (!m_Root)
			return;

		m_Segs = new array<TextWidget>;
		int i, j;
		for (i = 0; i < ROWS; i++)
			for (j = 0; j < SEGS; j++)
				m_Segs.Insert(TextWidget.Cast(m_Root.FindAnyWidget("ExorChR" + i.ToString() + "S" + j.ToString())));

		m_Entries = new array<ref ExorChatEntry>;
	}

	// Parte 'text' en <= maxLines trozos cortando en espacios. La PRIMERA linea usa
	// 'firstMax' caracteres y las SIGUIENTES 'restMax' (asi, en mensajes con prefijo
	// "[G] nombre: ", la 1ra linea descuenta ese prefijo y TODAS las lineas quedan del
	// mismo ancho total que un mensaje del server sin prefijo). Si sobra, la ultima "…".
	static TStringArray WrapText(string text, int firstMax, int restMax, int maxLines)
	{
		TStringArray res = new TStringArray;
		int n = text.Length();
		if (n == 0)
		{
			res.Insert("");
			return res;
		}
		int start = 0;
		while (start < n && res.Count() < maxLines)
		{
			int lim;
			if (res.Count() == 0)
				lim = firstMax;
			else
				lim = restMax;
			if (lim < 6)
				lim = 6;

			int remaining = n - start;
			if (remaining <= lim)
			{
				res.Insert(text.Substring(start, remaining));
				start = n;
				break;
			}
			// buscar el ultimo espacio dentro de la ventana [start, start+lim]
			int breakAt = -1;
			int i;
			for (i = start + lim; i > start; i--)
			{
				if (text.Substring(i, 1) == " ")
				{
					breakAt = i;
					break;
				}
			}
			int lineLen;
			int nextStart;
			if (breakAt <= start)	// palabra mas larga que la linea -> corte duro
			{
				lineLen = lim;
				nextStart = start + lim;
			}
			else
			{
				lineLen = breakAt - start;
				nextStart = breakAt + 1;	// saltar el espacio
			}
			res.Insert(text.Substring(start, lineLen));
			start = nextStart;
		}
		// quedo texto sin mostrar (mas de maxLines) -> marcar con "…"
		if (start < n && res.Count() > 0)
		{
			string last = res.Get(res.Count() - 1);
			if (last.Length() > 1)
				last = last.Substring(0, last.Length() - 1);
			res.Set(res.Count() - 1, last + "…");
		}
		if (res.Count() == 0)
			res.Insert(text);
		return res;
	}

	void Add(ExorChatMsg dto)
	{
		if (!m_Root || !dto)
			return;

		ExorChatEntry e = new ExorChatEntry();
		e.name = dto.name;
		e.text = dto.text;
		e.channel = dto.channel;
		e.kind = dto.kind;
		// ancho de linea y max de lineas vienen de la config (chat.json), por mensaje.
		// Fallback a los defaults si el server manda 0 (compat con DTOs viejos).
		int chars = dto.chars;
		if (chars <= 0)
			chars = MAX_CHARS;
		int maxlin = dto.maxlin;
		if (maxlin <= 0)
			maxlin = MAX_LINES;
		if (maxlin > ROWS)
			maxlin = ROWS;
		// FORMATO UNIFORME player y server: el TEXTO se parte a 'chars' caracteres por
		// linea (todas las lineas igual) y hasta 'maxlin' lineas. El prefijo "[G] nombre:"
		// del jugador queda ANTES del texto en la 1ra linea (no se descuenta): asi ambos
		// tipos usan el mismo maximo de caracteres por linea y el mismo maximo de lineas.
		e.lines = WrapText(dto.text, chars, chars, maxlin);
		int dur = dto.dur;
		if (dur <= 0)
			dur = 20;
		e.expireMs = GetGame().GetTime() + (dur * 1000);

		m_Entries.InsertAt(e, 0);	// la mas nueva primero (va abajo)

		int cap = dto.max;
		if (cap > ROWS)
			cap = ROWS;
		if (cap < 1)
			cap = 1;
		while (m_Entries.Count() > cap)
			m_Entries.Remove(m_Entries.Count() - 1);	// se va la mas vieja

		Render();
	}

	// Cada frame: drena la cola (3_Game) y saca las lineas vencidas.
	void Update()
	{
		if (!m_Root)
			return;

		if (ExorChatQueue.s_Pending && ExorChatQueue.s_Pending.Count() > 0)
		{
			int k;
			for (k = 0; k < ExorChatQueue.s_Pending.Count(); k++)
				Add(ExorChatQueue.s_Pending.Get(k));
			ExorChatQueue.s_Pending.Clear();
		}

		if (m_Entries.Count() == 0)
			return;

		int now = GetGame().GetTime();
		bool changed = false;
		int i = m_Entries.Count() - 1;
		while (i >= 0)
		{
			if (now >= m_Entries.Get(i).expireMs)
			{
				m_Entries.Remove(i);
				changed = true;
			}
			i--;
		}
		if (changed)
			Render();
	}

	// Arma la lista de lineas VISUALES (de abajo hacia arriba): por cada entrada, de
	// la mas nueva a la mas vieja, sus lineas en orden inverso (ultima abajo, header
	// arriba). Se corta al llenar los ROWS slots.
	array<ref ExorChatVisLine> BuildVisLines()
	{
		int cBlue  = ARGB(255, 90, 150, 255);   // nombre
		int cWhite = ARGB(255, 240, 240, 240);  // texto jugador
		int cGreen = ARGB(255, 80, 230, 90);    // texto del SERVER
		int cRed   = ARGB(255, 235, 70, 70);    // aviso/error (kind=2)
		int cTagG  = ARGB(255, 150, 150, 165);  // tag global
		int cTagZ  = ARGB(255, 90, 220, 120);   // tag zona

		array<ref ExorChatVisLine> vis = new array<ref ExorChatVisLine>;
		int ei;
		for (ei = 0; ei < m_Entries.Count() && vis.Count() < ROWS; ei++)
		{
			ExorChatEntry e = m_Entries.Get(ei);
			int nl = e.lines.Count();
			int li;
			for (li = nl - 1; li >= 0 && vis.Count() < ROWS; li--)
			{
				ExorChatVisLine v = new ExorChatVisLine();
				v.segs = new TStringArray;
				v.cols = new array<int>;
				v.isHeader = (li == 0);

				if (li == 0)
				{
					if (e.kind == 1)
					{
						// SERVER: verde, sin remitente
						v.startX = LEFT_M;
						v.segs.Insert(e.lines.Get(0)); v.cols.Insert(cGreen);
					}
					else if (e.kind == 2)
					{
						// AVISO/ERROR: rojo, sin remitente
						v.startX = LEFT_M;
						v.segs.Insert(e.lines.Get(0)); v.cols.Insert(cRed);
					}
					else
					{
						// JUGADOR: [tag] nombre: texto
						string tag = "[G] ";
						int tagCol = cTagG;
						if (e.channel == 1) { tag = "[Z] "; tagCol = cTagZ; }
						v.startX = LEFT_M;
						v.segs.Insert(tag);            v.cols.Insert(tagCol);
						v.segs.Insert(e.name);         v.cols.Insert(cBlue);
						v.segs.Insert(": ");           v.cols.Insert(cWhite);
						v.segs.Insert(e.lines.Get(0)); v.cols.Insert(cWhite);
					}
				}
				else
				{
					// linea de continuacion: indentada, mismo color del cuerpo
					int bodyCol = cWhite;
					if (e.kind == 1)
						bodyCol = cGreen;
					else if (e.kind == 2)
						bodyCol = cRed;
					v.startX = LEFT_M + CONT_INDENT;
					v.segs.Insert(e.lines.Get(li)); v.cols.Insert(bodyCol);
				}
				vis.Insert(v);
			}
		}
		return vis;
	}

	void Render()
	{
		float sw, sh;
		m_Root.GetScreenSize(sw, sh);

		array<ref ExorChatVisLine> vis = BuildVisLines();

		// posicionar de abajo hacia arriba con espaciado acumulado (gap de grupo entre
		// mensajes distintos: se agrega DESPUES de pintar la header de una entrada).
		float yUp = BOTTOM_M;
		int slot;
		for (slot = 0; slot < ROWS; slot++)
		{
			if (slot < vis.Count())
			{
				ExorChatVisLine v = vis.Get(slot);
				float y = sh - yUp - LINE_H;
				PaintSlot(slot, v, y);
				// paso chico dentro del MISMO mensaje (lineas juntas); paso grande al pasar
				// a un mensaje distinto (la header de una entrada -> arriba va otro mensaje).
				if (v.isHeader)
					yUp = yUp + MSG_STEP;
				else
					yUp = yUp + CONT_STEP;
			}
			else
			{
				HideSlot(slot);
			}
		}
	}

	void PaintSlot(int slot, ExorChatVisLine v, float y)
	{
		int count = v.segs.Count();
		float x = v.startX;
		int j;
		int w, h;
		for (j = 0; j < SEGS; j++)
		{
			TextWidget seg = m_Segs.Get(slot * SEGS + j);
			if (!seg)
				continue;
			if (j < count)
			{
				seg.SetText(v.segs.Get(j));
				seg.SetColor(v.cols.Get(j));
				seg.Show(true);
				seg.SetPos(x, y);
				seg.GetTextSize(w, h);
				x = x + w;
			}
			else
			{
				seg.Show(false);
			}
		}
	}

	void HideSlot(int slot)
	{
		int j;
		for (j = 0; j < SEGS; j++)
		{
			if (m_Segs.Get(slot * SEGS + j))
				m_Segs.Get(slot * SEGS + j).Show(false);
		}
	}
}

// ===========================================================================
// Intercepta la caja de chat VANILLA: reusa el input, pero el envio lo ruteamos
// por NUESTRO sistema con el canal actual. Vaciamos el texto antes de super para
// que el chat vanilla NO mande nada (reemplazo total).
// ===========================================================================
modded class ChatInputMenu
{
	override Widget Init()
	{
		Widget r = super.Init();
		// Mostrar NUESTRO canal en la cajita en vez de "Direct".
		TextWidget ct = TextWidget.Cast(layoutRoot.FindAnyWidget("ChannelText"));
		if (ct)
			ct.SetText(ExorChat.ChannelName());
		return r;
	}

	override bool OnChange(Widget w, int x, int y, bool finished)
	{
		if (finished)
		{
			EditBoxWidget eb = EditBoxWidget.Cast(layoutRoot.FindAnyWidget("InputEditBoxWidget"));
			if (eb)
			{
				string text = eb.GetText();
				if (text != "")
				{
					PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
					if (p)
						p.ExorReqChat(text, ExorChat.s_Channel);
				}
				eb.SetText("");	// vaciar -> vanilla no envia ni muestra nada
			}
		}
		return super.OnChange(w, x, y, finished);
	}
}
