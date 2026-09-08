// ============================================================================
// 3xor_Vanilla_Optimization - Pantalla de seleccion de spawn (Fase F UI)
// Menu scripteado que aparece al morir (lo dispara el server con SPAWN_OPEN).
// Lista los puntos de spawns.json + opcion "Mi base". Los puntos / la base que
// estan en cooldown (o la base con la bandera abajo) salen GRISES, con el tiempo
// restante al lado y NO se pueden clickear. El contador baja en vivo y se habilita
// solo cuando llega a 0. Al elegir, manda la eleccion al server (SPAWN_PICK).
// El boton de abajo NO es un punto: es el INTERRUPTOR "Equipamiento VIP". Arranca
// APAGADO; el VIP lo prende si quiere aparecer con su pack de ropa (gasta 1 uso) y
// recien despues elige el punto. Por eso no cierra el menu al clickearlo.
// ============================================================================
class ExorSpawnMenu extends UIScriptedMenu
{
	// morado de "esto esta elegido" (lo comparten hombre/mujer y el interruptor VIP)
	static const int MORADO = 0xFF5A4696;   // ARGB opaco = (90, 70, 150)

	protected ref array<ButtonWidget> m_Buttons;
	protected ButtonWidget m_BtnBase;

	protected ref TStringArray m_Names;
	protected ref array<float> m_PointRemain;	// seg restantes por punto (0 = disponible)
	protected ref array<int> m_PointIdx;		// indice REAL en spawns.json (la lista viene filtrada)
	protected int m_Count;

	protected bool m_BaseShown;
	protected float m_BaseRemain;	// seg restantes de cooldown de base
	protected bool m_BaseFlagDown;	// bandera abajo bloquea base

	protected ButtonWidget m_BtnEquip;	// interruptor "Equipamiento VIP"
	protected bool m_EquipShown;
	protected int m_EquipRemaining;		// usos VIP que le quedan
	protected string m_EquipPack;		// nombre del pack que le toca (vip.json)
	protected bool m_EquipOn;			// arranca APAGADO: el VIP lo prende si quiere

	protected ButtonWidget m_BtnHombre;	// hombre / mujer: es para TODOS, no solo VIP
	protected ButtonWidget m_BtnMujer;
	// Paneles de fondo de los botones CON ESTADO. SetColor sobre un ButtonWidget no pinta
	// el estado normal: lo unico que se veia era el hover del motor, que se va al sacar el
	// mouse. Un PanelWidget si respeta SetColor, asi que el "elegido" se pinta aca.
	protected Widget m_BgHombre;
	protected Widget m_BgMujer;
	protected Widget m_BgEquip;
	protected bool m_GeneroShown;
	protected int m_GeneroSel;			// 0 = hombre, 1 = mujer (arranca en el que ya tiene)

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
		m_BtnHombre = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ExorSpawnBtnHombre"));
		m_BtnMujer = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ExorSpawnBtnMujer"));
		m_BgHombre = layoutRoot.FindAnyWidget("ExorSpawnBgHombre");
		m_BgMujer = layoutRoot.FindAnyWidget("ExorSpawnBgMujer");
		m_BgEquip = layoutRoot.FindAnyWidget("ExorSpawnBgEquip");

		m_Names = new TStringArray;
		m_PointRemain = new array<float>;
		m_PointIdx = new array<int>;
		m_Count = 0;
		m_BaseShown = false;
		m_BaseRemain = 0;
		m_BaseFlagDown = false;
		m_EquipShown = false;
		m_EquipRemaining = 0;
		m_EquipPack = "";
		m_EquipOn = false;
		m_GeneroShown = false;
		m_GeneroSel = 0;

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
			m_EquipPack = dto.equip_pack;
			m_GeneroShown = dto.genero_enabled;
			// Arranca SIEMPRE en Hombre (pedido del admin). Ojo: como lo que se manda es lo
			// que esta marcado, una jugadora que quiera seguir mujer tiene que tocar "Mujer"
			// en cada respawn; si no toca nada, aparece hombre.
			m_GeneroSel = 0;
		}

		Relayout();
		Refresh();
		return layoutRoot;
	}

	// ------------------------- alto del panel a medida -------------------------
	// El .layout trae 12 slots de punto: con 3 puntos configurados quedaba medio panel
	// vacio y el ultimo boton se metia ABAJO, encima de la hotbar (los items 1/2/3/4).
	// Aca se recalcula todo segun las filas que REALMENTE se muestran y se corta el panel
	// antes de LIMITE_ABAJO, asi nunca pisa la hotbar. Adentro del panel las posiciones son
	// relativas a SU alto, por eso todo va dividido por panelH.
	void Relayout()
	{
		if (!layoutRoot)
			return;
		Widget panel = layoutRoot.FindAnyWidget("ExorSpawnPanel");
		if (!panel)
			return;

		int filas = m_Count;
		if (m_GeneroShown)
			filas = filas + 1;	// hombre/mujer van en UNA fila (mitad y mitad)
		if (m_BaseShown)
			filas = filas + 1;
		if (m_EquipShown)
			filas = filas + 1;
		if (filas < 1)
			filas = 1;

		float pad = 0.012;
		float titleH = 0.048;
		float rowH = 0.040;
		float gap = 0.009;
		float top = 0.055;
		float LIMITE_ABAJO = 0.86;	// la hotbar vive de ~0.88 para abajo

		float fijo = pad * 3 + titleH;
		float panelH = fijo + filas * rowH + (filas - 1) * gap;
		float maxH = LIMITE_ABAJO - top;
		if (panelH > maxH)
		{
			// muchos puntos configurados: se achican las filas para que entren igual
			float k = (maxH - fijo) / (filas * rowH + (filas - 1) * gap);
			if (k < 0.35)
				k = 0.35;
			rowH = rowH * k;
			gap = gap * k;
			panelH = fijo + filas * rowH + (filas - 1) * gap;
			if (panelH > maxH)
				panelH = maxH;
		}

		panel.SetPos(0.34, top);
		panel.SetSize(0.32, panelH);

		float hRel = rowH / panelH;
		Widget title = layoutRoot.FindAnyWidget("ExorSpawnTitle");
		if (title)
		{
			title.SetPos(0.02, pad / panelH);
			title.SetSize(0.96, titleH / panelH);
		}

		float y = pad + titleH + pad;
		int i;
		for (i = 0; i < m_Buttons.Count() && i < m_Count; i++)
		{
			ButtonWidget b = m_Buttons.Get(i);
			if (!b)
				continue;
			b.SetPos(0.05, y / panelH);
			b.SetSize(0.9, hRel);
			y = y + rowH + gap;
		}
		if (m_GeneroShown)
		{
			// el panel de fondo va EXACTAMENTE donde su boton (es lo que se ve pintado)
			if (m_BtnHombre)
			{
				m_BtnHombre.SetPos(0.05, y / panelH);
				m_BtnHombre.SetSize(0.44, hRel);
			}
			if (m_BgHombre)
			{
				m_BgHombre.SetPos(0.05, y / panelH);
				m_BgHombre.SetSize(0.44, hRel);
			}
			if (m_BtnMujer)
			{
				m_BtnMujer.SetPos(0.51, y / panelH);
				m_BtnMujer.SetSize(0.44, hRel);
			}
			if (m_BgMujer)
			{
				m_BgMujer.SetPos(0.51, y / panelH);
				m_BgMujer.SetSize(0.44, hRel);
			}
			y = y + rowH + gap;
		}
		if (m_BaseShown && m_BtnBase)
		{
			m_BtnBase.SetPos(0.05, y / panelH);
			m_BtnBase.SetSize(0.9, hRel);
			y = y + rowH + gap;
		}
		if (m_EquipShown)
		{
			if (m_BtnEquip)
			{
				m_BtnEquip.SetPos(0.05, y / panelH);
				m_BtnEquip.SetSize(0.9, hRel);
			}
			if (m_BgEquip)
			{
				m_BgEquip.SetPos(0.05, y / panelH);
				m_BgEquip.SetSize(0.9, hRel);
			}
		}
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

		// Interruptor VIP "Equipamiento": prendido = morado, apagado = gris. Muestra el
		// pack que le toca y cuantos usos le quedan. Sin usos queda muerto (rojo).
		if (m_BtnEquip)
		{
			m_BtnEquip.Show(m_EquipShown);
			if (m_EquipShown)
			{
				if (m_EquipRemaining <= 0)
				{
					m_BtnEquip.SetText("Equipamiento VIP   (0 - sin usos)");
					m_BtnEquip.SetColor(colGrey);
					m_BtnEquip.SetTextColor(colRed);
				}
				else if (m_EquipOn)
				{
					m_BtnEquip.SetText(string.Format("Equipamiento VIP: SI - %1   (quedan %2)", m_EquipPack, m_EquipRemaining));
					m_BtnEquip.SetTextColor(ARGB(255, 245, 245, 245));
				}
				else
				{
					m_BtnEquip.SetText(string.Format("Equipamiento VIP: NO - %1   (quedan %2)", m_EquipPack, m_EquipRemaining));
					m_BtnEquip.SetTextColor(ARGB(255, 130, 130, 140));
				}
			}
		}
		// prendido = panel morado detras del boton (el boton solo no lo pinta)
		if (m_BgEquip)
		{
			m_BgEquip.Show(m_EquipShown && m_EquipOn && m_EquipRemaining > 0);
			m_BgEquip.SetColor(MORADO);
		}

		RefreshGenero();
	}

	// Hombre / mujer: el elegido va en verde, el otro gris. Se pinta siempre igual (no
	// tiene cooldown ni condiciones): el cambio se aplica al elegir el punto de spawn.
	void RefreshGenero()
	{
		PintarGenero(m_BtnHombre, m_BgHombre, "Hombre", m_GeneroSel == 0);
		PintarGenero(m_BtnMujer, m_BgMujer, "Mujer", m_GeneroSel == 1);
	}

	// El elegido queda MORADO con letra clara (igual que el interruptor VIP prendido) y
	// con una marca delante; el otro, gris con letra apagada. Se cambia el fondo Y la
	// letra a proposito: si el estilo del boton no pinta el fondo, el texto igual delata
	// cual esta elegido.
	void PintarGenero(ButtonWidget b, Widget bg, string txt, bool elegido)
	{
		if (b)
		{
			b.Show(m_GeneroShown);
			b.SetText(txt);
			if (m_GeneroShown && elegido)
				b.SetTextColor(ARGB(255, 245, 245, 245));
			else
				b.SetTextColor(ARGB(255, 130, 130, 140));
		}
		// el morado del elegido vive en el panel de atras (ver arriba: el boton no lo pinta)
		if (bg)
		{
			bg.Show(m_GeneroShown && elegido);
			bg.SetColor(MORADO);
		}
	}

	// Sexo que se le manda al server con la eleccion (-1 = no tocar, feature apagada).
	int GeneroPedido()
	{
		if (!m_GeneroShown)
			return -1;
		return m_GeneroSel;
	}

	// El interruptor se puede tocar si es VIP y le quedan usos (ya no depende de la base).
	bool EquipAvailable()
	{
		return m_EquipShown && m_EquipRemaining > 0;
	}

	// true = el spawn que se mande tiene que venir con el equipamiento VIP puesto
	bool EquipPedido()
	{
		return m_EquipOn && EquipAvailable();
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
				p.ExorReqSpawnPick(m_PointIdx.Get(i), EquipPedido(), GeneroPedido());
				Close();
				return true;
			}
		}
		if (w == m_BtnBase)
		{
			if (!BaseAvailable())
				return true;	// base no disponible: ignora el click
			p.ExorReqSpawnPick(-1, EquipPedido(), GeneroPedido());
			Close();
			return true;
		}
		if (w == m_BtnEquip)
		{
			if (!EquipAvailable())
				return true;	// sin usos: ignora el click
			m_EquipOn = !m_EquipOn;	// interruptor: NO cierra el menu, solo se pinta
			Refresh();
			return true;
		}
		if (w == m_BtnHombre || w == m_BtnMujer)
		{
			// Solo marca la eleccion: el personaje se cambia recien al elegir el punto.
			if (!m_GeneroShown)
				return true;
			if (w == m_BtnHombre)
				m_GeneroSel = 0;
			else
				m_GeneroSel = 1;
			RefreshGenero();
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
