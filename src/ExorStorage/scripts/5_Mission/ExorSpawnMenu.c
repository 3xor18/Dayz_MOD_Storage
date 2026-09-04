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
	protected ref array<int> m_PointIdx;		// indice REAL en spawns.json (la lista viene filtrada)
	protected int m_Count;

	protected bool m_BaseShown;
	protected float m_BaseRemain;	// seg restantes de cooldown de base
	protected bool m_BaseFlagDown;	// bandera abajo bloquea base

	protected ButtonWidget m_BtnEquip;	// "Spawn en base + Equipamiento" (VIP)
	protected bool m_EquipShown;
	protected int m_EquipRemaining;		// usos VIP restantes en el ciclo

	override Widget Init()
	{
		layoutRoot = GetGame().GetWorkspace().CreateWidgets("ExorStorage/gui/exor_spawn_menu.layout");
		if (!layoutRoot)
			return null;

		m_Buttons = new array<ButtonWidget>;
		int i;
		// Debe coincidir con la cantidad de botones ExorSpawnBtnN del .layout.
		for (i = 0; i < 12; i++)
		{
			ButtonWidget b = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ExorSpawnBtn" + i.ToString()));
			m_Buttons.Insert(b);
		}
		m_BtnBase = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ExorSpawnBtnBase"));
		m_BtnEquip = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ExorSpawnBtnBaseEquip"));

		m_Names = new TStringArray;
		m_PointRemain = new array<float>;
		m_PointIdx = new array<int>;
		m_Count = 0;
		m_BaseShown = false;
		m_BaseRemain = 0;
		m_BaseFlagDown = false;
		m_EquipShown = false;
		m_EquipRemaining = 0;

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
				// indice real en la config: la lista puede venir filtrada (puntos solo-VIP)
				int real = i;
				if (dto.punto_idx && i < dto.punto_idx.Count())
					real = dto.punto_idx.Get(i);
				m_PointIdx.Insert(real);
			}
			m_BaseShown = dto.base_enabled;
			m_BaseRemain = dto.base_cd_seg;
			m_BaseFlagDown = dto.base_flag_down;
			m_EquipShown = dto.equip_enabled;
			m_EquipRemaining = dto.equip_remaining;
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
	// En cooldown: fondo gris + LETRA ROJA. Disponible: color propio + letra clara.
	override void Refresh()
	{
		int colRed = ARGB(255, 225, 70, 70);
		int colTxt = ARGB(255, 235, 235, 235);
		int colGrey = ARGB(255, 40, 40, 46);

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
				b.SetColor(colGrey);
				b.SetTextColor(colRed);
			}
			else
			{
				b.SetText(m_Names.Get(i));
				b.SetColor(ARGB(255, 51, 51, 77));
				b.SetTextColor(colTxt);
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
					m_BtnBase.SetColor(colGrey);
					m_BtnBase.SetTextColor(colRed);
				}
				else if (m_BaseRemain > 0.5)
				{
					m_BtnBase.SetText("Mi base   (" + FormatMMSS(m_BaseRemain) + ")");
					m_BtnBase.SetColor(colGrey);
					m_BtnBase.SetTextColor(colRed);
				}
				else
				{
					m_BtnBase.SetText("Mi base");
					m_BtnBase.SetColor(ARGB(255, 31, 102, 31));	// verde = disponible
					m_BtnBase.SetTextColor(colTxt);
				}
			}
		}

		// Boton VIP "Spawn en base + Equipamiento (usos)" + timer de la base.
		if (m_BtnEquip)
		{
			m_BtnEquip.Show(m_EquipShown);
			if (m_EquipShown)
			{
				string baseTxt = string.Format("Spawn en base + Equipamiento (%1)", m_EquipRemaining);
				if (m_EquipRemaining <= 0)
				{
					m_BtnEquip.SetText("Spawn en base + Equipamiento (0 - sin usos)");
					m_BtnEquip.SetColor(colGrey);
					m_BtnEquip.SetTextColor(colRed);
				}
				else if (m_BaseFlagDown)
				{
					m_BtnEquip.SetText(baseTxt + "   (bandera abajo)");
					m_BtnEquip.SetColor(colGrey);
					m_BtnEquip.SetTextColor(colRed);
				}
				else if (m_BaseRemain > 0.5)
				{
					m_BtnEquip.SetText(baseTxt + "   (" + FormatMMSS(m_BaseRemain) + ")");
					m_BtnEquip.SetColor(colGrey);
					m_BtnEquip.SetTextColor(colRed);
				}
				else
				{
					m_BtnEquip.SetText(baseTxt);
					m_BtnEquip.SetColor(ARGB(255, 90, 70, 150));	// morado VIP disponible
					m_BtnEquip.SetTextColor(colTxt);
				}
			}
		}
	}

	bool EquipAvailable()
	{
		return m_EquipShown && m_EquipRemaining > 0 && BaseAvailable();
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
				// se manda el indice REAL de spawns.json, no la posicion en la lista
				p.ExorReqSpawnPick(m_PointIdx.Get(i));
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
		if (w == m_BtnEquip)
		{
			if (!EquipAvailable())
				return true;	// sin usos o base no disponible: ignora el click
			p.ExorReqSpawnPick(-2);
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
