// ============================================================================
// 3xor_Vanilla_Optimization - Mapa del party (Fase E)
// Se abre con M (UAMapToggle). Muestra el mapa del mundo con marcadores:
// tu posicion (azul), los miembros del party (verde) y las marcas (amarillo).
// Usa el MapWidget vanilla + AddUserMark (posiciones del mundo).
// ============================================================================
class ExorMapMenu extends UIScriptedMenu
{
	protected MapWidget m_Map;
	protected bool m_MReleased;	// la M que abrio el mapa todavia esta apretada: esperar a soltarla
	protected bool m_Centered;	// ya se centro el mapa en el jugador (1ra vez en Update)

	override Widget Init()
	{
		layoutRoot = GetGame().GetWorkspace().CreateWidgets("ExorStorage/gui/exor_map.layout");
		if (!layoutRoot)
			return null;
		m_Map = MapWidget.Cast(layoutRoot.FindAnyWidget("ExorMapWidget"));

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

		bool mDown = KeyState(KeyCode.KC_M) > 0;
		if (!mDown)
			m_MReleased = true;
		if ((mDown && m_MReleased) || KeyState(KeyCode.KC_ESCAPE) > 0)
		{
			Close();
			return;
		}
	}
}
