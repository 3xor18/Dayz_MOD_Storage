// ============================================================================
// 3xor_Vanilla_Optimization - Menu de party (Fase E)
// Se abre con "Administrar party" apuntando al mastil. Lista los miembros como
// botones; al tocar uno, pregunta "Expulsar? Si/No". Cualquier miembro puede
// expulsar a otro (menos al dueno). Tambien "Salir del party".
// ============================================================================
class ExorPartyMenu extends UIScriptedMenu
{
	protected ref array<ButtonWidget> m_Members;
	protected ref array<string> m_MemberSids;
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
		int i;
		for (i = 0; i < 8; i++)
		{
			m_Members.Insert(ButtonWidget.Cast(layoutRoot.FindAnyWidget("ExorPartyMember" + i.ToString())));
			m_MemberSids.Insert("");
		}
		m_BtnLeave = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ExorPartyBtnLeave"));
		m_BtnClose = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ExorPartyBtnClose"));
		m_Confirm = layoutRoot.FindAnyWidget("ExorPartyConfirm");
		m_ConfirmText = TextWidget.Cast(layoutRoot.FindAnyWidget("ExorPartyConfirmText"));
		m_BtnYes = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ExorPartyBtnYes"));
		m_BtnNo = ButtonWidget.Cast(layoutRoot.FindAnyWidget("ExorPartyBtnNo"));

		ExorRefresh();
		return layoutRoot;
	}

	void ExorRefresh()
	{
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
		for (i = 0; i < 8; i++)
		{
			if (!m_Members.Get(i))
				continue;
			if (i < count)
			{
				ExorGroupMember m = roster.members.Get(i);
				m_MemberSids.Set(i, m.steamid);
				string tag = "";
				if (m.steamid == ownerId)
					tag = "  (lider)";
				m_Members.Get(i).SetText(m.name + tag);
				m_Members.Get(i).Show(true);
			}
			else
			{
				m_MemberSids.Set(i, "");
				m_Members.Get(i).Show(false);
			}
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
				if (roster && i < roster.members.Count())
					nm = roster.members.Get(i).name;

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
}
