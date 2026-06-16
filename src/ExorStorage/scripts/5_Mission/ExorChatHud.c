// ============================================================================
// 3xor_Vanilla_Optimization - Chat custom (cliente)
// Panel transparente abajo-izquierda: lineas "[canal] nombre: mensaje" con el
// NOMBRE en azul + negrita y el MENSAJE en blanco. Cada linea dura X seg (config)
// y se va; la mas nueva abajo. Posicion en pixeles absolutos (como el killfeed).
// El input reusa la caja vanilla (ChatInputMenu) pero el envio lo interceptamos
// para rutearlo por nuestro sistema con el canal elegido (tecla ".").
// ============================================================================
class ExorChatEntry
{
	string name;
	string text;
	int channel;
	int expireMs;
}

class ExorChatHud
{
	protected Widget m_Root;
	protected bool m_Tried;
	protected ref array<TextWidget> m_Segs;
	protected ref array<ref ExorChatEntry> m_Entries;

	const int ROWS = 9;
	const int SEGS = 4;
	const float LEFT_M = 18;     // margen izquierdo
	const float BOTTOM_M = 150;  // separacion del borde inferior (arriba del hotbar/stamina)
	const float LINE_H = 32;     // alto de cada linea

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

	void Add(ExorChatMsg dto)
	{
		if (!m_Root || !dto)
			return;

		ExorChatEntry e = new ExorChatEntry();
		e.name = dto.name;
		e.text = dto.text;
		e.channel = dto.channel;
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

	void Render()
	{
		float sw, sh;
		m_Root.GetScreenSize(sw, sh);

		int i;
		for (i = 0; i < ROWS; i++)
		{
			if (i < m_Entries.Count())
				RenderRow(i, m_Entries.Get(i), sh);
			else
				HideRow(i);
		}
	}

	void HideRow(int row)
	{
		int j;
		for (j = 0; j < SEGS; j++)
		{
			if (m_Segs.Get(row * SEGS + j))
				m_Segs.Get(row * SEGS + j).Show(false);
		}
	}

	void RenderRow(int row, ExorChatEntry e, float screenH)
	{
		int cBlue = ARGB(255, 90, 150, 255);    // nombre (azul)
		int cWhite = ARGB(255, 240, 240, 240);  // mensaje (blanco)
		int cTagG = ARGB(255, 150, 150, 165);   // tag global (gris)
		int cTagZ = ARGB(255, 90, 220, 120);    // tag zona (verde)

		string tag = "[G] ";
		int tagCol = cTagG;
		if (e.channel == 1)
		{
			tag = "[Z] ";
			tagCol = cTagZ;
		}

		TStringArray txt = new TStringArray;
		array<int> col = new array<int>;
		txt.Insert(tag);       col.Insert(tagCol);
		txt.Insert(e.name);    col.Insert(cBlue);
		txt.Insert(": ");      col.Insert(cWhite);
		txt.Insert(e.text);    col.Insert(cWhite);

		int count = txt.Count();

		// pass 1: texto/color, medir anchos
		array<int> widths = new array<int>;
		int j;
		TextWidget seg;
		int w, h;
		for (j = 0; j < SEGS; j++)
		{
			seg = m_Segs.Get(row * SEGS + j);
			if (!seg)
			{
				widths.Insert(0);
				continue;
			}
			if (j < count)
			{
				seg.SetText(txt.Get(j));
				seg.SetColor(col.Get(j));
				seg.Show(true);
				seg.GetTextSize(w, h);
				widths.Insert(w);
			}
			else
			{
				seg.Show(false);
				widths.Insert(0);
			}
		}

		// pass 2: posicionar de izq a der; la fila 0 (mas nueva) abajo del todo
		float x = LEFT_M;
		float y = screenH - BOTTOM_M - ((row + 1) * LINE_H);
		for (j = 0; j < count; j++)
		{
			seg = m_Segs.Get(row * SEGS + j);
			if (seg)
				seg.SetPos(x, y);
			x = x + widths.Get(j);
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
