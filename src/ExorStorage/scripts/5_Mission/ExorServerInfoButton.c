// ============================================================================
// 3xor_Vanilla_Optimization - Lo que el mod agrega al menu de pausa (cliente)
// 1) Boton "Server Info" en la lista de botones (mismo estilo vanilla).
// 2) PANTALLA DE MUERTE: panel unico para elegir sexo + zona de aparicion +
//    equipamiento VIP, todo antes de apretar REAPARECER.
// ============================================================================
// POR QUE LA ELECCION VA ACA Y NO DESPUES DE APARECER:
// el motor crea el personaje ANTES de que exista cualquier pantalla del mod. El sexo NO se
// puede cambiar despues (reemplazar la entidad deja al jugador fuera del guardado y le
// borra el personaje en el primer reinicio: paso el 9-sep con 3 jugadores). Asi que hay que
// preguntarlo mientras esta muerto, que es el ultimo momento util.
//
// Los dos datos viajan por caminos distintos, y no es capricho:
//   SEXO      se escribe en MenuDefaultCharacterData, que es lo que DayZ serializa y manda
//             solo al reaparecer. Llega justo antes de que se cree el cuerpo.
//   ZONA/VIP  necesitan RPC, y estando muerto la entidad del jugador ya no sirve de canal
//             (probado: el boton se pintaba y al server no llegaba nada). Quedan anotados
//             en ExorSpawnPend y se mandan apenas el personaje nuevo esta vivo.
modded class InGameMenu
{
	protected ButtonWidget m_ExorSiBtn;

	// ---- panel de la pantalla de muerte ----
	protected Widget m_ExorMuRoot;
	protected TextWidget m_ExorMuTitulo;
	protected ButtonWidget m_ExorMuBtnH;
	protected ButtonWidget m_ExorMuBtnM;
	protected Widget m_ExorMuBgH;
	protected Widget m_ExorMuBgM;
	protected ref array<ButtonWidget> m_ExorMuPuntos;
	protected ref array<Widget> m_ExorMuPuntosBg;
	// El texto va en una etiqueta HIJA, no en el boton: un ButtonWidget transparente (que es
	// lo que deja ver el morado de atras) tampoco dibuja su propio texto.
	protected ref array<TextWidget> m_ExorMuPuntosLbl;
	protected ButtonWidget m_ExorMuBtnVip;
	protected Widget m_ExorMuBgVip;
	protected TextWidget m_ExorMuBtnVipLbl;
	protected bool m_ExorMuEstabaMuerto;

	static const int EXOR_MU_MAX_PUNTOS = 8;	// tiene que coincidir con el .layout

	override Widget Init()
	{
		Widget root = super.Init();

		m_ExorMuPuntos = new array<ButtonWidget>;
		m_ExorMuPuntosBg = new array<Widget>;
		m_ExorMuPuntosLbl = new array<TextWidget>;

		if (GetExorConfig().spawns.habilitado && root)
		{
			m_ExorMuRoot = GetGame().GetWorkspace().CreateWidgets("ExorStorage/gui/exor_genero_muerte.layout", root);
			if (m_ExorMuRoot)
			{
				m_ExorMuTitulo = TextWidget.Cast(m_ExorMuRoot.FindAnyWidget("ExorMuTitulo"));
				m_ExorMuBtnH = ButtonWidget.Cast(m_ExorMuRoot.FindAnyWidget("ExorMuBtnH"));
				m_ExorMuBtnM = ButtonWidget.Cast(m_ExorMuRoot.FindAnyWidget("ExorMuBtnM"));
				m_ExorMuBgH = m_ExorMuRoot.FindAnyWidget("ExorMuBgH");
				m_ExorMuBgM = m_ExorMuRoot.FindAnyWidget("ExorMuBgM");
				int i;
				for (i = 0; i < EXOR_MU_MAX_PUNTOS; i++)
				{
					m_ExorMuPuntos.Insert(ButtonWidget.Cast(m_ExorMuRoot.FindAnyWidget("ExorMuBtn" + i.ToString())));
					m_ExorMuPuntosBg.Insert(m_ExorMuRoot.FindAnyWidget("ExorMuBg" + i.ToString()));
					m_ExorMuPuntosLbl.Insert(TextWidget.Cast(m_ExorMuRoot.FindAnyWidget("ExorMuBtn" + i.ToString() + "Label")));
				}
				m_ExorMuBtnVip = ButtonWidget.Cast(m_ExorMuRoot.FindAnyWidget("ExorMuBtnVip"));
				m_ExorMuBgVip = m_ExorMuRoot.FindAnyWidget("ExorMuBgVip");
				m_ExorMuBtnVipLbl = TextWidget.Cast(m_ExorMuRoot.FindAnyWidget("ExorMuBtnVipLabel"));
				m_ExorMuRoot.Show(false);	// solo estando muerto (ver UpdateGUI)
			}
		}

		if (GetExorConfig().serverinfo.habilitado && m_ContinueButton)
		{
			Widget parent = m_ContinueButton.GetParent();
			if (parent)
			{
				Widget w = GetGame().GetWorkspace().CreateWidgets("ExorStorage/gui/exor_serverinfo_btn.layout", parent);
				m_ExorSiBtn = ButtonWidget.Cast(w);

				// Colocarlo justo DEBAJO de "Continuar", mismo ancho/alto y pegado
				// (sin el hueco grande). Mismo padre => mismas unidades que continuebtn.
				if (m_ExorSiBtn)
				{
					float cx, cy, cw, ch;
					m_ContinueButton.GetPos(cx, cy);
					m_ContinueButton.GetSize(cw, ch);
					m_ExorSiBtn.SetSize(cw, ch);
					m_ExorSiBtn.SetPos(cx, cy + ch);
				}
			}
		}

		return root;
	}

	// ------------------------- armado del panel -------------------------
	// Cuantas zonas se pueden ofrecer: las que mando el server la ultima vez (el cache se
	// llena al aparecer). Si todavia no llego ninguna lista, el panel muestra solo el sexo.
	int ExorMuCantPuntos()
	{
		ExorSpawnMenuDTO dto = ExorSpawnClient.s_DTO;
		if (!dto || !dto.nombres)
			return 0;
		int n = dto.nombres.Count();
		if (n > EXOR_MU_MAX_PUNTOS)
			n = EXOR_MU_MAX_PUNTOS;
		return n;
	}

	bool ExorMuGeneroVisible()
	{
		return GetExorConfig().spawns.elegir_genero;
	}

	bool ExorMuVipVisible()
	{
		ExorSpawnMenuDTO dto = ExorSpawnClient.s_DTO;
		return dto && dto.equip_enabled && dto.equip_remaining > 0;
	}

	// Alto del panel y posicion de cada fila. Se recalcula cada vez que se muestra porque
	// la cantidad de zonas puede cambiar entre una muerte y otra (config del admin).
	void ExorMuRelayout()
	{
		if (!m_ExorMuRoot)
			return;
		int puntos = ExorMuCantPuntos();
		int filas = puntos;
		if (ExorMuGeneroVisible())
			filas = filas + 1;
		if (ExorMuVipVisible())
			filas = filas + 1;
		if (filas < 1)
			filas = 1;

		float titulo = 0.045;	// alto del titulo, en pantalla
		float fila = 0.042;		// alto de cada fila
		float sep = 0.008;
		float ayuda = 0.030;
		float alto = 0.020 + titulo + filas * (fila + sep) + ayuda + 0.012;
		m_ExorMuRoot.SetPos(0.32, 0.045);
		m_ExorMuRoot.SetSize(0.36, alto);

		float hRel = fila / alto;
		float y = (0.020 + titulo) / alto;
		float paso = (fila + sep) / alto;

		if (m_ExorMuTitulo)
		{
			m_ExorMuTitulo.SetPos(0.03, 0.016 / alto);
			m_ExorMuTitulo.SetSize(0.94, titulo / alto);
		}

		// fila 1: hombre / mujer (mitad y mitad)
		if (ExorMuGeneroVisible())
		{
			ExorMuColocar(m_ExorMuBtnH, m_ExorMuBgH, 0.04, y, 0.44, hRel);
			ExorMuColocar(m_ExorMuBtnM, m_ExorMuBgM, 0.52, y, 0.44, hRel);
			y = y + paso;
		}

		int i;
		for (i = 0; i < m_ExorMuPuntos.Count(); i++)
		{
			if (i >= puntos)
			{
				if (m_ExorMuPuntos.Get(i))
					m_ExorMuPuntos.Get(i).Show(false);
				if (m_ExorMuPuntosBg.Get(i))
					m_ExorMuPuntosBg.Get(i).Show(false);
				continue;
			}
			ExorMuColocar(m_ExorMuPuntos.Get(i), m_ExorMuPuntosBg.Get(i), 0.04, y, 0.92, hRel);
			y = y + paso;
		}

		if (ExorMuVipVisible())
		{
			ExorMuColocar(m_ExorMuBtnVip, m_ExorMuBgVip, 0.04, y, 0.92, hRel);
			y = y + paso;
		}
		else
		{
			if (m_ExorMuBtnVip)
				m_ExorMuBtnVip.Show(false);
			if (m_ExorMuBgVip)
				m_ExorMuBgVip.Show(false);
		}

		Widget ayudaW = m_ExorMuRoot.FindAnyWidget("ExorMuAyuda");
		if (ayudaW)
		{
			ayudaW.SetPos(0.03, y + 0.004);
			ayudaW.SetSize(0.94, ayuda / alto);
		}
	}

	void ExorMuColocar(ButtonWidget b, Widget bg, float x, float y, float w, float h)
	{
		if (b)
		{
			b.SetPos(x, y);
			b.SetSize(w, h);
			b.Show(true);
		}
		// el fondo va EXACTAMENTE donde el boton: es lo que se ve pintado al elegir
		if (bg)
		{
			bg.SetPos(x, y);
			bg.SetSize(w, h);
		}
	}

	// Textos y marcas de lo elegido.
	void ExorMuRefresh()
	{
		if (!m_ExorMuRoot)
			return;
		int claro = ARGB(255, 240, 240, 240);

		if (m_ExorMuBtnH)
		{
			m_ExorMuBtnH.Show(ExorMuGeneroVisible());
			m_ExorMuBtnH.SetTextColor(claro);
		}
		if (m_ExorMuBtnM)
		{
			m_ExorMuBtnM.Show(ExorMuGeneroVisible());
			m_ExorMuBtnM.SetTextColor(claro);
		}
		if (m_ExorMuBgH)
			m_ExorMuBgH.Show(ExorMuGeneroVisible() && ExorGeneroClient.s_Sel == 0);
		if (m_ExorMuBgM)
			m_ExorMuBgM.Show(ExorMuGeneroVisible() && ExorGeneroClient.s_Sel == 1);

		int apagado = ARGB(255, 140, 140, 150);
		ExorSpawnMenuDTO dto = ExorSpawnClient.s_DTO;
		int puntos = ExorMuCantPuntos();
		int i;
		for (i = 0; i < puntos; i++)
		{
			ButtonWidget b = m_ExorMuPuntos.Get(i);
			if (!b)
				continue;
			int real = i;
			if (dto.punto_idx && i < dto.punto_idx.Count())
				real = dto.punto_idx.Get(i);
			// elegido = fondo morado + letra clara, igual que hombre/mujer. El boton es
			// transparente a proposito para que se vea el panel de atras.
			bool elegido = (ExorSpawnPend.s_Idx == real);
			TextWidget lbl = m_ExorMuPuntosLbl.Get(i);
			if (lbl)
			{
				lbl.SetText(dto.nombres.Get(i));
				if (elegido)
					lbl.SetColor(claro);
				else
					lbl.SetColor(apagado);
			}
			if (m_ExorMuPuntosBg.Get(i))
				m_ExorMuPuntosBg.Get(i).Show(elegido);
		}

		if (m_ExorMuBtnVip && ExorMuVipVisible())
		{
			string estado = "NO";
			if (ExorSpawnPend.s_Equip)
				estado = "SI";
			if (m_ExorMuBtnVipLbl)
			{
				m_ExorMuBtnVipLbl.SetText(string.Format("Equipamiento VIP: %1 - %2   (quedan %3)", estado, dto.equip_pack, dto.equip_remaining));
				if (ExorSpawnPend.s_Equip)
					m_ExorMuBtnVipLbl.SetColor(claro);
				else
					m_ExorMuBtnVipLbl.SetColor(apagado);
			}
			if (m_ExorMuBgVip)
				m_ExorMuBgVip.Show(ExorSpawnPend.s_Equip);
		}
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_ExorSiBtn && w == m_ExorSiBtn)
		{
			GetGame().GetUIManager().EnterScriptedMenu(ExorMenuIDs.SERVERINFO, this);
			return true;
		}
		if (m_ExorMuBtnH && w == m_ExorMuBtnH)
		{
			ExorElegirGenero(0);
			return true;
		}
		if (m_ExorMuBtnM && w == m_ExorMuBtnM)
		{
			ExorElegirGenero(1);
			return true;
		}
		if (m_ExorMuBtnVip && w == m_ExorMuBtnVip)
		{
			ExorSpawnPend.s_Equip = !ExorSpawnPend.s_Equip;
			ExorMuRefresh();
			return true;
		}
		int i;
		for (i = 0; i < m_ExorMuPuntos.Count(); i++)
		{
			if (w != m_ExorMuPuntos.Get(i))
				continue;
			ExorSpawnMenuDTO dto = ExorSpawnClient.s_DTO;
			int real = i;
			if (dto && dto.punto_idx && i < dto.punto_idx.Count())
				real = dto.punto_idx.Get(i);
			ExorSpawnPend.s_Idx = real;	// se manda recien cuando el personaje este vivo
			ExorMuRefresh();
			return true;
		}
		return super.OnClick(w, x, y, button);
	}

	// El sexo NO va por RPC: se escribe en el dato que DayZ serializa y manda solo al
	// reaparecer. El classname exacto da igual, el server le mira el sexo y despues sortea
	// el tipo de su propia lista de spawns.json.
	void ExorElegirGenero(int genero)
	{
		MenuDefaultCharacterData d = GetGame().GetMenuDefaultCharacterData(false);
		if (!d)
			return;
		if (genero == 1)
			d.m_CharacterType = "SurvivorF_Eva";
		else
			d.m_CharacterType = "SurvivorM_Mirek";
		d.m_ForceRandomCharacter = false;
		ExorGeneroClient.s_Sel = genero;
		ExorMuRefresh();
	}

	// Sexo que tiene marcado AHORA el cliente (lo que se va a mandar al reaparecer).
	int ExorGeneroDelCliente()
	{
		MenuDefaultCharacterData d = GetGame().GetMenuDefaultCharacterData(false);
		if (!d || d.m_CharacterType == "")
			return -1;
		return ExorSpawn.GeneroDeTipo(d.m_CharacterType);
	}

	// Vanilla decide aca que botones se ven segun si el jugador esta vivo. Se aprovecha el
	// mismo punto para el panel: visible SOLO estando muerto, que es cuando la eleccion
	// todavia llega a tiempo para el personaje que se va a crear.
	override protected void UpdateGUI()
	{
		super.UpdateGUI();

		if (!m_ExorMuRoot)
			return;
		Man player = GetGame().GetPlayer();
		bool muerto = player && player.GetPlayerState() != EPlayerStates.ALIVE;
		bool mostrar = muerto && GetGame().IsMultiplayer();
		m_ExorMuRoot.Show(mostrar);
		if (!mostrar)
		{
			m_ExorMuEstabaMuerto = false;
			return;
		}

		// primer frame de ESTA muerte: se limpia lo elegido en la vida anterior
		if (!m_ExorMuEstabaMuerto)
		{
			m_ExorMuEstabaMuerto = true;
			ExorSpawnPend.Reset();
			ExorGeneroClient.s_Sel = ExorGeneroDelCliente();
			ExorMuRelayout();
		}
		ExorMuRefresh();
	}
}
