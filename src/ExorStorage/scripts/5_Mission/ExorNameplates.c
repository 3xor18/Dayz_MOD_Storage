// ============================================================================
// 3xor_Vanilla_Optimization - Nameplates 3D del party (Fase E, cliente)
// Sobre cada miembro del party, flotando en el mundo: su nombre + distancia.
// Solo lo ven los miembros del party (usa la data sincronizada del party).
// Proyecta la posicion del mundo a la pantalla (GetScreenPos) cada frame.
// ============================================================================

// Posicion suavizada por compañero (steamid). La pos sincronizada del party llega
// a saltos (8 Hz + jitter de red en server online); en vez de teletransportar el
// nombre a cada update, lo movemos suave hacia el ultimo objetivo cada frame -> el
// nombre SIGUE al jugador fluido cuando corre, sin pedir mas red (todo client-side).
class ExorPlateInterp
{
	vector pos;
	bool seen;
}

class ExorNameplates
{
	protected Widget m_Root;
	protected bool m_Tried;
	protected ref array<TextWidget> m_Plates;
	protected ref map<string, ref ExorPlateInterp> m_Interp;

	// tiempo (seg) en que el nombre alcanza la pos nueva. Mas chico = mas pegado al
	// jugador (menos arrastre) pero menos suave; 0.12 = fluido y casi sin lag visible.
	static const float SMOOTH_TAU = 0.12;
	// si el objetivo salta mas que esto (respawn/teleport) NO suaviza: snap directo.
	static const float SNAP_DIST = 20.0;

	void Create()
	{
		if (m_Tried)
			return;
		m_Tried = true;
		m_Root = GetGame().GetWorkspace().CreateWidgets("ExorStorage/gui/exor_nameplates.layout");
		if (!m_Root)
			return;
		m_Plates = new array<TextWidget>;
		m_Interp = new map<string, ref ExorPlateInterp>;
		int i;
		for (i = 0; i < 8; i++)
			m_Plates.Insert(TextWidget.Cast(m_Root.FindAnyWidget("ExorPlate" + i.ToString())));
	}

	void Update(float dt)
	{
		if (!m_Root)
			return;

		ExorCfgPartyGrupo g = GetExorConfig().party.grupo;
		bool platesOn = g.habilitado && g.mostrar_posicion_miembros && g.mostrar_nameplates;

		PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
		ExorLiveDTO live;
		if (p && platesOn)
			live = ExorPartyClient.s_Live;

		// ocultar si hay un menu abierto
		bool menuOpen = GetGame().GetUIManager() && GetGame().GetUIManager().GetMenu() != null;

		// marcar todas las interpolaciones como "no vistas" este frame (para purgar despues
		// las de companeros que ya no estan: relog, salida del party, fuera de rango)
		foreach (string sk, ExorPlateInterp si : m_Interp)
			si.seen = false;

		int shown = 0;
		if (p && live && !menuOpen)
		{
			vector me = p.GetPosition();
			int i;
			for (i = 0; i < live.members.Count() && shown < 8; i++)
			{
				ExorLiveMember m = live.members.Get(i);
				if (m.is_self && !g.mostrar_propio)
					continue;	// solo amigos (salvo que mostrar_propio este on)

				// para uno mismo: posicion REAL local (siempre fluida);
				// para otros: la sincronizada, SUAVIZADA hacia el ultimo objetivo
				vector mpos;
				if (m.is_self)
				{
					mpos = me;
				}
				else
				{
					vector target = Vector(m.x, m.y, m.z);
					ExorPlateInterp it;
					if (!m_Interp.Find(m.steamid, it))
					{
						it = new ExorPlateInterp();
						it.pos = target;	// primera vez: arranca en su lugar (sin barrido)
						m_Interp.Set(m.steamid, it);
					}
					else if (vector.Distance(it.pos, target) > SNAP_DIST)
					{
						it.pos = target;	// teleport/respawn: snap, no arrastrar por el mapa
					}
					else
					{
						float a = dt / SMOOTH_TAU;	// frame-rate independiente (escala con dt)
						if (a > 1) a = 1;
						if (a < 0) a = 0;
						it.pos = it.pos + (target - it.pos) * a;
					}
					it.seen = true;
					mpos = it.pos;
				}

				vector world = mpos;
				world[1] = world[1] + 1.9;	// sobre la cabeza
				vector screen = GetGame().GetScreenPos(world);
				if (screen[2] <= 0)
					continue;	// detras de la camara

				TextWidget tw = m_Plates.Get(shown);
				if (!tw)
				{
					shown++;
					continue;
				}

				string plate = m.name;
				if (g.mostrar_distancia_miembros)
				{
					int dist = Math.Round(vector.Distance(me, mpos));
					plate = plate + " [" + dist.ToString() + "m]";
				}
				tw.SetText(plate);
				tw.SetPos(screen[0] - 130, screen[1]);	// centrar (ancho 260)
				tw.Show(true);
				shown++;
			}
		}

		// purgar las interpolaciones no vistas este frame (companeros que ya no aparecen)
		array<string> dead = new array<string>;
		foreach (string dk, ExorPlateInterp dv : m_Interp)
			if (!dv.seen)
				dead.Insert(dk);
		int d;
		for (d = 0; d < dead.Count(); d++)
			m_Interp.Remove(dead.Get(d));

		int j;
		for (j = shown; j < 8; j++)
			if (m_Plates.Get(j))
				m_Plates.Get(j).Show(false);
	}
}

// ============================================================================
// Marcas 3D del party: triangulo + nombre del que la puso + distancia.
// Se ponen con la accion "Poner marca" (donde apuntas) y se ven en el mundo.
// ============================================================================
class ExorMarkersHud
{
	protected Widget m_Root;
	protected bool m_Tried;
	protected ref array<TextWidget> m_Texts;
	protected ref array<Widget> m_Arrows;
	protected ref array<TextWidget> m_Dists;	// distancia en verde (solo VIP)

	void Create()
	{
		if (m_Tried)
			return;
		m_Tried = true;
		m_Root = GetGame().GetWorkspace().CreateWidgets("ExorStorage/gui/exor_markers.layout");
		if (!m_Root)
			return;
		m_Texts = new array<TextWidget>;
		m_Arrows = new array<Widget>;
		m_Dists = new array<TextWidget>;
		int i;
		for (i = 0; i < 8; i++)
		{
			m_Texts.Insert(TextWidget.Cast(m_Root.FindAnyWidget("ExorMarkText" + i.ToString())));
			m_Arrows.Insert(m_Root.FindAnyWidget("ExorMarkArrow" + i.ToString()));
			m_Dists.Insert(TextWidget.Cast(m_Root.FindAnyWidget("ExorMarkDist" + i.ToString())));
		}
	}

	void Update()
	{
		if (!m_Root)
			return;

		ExorCfgPartyGrupo g = GetExorConfig().party.grupo;
		bool marksOn = g.habilitado && g.permitir_marker_equipo;

		ExorMarkersDTO mk;
		if (marksOn)
			mk = ExorPartyClient.s_Markers;
		PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
		bool menuOpen = GetGame().GetUIManager() && GetGame().GetUIManager().GetMenu() != null;

		int shown = 0;
		if (p && mk && !menuOpen)
		{
			vector me = p.GetPosition();
			int i;
			for (i = 0; i < mk.markers.Count() && shown < 8; i++)
			{
				ExorMarker k = mk.markers.Get(i);
				vector mpos = Vector(k.x, k.y, k.z);
				vector world = Vector(k.x, k.y + 1.0, k.z);	// flota un poco sobre el suelo
				vector screen = GetGame().GetScreenPos(world);
				if (screen[2] <= 0)
					continue;

				TextWidget tw = m_Texts.Get(shown);
				Widget aw = m_Arrows.Get(shown);
				if (!tw)
				{
					shown++;
					continue;
				}

				// escala por distancia: cerca (<=25m) = tamano max, lejos = mas chico
				float dist = vector.Distance(me, mpos);
				if (dist < 1)
					dist = 1;
				float factor = 25.0 / dist;
				if (factor > 1)
					factor = 1;
				if (factor < 0.35)
					factor = 0.35;
				float triSize = 22.0 * factor;	// mas chico (antes 36) para no tapar tanto la vision

				// flecha (tip abajo) apuntando al punto, escalada
				if (aw)
				{
					aw.SetSize(triSize, triSize);
					aw.SetPos(screen[0] - triSize / 2.0, screen[1] - triSize);
					aw.Show(true);
				}
				// nombre arriba del triangulo
				tw.SetText(k.label);
				tw.SetPos(screen[0] - 100, screen[1] - triSize - 20);
				tw.Show(true);

				// distancia en verde, pegada despues del nombre. SOLO VIP; el resto
				// ve la marca igual que hoy (sin distancia).
				TextWidget dw = m_Dists.Get(shown);
				if (dw)
				{
					if (ExorVipClient.s_IsVip)
					{
						int nw, nh;
						tw.GetTextSize(nw, nh);	// ancho del nombre para pegar la distancia al lado
						int mts = Math.Round(dist);
						dw.SetText("[" + mts.ToString() + "mts]");
						dw.SetPos(screen[0] + (nw / 2.0) + 2, screen[1] - triSize - 20);
						dw.Show(true);
					}
					else
						dw.Show(false);
				}
				shown++;
			}
		}

		int j;
		for (j = shown; j < 8; j++)
		{
			if (m_Texts.Get(j)) m_Texts.Get(j).Show(false);
			if (m_Arrows.Get(j)) m_Arrows.Get(j).Show(false);
			if (m_Dists.Get(j)) m_Dists.Get(j).Show(false);
		}
	}
}
