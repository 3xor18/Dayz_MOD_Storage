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
		// Auto-detecta cuantas filas ExorHudNameN hay en el layout (para 10+ miembros basta
		// agregar filas al .layout, sin tocar el codigo). Antes estaba fijo en 8.
		int i = 0;
		while (true)
		{
			TextWidget nm = TextWidget.Cast(m_Root.FindAnyWidget("ExorHudName" + i.ToString()));
			if (!nm)
				break;
			m_Names.Insert(nm);
			m_Bars.Insert(m_Root.FindAnyWidget("ExorHudBarBg" + i.ToString()));
			m_Fills.Insert(m_Root.FindAnyWidget("ExorHudBarFill" + i.ToString()));
			i++;
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
			// is_self LOCAL (ver ExorNameplates/ExorPartyLive): derivamos "soy yo" comparando
			// el network ID del miembro con el de nuestro propio player, en vez de un flag
			// serializado por receptor (el server ahora manda 1 solo payload por grupo).
			int myLo, myHi;
			p.GetNetworkID(myLo, myHi);
			int i;
			for (i = 0; i < live.members.Count() && shown < m_Names.Count(); i++)
			{
				ExorLiveMember m = live.members.Get(i);
				bool selfM = (myLo != 0 || myHi != 0) && m.nid_low == myLo && m.nid_high == myHi;
				if (selfM && !g.mostrar_propio)
					continue;	// solo amigos (salvo que mostrar_propio este on)
				float h = m.health / 100.0;	// vida viene cuantizada 0..100 -> pasar a 0..1
				if (h < 0) h = 0;
				if (h > 1) h = 1;

				string txt = m.name;
				if (!selfM && g.mostrar_distancia_miembros)
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
		for (j = shown; j < m_Names.Count(); j++)
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
	ref ExorChatHud m_ExorChat;
	float m_ExorHudAccum;
	bool m_ExorTPrev;
	bool m_ExorYPrev;
	bool m_ExorDotPrev;

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
		if (id == ExorMenuIDs.PARKING)
			return new ExorParkingMenu();
		if (id == ExorMenuIDs.LOCKKEY)
			return new ExorLockKeyMenu();
		return super.CreateScriptedMenu(id);
	}

	protected float m_ExorMarksAccum;	// acumulador del throttle de marcas 3D
	protected float m_ExorTextAccum;	// acumulador del throttle de killfeed + chat

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

		// Nameplates 3D: cada frame (la proyeccion sigue a la camara). Pasa el delta-time
		// para suavizar (interpolar) la posicion de los companeros entre updates de red.
		if (!m_ExorPlates)
		{
			m_ExorPlates = new ExorNameplates();
			m_ExorPlates.Create();
		}
		m_ExorPlates.Update(timeslice);

		// Marcas 3D: a 20 Hz, no por frame.
		// Los nameplates SI necesitan cada frame (siguen la camara y se notaria el tironeo),
		// pero las marcas del party son puntos fijos del mundo: a 20 Hz el ojo no distingue
		// la diferencia y en un mapa denso con 70 jugadores se recupera FPS de cliente.
		m_ExorMarksAccum += timeslice;
		if (m_ExorMarksAccum >= 0.05)
		{
			m_ExorMarksAccum = 0;
			if (!m_ExorMarks)
			{
				m_ExorMarks = new ExorMarkersHud();
				m_ExorMarks.Create();
			}
			m_ExorMarks.Update();
		}

		// Killfeed y chat: a 10 Hz. Los dos solo hacen barrer lineas VENCIDAS por timestamp
		// (nada se mueve ni se interpola), asi que hacerlo cada frame era gastar 60 pasadas
		// por segundo para un cambio que ocurre cada varios segundos.
		m_ExorTextAccum += timeslice;
		bool tocaTexto = (m_ExorTextAccum >= 0.1);
		if (tocaTexto)
			m_ExorTextAccum = 0;

		if (!m_ExorKf)
		{
			m_ExorKf = new ExorKillfeed();
			m_ExorKf.Create();
		}
		if (tocaTexto)
			m_ExorKf.Update();

		// Chat custom: crear una vez; el drenaje/expiracion va con el mismo throttle de 10 Hz
		if (!m_ExorChat)
		{
			m_ExorChat = new ExorChatHud();
			m_ExorChat.Create();
		}
		if (tocaTexto)
			m_ExorChat.Update();

		// Tecla "." -> cambiar canal de chat (global/zona)
		ExorChatKey();

		// Tecla M -> abrir/cerrar el mapa del party (lee UAMapToggle, accion existente)
		if (GetExorConfig().mapa.abrir_con_m && !GetGame().IsInventoryOpen())
		{
			UAInput inMap = GetUApi().GetInputByName("UAMapToggle");
			if (inMap && inMap.LocalPress())
				ExorToggleMap();
		}

		// Teclas T/Y -> marcas del party (lectura cruda de tecla, sin tocar config)
		ExorMarkerKeys();
		// (el auto-run se maneja en PlayerBase.ModCommandHandlerBefore, no aca:
		//  el override de movimiento necesita aplicarse en el tick del command handler)
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

	// Tecla "." -> togglea el canal de chat (GLOBAL/ZONA). Bloqueada si hay menu o
	// inventario abierto (asi el "." se escribe normal dentro de la caja de chat).
	void ExorChatKey()
	{
		bool blocked = GetGame().IsInventoryOpen();
		if (GetGame().GetUIManager() && GetGame().GetUIManager().GetMenu())
			blocked = true;

		bool dot = KeyState(KeyCode.KC_PERIOD) > 0;
		if (!blocked && dot && !m_ExorDotPrev)
		{
			ExorChat.Toggle();
			PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
			if (p)
				ExorAviso.Enviar(p, "Canal de chat: " + ExorChat.ChannelName());
		}
		m_ExorDotPrev = dot;
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
