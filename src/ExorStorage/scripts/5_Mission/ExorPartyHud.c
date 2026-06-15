// ============================================================================
// 3xor_Vanilla_Optimization - HUD de party (Fase E, cliente)
// Arriba a la izquierda: por cada miembro del party, nombre + distancia + una
// barra de vida que se rellena (%) y cambia de color (verde/amarillo/rojo).
// Data sincronizada por ExorPartyLive (posicion/vida). Null-safe.
// ============================================================================
class ExorPartyHud
{
	protected Widget m_Root;
	protected bool m_Tried;
	protected ref array<TextWidget> m_Names;
	protected ref array<Widget> m_Bars;
	protected ref array<Widget> m_Fills;

	void Create()
	{
		if (m_Tried)
			return;
		m_Tried = true;

		m_Root = GetGame().GetWorkspace().CreateWidgets("ExorStorage/gui/exor_party_hud.layout");
		if (!m_Root)
			return;

		m_Names = new array<TextWidget>;
		m_Bars = new array<Widget>;
		m_Fills = new array<Widget>;
		int i;
		for (i = 0; i < 8; i++)
		{
			m_Names.Insert(TextWidget.Cast(m_Root.FindAnyWidget("ExorHudName" + i.ToString())));
			m_Bars.Insert(m_Root.FindAnyWidget("ExorHudBarBg" + i.ToString()));
			m_Fills.Insert(m_Root.FindAnyWidget("ExorHudBarFill" + i.ToString()));
		}
	}

	int HealthColor(float h)
	{
		if (h >= 0.7)
			return ARGB(255, 40, 210, 40);    // verde
		if (h >= 0.4)
			return ARGB(255, 230, 210, 30);   // amarillo
		return ARGB(255, 220, 40, 40);        // rojo
	}

	void Update()
	{
		if (!m_Root)
			return;

		// Gating por config: party activo + ver posicion + HUD activo
		ExorCfgPartyGrupo g = GetExorConfig().party.grupo;
		bool hudOn = g.habilitado && g.mostrar_posicion_miembros && g.mostrar_hud;

		PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
		ExorLiveDTO live;
		if (p && hudOn)
			live = ExorPartyClient.s_Live;

		int shown = 0;
		if (p && live)
		{
			vector me = p.GetPosition();
			int i;
			for (i = 0; i < live.members.Count() && shown < 8; i++)
			{
				ExorLiveMember m = live.members.Get(i);
				if (m.is_self && !g.mostrar_propio)
					continue;	// solo amigos (salvo que mostrar_propio este on)
				float h = m.health;
				if (h < 0) h = 0;
				if (h > 1) h = 1;

				string txt = m.name;
				if (!m.is_self && g.mostrar_distancia_miembros)
				{
					int dist = Math.Round(vector.Distance(me, Vector(m.x, m.y, m.z)));
					txt = txt + "  " + dist.ToString() + "m";
				}

				if (m_Names.Get(shown))
				{
					m_Names.Get(shown).SetText(txt);
					m_Names.Get(shown).Show(true);
				}
				if (m_Fills.Get(shown))
				{
					m_Fills.Get(shown).SetSize(h, 1.0);
					m_Fills.Get(shown).SetColor(HealthColor(h));
				}
				if (m_Bars.Get(shown))
					m_Bars.Get(shown).Show(true);
				shown++;
			}
		}

		// ocultar filas sin usar
		int j;
		for (j = shown; j < 8; j++)
		{
			if (m_Names.Get(j)) m_Names.Get(j).Show(false);
			if (m_Bars.Get(j)) m_Bars.Get(j).Show(false);
		}
	}
}

// ---------------------------------------------------------------------------
// Mision cliente: registra menus + crea/refresca el HUD (~cada 0.5 s).
// ---------------------------------------------------------------------------
modded class MissionGameplay
{
	ref ExorPartyHud m_ExorHud;
	ref ExorNameplates m_ExorPlates;
	ref ExorMarkersHud m_ExorMarks;
	ref ExorKillfeed m_ExorKf;
	float m_ExorHudAccum;
	bool m_ExorTPrev;
	bool m_ExorYPrev;

	override UIScriptedMenu CreateScriptedMenu(int id)
	{
		if (id == ExorMenuIDs.SPAWN)
			return new ExorSpawnMenu();
		if (id == ExorMenuIDs.PARTY)
			return new ExorPartyMenu();
		if (id == ExorMenuIDs.MAP)
			return new ExorMapMenu();
		if (id == ExorMenuIDs.SERVERINFO)
			return new ExorServerInfoMenu();
		return super.CreateScriptedMenu(id);
	}

	override void OnUpdate(float timeslice)
	{
		super.OnUpdate(timeslice);

		if (!m_ExorHud)
		{
			m_ExorHud = new ExorPartyHud();
			m_ExorHud.Create();
		}

		m_ExorHudAccum += timeslice;
		if (m_ExorHudAccum >= 0.5)
		{
			m_ExorHudAccum = 0;
			m_ExorHud.Update();
		}

		// Nameplates 3D: cada frame (la proyeccion sigue a la camara)
		if (!m_ExorPlates)
		{
			m_ExorPlates = new ExorNameplates();
			m_ExorPlates.Create();
		}
		m_ExorPlates.Update();

		// Marcas 3D: cada frame
		if (!m_ExorMarks)
		{
			m_ExorMarks = new ExorMarkersHud();
			m_ExorMarks.Create();
		}
		m_ExorMarks.Update();

		// Killfeed: crear una vez y barrer lineas vencidas cada frame
		if (!m_ExorKf)
		{
			m_ExorKf = new ExorKillfeed();
			m_ExorKf.Create();
		}
		m_ExorKf.Update();

		// Tecla M -> abrir/cerrar el mapa del party (lee UAMapToggle, accion existente)
		if (GetExorConfig().mapa.abrir_con_m && !GetGame().IsInventoryOpen())
		{
			UAInput inMap = GetUApi().GetInputByName("UAMapToggle");
			if (inMap && inMap.LocalPress())
				ExorToggleMap();
		}

		// Teclas T/Y -> marcas del party (lectura cruda de tecla, sin tocar config)
		ExorMarkerKeys();
	}

	// Detecta flanco de subida de T (poner marca) e Y (limpiar mis marcas).
	// Usa KeyState (no requiere cfgInputActions). Bloqueado si hay menu/inventario.
	void ExorMarkerKeys()
	{
		ExorCfgPartyGrupo g = GetExorConfig().party.grupo;
		if (!g.habilitado || !g.permitir_marker_equipo)
			return;

		PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
		bool blocked = GetGame().IsInventoryOpen();
		if (GetGame().GetUIManager() && GetGame().GetUIManager().GetMenu())
			blocked = true;
		if (!p || !p.IsAlive() || !p.ExorClientInGroup())
			blocked = true;

		bool tDown = KeyState(KeyCode.KC_T) > 0;
		bool yDown = KeyState(KeyCode.KC_Y) > 0;

		if (!blocked)
		{
			if (tDown && !m_ExorTPrev)
			{
				vector aim = ExorActionPlaceMarker.ExorMarkerAim(p);
				p.ExorReqMarkerAdd(aim);
			}
			if (yDown && !m_ExorYPrev)
				p.ExorReqMarkerClear();
		}

		m_ExorTPrev = tDown;
		m_ExorYPrev = yDown;
	}

	void ExorToggleMap()
	{
		UIManager ui = GetGame().GetUIManager();
		if (!ui)
			return;
		if (ui.GetMenu() && ui.GetMenu().GetID() == ExorMenuIDs.MAP)
			ui.Back();
		else if (!ui.GetMenu())
			ui.EnterScriptedMenu(ExorMenuIDs.MAP, null);
	}

	// Forzar abrir el inventario estando en un vehiculo (vanilla bloquea al
	// conductor via CanOpenInventory). Replica la apertura sin ese gate cuando
	// vehiculos.inventario.ver_ambos_dentro esta on. Asi ves tu gear + el del auto.
	override void ShowInventory()
	{
		PlayerBase p = PlayerBase.Cast(g_Game.GetPlayer());
		bool forzar = p && p.GetCommand_Vehicle() && GetExorConfig().vehiculos.inventario.ver_ambos_dentro && !GetUIManager().GetMenu() && !p.IsInventorySoftLocked();
		if (forzar)
		{
			if (!m_InventoryMenu)
				InitInventory();
			if (!GetUIManager().FindMenu(MENU_INVENTORY))
			{
				GetUIManager().ShowScriptedMenu(m_InventoryMenu, null);
				p.OnInventoryMenuOpen();
			}
			AddActiveInputExcludes({"inventory"});
			AddActiveInputRestriction(EInputRestrictors.INVENTORY);
			return;
		}
		super.ShowInventory();
	}
}
