#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Quest/QuestTypes.h"
#include "QuestSubsystem.generated.h"

class UQuestHUDWidget;
class UQuestLogWidget;
class AQuestInteractActor;
class AQuestReachVolume;

UCLASS()
class SLIMEFABLE_API UQuestSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Quest")
	bool HasActiveQuest() const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	bool HasTrackedQuest() const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	FText GetActiveChapterLabel() const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	FText GetActiveMainTitle() const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	FText GetActiveBranchTitle() const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	int32 GetActiveCount() const { return BranchCount; }

	UFUNCTION(BlueprintPure, Category = "Quest")
	int32 GetActiveRequired() const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	FText GetTrackedChapterLabel() const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	FText GetTrackedMainTitle() const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	FText GetTrackedBranchTitle() const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	int32 GetTrackedCount() const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	int32 GetTrackedRequired() const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	bool IsTrackingSide() const { return bTrackingSide; }

	UFUNCTION(BlueprintPure, Category = "Quest")
	bool GetActiveToast(FText& OutToast) const;

	UFUNCTION(BlueprintPure, Category = "Quest")
	bool CanContribute(FName ChapterId, FName QuestId, FName BranchId) const;

	UFUNCTION(BlueprintCallable, Category = "Quest")
	bool NotifyProgress(FName ChapterId, FName QuestId, FName BranchId, int32 Amount = 1);

	UFUNCTION(BlueprintCallable, Category = "Quest")
	void SetTracked(FName ChapterId, FName QuestId, FName BranchId, bool bSide);

	UFUNCTION(BlueprintCallable, Category = "Quest")
	void ResetTrackToMain();

	UFUNCTION(BlueprintCallable, Category = "Quest")
	void ToggleQuestLog();

	UFUNCTION(BlueprintCallable, Category = "Quest")
	void CloseQuestLog();

	UFUNCTION(BlueprintCallable, Category = "Quest")
	void Enter0815Chapter(FName Year);

	UFUNCTION(BlueprintPure, Category = "Quest")
	bool IsQuestLogOpen() const { return LogWidget != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Quest")
	bool IsChapterComplete(FName ChapterId) const { return CompletedChapters.Contains(ChapterId); }

	UFUNCTION(BlueprintPure, Category = "Quest")
	bool IsSideComplete(FName ChapterId, FName QuestId) const;

	int32 GetSideCount(FName ChapterId, FName QuestId, FName BranchId) const;
	const UDayQuestBook* GetBook() const { return Book; }
	FName GetActiveDayId() const { return ActiveDayId; }
	FName GetActiveChapterId() const;
	FName GetActiveQuestId() const;
	FName GetActiveBranchId() const;
	FName GetTrackedChapterId() const { return TrackedChapterId; }
	FName GetTrackedQuestId() const { return TrackedQuestId; }
	FName GetTrackedBranchId() const { return TrackedBranchId; }

	bool GetActiveWaypointLocation(FVector& OutLocation) const;

	void SpawnSlimeLabTestActors(UWorld* World);
	void BeginForWorld(UWorld* World);

protected:
	void HandlePostLoadMap(UWorld* World);
	void TryCreateHUD(UWorld* World, int32 AttemptsLeft);
	void TeardownUI();
	void ShowCompleteBanner(const FText& TaskName, float DurationSeconds);
	void PlayCompleteVoice() const;
	void AdvanceAfterBranch();
	void CompleteChapter();
	void FinishPendingTravel();
	void PersistProgress() const;
	void RestoreProgress();
	void SyncTrackToMain();
	void TryCatchUpDefeatObjectives();
	FString MakeSideKey(FName ChapterId, FName QuestId) const;
	FString MakeSideCountKey(FName ChapterId, FName QuestId, FName BranchId) const;
	UDayQuestBook* ResolveBookForWorld(UWorld* World) const;
	static FName InferDayIdFromWorld(UWorld* World);
	static bool IsPlayWorld(const UWorld* World);
	const FQuestChapter* GetActiveChapter() const;
	const FQuestMain* GetActiveMain() const;
	const FQuestBranch* GetActiveBranch() const;
	const FQuestChapter* GetTrackedChapter() const;
	const FQuestMain* GetTrackedMain() const;
	const FQuestBranch* GetTrackedBranch() const;
	FString MakeSaveSlot() const;

	UPROPERTY()
	TObjectPtr<UDayQuestBook> Book;

	UPROPERTY()
	TObjectPtr<UQuestHUDWidget> HUDWidget;

	UPROPERTY()
	TObjectPtr<UQuestLogWidget> LogWidget;

	TSet<FName> CompletedChapters;
	TSet<FName> CompletedSideQuestIds;
	TMap<FString, int32> SideBranchCounts;
	FName ActiveDayId;
	int32 ChapterIndex = INDEX_NONE;
	int32 MainIndex = INDEX_NONE;
	int32 BranchIndex = INDEX_NONE;
	int32 BranchCount = 0;

	FName TrackedChapterId;
	FName TrackedQuestId;
	FName TrackedBranchId;
	bool bTrackingSide = false;

	FText ToastMessage;
	double ToastUntilSeconds = 0.0;

	FName PendingTravelChapterId;
	FTimerHandle PendingTravelHandle;

	FDelegateHandle PostLoadHandle;
	TWeakObjectPtr<UWorld> ActiveWorld;
};
