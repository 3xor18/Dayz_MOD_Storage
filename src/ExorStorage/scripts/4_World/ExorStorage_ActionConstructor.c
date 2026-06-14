// ============================================================================
// 3xor_Vanilla_Optimization - Registro de acciones custom en el motor
// ============================================================================
modded class ActionConstructor
{
	override void RegisterActions(TTypenameArray actions)
	{
		super.RegisterActions(actions);

		actions.Insert(ExorActionPackBarrel);
		actions.Insert(ExorActionDeployBarrel);
		actions.Insert(ExorActionFlipVehicle);
		actions.Insert(ExorActionOpenInvite);
		actions.Insert(ExorActionJoinGroup);
		actions.Insert(ExorActionCancelInvite);
		actions.Insert(ExorActionOpenPartyMenu);
		actions.Insert(ExorActionRaiseFlag);
		actions.Insert(ExorActionLowerFlag);
		actions.Insert(ExorActionAcceptInvite);
		actions.Insert(ExorActionDeclineInvite);
		actions.Insert(ExorActionLeaveParty);
		actions.Insert(ExorActionKickMember);
		actions.Insert(ExorActionPlaceMarker);
		actions.Insert(ExorActionClearMarkers);
	}
}
