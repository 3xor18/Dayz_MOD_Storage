// ============================================================================
// 3xor_Vanilla_Optimization - Killfeed (cliente)
// Arriba a la DERECHA: lineas "Killer mató a Victim con Arma a N mts" (killer en
// verde, victim en rojo, ambos en negrita) o "Victim se ha suicidado" (en rojo).
// Cada linea dura unos segundos (config) y hay un maximo simultaneo (config); al
// entrar una nueva con el cupo lleno se va la mas vieja. La mas nueva va arriba.
// Posicionamiento: igual que los nameplates -> root full-screen + pixeles de
// pantalla absolutos. Cada linea se mide (GetTextSize) y se pega al borde derecho.
// El server decide si mandar el evento (killfeed.habilitado) y manda duracion/max.
// ============================================================================
class ExorKfEntry
{
	string killer;
	string victim;
	string weapon;
	int dist;
	bool suicide;
	int expireMs;
}

class ExorKillfeed
{
	protected Widget m_Root;
	protected bool m_Tried;
	protected ref array<TextWidget> m_Segs;
	protected ref array<ref ExorKfEntry> m_Entries;

	// layout (pixeles de pantalla)
	const float MARGIN_R = 16;   // separacion del borde derecho
	const float TOP_M    = 8;    // separacion del borde superior
	const float LINE_H   = 32;   // alto de cada linea

	void Create()
	{
		if (m_Tried)
			return;
		m_Tried = true;

		m_Root = GetGame().GetWorkspace().CreateWidgets("ExorStorage/gui/exor_killfeed.layout");
		if (!m_Root)
			return;

		m_Segs = new array<TextWidget>;
		int i, j;
		for (i = 0; i < 5; i++)
			for (j = 0; j < 4; j++)
				m_Segs.Insert(TextWidget.Cast(m_Root.FindAnyWidget("ExorKfR" + i.ToString() + "S" + j.ToString())));

		m_Entries = new array<ref ExorKfEntry>;
	}

	void Add(ExorKfDTO dto)
	{
		if (!m_Root || !dto)
			return;

		ExorKfEntry e = new ExorKfEntry();
		e.killer = dto.killer;
		e.victim = dto.victim;
		e.weapon = dto.weapon;
		e.dist = dto.dist;
		e.suicide = dto.suicide;
		int dur = dto.dur;
		if (dur <= 0)
			dur = 6;
		e.expireMs = GetGame().GetTime() + (dur * 1000);

		m_Entries.InsertAt(e, 0);	// la mas nueva arriba

		int cap = dto.max;
		if (cap > 5)
			cap = 5;
		if (cap < 1)
			cap = 1;
		while (m_Entries.Count() > cap)
			m_Entries.Remove(m_Entries.Count() - 1);	// se va la mas vieja

		Render();
	}

	// Cada frame: mete los killfeed nuevos (cola en 3_Game) y saca los vencidos.
	void Update()
	{
		if (!m_Root)
			return;

		if (ExorKillfeedQueue.s_Pending && ExorKillfeedQueue.s_Pending.Count() > 0)
		{
			int k;
			for (k = 0; k < ExorKillfeedQueue.s_Pending.Count(); k++)
				Add(ExorKillfeedQueue.s_Pending.Get(k));
			ExorKillfeedQueue.s_Pending.Clear();
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
		for (i = 0; i < 5; i++)
		{
			if (i < m_Entries.Count())
				RenderRow(i, m_Entries.Get(i), sw);
			else
				HideRow(i);
		}
	}

	void HideRow(int row)
	{
		int j;
		for (j = 0; j < 4; j++)
		{
			if (m_Segs.Get(row * 4 + j))
				m_Segs.Get(row * 4 + j).Show(false);
		}
	}

	void RenderRow(int row, ExorKfEntry e, float screenW)
	{
		int cG = ARGB(255, 45, 215, 60);    // verde (killer)
		int cR = ARGB(255, 230, 50, 50);    // rojo (victima)
		int cW = ARGB(255, 240, 240, 240);  // blanco (texto)

		TStringArray txt = new TStringArray;
		array<int> col = new array<int>;
		if (e.suicide)
		{
			txt.Insert(e.victim);              col.Insert(cR);
			txt.Insert(" se ha suicidado");    col.Insert(cW);
		}
		else
		{
			txt.Insert(e.killer);                                                   col.Insert(cG);
			txt.Insert(" mató a ");                                                 col.Insert(cW);
			txt.Insert(e.victim);                                                   col.Insert(cR);
			txt.Insert(" con " + e.weapon + " a " + e.dist.ToString() + " mts");    col.Insert(cW);
		}

		int count = txt.Count();

		// pass 1: setear texto/color, medir anchos y total
		array<int> widths = new array<int>;
		int total = 0;
		int j;
		TextWidget seg;
		int w, h;
		for (j = 0; j < 4; j++)
		{
			seg = m_Segs.Get(row * 4 + j);
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
				total = total + w;
			}
			else
			{
				seg.Show(false);
				widths.Insert(0);
			}
		}

		// pass 2: posicionar pegado al borde derecho, fila segun el indice
		float x = screenW - MARGIN_R - total;
		float y = TOP_M + (row * LINE_H);
		for (j = 0; j < count; j++)
		{
			seg = m_Segs.Get(row * 4 + j);
			if (seg)
				seg.SetPos(x, y);
			x = x + widths.Get(j);
		}
	}
}
