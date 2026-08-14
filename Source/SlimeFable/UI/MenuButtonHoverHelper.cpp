// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MenuButtonHoverHelper.h"
#include "UI/MenuUIStyle.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UMenuButtonHoverHelper::HandleHovered()
{
	if (Button)
	{
		Button->SetRenderScale(FVector2D(1.03f, 1.03f));
	}
	if (Label)
	{
		Label->SetColorAndOpacity(FSlateColor(FMenuUIStyle::TodayEdgeColor()));
	}
}

void UMenuButtonHoverHelper::HandleUnhovered()
{
	if (Button)
	{
		Button->SetRenderScale(FVector2D(1.f, 1.f));
	}
	if (Label)
	{
		Label->SetColorAndOpacity(FSlateColor(NormalLabelColor));
	}
}
