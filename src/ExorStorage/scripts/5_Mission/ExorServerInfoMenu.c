// ============================================================================
// 3xor_Vanilla_Optimization - Panel de Server Info (cliente)
// Menu con 3 tabs: General (texto editable), Reglas (texto editable) y Score
// (leaderboard: nombre | kills | deaths | suicidios | dist max + arma).
// El texto de General/Reglas viene de serverinfo.json (sincronizado al cliente).
// El leaderboard se pide al server por RPC (SCORE_REQ) al abrir y se cachea en
// ExorScoreClient.s_Data. Click en un header de columna ordena de mayor a menor.
// ============================================================================
class ExorServerInfoMenu extends UIScriptedMenu
{
	protected ButtonWidget m_TabGeneral;
	protected ButtonWidget m_TabReglas;
	protected ButtonWidget m_TabScore;
	protected ButtonWidget m_Close;
	protected ButtonWidget m_DiscordBtn;
	protected Widget m_PanelGeneral;
	protected Widget m_PanelReglas;
	protected Widget m_PanelScore;
	protected MultilineTextWidget m_TxtGeneral;
	protected MultilineTextWidget m_TxtReglas;
	protected ButtonWidget m_HdrName;
	protected ButtonWidget m_HdrKills;
	protected ButtonWidget m_HdrDeaths;
	protected ButtonWidget m_HdrSuic;
	protected ButtonWidget m_HdrDist;
	protected Widget m_ScoreList;

	protected int m_Tab;        // 0 general, 1 reglas, 2 score
	protected int m_SortCol;    // 0 nombre, 1 kills, 2 deaths, 3 suic, 4 dist
	protected ExorStatsFile m_LastData;

	override Widget Init()
	{
		layoutRoot = GetGame().GetWorkspace().CreateWidgets("ExorStorage/gui/exor_serverinfo.layout");

		m_TabGeneral = ButtonWidget.Cast(layoutRoot.FindAnyWidget("tab_general"));
		m_TabReglas  = ButtonWidget.Cast(layoutRoot.FindAnyWidget("tab_reglas"));
		m_TabScore   = ButtonWidget.Cast(layoutRoot.FindAnyWidget("tab_score"));
		m_Close      = ButtonWidget.Cast(layoutRoot.FindAnyWidget("close_btn"));
		m_DiscordBtn = ButtonWidget.Cast(layoutRoot.FindAnyWidget("discord_btn"));
		m_PanelGeneral = layoutRoot.FindAnyWidget("panel_general");
		m_PanelReglas  = layoutRoot.FindAnyWidget("panel_reglas");
		m_PanelScore   = layoutRoot.FindAnyWidget("panel_score");
		m_TxtGeneral = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget("txt_general"));
		m_TxtReglas  = MultilineTextWidget.Cast(layoutRoot.FindAnyWidget("txt_reglas"));
		m_HdrName   = ButtonWidget.Cast(layoutRoot.FindAnyWidget("hdr_name"));
		m_HdrKills  = ButtonWidget.Cast(layoutRoot.FindAnyWidget("hdr_kills"));
		m_HdrDeaths = ButtonWidget.Cast(layoutRoot.FindAnyWidget("hdr_deaths"));
		m_HdrSuic   = ButtonWidget.Cast(layoutRoot.FindAnyWidget("hdr_suic"));
		m_HdrDist   = ButtonWidget.Cast(layoutRoot.FindAnyWidget("hdr_dist"));
		m_ScoreList = layoutRoot.FindAnyWidget("score_list");

		m_SortCol = 1;	// por defecto ordena por kills

		ExorCfgServerInfo cfg = GetExorConfig().serverinfo;

		TextWidget title = TextWidget.Cast(layoutRoot.FindAnyWidget("si_title"));
		if (title)
			title.SetText(cfg.titulo);

		SetBtnLabel(m_TabGeneral, cfg.general_titulo);
		SetBtnLabel(m_TabReglas, cfg.reglas_titulo);
		SetBtnLabel(m_TabScore, cfg.score_titulo);
		if (m_TabGeneral) m_TabGeneral.Show(cfg.tab_general);
		if (m_TabReglas)  m_TabReglas.Show(cfg.tab_reglas);
		if (m_TabScore)   m_TabScore.Show(cfg.tab_score);

		if (m_TxtGeneral) m_TxtGeneral.SetText(JoinLines(cfg.general_lineas));
		if (m_TxtReglas)  m_TxtReglas.SetText(JoinLines(cfg.reglas_lineas));

		// tab inicial = la primera visible
		int def = 0;
		if (!cfg.tab_general)
		{
			if (cfg.tab_reglas)
				def = 1;
			else if (cfg.tab_score)
				def = 2;
		}
		ShowTab(def);

		// pedir el leaderboard al server
		PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
		if (p)
			p.ExorReqScore();

		return layoutRoot;
	}

	string JoinLines(TStringArray a)
	{
		string s = "";
		if (!a)
			return s;
		int i;
		for (i = 0; i < a.Count(); i++)
		{
			if (i > 0)
				s = s + "\n";
			s = s + WrapLine(a.Get(i), 70);
		}
		return s;
	}

	// El MultilineTextWidget no corta las lineas largas (se salen del cuadro), asi que
	// las cortamos por palabras a <= maxChars y unimos con \n. Asi entra en el ancho del
	// box y el scroll vertical maneja el alto.
	string WrapLine(string line, int maxChars)
	{
		int n = line.Length();
		if (n <= maxChars)
			return line;
		string outp = "";
		string cur = "";
		int i = 0;
		while (i < n)
		{
			int sp = line.IndexOfFrom(i, " ");
			string word;
			if (sp == -1)
			{
				word = line.Substring(i, n - i);
				i = n;
			}
			else
			{
				word = line.Substring(i, sp - i);
				i = sp + 1;
			}
			if (cur == "")
				cur = word;
			else if (cur.Length() + 1 + word.Length() <= maxChars)
				cur = cur + " " + word;
			else
			{
				outp = outp + cur + "\n";
				cur = word;
			}
		}
		if (cur != "")
			outp = outp + cur;
		return outp;
	}

	void SetBtnLabel(ButtonWidget b, string txt)
	{
		if (!b)
			return;
		TextWidget l = TextWidget.Cast(b.FindAnyWidget(b.GetName() + "_label"));
		if (l)
			l.SetText(txt);
	}

	void SetTabActive(ButtonWidget b, bool active)
	{
		if (!b)
			return;
		Widget bg = b.FindAnyWidget(b.GetName() + "_bg");
		if (!bg)
			return;
		if (active)
			bg.SetColor(ARGB(255, 70, 110, 170));
		else
			bg.SetColor(ARGB(255, 46, 46, 56));
	}

	void ShowTab(int tab)
	{
		m_Tab = tab;
		if (m_PanelGeneral) m_PanelGeneral.Show(tab == 0);
		if (m_PanelReglas)  m_PanelReglas.Show(tab == 1);
		if (m_PanelScore)   m_PanelScore.Show(tab == 2);
		SetTabActive(m_TabGeneral, tab == 0);
		SetTabActive(m_TabReglas, tab == 1);
		SetTabActive(m_TabScore, tab == 2);
		if (tab == 2)
		{
			// Re-pedir el leaderboard al server cada vez que se entra a Score
			// (el pedido del Init puede correr antes de que el player este listo).
			PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
			if (p)
				p.ExorReqScore();
			RenderScore();
		}
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (w == m_TabGeneral) { ShowTab(0); return true; }
		if (w == m_TabReglas)  { ShowTab(1); return true; }
		if (w == m_TabScore)   { ShowTab(2); return true; }
		if (w == m_Close)      { Close(); return true; }
		if (w == m_DiscordBtn)
		{
			string url = GetExorConfig().serverinfo.discord_url;
			if (url != "")
				GetGame().OpenURL(url);
			return true;
		}
		if (w == m_HdrName)    { m_SortCol = 0; RenderScore(); return true; }
		if (w == m_HdrKills)   { m_SortCol = 1; RenderScore(); return true; }
		if (w == m_HdrDeaths)  { m_SortCol = 2; RenderScore(); return true; }
		if (w == m_HdrSuic)    { m_SortCol = 3; RenderScore(); return true; }
		if (w == m_HdrDist)    { m_SortCol = 4; RenderScore(); return true; }
		return super.OnClick(w, x, y, button);
	}

	override void Update(float timeslice)
	{
		super.Update(timeslice);

		// ESC cierra (igual que la X). OnKeyPress no siempre recibe el ESC, asi que
		// leemos la tecla cruda aca (misma tecnica que el mapa).
		if (KeyState(KeyCode.KC_ESCAPE) > 0)
		{
			Close();
			return;
		}

		// si llego el leaderboard nuevo y estamos en la tab score, renderizar
		if (m_Tab == 2 && ExorScoreClient.s_Data && ExorScoreClient.s_Data != m_LastData)
			RenderScore();
	}

	// ESC cierra el panel (antes solo cerraba con la X arriba a la derecha).
	override bool OnKeyPress(Widget w, int x, int y, int key)
	{
		if (key == KeyCode.KC_ESCAPE)
		{
			Close();
			return true;
		}
		return super.OnKeyPress(w, x, y, key);
	}

	void ClearRows()
	{
		if (!m_ScoreList)
			return;
		Widget c = m_ScoreList.GetChildren();
		while (c)
		{
			Widget next = c.GetSibling();
			c.Unlink();
			c = next;
		}
	}

	void RenderScore()
	{
		if (!m_ScoreList)
			return;
		ClearRows();

		ExorStatsFile data = ExorScoreClient.s_Data;
		m_LastData = data;
		if (!data || !data.rows)
			return;

		array<ref ExorStatRow> rows = new array<ref ExorStatRow>;
		int i;
		for (i = 0; i < data.rows.Count(); i++)
			rows.Insert(data.rows.Get(i));
		SortRows(rows, m_SortCol);

		int k;
		for (k = 0; k < rows.Count(); k++)
		{
			ExorStatRow r = rows.Get(k);
			Widget rw = GetGame().GetWorkspace().CreateWidgets("ExorStorage/gui/exor_serverinfo_row.layout", m_ScoreList);
			if (!rw)
				continue;
			SetCell(rw, "col_name", r.name);
			SetCell(rw, "col_kills", r.kills.ToString());
			SetCell(rw, "col_deaths", r.deaths.ToString());
			SetCell(rw, "col_suic", r.suicides.ToString());
			string d = r.max_dist.ToString() + "m";
			if (r.max_weapon != "")
				d = d + "  " + r.max_weapon;
			SetCell(rw, "col_dist", d);
		}
	}

	void SetCell(Widget row, string name, string txt)
	{
		TextWidget t = TextWidget.Cast(row.FindAnyWidget(name));
		if (t)
			t.SetText(txt);
	}

	// Orden descendente por la columna elegida (listas chicas -> bubble sort).
	void SortRows(array<ref ExorStatRow> rows, int col)
	{
		int n = rows.Count();
		int i, j;
		for (i = 0; i < n - 1; i++)
		{
			for (j = 0; j < n - 1 - i; j++)
			{
				ExorStatRow a = rows.Get(j);
				ExorStatRow b = rows.Get(j + 1);
				bool swap = false;
				if (col == 1)
					swap = b.kills > a.kills;
				else if (col == 2)
					swap = b.deaths > a.deaths;
				else if (col == 3)
					swap = b.suicides > a.suicides;
				else if (col == 4)
					swap = b.max_dist > a.max_dist;
				if (swap)
				{
					rows.Set(j, b);
					rows.Set(j + 1, a);
				}
			}
		}
	}
}
