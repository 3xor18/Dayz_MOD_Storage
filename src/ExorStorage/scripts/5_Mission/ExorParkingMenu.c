// ============================================================================
// 3xor_Vanilla_Optimization - Menu de PARKING (administrar autos del clan)
// ============================================================================
// Se abre al interactuar con el parking ("Administrar autos"). DOS paneles:
//   - IZQUIERDA "Almacenados"   = autos VIRTUALIZADOS del clan (a disco).
//   - DERECHA   "Sin Almacenar" = autos REALES en el radio del parking.
// Se selecciona UN auto (se pone verde; solo uno a la vez en cualquiera de los dos
// paneles) y con las flechas del medio:
//   - "<< Guardar" (apunta a Almacenados) = VIRTUALIZAR el auto real seleccionado.
//   - "Sacar >>"   (apunta a Sin Almacenar) = DESVIRTUALIZAR el almacenado seleccionado.
// Tras la accion el server re-manda la lista (PARKING_OPEN) -> ExorRefresh() repinta.
// La logica (permisos, virtualizar/spawnear) es toda server-side; ver ExorStorage_Player.c
// (ExorDoParkingVirt/Spawn) + ExorVehicleGarage.
// ============================================================================
class ExorParkingMenu extends UIScriptedMenu
{
	protected Widget m_ListStored;		// WrapSpacer izquierda (almacenados)
	protected Widget m_ListReal;		// WrapSpacer derecha (reales)
	protected ButtonWidget m_BtnClose;
	protected TextWidget m_Info;

	// filas dinamicas + su dato asociado (indices paralelos)
	protected ref array<ButtonWidget> m_StoredBtns;
	protected ref array<string> m_StoredIds;
	protected ref array<ButtonWidget> m_RealBtns;
	protected ref array<int> m_RealLow;
	protected ref array<int> m_RealHigh;

	// seleccion actual (un solo auto en cualquiera de los dos paneles)
	protected ButtonWidget m_Selected;
	protected bool m_SelIsStored;
	protected string m_SelId;
	protected int m_SelLow;
	protected int m_SelHigh;

	// version del cache vista por ultima vez: si el server manda una lista nueva
	// (ExorParkingClient.Set incrementa s_Version), el Update() la detecta y repinta.
	protected int m_LastVersion;

	protected int COL_NORMAL;	// gris-azulado (no seleccionado)
	protected int COL_SEL;		// verde (seleccionado)

	override Widget Init()
	{
		COL_NORMAL = ARGB(255, 51, 51, 77);
		COL_SEL    = ARGB(255, 46, 160, 46);	// verde vivo (auto seleccionado)

		layoutRoot = GetGame().GetWorkspace().CreateWidgets("ExorStorage/gui/exor_parking_menu.layout");
		if (!layoutRoot)
			return null;

		m_StoredBtns = new array<ButtonWidget>;
		m_StoredIds = new array<string>;
		m_RealBtns = new array<ButtonWidget>;
		m_RealLow = new array<int>;
		m_RealHigh = new array<int>;

		m_ListStored = layoutRoot.FindAnyWidget("ExorParkingListStored");
		m_ListReal = layoutRoot.FindAnyWidget("ExorParkingListReal");
		m_BtnClose = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ExorParkingBtnClose"));
		m_Info = TextWidget.Cast(layoutRoot.FindAnyWidget("ExorParkingInfo"));

		ExorRefresh();
		return layoutRoot;
	}

	// borra las filas creadas dinamicamente en un contenedor
	void ExorClearList(Widget list)
	{
		if (!list)
			return;
		Widget c = list.GetChildren();
		while (c)
		{
			Widget next = c.GetSibling();
			c.Unlink();
			c = next;
		}
	}

	void ExorClearRows()
	{
		ExorClearList(m_ListStored);
		ExorClearList(m_ListReal);
		m_StoredBtns.Clear();
		m_StoredIds.Clear();
		m_RealBtns.Clear();
		m_RealLow.Clear();
		m_RealHigh.Clear();
		m_Selected = null;
	}

	// crea una fila-boton en 'list' con el texto dado; devuelve el boton (o null)
	ButtonWidget ExorMakeRow(Widget list, string text)
	{
		Widget rw = GetGame().GetWorkspace().CreateWidgets("ExorStorage/gui/exor_parking_row.layout", list);
		if (!rw)
			return null;
		ButtonWidget btn = ButtonWidget.Cast(rw);
		if (!btn)
			btn = ButtonWidget.Cast(rw.FindAnyWidget("ExorParkingRowBtn"));
		if (!btn)
			return null;
		btn.SetText(text);
		btn.SetColor(COL_NORMAL);
		return btn;
	}

	// (re)dibuja ambas listas desde el cache del cliente + repinta las flechas
	void ExorRefresh()
	{
		ExorClearRows();

		ExorParkingMenuDTO dto = ExorParkingClient.s_DTO;
		int i;
		ButtonWidget b;
		if (dto)
		{
			for (i = 0; i < dto.almacenados.Count(); i++)
			{
				ExorParkingCarDTO a = dto.almacenados.Get(i);
				b = ExorMakeRow(m_ListStored, a.name);
				if (!b)
					continue;
				m_StoredBtns.Insert(b);
				m_StoredIds.Insert(a.id);
			}
			for (i = 0; i < dto.reales.Count(); i++)
			{
				ExorParkingCarDTO r = dto.reales.Get(i);
				b = ExorMakeRow(m_ListReal, r.name);
				if (!b)
					continue;
				m_RealBtns.Insert(b);
				m_RealLow.Insert(r.netLow);
				m_RealHigh.Insert(r.netHigh);
			}
		}
		ExorUpdateInfo();
		m_LastVersion = ExorParkingClient.s_Version;	// marcamos la version dibujada
	}

	// texto de arriba: contadores + como se usa (doble click)
	void ExorUpdateInfo()
	{
		if (m_Info)
		{
			int nS = m_StoredBtns.Count();
			int nR = m_RealBtns.Count();
			m_Info.SetText(string.Format("Almacenados: %1   |   Cerca: %2   -   Doble click sobre el auto que quieres interactuar", nS, nR));
		}
	}

	// deja verde el boton elegido y el resto en normal
	void ExorPaintSelection()
	{
		int i;
		for (i = 0; i < m_StoredBtns.Count(); i++)
		{
			if (m_StoredBtns.Get(i) == m_Selected)
				m_StoredBtns.Get(i).SetColor(COL_SEL);
			else
				m_StoredBtns.Get(i).SetColor(COL_NORMAL);
		}
		for (i = 0; i < m_RealBtns.Count(); i++)
		{
			if (m_RealBtns.Get(i) == m_Selected)
				m_RealBtns.Get(i).SetColor(COL_SEL);
			else
				m_RealBtns.Get(i).SetColor(COL_NORMAL);
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

	// CLICK SIMPLE = solo seleccionar (resalta en verde para saber cual vas a mover).
	override bool OnClick(Widget w, int x, int y, int button)
	{
		super.OnClick(w, x, y, button);

		int i;
		for (i = 0; i < m_StoredBtns.Count(); i++)
		{
			if (w == m_StoredBtns.Get(i))
			{
				m_Selected = m_StoredBtns.Get(i);
				m_SelIsStored = true;
				m_SelId = m_StoredIds.Get(i);
				ExorPaintSelection();
				return true;
			}
		}
		for (i = 0; i < m_RealBtns.Count(); i++)
		{
			if (w == m_RealBtns.Get(i))
			{
				m_Selected = m_RealBtns.Get(i);
				m_SelIsStored = false;
				m_SelLow = m_RealLow.Get(i);
				m_SelHigh = m_RealHigh.Get(i);
				ExorPaintSelection();
				return true;
			}
		}
		if (w == m_BtnClose)
		{
			Close();
			return true;
		}
		return false;
	}

	// DOBLE CLICK = accion directa: sobre un REAL lo GUARDA (virtualiza); sobre un
	// ALMACENADO lo SACA (desvirtualiza). El server refresca ambas listas al terminar.
	override bool OnDoubleClick(Widget w, int x, int y, int button)
	{
		super.OnDoubleClick(w, x, y, button);
		PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
		if (!p)
			return false;

		int i;
		// doble click sobre un ALMACENADO -> sacar
		for (i = 0; i < m_StoredBtns.Count(); i++)
		{
			if (w == m_StoredBtns.Get(i))
			{
				p.ExorReqParkingSpawn(m_StoredIds.Get(i));
				m_Selected = null;
				return true;
			}
		}
		// doble click sobre un REAL -> guardar
		for (i = 0; i < m_RealBtns.Count(); i++)
		{
			if (w == m_RealBtns.Get(i))
			{
				p.ExorReqParkingVirt(m_RealLow.Get(i), m_RealHigh.Get(i));
				m_Selected = null;
				return true;
			}
		}
		return false;
	}

	override bool OnKeyPress(Widget w, int x, int y, int key)
	{
		if (key == KeyCode.KC_ESCAPE)
			Close();
		return false;
	}

	// ESC crudo (el engine a veces se come el OnKeyPress), igual que el mapa/party.
	// Ademas: si el server mando una lista nueva (bump de version) mientras el menu esta
	// abierto -> repintar (asi las flechas actualizan ambos paneles tras guardar/sacar).
	override void Update(float timeslice)
	{
		super.Update(timeslice);
		if (KeyState(KeyCode.KC_ESCAPE) > 0)
		{
			Close();
			return;
		}
		if (ExorParkingClient.s_Version != m_LastVersion)
			ExorRefresh();
		// Reaplicar el resaltado cada frame: el estilo del boton (rover_sim_colorable) pisa
		// nuestro SetColor en hover/foco, asi que el verde del seleccionado se perdia. Volver
		// a pintarlo cada frame lo mantiene fijo (barato: son pocos botones). Igual que el spawn.
		if (m_Selected)
			ExorPaintSelection();
	}
}
