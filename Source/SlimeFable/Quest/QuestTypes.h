#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/SaveGame.h"
#include "GameplayTagContainer.h"
#include "QuestTypes.generated.h"

UENUM(BlueprintType)
enum class EQuestObjectiveType : uint8
{
	Collect UMETA(DisplayName = "Collect"),
	Talk UMETA(DisplayName = "Talk"),
	Defeat UMETA(DisplayName = "Defeat"),
	Reach UMETA(DisplayName = "Reach")
};

USTRUCT(BlueprintType)
struct FQuestBranch
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FName BranchId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	EQuestObjectiveType Type = EQuestObjectiveType::Talk;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest", meta = (ClampMin = "1"))
	int32 RequiredCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FGameplayTag ObjectiveTag;
};

USTRUCT(BlueprintType)
struct FQuestMain
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FName QuestId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FQuestBranch> Branches;
};

USTRUCT(BlueprintType)
struct FQuestChapter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FName ChapterId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FQuestMain> MainQuests;

	/** Optional quests that never block chapter travel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FQuestMain> SideQuests;

	/** None = no auto-travel after this chapter. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FName NextChapterId;
};

UCLASS(BlueprintType)
class SLIMEFABLE_API UDayQuestBook : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	FName DayId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	bool bDoNotSave = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Quest")
	TArray<FQuestChapter> Chapters;

	const FQuestChapter* FindChapter(FName ChapterId) const;
	const FQuestMain* FindMain(FName ChapterId, FName QuestId) const;
	const FQuestMain* FindSide(FName ChapterId, FName QuestId) const;
	const FQuestBranch* FindBranch(FName ChapterId, FName QuestId, FName BranchId) const;
	bool IsSideQuest(FName ChapterId, FName QuestId) const;
	bool GetFirstIncomplete(const TSet<FName>& CompletedChapters, int32& OutChapter, int32& OutMain, int32& OutBranch) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("DayQuestBook"), DayId.IsNone() ? GetFName() : DayId);
	}

	static UDayQuestBook* MakeSlimeLabTestBook(UObject* Outer);
	static UDayQuestBook* Make0815Book(UObject* Outer);
};

UCLASS()
class SLIMEFABLE_API UDayQuestSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FName> CompletedChapters;

	UPROPERTY()
	FName ActiveChapterId;

	UPROPERTY()
	FName ActiveQuestId;

	UPROPERTY()
	FName ActiveBranchId;

	UPROPERTY()
	int32 BranchCount = 0;

	UPROPERTY()
	TArray<FName> CompletedSideQuestIds;

	UPROPERTY()
	TMap<FString, int32> SideBranchCounts;

	UPROPERTY()
	FName TrackedChapterId;

	UPROPERTY()
	FName TrackedQuestId;

	UPROPERTY()
	FName TrackedBranchId;

	UPROPERTY()
	bool bTrackingSide = false;
};
