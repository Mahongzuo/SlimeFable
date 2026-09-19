// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "LyraTeamSubsystem.generated.h"

#define UE_API LYRAGAME_API

class AActor;
class ALyraPlayerState;
class ALyraTeamInfoBase;
class ALyraTeamPrivateInfo;
class ALyraTeamPublicInfo;
class FSubsystemCollectionBase;
class ULyraTeamDisplayAsset;
struct FFrame;
struct FGameplayTag;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLyraTeamDisplayAssetChangedDelegate, const ULyraTeamDisplayAsset*, DisplayAsset);

USTRUCT()
struct FLyraTeamTrackingInfo
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<ALyraTeamPublicInfo> PublicInfo = nullptr;

	UPROPERTY()
	TObjectPtr<ALyraTeamPrivateInfo> PrivateInfo = nullptr;

	UPROPERTY()
	TObjectPtr<ULyraTeamDisplayAsset> DisplayAsset = nullptr;

	UPROPERTY()
	FOnLyraTeamDisplayAssetChangedDelegate OnTeamDisplayAssetChanged;

public:
	void SetTeamInfo(ALyraTeamInfoBase* Info);
	void RemoveTeamInfo(ALyraTeamInfoBase* Info);
};

// Result of comparing the team affiliation for two actors
UENUM(BlueprintType)
enum class ELyraTeamComparison : uint8
{
	// Both actors are members of the same team
	OnSameTeam,

	// The actors are members of opposing teams
	DifferentTeams,

	// One (or both) of the actors was invalid or not part of any team
	InvalidArgument
};

/** A subsystem for easy access to team information for team-based actors (e.g., pawns or player states) */
UCLASS(MinimalAPI)
class ULyraTeamSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UE_API ULyraTeamSubsystem();

	//~USubsystem interface
	UE_API virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UE_API virtual void Deinitialize() override;
	//~End of USubsystem interface

	// Tries to registers a new team
	UE_API bool RegisterTeamInfo(ALyraTeamInfoBase* TeamInfo);

	// Tries to unregister a team, will return false if it didn't work
	UE_API bool UnregisterTeamInfo(ALyraTeamInfoBase* TeamInfo);

	// Changes the team associated with this actor if possible
	// Note: This function can only be called on the authority
	UE_API bool ChangeTeamForActor(AActor* ActorToChange, int32 NewTeamId);

	// Returns the team this object belongs to, or INDEX_NONE if it is not part of a team
	UE_API int32 FindTeamFromObject(const UObject* TestObject) const;

	// Returns the associated player state for this actor, or INDEX_NONE if it is not associated with a player
	UE_API const ALyraPlayerState* FindPlayerStateFromActor(const AActor* PossibleTeamActor) const;

	// Returns the team this object belongs to, or INDEX_NONE if it is not part of a team
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=Teams, meta=(Keywords="Get"))
	UE_API void FindTeamFromActor(const UObject* TestActor, bool& bIsPartOfTeam, int32& TeamId) const;

	// Compare the teams of two actors and returns a value indicating if they are on same teams, different teams, or one/both are invalid
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=Teams, meta=(ExpandEnumAsExecs=ReturnValue))
	UE_API ELyraTeamComparison CompareTeams(const UObject* A, const UObject* B, int32& TeamIdA, int32& TeamIdB) const;

	// Compare the teams of two actors and returns a value indicating if they are on same teams, different teams, or one/both are invalid
	UE_API ELyraTeamComparison CompareTeams(const UObject* A, const UObject* B) const;

	// Returns true if the instigator can damage the target, taking into account the friendly fire settings
	UE_API bool CanCauseDamage(const UObject* Instigator, const UObject* Target, bool bAllowDamageToSelf = true) const;

	// Adds a specified number of stacks to the tag (does nothing if StackCount is below 1)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Teams)
	UE_API void AddTeamTagStack(int32 TeamId, FGameplayTag Tag, int32 StackCount);

	// Removes a specified number of stacks from the tag (does nothing if StackCount is below 1)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Teams)
	UE_API void RemoveTeamTagStack(int32 TeamId, FGameplayTag Tag, int32 StackCount);

	// Returns the stack count of the specified tag (or 0 if the tag is not present)
	UFUNCTION(BlueprintCallable, Category=Teams)
	UE_API int32 GetTeamTagStackCount(int32 TeamId, FGameplayTag Tag) const;

	// Returns true if there is at least one stack of the specified tag
	UFUNCTION(BlueprintCallable, Category=Teams)
	UE_API bool TeamHasTag(int32 TeamId, FGameplayTag Tag) const;

	// Returns true if the specified team exists
	UFUNCTION(BlueprintCallable, Category=Teams)
	UE_API bool DoesTeamExist(int32 TeamId) const;

	// Gets the team display asset for the specified team, from the perspective of the specified team
	// (You have to specify a viewer too, in case the game mode is in a 'local player is always blue team' sort of situation)
	UFUNCTION(BlueprintCallable, Category=Teams)
	UE_API ULyraTeamDisplayAsset* GetTeamDisplayAsset(int32 TeamId, int32 ViewerTeamId);

	// Gets the team display asset for the specified team, from the perspective of the specified team
	// (You have to specify a viewer too, in case the game mode is in a 'local player is always blue team' sort of situation)
	UFUNCTION(BlueprintCallable, Category = Teams)
	UE_API ULyraTeamDisplayAsset* GetEffectiveTeamDisplayAsset(int32 TeamId, UObject* ViewerTeamAgent);

	// Gets the list of teams
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category=Teams)
	UE_API TArray<int32> GetTeamIDs() const;

	// Called when a team display asset has been edited, causes all team color observers to update
	UE_API void NotifyTeamDisplayAssetModified(ULyraTeamDisplayAsset* ModifiedAsset);

	// Register for a team display asset notification for the specified team ID
	UE_API FOnLyraTeamDisplayAssetChangedDelegate& GetTeamDisplayAssetChangedDelegate(int32 TeamId);

private:
	UPROPERTY()
	TMap<int32, FLyraTeamTrackingInfo> TeamMap;

	FDelegateHandle CheatManagerRegistrationHandle;
};

#undef UE_API
