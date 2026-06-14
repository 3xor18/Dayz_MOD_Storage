// ============================================================================
// 3xor_Vanilla_Optimization - Pantalla de seleccion de spawn (Fase F UI)
// Menu scripteado que aparece al morir (lo dispara el server con SPAWN_OPEN).
// Lista los puntos de spawns.json + opcion "Mi base". Al clickear, manda la
// eleccion al server (SPAWN_PICK) que teleporta. Hasta 6 puntos en pantalla.
// ============================================================================
class ExorSpawnMenu extends UIScriptedMenu
{
	protected ref array<ButtonWidget> m_Buttons;
	protected ButtonWidget m_BtnBase;

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

		ExorSpawnMenuDTO dto = ExorSpawnClient.s_DTO;
		int count = 0;
		if (dto)
			count = dto.nombres.Count();

		for (i = 0; i < 6; i++)
		{
			if (!m_Buttons.Get(i))
				continue;
			if (i < count)
			{
				m_Buttons.Get(i).SetText(dto.nombres.Get(i));
				m_Buttons.Get(i).Show(true);
			}
			else
			{
				m_Buttons.Get(i).Show(false);
			}
		}
		if (m_BtnBase)
			m_BtnBase.Show(dto && dto.base_enabled);

		return layoutRoot;
	}

	override void OnShow()
	{
		super.OnShow();
		// Tomar el foco del juego + mostrar cursor (asi el mouse selecciona sin mover la camara)
		GetGame().GetInput().ChangeGameFocus(1);
		GetGame().GetUIManager().ShowUICursor(true);
	}

	override void OnHide()
	{
		super.OnHide();
		GetGame().GetUIManager().ShowUICursor(false);
		GetGame().GetInput().ChangeGameFocus(-1);
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		super.OnClick(w, x, y, button);

		PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
		if (!p)
			return false;

		int i;
		for (i = 0; i < m_Buttons.Count(); i++)
		{
			if (w == m_Buttons.Get(i))
			{
				p.ExorReqSpawnPick(i);
				Close();
				return true;
			}
		}
		if (w == m_BtnBase)
		{
			p.ExorReqSpawnPick(-1);
			Close();
			return true;
		}
		return false;
	}

	override bool OnKeyPress(Widget w, int x, int y, int key)
	{
		if (key == KeyCode.KC_ESCAPE)
			Close();
		return false;
	}
}
