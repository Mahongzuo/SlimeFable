// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LyraTabListWidgetBase.h"
#include "UI/Foundation/LyraButtonBase.h"

#include "LyraTabButtonBase.generated.h"

#define UE_API LYRAGAME_API

class UCommonLazyImage;
class UObject;
struct FFrame;
struct FSlateBrush;

UCLASS(MinimalAPI, Abstract, Blueprintable, meta = (DisableNativeTick))
class ULyraTabButtonBase : public ULyraButtonBase, public ILyraTabButtonInterface
{
	GENERATED_BODY()

public:

	UE_API void SetIconFromLazyObject(TSoftObjectPtr<UObject> LazyObject);
	UE_API void SetIconBrush(const FSlateBrush& Brush);

protected:

	UFUNCTION()
	UE_API virtual void SetTabLabelInfo_Implementation(const FLyraTabDescriptor& TabLabelInfo) override;

private:

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCommonLazyImage> LazyImage_Icon;
};

#undef UE_API
