// ============================================================================
// 3xor_Vanilla_Optimization - Mapa del party (Fase E)
// Se abre con M (UAMapToggle). Muestra el mapa del mundo con marcadores:
// tu posicion (azul), los miembros del party (verde) y las marcas (amarillo).
// Usa el MapWidget vanilla + AddUserMark (posiciones del mundo).
//
// + Marcas PERSONALES (PIN): boton "Nueva marca" -> clic en el mapa -> nombre +
//   categoria (base enemiga/loot/otro). Privadas, guardadas en el cliente
//   (ExorMapPins). Boton "Borrar" -> clic sobre una marca para eliminarla.
// ============================================================================
class ExorMapMenu extends UIScriptedMenu
{
	protected MapWidget m_Map;
	protected bool m_MReleased;	// la M que abrio el mapa todavia esta apretada: esperar a soltarla
	protected bool m_EscReleased;	// igual para ESC: tras cancelar el input con ESC, re-pedir soltar antes de cerrar el mapa
	protected bool m_Centered;	// ya se centro el mapa en el jugador (1ra vez en Update)

	// marcas personales (PIN)
	protected ref ExorMapClickHandler m_ClickHandler;
	protected TextWidget m_Hint;
	protected ButtonWidget m_BtnNew;
	protected ButtonWidget m_BtnDel;
	protected Widget m_InputPanel;
	protected EditBoxWidget m_NameEdit;
	protected ButtonWidget m_CatBase;
	protected ButtonWidget m_CatLoot;
	protected ButtonWidget m_CatOtro;
	protected ButtonWidget m_BtnCancel;

	protected bool m_Placing;	// esperando el clic en el mapa para ubicar la marca
	protected bool m_Deleting;	// esperando el clic sobre una marca para borrarla
	protected bool m_InputOpen;	// el panel de nombre/categoria esta abierto
	protected vector m_PendingPos;	// donde se va a crear la marca pendiente

	override Widget Init()
	{
		layoutRoot = GetGame().GetWorkspace().CreateWidgets("ExorStorage/gui/exor_map.layout");
		if (!layoutRoot)
			return null;
		m_Map = MapWidget.Cast(layoutRoot.FindAnyWidget("ExorMapWidget"));

		m_Hint    = TextWidget.Cast(layoutRoot.FindAnyWidget("ExorMapHint"));
		m_BtnNew  = ButtonWidget.Cast(layoutRoot.FindAnyWidget("btn_new_pin"));
		m_BtnDel  = ButtonWidget.Cast(layoutRoot.FindAnyWidget("btn_del_pin"));
		m_InputPanel = layoutRoot.FindAnyWidget("pin_input");
		m_NameEdit = EditBoxWidget.Cast(layoutRoot.FindAnyWidget("pin_name_edit"));
		m_CatBase = ButtonWidget.Cast(layoutRoot.FindAnyWidget("cat_base"));
		m_CatLoot = ButtonWidget.Cast(layoutRoot.FindAnyWidget("cat_loot"));
		m_CatOtro = ButtonWidget.Cast(layoutRoot.FindAnyWidget("cat_otro"));
		m_BtnCancel = ButtonWidget.Cast(layoutRoot.FindAnyWidget("pin_cancel"));

		if (m_InputPanel)
			m_InputPanel.Show(false);

		m_EscReleased = true;	// ESC cierra el mapa de una (salvo justo tras cancelar un input)

		// enganchar el handler de clics al MapWidget (no rompe paneo/zoom nativo)
		if (m_Map)
		{
			m_ClickHandler = new ExorMapClickHandler(this);
			m_Map.SetHandler(m_ClickHandler);
		}

		UpdateHint();

		// El centrado real se hace en el 1er tick del Update (aca el widget aun no
		// tiene tamaño, asi que SetMapPos no agarra y el mapa abre descentrado).
		RefreshMarks();
		return layoutRoot;
	}

	void RefreshMarks()
	{
		if (!m_Map)
			return;
		m_Map.ClearUserMarks();

		ExorCfgMapa cfg = GetExorConfig().mapa;
		string icon = "ExorStorage\\data\\exor_triangle_ca.paa";
		PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());

		// tu posicion (verde llamativo)
		if (p && cfg.mostrar_mi_posicion)
			m_Map.AddUserMark(p.GetPosition(), "Vos", ARGB(255, 50, 245, 80), icon);

		// miembros del party (verde) - requiere party activo + ver posicion
		ExorCfgPartyGrupo g = GetExorConfig().party.grupo;
		bool verMiembros = cfg.mostrar_miembros_party && g.habilitado && g.mostrar_posicion_miembros;
		ExorLiveDTO live = ExorPartyClient.s_Live;
		if (live && verMiembros)
		{
			int i;
			for (i = 0; i < live.members.Count(); i++)
			{
				ExorLiveMember m = live.members.Get(i);
				if (m.is_self)
					continue;
				m_Map.AddUserMark(Vector(m.x, m.y, m.z), m.name, ARGB(255, 60, 220, 60), icon);
			}
		}

		// marcas del party (amarillo) - requiere party activo + marcas permitidas
		ExorMarkersDTO mk = ExorPartyClient.s_Markers;
		if (mk && g.habilitado && g.permitir_marker_equipo)
		{
			int j;
			for (j = 0; j < mk.markers.Count(); j++)
			{
				ExorMarker k = mk.markers.Get(j);
				m_Map.AddUserMark(Vector(k.x, k.y, k.z), k.label, ARGB(255, 230, 210, 30), icon);
			}
		}

		// marcas PERSONALES (PIN) - color por categoria (rojo/verde/blanco)
		array<ref ExorMapPin> pins = ExorMapPins.Get();
		int n;
		for (n = 0; n < pins.Count(); n++)
		{
			ExorMapPin pin = pins.Get(n);
			m_Map.AddUserMark(Vector(pin.x, pin.y, pin.z), pin.label, ExorMapPins.CatColor(pin.cat), icon);
		}
	}

	// --------------------------- marcas personales ---------------------------

	// Devuelto por el handler de clics del mapa (coords de PANTALLA). Devuelve true
	// si consumimos el clic (estabamos colocando/borrando) para que no panee.
	bool OnMapClick(int sx, int sy, int button)
	{
		if (button != MouseState.LEFT)
			return false;

		if (m_Placing && m_Map)
		{
			m_PendingPos = m_Map.ScreenToMap(Vector(sx, sy, 0));
			m_Placing = false;
			OpenInput();
			return true;
		}

		if (m_Deleting && m_Map)
		{
			DeleteNearest(sx, sy);
			m_Deleting = false;
			UpdateHint();
			return true;
		}

		return false;
	}

	// borra la marca personal mas cercana al clic (dentro de ~30px de pantalla)
	void DeleteNearest(int sx, int sy)
	{
		array<ref ExorMapPin> pins = ExorMapPins.Get();
		int best = -1;
		float bestD = 30.0 * 30.0;	// umbral en px^2
		int i;
		for (i = 0; i < pins.Count(); i++)
		{
			ExorMapPin pin = pins.Get(i);
			vector sp = m_Map.MapToScreen(Vector(pin.x, pin.y, pin.z));
			float dx = sp[0] - sx;
			float dy = sp[1] - sy;
			float d = dx * dx + dy * dy;
			if (d < bestD)
			{
				bestD = d;
				best = i;
			}
		}
		if (best >= 0)
		{
			ExorMapPins.RemoveAt(best);
			RefreshMarks();
		}
	}

	void OpenInput()
	{
		m_InputOpen = true;
		// al volver del input, re-pedir soltar M y ESC antes de que cierren el mapa
		m_MReleased = false;
		m_EscReleased = false;
		if (m_NameEdit)
			m_NameEdit.SetText("");
		if (m_InputPanel)
			m_InputPanel.Show(true);
		UpdateHint();
	}

	void CloseInput()
	{
		m_InputOpen = false;
		if (m_InputPanel)
			m_InputPanel.Show(false);
		UpdateHint();
	}

	void SavePending(int cat)
	{
		string name = "";
		if (m_NameEdit)
			name = m_NameEdit.GetText();
		ExorMapPins.Add(m_PendingPos, name, cat);
		CloseInput();
		RefreshMarks();
	}

	void UpdateHint()
	{
		if (!m_Hint)
			return;
		if (m_InputOpen)
			m_Hint.SetText("Escribí el nombre y elegí categoría (Base enemiga / Loot / Otro)");
		else if (m_Placing)
			m_Hint.SetText("Hacé clic en el mapa donde querés la marca   (clic derecho = cancelar)");
		else if (m_Deleting)
			m_Hint.SetText("Hacé clic sobre una marca tuya para borrarla   (clic derecho = cancelar)");
		else
			m_Hint.SetText("M / ESC cierra  -  rojo: base enemiga · verde: loot · blanco: otro  -  verde: party · amarillo: marcas T");
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (w == m_BtnNew)
		{
			m_Deleting = false;
			m_Placing = !m_Placing;	// toggle
			if (m_InputOpen)
				CloseInput();
			UpdateHint();
			return true;
		}
		if (w == m_BtnDel)
		{
			m_Placing = false;
			if (m_InputOpen)
				CloseInput();
			m_Deleting = !m_Deleting;	// toggle
			UpdateHint();
			return true;
		}
		if (w == m_CatBase) { SavePending(0); return true; }
		if (w == m_CatLoot) { SavePending(1); return true; }
		if (w == m_CatOtro) { SavePending(2); return true; }
		if (w == m_BtnCancel) { CloseInput(); return true; }
		return super.OnClick(w, x, y, button);
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

	override bool OnKeyPress(Widget w, int x, int y, int key)
	{
		// Si el panel de nombre esta abierto, ESC cancela el INPUT (no cierra el mapa)
		// y la M se escribe normal en la caja.
		if (m_InputOpen)
		{
			if (key == KeyCode.KC_ESCAPE)
				CloseInput();
			return false;
		}
		if (key == KeyCode.KC_ESCAPE || key == KeyCode.KC_M)
			Close();
		return false;
	}

	// El menu tiene foco de cursor y OnKeyPress no siempre llega, asi que leemos
	// las teclas crudas: M o ESC cierran. La M que abrio el mapa hay que soltarla
	// primero (si no, se cerraria en el mismo frame que se abre).
	override void Update(float timeslice)
	{
		super.Update(timeslice);

		// Centrar + zoom en TU posicion la 1ra vez (ya con el mapa dimensionado).
		if (!m_Centered && m_Map)
		{
			PlayerBase me = PlayerBase.Cast(GetGame().GetPlayer());
			if (me)
			{
				m_Map.SetScale(0.18);
				m_Map.SetMapPos(me.GetPosition());
				m_Centered = true;
			}
		}

		// Mientras se escribe el nombre, NO cerrar el mapa con M/ESC (la M es texto;
		// ESC cancela el input via OnKeyPress). Asi no se cierra al tipear.
		if (m_InputOpen)
		{
			m_MReleased = false;	// re-pedir soltar M/ESC al volver del input (no cerrar el mapa)
			m_EscReleased = false;
			return;
		}

		bool mDown = KeyState(KeyCode.KC_M) > 0;
		if (!mDown)
			m_MReleased = true;
		bool escDown = KeyState(KeyCode.KC_ESCAPE) > 0;
		if (!escDown)
			m_EscReleased = true;
		if ((mDown && m_MReleased) || (escDown && m_EscReleased))
		{
			Close();
			return;
		}
	}
}
