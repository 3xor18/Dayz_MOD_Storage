// ============================================================================
// 3xor_Vanilla_Optimization - Modal de CLAVE del locker
// ============================================================================
// Se abre al poner/cambiar clave (2 inputs: nueva + repetir, deben coincidir) o al meter
// la clave para abrir (1 input). Los inputs son EditBox SIN ofuscar (se ve lo que se
// escribe). OK guarda/confirma, Cancelar y ESC cierran. El modo lo trae ExorLockKeyClient.
// La logica real (permisos, guardar la clave, abrir) es server-side (ExorDoLockSubmit).
// ============================================================================
class ExorLockKeyMenu extends UIScriptedMenu
{
	protected TextWidget m_Title;
	protected TextWidget m_Sub;
	protected TextWidget m_Error;
	protected Widget m_Box2;			// contenedor del 2do input (se oculta en modo METER)
	protected EditBoxWidget m_Input1;
	protected EditBoxWidget m_Input2;
	protected ButtonWidget m_BtnOk;
	protected ButtonWidget m_BtnCancel;

	protected int m_Mode;				// 0 = meter clave (abrir), 1 = setear/cambiar
	protected int m_LastVersion;

	override Widget Init()
	{
		layoutRoot = GetGame().GetWorkspace().CreateWidgets("ExorStorage/gui/exor_locker_key.layout");
		if (!layoutRoot)
			return null;

		m_Title = TextWidget.Cast(layoutRoot.FindAnyWidget("ExorLockKeyTitle"));
		m_Sub = TextWidget.Cast(layoutRoot.FindAnyWidget("ExorLockKeySub"));
		m_Error = TextWidget.Cast(layoutRoot.FindAnyWidget("ExorLockKeyError"));
		m_Box2 = layoutRoot.FindAnyWidget("ExorLockKeyBox2");
		m_Input1 = EditBoxWidget.Cast(layoutRoot.FindAnyWidget("ExorLockKeyInput1"));
		m_Input2 = EditBoxWidget.Cast(layoutRoot.FindAnyWidget("ExorLockKeyInput2"));
		m_BtnOk = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ExorLockKeyBtnOk"));
		m_BtnCancel = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ExorLockKeyBtnCancel"));

		ExorApplyMode();
		return layoutRoot;
	}

	void ExorApplyMode()
	{
		m_Mode = ExorLockKeyClient.s_Mode;
		m_LastVersion = ExorLockKeyClient.s_Version;

		if (m_Input1) m_Input1.SetText("");
		if (m_Input2) m_Input2.SetText("");
		if (m_Error) m_Error.Show(false);

		if (m_Mode == ExorLockKeyClient.MODE_SET)
		{
			if (m_Title) m_Title.SetText("PONER / CAMBIAR CLAVE");
			if (m_Sub) m_Sub.SetText("Escribí la clave nueva y repetila. Deben coincidir.");
			if (m_Box2) m_Box2.Show(true);
		}
		else
		{
			if (m_Title) m_Title.SetText("CLAVE DEL LOCKER");
			if (m_Sub) m_Sub.SetText("Ingresá la clave para abrir.");
			if (m_Box2) m_Box2.Show(false);
		}
	}

	void ExorShowError(string msg)
	{
		if (m_Error)
		{
			m_Error.SetText(msg);
			m_Error.Show(true);
		}
	}

	void ExorSubmit()
	{
		PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
		if (!p || !m_Input1)
			return;
		string k1 = m_Input1.GetText();

		if (m_Mode == ExorLockKeyClient.MODE_SET)
		{
			string k2 = "";
			if (m_Input2)
				k2 = m_Input2.GetText();
			if (k1 == "")
			{
				ExorShowError("La clave no puede estar vacía.");
				return;
			}
			if (k1 != k2)
			{
				ExorShowError("Las claves no coinciden.");
				return;
			}
			p.ExorReqLockSubmit(ExorLockKeyClient.MODE_SET, k1);
			Close();
		}
		else
		{
			if (k1 == "")
			{
				ExorShowError("Ingresá la clave.");
				return;
			}
			p.ExorReqLockSubmit(ExorLockKeyClient.MODE_ENTER, k1);
			Close();
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

	override bool OnClick(Widget w, int x, int y, int button)
	{
		super.OnClick(w, x, y, button);
		if (w == m_BtnOk)
		{
			ExorSubmit();
			return true;
		}
		if (w == m_BtnCancel)
		{
			Close();
			return true;
		}
		return false;
	}

	override bool OnKeyPress(Widget w, int x, int y, int key)
	{
		if (key == KeyCode.KC_ESCAPE)
		{
			Close();
			return true;
		}
		if (key == KeyCode.KC_RETURN || key == KeyCode.KC_NUMPADENTER)
		{
			ExorSubmit();
			return true;
		}
		return false;
	}

	override void Update(float timeslice)
	{
		super.Update(timeslice);
		if (KeyState(KeyCode.KC_ESCAPE) > 0)
		{
			Close();
			return;
		}
		// si el server reabrio el modal en otro modo mientras estaba abierto -> reconfigurar
		if (ExorLockKeyClient.s_Version != m_LastVersion)
			ExorApplyMode();
	}
}
