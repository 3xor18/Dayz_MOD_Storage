// ============================================================================
// 3xor_Vanilla_Optimization - Pantalla de seleccion de spawn (Fase F UI)
// Menu scripteado que aparece al morir (lo dispara el server con SPAWN_OPEN).
// Lista los puntos de spawns.json + opcion "Mi base". Los puntos / la base que
// estan en cooldown (o la base con la bandera abajo) salen GRISES, con el tiempo
// restante al lado y NO se pueden clickear. El contador baja en vivo y se habilita
// solo cuando llega a 0. Al elegir, manda la eleccion al server (SPAWN_PICK).
// ============================================================================
class ExorSpawnMenu extends UIScriptedMenu
{
	protected ref array<ButtonWidget> m_Buttons;
	protected ButtonWidget m_BtnBase;

	protected ref TStringArray m_Names;
	protected ref array<float> m_PointRemain;	// seg restantes por punto (0 = disponible)
	protected int m_Count;

	protected bool m_BaseShown;
	protected float m_BaseRemain;	// seg restantes de cooldown de base
	protected bool m_BaseFlagDown;	// bandera abajo bloquea base

	override Widget Init()
	{
		layoutRoot = GetGame().GetWorkspace().CreateWidgets("ExorStorage/gui/exor_spawn_menu.layout");
		if (!layoutRoot)
			return null;

		m_Buttons = new array<ButtonWidget>;
		int i;
		for (i = 0; i < 6; i++)
		{
			ButtonWidget b = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ExorSpawnBtn" + i.ToString()));
			m_Buttons.Insert(b);
		}
		m_BtnBase = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ExorSpawnBtnBase"));

		m_Names = new TStringArray;
		m_PointRemain = new array<float>;
		m_Count = 0;
		m_BaseShown = false;
		m_BaseRemain = 0;
		m_BaseFlagDown = false;

		ExorSpawnMenuDTO dto = ExorSpawnClient.s_DTO;
		if (dto)
		{
			m_Count = dto.nombres.Count();
			for (i = 0; i < m_Count; i++)
			{
				m_Names.Insert(dto.nombres.Get(i));
				int cd = 0;
				if (dto.punto_cd_seg && i < dto.punto_cd_seg.Count())
					cd = dto.punto_cd_seg.Get(i);
				m_PointRemain.Insert(cd);
			}
			m_BaseShown = dto.base_enabled;
			m_BaseRemain = dto.base_cd_seg;
			m_BaseFlagDown = dto.base_flag_down;
		}

		Refresh();
		return layoutRoot;
	}

	// mm:ss a partir de segundos
	string FormatMMSS(float secs)
	{
		int total = secs;
		if (total < 0)
			total = 0;
		int mm = total / 60;
		int ss = total % 60;
		string sss = ss.ToString();
		if (ss < 10)
			sss = "0" + sss;
		return mm.ToString() + ":" + sss;
	}

	// Pinta texto/color/disponibilidad de cada boton segun el cooldown actual.
	void Refresh()
	{
		int i;
		for (i = 0; i < m_Buttons.Count(); i++)
		{
			ButtonWidget b = m_Buttons.Get(i);
			if (!b)
				continue;
			if (i >= m_Count)
			{
				b.Show(false);
				continue;
			}
			b.Show(true);
			if (m_PointRemain.Get(i) > 0.5)
			{
				b.SetText(m_Names.Get(i) + "   (" + FormatMMSS(m_PointRemain.Get(i)) + ")");
				b.SetColor(ARGB(255, 40, 40, 46));	// gris = en cooldown
			}
			else
			{
				b.SetText(m_Names.Get(i));
				b.SetColor(ARGB(255, 51, 51, 77));	// normal
			}
		}

		if (m_BtnBase)
		{
			m_BtnBase.Show(m_BaseShown);
			if (m_BaseShown)
			{
				if (m_BaseFlagDown)
				{
					m_BtnBase.SetText("Mi base   (bandera abajo)");
					m_BtnBase.SetColor(ARGB(255, 46, 46, 46));
				}
				else if (m_BaseRemain > 0.5)
				{
					m_BtnBase.SetText("Mi base   (" + FormatMMSS(m_BaseRemain) + ")");
					m_BtnBase.SetColor(ARGB(255, 46, 46, 46));
				}
				else
				{
					m_BtnBase.SetText("Mi base");
					m_BtnBase.SetColor(ARGB(255, 31, 102, 31));	// verde = disponible
				}
			}
		}
	}

	bool PointAvailable(int i)
	{
		return i >= 0 && i < m_Count && m_PointRemain.Get(i) <= 0.5;
	}

	bool BaseAvailable()
	{
		return m_BaseShown && !m_BaseFlagDown && m_BaseRemain <= 0.5;
	}

	override void OnShow()
	{
		super.OnShow();
		GetGame().GetInput().ChangeGameFocus(1);
		GetGame().GetUIManager().ShowUICursor(true);
	}

	override void OnHide()
	{
		super.OnHide();
		GetGame().GetUIManager().ShowUICursor(false);
		GetGame().GetInput().ChangeGameFocus(-1);
	}

	// Cuenta regresiva en vivo: baja los cooldowns y re-pinta (habilita al llegar a 0).
	override void Update(float timeslice)
	{
		super.Update(timeslice);
		bool dirty = false;
		int i;
		for (i = 0; i < m_PointRemain.Count(); i++)
		{
			if (m_PointRemain.Get(i) > 0)
			{
				float v = m_PointRemain.Get(i) - timeslice;
				if (v < 0)
					v = 0;
				m_PointRemain.Set(i, v);
				dirty = true;
			}
		}
		if (m_BaseRemain > 0)
		{
			m_BaseRemain = m_BaseRemain - timeslice;
			if (m_BaseRemain < 0)
				m_BaseRemain = 0;
			dirty = true;
		}
		if (dirty)
			Refresh();
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
		if (!p)
			return false;

		int i;
		for (i = 0; i < m_Buttons.Count(); i++)
		{
			if (w == m_Buttons.Get(i))
			{
				if (!PointAvailable(i))
					return true;	// en cooldown: ignora el click (no cierra)
				p.ExorReqSpawnPick(i);
				Close();
				return true;
			}
		}
		if (w == m_BtnBase)
		{
			if (!BaseAvailable())
				return true;	// base no disponible: ignora el click
			p.ExorReqSpawnPick(-1);
			Close();
			return true;
		}
		return super.OnClick(w, x, y, button);
	}

	override bool OnKeyPress(Widget w, int x, int y, int key)
	{
		if (key == KeyCode.KC_ESCAPE)
			Close();
		return false;
	}
}
