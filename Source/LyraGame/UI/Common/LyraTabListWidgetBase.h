// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonTabListWidgetBase.h"

#include "LyraTabListWidgetBase.generated.h"

#define UE_API LYRAGAME_API

class UCommonButtonBase;
class UCommonUserWidget;
class UObject;
class UWidget;
struct FFrame;

USTRUCT(BlueprintType)
struct FLyraTabDescriptor
{
	GENERATED_BODY()

public:
	FLyraTabDescriptor()
	: bHidden(false)
	, CreatedTabContentWidget(nullptr)
	{ }

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName TabId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText TabText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSlateBrush IconBrush;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bHidden;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UCommonButtonBase> TabButtonType;

	//TODO NDarnell - This should become a TSoftClassPtr<>, the underlying common tab list needs to be able to handle lazy tab content construction.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UCommonUserWidget> TabContentType;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> CreatedTabContentWidget;
};

UINTERFACE(BlueprintType)
class ULyraTabButtonInterface : public UInterface
{
	GENERATED_BODY()
};

class ILyraTabButtonInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Tab Button")
	void SetTabLabelInfo(const FLyraTabDescriptor& TabDescriptor);
};

UCLASS(MinimalAPI, Blueprintable, BlueprintType, Abstract, meta = (DisableNativeTick))
class ULyraTabListWidgetBase : public UCommonTabListWidgetBase
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tab List")
	UE_API bool GetPreregisteredTabInfo(const FName TabNameId, FLyraTabDescriptor& OutTabInfo);

	/** Helper method to get at all the preregistered tab infos */
	const TArray<FLyraTabDescriptor>& GetAllPreregisteredTabInfos() { return PreregisteredTabInfoArray; }

	// Toggles whether or not a specified tab is hidden, can only be called before the switcher is associated
	UFUNCTION(BlueprintCallable, Category = "Tab List")
	UE_API void SetTabHiddenState(FName TabNameId, bool bHidden);

	UFUNCTION(BlueprintCallable, Category = "Tab List")
	UE_API bool RegisterDynamicTab(const FLyraTabDescriptor& TabDescriptor);

	UFUNCTION(BlueprintCallable, Category = "Tab List")
	UE_API bool IsFirstTabActive() const;

	UFUNCTION(BlueprintCallable, Category = "Tab List")
	UE_API bool IsLastTabActive() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tab List")
	UE_API bool IsTabVisible(FName TabId);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Tab List")
	UE_API int32 GetVisibleTabCount();

	/** Delegate broadcast when a new tab is created. Allows hook ups after creation. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTabContentCreated, FName, TabId, UCommonUserWidget*, TabWidget);
	DECLARE_EVENT_TwoParams(ULyraTabListWidgetBase, FOnTabContentCreatedNative, FName /* TabId */, UCommonUserWidget* /* TabWidget */);

	/** Broadcasts when a new tab is created. */
	UPROPERTY(BlueprintAssignable, Category = "Tab List")
	FOnTabContentCreated OnTabContentCreated;
	FOnTabContentCreatedNative OnTabContentCreatedNative;

protected:
	// UUserWidget interface
	UE_API virtual void NativeOnInitialized() override;
	UE_API virtual void NativeConstruct() override;
	UE_API virtual void NativeDestruct() override;
	// End UUserWidget

	UE_API virtual void HandlePreLinkedSwitcherChanged() override;
	UE_API virtual void HandlePostLinkedSwitcherChanged() override;

	UE_API virtual void HandleTabCreation_Implementation(FName TabId, UCommonButtonBase* TabButton) override;

private:
	UE_API void SetupTabs();

	UPROPERTY(EditAnywhere, meta=(TitleProperty="TabId"))
	TArray<FLyraTabDescriptor> PreregisteredTabInfoArray;
	
	/**
	 * Stores label info for tabs that have been registered at runtime but not yet created. 
	 * Elements are removed once they are created.
	 */
	UPROPERTY()
	TMap<FName, FLyraTabDescriptor> PendingTabLabelInfoMap;
};

#undef UE_API
