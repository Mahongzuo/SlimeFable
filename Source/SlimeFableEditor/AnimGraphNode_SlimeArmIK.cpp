// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraphNode_SlimeArmIK.h"

#define LOCTEXT_NAMESPACE "AnimGraphNode_SlimeArmIK"

FText UAnimGraphNode_SlimeArmIK::GetControllerDescription() const
{
	return LOCTEXT("SlimeArmIK", "Slime Arm IK");
}

FText UAnimGraphNode_SlimeArmIK::GetTooltipText() const
{
	return LOCTEXT("SlimeArmIK_Tooltip",
		"Two-bone IK with explicit upper / lower / end bones. Skips twist bones between elbow and hand.");
}

FText UAnimGraphNode_SlimeArmIK::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (Node.EndBone.BoneName.IsNone())
	{
		return GetControllerDescription();
	}

	const FText Description = GetControllerDescription();
	const FText BoneName = FText::FromName(Node.EndBone.BoneName);
	if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
	{
		return FText::Format(LOCTEXT("SlimeArmIK_ListTitle", "{0} - Bone: {1}"), Description, BoneName);
	}
	return FText::Format(LOCTEXT("SlimeArmIK_Title", "{0}\nBone: {1}"), Description, BoneName);
}

#undef LOCTEXT_NAMESPACE
