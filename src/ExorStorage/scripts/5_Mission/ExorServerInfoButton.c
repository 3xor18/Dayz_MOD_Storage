// ============================================================================
// 3xor_Vanilla_Optimization - Boton "Server Info" en el menu de ESC (cliente)
// Inyecta un boton (mismo estilo vanilla) en la lista de botones del menu de
// pausa, colgandolo del MISMO padre que 'continuebtn'. Al click abre el panel.
// El hover/color lo da gratis el OnMouseEnter heredado (respetamos el naming
// *_panel / *_label). Se puede desactivar con serverinfo.habilitado=false.
// ============================================================================
modded class InGameMenu
{
	protected ButtonWidget m_ExorSiBtn;

	override Widget Init()
	{
		Widget root = super.Init();

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

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_ExorSiBtn && w == m_ExorSiBtn)
		{
			GetGame().GetUIManager().EnterScriptedMenu(ExorMenuIDs.SERVERINFO, this);
			return true;
		}
		return super.OnClick(w, x, y, button);
	}
}
