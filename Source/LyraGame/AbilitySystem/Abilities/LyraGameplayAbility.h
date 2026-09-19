// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Abilities/GameplayAbility.h"

#include "LyraGameplayAbility.generated.h"

#define UE_API LYRAGAME_API

struct FGameplayAbilityActivationInfo;
struct FGameplayAbilitySpec;
struct FGameplayAbilitySpecHandle;

class AActor;
class AController;
class ALyraCharacter;
class ALyraPlayerController;
class APlayerController;
class FText;
class ILyraAbilitySourceInterface;
class UAnimMontage;
class ULyraAbilityCost;
class ULyraAbilitySystemComponent;
class ULyraCameraMode;
class ULyraHeroComponent;
class UObject;
struct FFrame;
struct FGameplayAbilityActorInfo;
struct FGameplayEffectSpec;
struct FGameplayEventData;

/**
 * ELyraAbilityActivationPolicy
 *
 *	Defines how an ability is meant to activate.
 */
UENUM(BlueprintType)
enum class ELyraAbilityActivationPolicy : uint8
{
	// Try to activate the ability when the input is triggered.
	OnInputTriggered,

	// Continually try to activate the ability while the input is active.
	WhileInputActive,

	// Try to activate the ability when an avatar is assigned.
	OnSpawn
};


/**
 * ELyraAbilityActivationGroup
 *
 *	Defines how an ability activates in relation to other abilities.
 */
UENUM(BlueprintType)
enum class ELyraAbilityActivationGroup : uint8
{
	// Ability runs independently of all other abilities.
	Independent,

	// Ability is canceled and replaced by other exclusive abilities.
	Exclusive_Replaceable,

	// Ability blocks all other exclusive abilities from activating.
	Exclusive_Blocking,

	MAX	UMETA(Hidden)
};

/** Failure reason that can be used to play an animation montage when a failure occurs */
USTRUCT(BlueprintType)
struct FLyraAbilityMontageFailureMessage
{
	GENERATED_BODY()

public:
	// Player controller that failed to activate the ability, if the AbilitySystemComponent was player owned
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<APlayerController> PlayerController = nullptr;

	// Avatar actor that failed to activate the ability
	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<AActor> AvatarActor = nullptr;

	// All the reasons why this ability has failed
	UPROPERTY(BlueprintReadWrite)
	FGameplayTagContainer FailureTags;

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<UAnimMontage> FailureMontage = nullptr;
};

/**
 * ULyraGameplayAbility
 *
 *	The base gameplay ability class used by this project.
 */
UCLASS(MinimalAPI, Abstract, HideCategories = Input, Meta = (ShortTooltip = "The base gameplay ability class used by this project."))
class ULyraGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
	friend class ULyraAbilitySystemComponent;

public:

	UE_API ULyraGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "Lyra|Ability")
	UE_API ULyraAbilitySystemComponent* GetLyraAbilitySystemComponentFromActorInfo() const;

	UFUNCTION(BlueprintCallable, Category = "Lyra|Ability")
	UE_API ALyraPlayerController* GetLyraPlayerControllerFromActorInfo() const;

	UFUNCTION(BlueprintCallable, Category = "Lyra|Ability")
	UE_API AController* GetControllerFromActorInfo() const;

	UFUNCTION(BlueprintCallable, Category = "Lyra|Ability")
	UE_API ALyraCharacter* GetLyraCharacterFromActorInfo() const;

	UFUNCTION(BlueprintCallable, Category = "Lyra|Ability")
	UE_API ULyraHeroComponent* GetHeroComponentFromActorInfo() const;

	ELyraAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }
	ELyraAbilityActivationGroup GetActivationGroup() const { return ActivationGroup; }

	UE_API void TryActivateAbilityOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) const;

	// Returns true if the requested activation group is a valid transition.
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Lyra|Ability", Meta = (ExpandBoolAsExecs = "ReturnValue"))
	UE_API bool CanChangeActivationGroup(ELyraAbilityActivationGroup NewGroup) const;

	// Tries to change the activation group.  Returns true if it successfully changed.
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Lyra|Ability", Meta = (ExpandBoolAsExecs = "ReturnValue"))
	UE_API bool ChangeActivationGroup(ELyraAbilityActivationGroup NewGroup);

	// Sets the ability's camera mode.
	UFUNCTION(BlueprintCallable, Category = "Lyra|Ability")
	UE_API void SetCameraMode(TSubclassOf<ULyraCameraMode> CameraMode);

	// Clears the ability's camera mode.  Automatically called if needed when the ability ends.
	UFUNCTION(BlueprintCallable, Category = "Lyra|Ability")
	UE_API void ClearCameraMode();

	void OnAbilityFailedToActivate(const FGameplayTagContainer& FailedReason) const
	{
		NativeOnAbilityFailedToActivate(FailedReason);
		ScriptOnAbilityFailedToActivate(FailedReason);
	}

protected:

	// Called when the ability fails to activate
	UE_API virtual void NativeOnAbilityFailedToActivate(const FGameplayTagContainer& FailedReason) const;

	// Called when the ability fails to activate
	UFUNCTION(BlueprintImplementableEvent)
	UE_API void ScriptOnAbilityFailedToActivate(const FGameplayTagContainer& FailedReason) const;

	//~UGameplayAbility interface
	UE_API virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	UE_API virtual void SetCanBeCanceled(bool bCanBeCanceled) override;
	UE_API virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	UE_API virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	UE_API virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	UE_API virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	UE_API virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	UE_API virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	UE_API virtual FGameplayEffectContextHandle MakeEffectContext(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo) const override;
	UE_API virtual void ApplyAbilityTagsToGameplayEffectSpec(FGameplayEffectSpec& Spec, FGameplayAbilitySpec* AbilitySpec) const override;
	UE_API virtual bool DoesAbilitySatisfyTagRequirements(const UAbilitySystemComponent& AbilitySystemComponent, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	//~End of UGameplayAbility interface

	UE_API virtual void OnPawnAvatarSet();

	UE_API virtual void GetAbilitySource(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, float& OutSourceLevel, const ILyraAbilitySourceInterface*& OutAbilitySource, AActor*& OutEffectCauser) const;

	/** Called when this ability is granted to the ability system component. */
	UFUNCTION(BlueprintImplementableEvent, Category = Ability, DisplayName = "OnAbilityAdded")
	UE_API void K2_OnAbilityAdded();

	/** Called when this ability is removed from the ability system component. */
	UFUNCTION(BlueprintImplementableEvent, Category = Ability, DisplayName = "OnAbilityRemoved")
	UE_API void K2_OnAbilityRemoved();

	/** Called when the ability system is initialized with a pawn avatar. */
	UFUNCTION(BlueprintImplementableEvent, Category = Ability, DisplayName = "OnPawnAvatarSet")
	UE_API void K2_OnPawnAvatarSet();

protected:

	// Defines how this ability is meant to activate.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|Ability Activation")
	ELyraAbilityActivationPolicy ActivationPolicy;

	// Defines the relationship between this ability activating and other abilities activating.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|Ability Activation")
	ELyraAbilityActivationGroup ActivationGroup;

	// Additional costs that must be paid to activate this ability
	UPROPERTY(EditDefaultsOnly, Instanced, Category = Costs)
	TArray<TObjectPtr<ULyraAbilityCost>> AdditionalCosts;

	// Map of failure tags to simple error messages
	UPROPERTY(EditDefaultsOnly, Category = "Advanced")
	TMap<FGameplayTag, FText> FailureTagToUserFacingMessages;

	// Map of failure tags to anim montages that should be played with them
	UPROPERTY(EditDefaultsOnly, Category = "Advanced")
	TMap<FGameplayTag, TObjectPtr<UAnimMontage>> FailureTagToAnimMontage;

	// If true, extra information should be logged when this ability is canceled. This is temporary, used for tracking a bug.
	UPROPERTY(EditDefaultsOnly, Category = "Advanced")
	bool bLogCancelation;

	// Current camera mode set by the ability.
	TSubclassOf<ULyraCameraMode> ActiveCameraMode;
};

#undef UE_API
