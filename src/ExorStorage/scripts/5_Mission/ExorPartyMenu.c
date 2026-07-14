// ============================================================================
// 3xor_Vanilla_Optimization - Menu de party (Fase E)
// Se abre con "Administrar party" apuntando al mastil. Lista los miembros como
// botones; al tocar uno, pregunta "Expulsar? Si/No". Cualquier miembro puede
// expulsar a otro (menos al dueno). Tambien "Salir del party".
// ============================================================================
class ExorPartyMenu extends UIScriptedMenu
{
	// Lista de miembros DINAMICA (una fila-boton por miembro) dentro de un ScrollWidget.
	// Antes eran 8 botones FIJOS en el layout -> un clan de 10 solo mostraba 8. Ahora se
	// crean tantas filas como miembros tenga el roster y el scroll las hace navegables.
	protected ref array<ButtonWidget> m_Members;
	protected ref array<string> m_MemberSids;
	protected Widget m_List;		// WrapSpacer contenedor de las filas (dentro del scroll)
	protected ButtonWidget m_BtnLeave;
	protected ButtonWidget m_BtnClose;
	protected Widget m_Confirm;
	protected TextWidget m_ConfirmText;
	protected ButtonWidget m_BtnYes;
	protected ButtonWidget m_BtnNo;
	protected string m_PendingKickSid;

	override Widget Init()
	{
		layoutRoot = GetGame().GetWorkspace().CreateWidgets("ExorStorage/gui/exor_party_menu.layout");
		if (!layoutRoot)
			return null;

		m_Members = new array<ButtonWidget>;
		m_MemberSids = new array<string>;
		m_List = layoutRoot.FindAnyWidget("ExorPartyList");
		m_BtnLeave = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ExorPartyBtnLeave"));
		m_BtnClose = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ExorPartyBtnClose"));
		m_Confirm = layoutRoot.FindAnyWidget("ExorPartyConfirm");
		m_ConfirmText = TextWidget.Cast(layoutRoot.FindAnyWidget("ExorPartyConfirmText"));
		m_BtnYes = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ExorPartyBtnYes"));
		m_BtnNo = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ExorPartyBtnNo"));

		ExorRefresh();
		return layoutRoot;
	}

	// Borra las filas creadas dinamicamente (mismo patron que ExorServerInfoMenu.ClearRows).
	void ExorClearRows()
	{
		if (m_List)
		{
			Widget c = m_List.GetChildren();
			while (c)
			{
				Widget next = c.GetSibling();
				c.Unlink();
				c = next;
			}
		}
		m_Members.Clear();
		m_MemberSids.Clear();
	}

	void ExorRefresh()
	{
		ExorClearRows();

		PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
		ExorRosterDTO roster;
		if (p)
			roster = p.ExorGetRoster();

		int count = 0;
		string ownerId = "";
		if (roster)
		{
			count = roster.members.Count();
			ownerId = roster.owner_id;
		}

		int i;
		for (i = 0; i < count; i++)
		{
			ExorGroupMember m = roster.members.Get(i);
			Widget rw = GetGame().GetWorkspace().CreateWidgets("ExorStorage/gui/exor_party_member_row.layout", m_List);
			if (!rw)
				continue;
			ButtonWidget btn = ButtonWidget.Cast(rw);
			if (!btn)
				btn = ButtonWidget.Cast(rw.FindAnyWidget("ExorPartyMemberBtn"));
			if (!btn)
				continue;
			string tag = "";
			if (m.steamid == ownerId)
				tag = "  (lider)";
			btn.SetText(m.name + tag);
			m_Members.Insert(btn);
			m_MemberSids.Insert(m.steamid);
		}
		if (m_Confirm)
			m_Confirm.Show(false);
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
		PlayerBase p = PlayerBase.Cast(GetGame().GetPlayer());
		if (!p)
			return false;

		// boton de un miembro -> abrir confirmacion
		ExorRosterDTO roster = p.ExorGetRoster();
		int i;
		for (i = 0; i < m_Members.Count(); i++)
		{
			if (w == m_Members.Get(i))
			{
				string sid = m_MemberSids.Get(i);
				string nm = "miembro";
				if (roster)
				{
					int j;
					for (j = 0; j < roster.members.Count(); j++)
					{
						if (roster.members.Get(j).steamid == sid)
						{
							nm = roster.members.Get(j).name;
							break;
						}
					}
				}

				m_PendingKickSid = sid;
				string ctext = "Expulsar a " + nm + "?";
				if (roster && sid == roster.owner_id)
					ctext = "Expulsar al lider DISUELVE el party. Expulsar a " + nm + "?";
				if (m_ConfirmText) m_ConfirmText.SetText(ctext);
				if (m_BtnYes) m_BtnYes.Show(true);
				if (m_Confirm) m_Confirm.Show(true);
				return true;
			}
		}

		if (w == m_BtnYes)
		{
			if (m_PendingKickSid != "")
				p.ExorReqKick(m_PendingKickSid);
			if (m_Confirm) m_Confirm.Show(false);
			Close();
			return true;
		}
		if (w == m_BtnNo)
		{
			if (m_Confirm) m_Confirm.Show(false);
			return true;
		}
		if (w == m_BtnLeave)
		{
			p.ExorReqLeave();
			Close();
			return true;
		}
		if (w == m_BtnClose)
		{
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

	// OnKeyPress no siempre recibe el ESC (lo intercepta el engine). Leemos la
	// tecla cruda en el Update (misma tecnica que el mapa) para cerrar con ESC.
	override void Update(float timeslice)
	{
		super.Update(timeslice);
		if (KeyState(KeyCode.KC_ESCAPE) > 0)
		{
			Close();
			return;
		}
	}
}
