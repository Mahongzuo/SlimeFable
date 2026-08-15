#include "Quest/QuestSubsystem.h"
#include "Quest/QuestHUDWidget.h"
#include "Quest/QuestLogWidget.h"
#include "Quest/QuestInteractActor.h"
#include "Quest/QuestReachVolume.h"
#include "Quest/QuestObjectiveComponent.h"
#include "Quest/Quest0815Content.h"
#include "DayLevel/DayLevelSubsystem.h"
#include "Enemy/EnemyCharacter.h"
#include "Combat/SlimeHealthComponent.h"
#include "SlimeFable.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "Misc/PackageName.h"
#include "Sound/SoundWave.h"

namespace
{
	constexpr float BannerBranchSeconds = 5.0f;
	constexpr float BannerChapterSeconds = 6.5f;
	const TCHAR* QuestStepDoneVoice = TEXT("/Game/_Slime/Quest/Audio/VO_QuestStepDone.VO_QuestStepDone");
}

void UQuestSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PostLoadHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UQuestSubsystem::HandlePostLoadMap);
}

void UQuestSubsystem::Deinitialize()
{
	if (PostLoadHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadHandle);
		PostLoadHandle.Reset();
	}
	TeardownUI();
	Super::Deinitialize();
}

void UQuestSubsystem::HandlePostLoadMap(UWorld* World)
{
	if (!IsPlayWorld(World))
	{
		return;
	}

	TWeakObjectPtr<UWorld> WeakWorld(World);
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([this, WeakWorld]()
	{
		if (UWorld* Alive = WeakWorld.Get())
		{
			BeginForWorld(Alive);
		}
	}));
}

bool UQuestSubsystem::IsPlayWorld(const UWorld* World)
{
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

FName UQuestSubsystem::InferDayIdFromWorld(UWorld* World)
{
	if (!World)
	{
		return NAME_None;
	}

	TArray<FString> Candidates;
	Candidates.Add(World->GetMapName());
	if (const UPackage* Package = World->GetOutermost())
	{
		Candidates.Add(Package->GetName());
	}

	FString Name;
	for (FString Candidate : Candidates)
	{
		const FString Prefix = World->StreamingLevelsPrefix;
		if (!Prefix.IsEmpty() && Candidate.StartsWith(Prefix))
		{
			Candidate.RightChopInline(Prefix.Len());
		}
		Candidate = FPackageName::GetShortName(Candidate);
		if (Candidate.Contains(TEXT("SlimeLab")))
		{
			return FName(TEXT("Lab"));
		}
		if (Name.IsEmpty())
		{
			Name = Candidate;
		}
	}
	if (Name.Equals(TEXT("Main"), ESearchCase::IgnoreCase))
	{
		return NAME_None;
	}
	if (Name.StartsWith(TEXT("SL_")) && Name.Len() >= 7)
	{
		return FName(*Name.Mid(3, 4));
	}
	if (Name.Len() == 4 && Name.IsNumeric())
	{
		return FName(*Name);
	}
	return NAME_None;
}

UDayQuestBook* UQuestSubsystem::ResolveBookForWorld(UWorld* World) const
{
	const FName DayId = InferDayIdFromWorld(World);
	if (DayId.IsNone())
	{
		return nullptr;
	}
	if (DayId == FName(TEXT("Lab")))
	{
		return UDayQuestBook::MakeSlimeLabTestBook(const_cast<UQuestSubsystem*>(this));
	}

	const FString Day = DayId.ToString();
	if (Day.Len() != 4)
	{
		return nullptr;
	}
	const FString Month = Day.Left(2);
	const FString Path = FString::Printf(
		TEXT("/Game/_Slime/Days/%s/%s/Quests/DA_Quest_%s.DA_Quest_%s"),
		*Month, *Day, *Day, *Day);
	if (UDayQuestBook* Loaded = LoadObject<UDayQuestBook>(nullptr, *Path))
	{
		if (Loaded->Chapters.Num() > 0)
		{
			return Loaded;
		}
	}
	if (DayId == FName(TEXT("0815")))
	{
		return UDayQuestBook::Make0815Book(const_cast<UQuestSubsystem*>(this));
	}
	return nullptr;
}

void UQuestSubsystem::BeginForWorld(UWorld* World)
{
	if (!IsPlayWorld(World))
	{
		return;
	}
	if (ActiveWorld.Get() == World && Book)
	{
		if (!HUDWidget)
		{
			TryCreateHUD(World, 10);
		}
		return;
	}

	TeardownUI();
	ActiveWorld = World;
	Book = ResolveBookForWorld(World);
	ActiveDayId = InferDayIdFromWorld(World);
	CompletedChapters.Reset();
	CompletedSideQuestIds.Reset();
	SideBranchCounts.Reset();
	ChapterIndex = INDEX_NONE;
	MainIndex = INDEX_NONE;
	BranchIndex = INDEX_NONE;
	BranchCount = 0;
	ToastUntilSeconds = 0.0;
	PendingTravelChapterId = NAME_None;
	bTrackingSide = false;
	TrackedChapterId = NAME_None;
	TrackedQuestId = NAME_None;
	TrackedBranchId = NAME_None;

	if (!Book)
	{
		UE_LOG(LogSlimeFable, Log, TEXT("QuestSubsystem: No quest book for world %s"), *World->GetMapName());
		return;
	}

	RestoreProgress();
	if (ChapterIndex == INDEX_NONE)
	{
		if (!Book->GetFirstIncomplete(CompletedChapters, ChapterIndex, MainIndex, BranchIndex))
		{
			return;
		}
		BranchCount = 0;
	}
	SyncTrackToMain();

	if (ActiveDayId == FName(TEXT("Lab")))
	{
		TWeakObjectPtr<UWorld> WeakWorld(World);
		FTimerHandle SpawnHandle;
		World->GetTimerManager().SetTimer(SpawnHandle, FTimerDelegate::CreateLambda([this, WeakWorld]()
		{
			if (UWorld* Alive = WeakWorld.Get())
			{
				SpawnSlimeLabTestActors(Alive);
			}
		}), 0.2f, false);
	}
	else if (ActiveDayId == FName(TEXT("0815")))
	{
		TWeakObjectPtr<UWorld> WeakWorld(World);
		FTimerHandle SpawnHandle;
		World->GetTimerManager().SetTimer(SpawnHandle, FTimerDelegate::CreateLambda([this, WeakWorld]()
		{
			if (UWorld* Alive = WeakWorld.Get())
			{
				FQuest0815Content::SpawnForWorld(Alive, GetActiveChapterId());
			}
		}), 0.2f, false);
	}

	TryCreateHUD(World, 10);
	UE_LOG(LogSlimeFable, Log, TEXT("QuestSubsystem: Started quests for %s (%s)"),
		*World->GetMapName(), *ActiveDayId.ToString());
}

void UQuestSubsystem::TryCreateHUD(UWorld* World, int32 AttemptsLeft)
{
	if (!World || HUDWidget)
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (PC)
	{
		HUDWidget = CreateWidget<UQuestHUDWidget>(PC, UQuestHUDWidget::StaticClass());
		if (HUDWidget)
		{
			HUDWidget->AddToViewport(15);
		}
		return;
	}

	if (AttemptsLeft <= 0)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("QuestSubsystem: PlayerController missing; quest HUD not created."));
		return;
	}

	TWeakObjectPtr<UWorld> WeakWorld(World);
	FTimerHandle RetryHandle;
	World->GetTimerManager().SetTimer(RetryHandle, FTimerDelegate::CreateLambda([this, WeakWorld, AttemptsLeft]()
	{
		if (UWorld* Alive = WeakWorld.Get())
		{
			TryCreateHUD(Alive, AttemptsLeft - 1);
		}
	}), 0.1f, false);
}

void UQuestSubsystem::TeardownUI()
{
	if (UWorld* World = ActiveWorld.Get())
	{
		World->GetTimerManager().ClearTimer(PendingTravelHandle);
	}
	CloseQuestLog();
	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}
}

bool UQuestSubsystem::HasActiveQuest() const
{
	return GetActiveBranch() != nullptr;
}

bool UQuestSubsystem::HasTrackedQuest() const
{
	return GetTrackedBranch() != nullptr;
}

const FQuestChapter* UQuestSubsystem::GetActiveChapter() const
{
	if (!Book || !Book->Chapters.IsValidIndex(ChapterIndex))
	{
		return nullptr;
	}
	return &Book->Chapters[ChapterIndex];
}

const FQuestMain* UQuestSubsystem::GetActiveMain() const
{
	const FQuestChapter* Chapter = GetActiveChapter();
	if (!Chapter || !Chapter->MainQuests.IsValidIndex(MainIndex))
	{
		return nullptr;
	}
	return &Chapter->MainQuests[MainIndex];
}

const FQuestBranch* UQuestSubsystem::GetActiveBranch() const
{
	const FQuestMain* Main = GetActiveMain();
	if (!Main || !Main->Branches.IsValidIndex(BranchIndex))
	{
		return nullptr;
	}
	return &Main->Branches[BranchIndex];
}

const FQuestChapter* UQuestSubsystem::GetTrackedChapter() const
{
	if (!Book)
	{
		return nullptr;
	}
	if (bTrackingSide)
	{
		return Book->FindChapter(TrackedChapterId);
	}
	return GetActiveChapter();
}

const FQuestMain* UQuestSubsystem::GetTrackedMain() const
{
	if (!Book)
	{
		return nullptr;
	}
	if (bTrackingSide)
	{
		return Book->FindSide(TrackedChapterId, TrackedQuestId);
	}
	return GetActiveMain();
}

const FQuestBranch* UQuestSubsystem::GetTrackedBranch() const
{
	if (!Book)
	{
		return nullptr;
	}
	if (bTrackingSide)
	{
		return Book->FindBranch(TrackedChapterId, TrackedQuestId, TrackedBranchId);
	}
	return GetActiveBranch();
}

FName UQuestSubsystem::GetActiveChapterId() const
{
	if (const FQuestChapter* Chapter = GetActiveChapter())
	{
		return Chapter->ChapterId;
	}
	return NAME_None;
}

FName UQuestSubsystem::GetActiveQuestId() const
{
	if (const FQuestMain* Main = GetActiveMain())
	{
		return Main->QuestId;
	}
	return NAME_None;
}

FName UQuestSubsystem::GetActiveBranchId() const
{
	if (const FQuestBranch* Branch = GetActiveBranch())
	{
		return Branch->BranchId;
	}
	return NAME_None;
}

FText UQuestSubsystem::GetActiveChapterLabel() const
{
	if (const FQuestChapter* Chapter = GetActiveChapter())
	{
		return FText::FromName(Chapter->ChapterId);
	}
	return FText::GetEmpty();
}

FText UQuestSubsystem::GetActiveMainTitle() const
{
	if (const FQuestMain* Main = GetActiveMain())
	{
		return Main->Title;
	}
	return FText::GetEmpty();
}

FText UQuestSubsystem::GetActiveBranchTitle() const
{
	if (const FQuestBranch* Branch = GetActiveBranch())
	{
		return Branch->Title;
	}
	return FText::GetEmpty();
}

int32 UQuestSubsystem::GetActiveRequired() const
{
	if (const FQuestBranch* Branch = GetActiveBranch())
	{
		return FMath::Max(1, Branch->RequiredCount);
	}
	return 1;
}

FText UQuestSubsystem::GetTrackedChapterLabel() const
{
	if (const FQuestChapter* Chapter = GetTrackedChapter())
	{
		return FText::FromName(Chapter->ChapterId);
	}
	return FText::GetEmpty();
}

FText UQuestSubsystem::GetTrackedMainTitle() const
{
	if (const FQuestMain* Main = GetTrackedMain())
	{
		return Main->Title;
	}
	return FText::GetEmpty();
}

FText UQuestSubsystem::GetTrackedBranchTitle() const
{
	if (const FQuestBranch* Branch = GetTrackedBranch())
	{
		return Branch->Title;
	}
	return FText::GetEmpty();
}

int32 UQuestSubsystem::GetTrackedCount() const
{
	if (bTrackingSide)
	{
		return GetSideCount(TrackedChapterId, TrackedQuestId, TrackedBranchId);
	}
	return BranchCount;
}

int32 UQuestSubsystem::GetTrackedRequired() const
{
	if (const FQuestBranch* Branch = GetTrackedBranch())
	{
		return FMath::Max(1, Branch->RequiredCount);
	}
	return 1;
}

bool UQuestSubsystem::GetActiveToast(FText& OutToast) const
{
	if (ToastUntilSeconds <= 0.0)
	{
		return false;
	}
	const UWorld* World = ActiveWorld.Get();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (Now >= ToastUntilSeconds)
	{
		return false;
	}
	OutToast = ToastMessage;
	return true;
}

void UQuestSubsystem::PlayCompleteVoice() const
{
	UWorld* World = ActiveWorld.Get();
	if (!World)
	{
		return;
	}
	if (USoundWave* Voice = LoadObject<USoundWave>(nullptr, QuestStepDoneVoice))
	{
		UGameplayStatics::PlaySound2D(World, Voice);
	}
}

void UQuestSubsystem::ShowCompleteBanner(const FText& TaskName, float DurationSeconds)
{
	ToastMessage = TaskName;
	if (UWorld* World = ActiveWorld.Get())
	{
		ToastUntilSeconds = World->GetTimeSeconds() + DurationSeconds;
	}
	PlayCompleteVoice();
}

FString UQuestSubsystem::MakeSideKey(FName ChapterId, FName QuestId) const
{
	return FString::Printf(TEXT("%s.%s"), *ChapterId.ToString(), *QuestId.ToString());
}

FString UQuestSubsystem::MakeSideCountKey(FName ChapterId, FName QuestId, FName BranchId) const
{
	return FString::Printf(TEXT("%s|%s|%s"), *ChapterId.ToString(), *QuestId.ToString(), *BranchId.ToString());
}

bool UQuestSubsystem::IsSideComplete(FName ChapterId, FName QuestId) const
{
	return CompletedSideQuestIds.Contains(FName(*MakeSideKey(ChapterId, QuestId)));
}

int32 UQuestSubsystem::GetSideCount(FName ChapterId, FName QuestId, FName BranchId) const
{
	if (const int32* Found = SideBranchCounts.Find(MakeSideCountKey(ChapterId, QuestId, BranchId)))
	{
		return *Found;
	}
	return 0;
}

void UQuestSubsystem::SyncTrackToMain()
{
	bTrackingSide = false;
	if (const FQuestChapter* Chapter = GetActiveChapter())
	{
		TrackedChapterId = Chapter->ChapterId;
	}
	else
	{
		TrackedChapterId = NAME_None;
	}
	if (const FQuestMain* Main = GetActiveMain())
	{
		TrackedQuestId = Main->QuestId;
	}
	else
	{
		TrackedQuestId = NAME_None;
	}
	if (const FQuestBranch* Branch = GetActiveBranch())
	{
		TrackedBranchId = Branch->BranchId;
	}
	else
	{
		TrackedBranchId = NAME_None;
	}
	PersistProgress();
}

void UQuestSubsystem::ResetTrackToMain()
{
	SyncTrackToMain();
}

void UQuestSubsystem::SetTracked(FName ChapterId, FName QuestId, FName BranchId, bool bSide)
{
	if (!Book)
	{
		return;
	}
	if (bSide)
	{
		if (IsSideComplete(ChapterId, QuestId) || !Book->FindSide(ChapterId, QuestId))
		{
			return;
		}
		const FQuestMain* Side = Book->FindSide(ChapterId, QuestId);
		if (!Side || Side->Branches.Num() == 0)
		{
			return;
		}
		if (BranchId.IsNone())
		{
			BranchId = Side->Branches[0].BranchId;
		}
		if (!Book->FindBranch(ChapterId, QuestId, BranchId))
		{
			return;
		}
		bTrackingSide = true;
		TrackedChapterId = ChapterId;
		TrackedQuestId = QuestId;
		TrackedBranchId = BranchId;
		PersistProgress();
		return;
	}

	SyncTrackToMain();
}

bool UQuestSubsystem::CanContribute(FName ChapterId, FName QuestId, FName BranchId) const
{
	if (!Book)
	{
		return false;
	}
	if (Book->IsSideQuest(ChapterId, QuestId))
	{
		if (IsSideComplete(ChapterId, QuestId))
		{
			return false;
		}
		const FQuestBranch* Branch = Book->FindBranch(ChapterId, QuestId, BranchId);
		if (!Branch)
		{
			return false;
		}
		return GetSideCount(ChapterId, QuestId, BranchId) < FMath::Max(1, Branch->RequiredCount);
	}

	const FQuestChapter* Chapter = GetActiveChapter();
	const FQuestMain* Main = GetActiveMain();
	const FQuestBranch* Branch = GetActiveBranch();
	return Chapter && Main && Branch
		&& Chapter->ChapterId == ChapterId
		&& Main->QuestId == QuestId
		&& Branch->BranchId == BranchId;
}

void UQuestSubsystem::TryCatchUpDefeatObjectives()
{
	UWorld* World = ActiveWorld.Get();
	if (!World)
	{
		return;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor))
		{
			continue;
		}
		UQuestObjectiveComponent* Objective = Actor->FindComponentByClass<UQuestObjectiveComponent>();
		if (!Objective || Objective->IsConsumed())
		{
			continue;
		}
		if (!CanContribute(Objective->ChapterId, Objective->QuestId, Objective->BranchId))
		{
			continue;
		}
		const AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(Actor);
		const USlimeHealthComponent* Health = Enemy ? Enemy->GetEnemyHealth() : nullptr;
		if (Health && !Health->IsAlive())
		{
			Objective->TryContribute();
		}
	}
}

bool UQuestSubsystem::NotifyProgress(FName ChapterId, FName QuestId, FName BranchId, int32 Amount)
{
	if (!CanContribute(ChapterId, QuestId, BranchId) || Amount <= 0)
	{
		return false;
	}

	if (Book && Book->IsSideQuest(ChapterId, QuestId))
	{
		const FQuestBranch* Branch = Book->FindBranch(ChapterId, QuestId, BranchId);
		const FQuestMain* Side = Book->FindSide(ChapterId, QuestId);
		if (!Branch || !Side)
		{
			return false;
		}
		const FString CountKey = MakeSideCountKey(ChapterId, QuestId, BranchId);
		int32& Count = SideBranchCounts.FindOrAdd(CountKey);
		Count = FMath::Min(FMath::Max(1, Branch->RequiredCount), Count + Amount);
		PersistProgress();

		if (Count >= Branch->RequiredCount)
		{
			bool bSideDone = true;
			for (const FQuestBranch& Other : Side->Branches)
			{
				if (GetSideCount(ChapterId, QuestId, Other.BranchId) < FMath::Max(1, Other.RequiredCount))
				{
					bSideDone = false;
					break;
				}
			}
			if (bSideDone)
			{
				CompletedSideQuestIds.Add(FName(*MakeSideKey(ChapterId, QuestId)));
				ShowCompleteBanner(Side->Title, BannerBranchSeconds);
				if (bTrackingSide && TrackedChapterId == ChapterId && TrackedQuestId == QuestId)
				{
					SyncTrackToMain();
				}
			}
			else
			{
				ShowCompleteBanner(Branch->Title, BannerBranchSeconds);
			}
			PersistProgress();
		}
		return true;
	}

	const FQuestBranch* Branch = GetActiveBranch();
	if (!Branch)
	{
		return false;
	}
	BranchCount = FMath::Min(Branch->RequiredCount, BranchCount + Amount);
	PersistProgress();

	if (BranchCount >= Branch->RequiredCount)
	{
		ShowCompleteBanner(Branch->Title, BannerBranchSeconds);
		AdvanceAfterBranch();
	}
	return true;
}

void UQuestSubsystem::AdvanceAfterBranch()
{
	const FQuestMain* Main = GetActiveMain();
	const FQuestChapter* Chapter = GetActiveChapter();
	if (!Main || !Chapter)
	{
		return;
	}

	if (BranchIndex + 1 < Main->Branches.Num())
	{
		++BranchIndex;
		BranchCount = 0;
		if (!bTrackingSide)
		{
			SyncTrackToMain();
		}
		PersistProgress();
		TryCatchUpDefeatObjectives();
		return;
	}

	if (MainIndex + 1 < Chapter->MainQuests.Num())
	{
		++MainIndex;
		BranchIndex = 0;
		BranchCount = 0;
		if (!bTrackingSide)
		{
			SyncTrackToMain();
		}
		PersistProgress();
		TryCatchUpDefeatObjectives();
		return;
	}

	CompleteChapter();
}

void UQuestSubsystem::CompleteChapter()
{
	const FQuestChapter* Chapter = GetActiveChapter();
	if (!Chapter)
	{
		return;
	}

	const FName Finished = Chapter->ChapterId;
	const FName NextId = Chapter->NextChapterId;
	CompletedChapters.Add(Finished);

	if (!NextId.IsNone() && Book && Book->FindChapter(NextId))
	{
		ShowCompleteBanner(
			FText::FromString(FString::Printf(TEXT("%s 完成，前往 %s"), *Finished.ToString(), *NextId.ToString())),
			BannerChapterSeconds);

		for (int32 Index = 0; Index < Book->Chapters.Num(); ++Index)
		{
			if (Book->Chapters[Index].ChapterId == NextId)
			{
				ChapterIndex = Index;
				MainIndex = 0;
				BranchIndex = 0;
				BranchCount = 0;
				break;
			}
		}
		SyncTrackToMain();
		PersistProgress();
		PendingTravelChapterId = NextId;
		if (UWorld* World = ActiveWorld.Get())
		{
			World->GetTimerManager().SetTimer(
				PendingTravelHandle,
				this,
				&UQuestSubsystem::FinishPendingTravel,
				BannerChapterSeconds,
				false);
		}
		return;
	}

	ShowCompleteBanner(
		FText::FromString(FString::Printf(TEXT("%s 完成"), *Finished.ToString())),
		BannerChapterSeconds);
	ChapterIndex = INDEX_NONE;
	MainIndex = INDEX_NONE;
	BranchIndex = INDEX_NONE;
	BranchCount = 0;
	SyncTrackToMain();
	PersistProgress();
}

void UQuestSubsystem::Enter0815Chapter(FName Year)
{
	UWorld* World = ActiveWorld.Get();
	if (!World || Year.IsNone())
	{
		return;
	}
	FQuest0815Content::EnterChapter(World, Year);
}

void UQuestSubsystem::FinishPendingTravel()
{
	const FName NextId = PendingTravelChapterId;
	PendingTravelChapterId = NAME_None;
	UWorld* World = ActiveWorld.Get();
	if (NextId.IsNone() || !World)
	{
		return;
	}

	if (ActiveDayId == FName(TEXT("0815")))
	{
		Enter0815Chapter(NextId);
		return;
	}

	if (UDayLevelSubsystem* Days = GetGameInstance()->GetSubsystem<UDayLevelSubsystem>())
	{
		TSoftObjectPtr<UWorld> SubLevel;
		if (Days->GetSubLevelForDayId(ActiveDayId, NextId, SubLevel) && !SubLevel.IsNull())
		{
			Days->TravelToSubLevel(World, ActiveDayId, NextId);
			return;
		}
		Days->TravelToDayId(World, ActiveDayId);
	}
}

void UQuestSubsystem::ToggleQuestLog()
{
	if (LogWidget)
	{
		CloseQuestLog();
		return;
	}

	UWorld* World = ActiveWorld.Get();
	APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
	if (!PC || !Book)
	{
		return;
	}

	LogWidget = CreateWidget<UQuestLogWidget>(PC, UQuestLogWidget::StaticClass());
	if (!LogWidget)
	{
		return;
	}
	LogWidget->AddToViewport(25);
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(LogWidget->TakeWidget());
	Mode.SetHideCursorDuringCapture(false);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	PC->SetInputMode(Mode);
	PC->bShowMouseCursor = true;
}

void UQuestSubsystem::CloseQuestLog()
{
	APlayerController* PC = nullptr;
	if (UWorld* World = ActiveWorld.Get())
	{
		PC = UGameplayStatics::GetPlayerController(World, 0);
	}
	if (LogWidget)
	{
		LogWidget->RemoveFromParent();
		LogWidget = nullptr;
	}
	if (PC)
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
}

bool UQuestSubsystem::GetActiveWaypointLocation(FVector& OutLocation) const
{
	UWorld* World = ActiveWorld.Get();
	const FQuestChapter* Chapter = GetTrackedChapter();
	const FQuestMain* Main = GetTrackedMain();
	const FQuestBranch* Branch = GetTrackedBranch();
	if (!World || !Chapter || !Main || !Branch)
	{
		return false;
	}

	APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	const FVector Origin = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
	float BestDistSq = TNumericLimits<float>::Max();
	bool bFound = false;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor))
		{
			continue;
		}
		UQuestObjectiveComponent* Objective = Actor->FindComponentByClass<UQuestObjectiveComponent>();
		if (!Objective || Objective->IsConsumed())
		{
			continue;
		}
		if (Objective->ChapterId != Chapter->ChapterId
			|| Objective->QuestId != Main->QuestId
			|| Objective->BranchId != Branch->BranchId)
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Origin, Actor->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			OutLocation = Objective->GetPromptWorldLocation();
			bFound = true;
		}
	}
	return bFound;
}

void UQuestSubsystem::SpawnSlimeLabTestActors(UWorld* World)
{
	if (!World)
	{
		return;
	}

	for (TActorIterator<AQuestInteractActor> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return;
		}
	}

	APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
	const FVector Origin = Player ? Player->GetActorLocation() : FVector::ZeroVector;
	const FVector Forward = Player ? Player->GetActorForwardVector() : FVector::ForwardVector;
	const FVector Right = Player ? Player->GetActorRightVector() : FVector::RightVector;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	const FName Chapter(TEXT("Lab"));
	const FName Quest(TEXT("Trial"));

	auto SpawnInteract = [&](const FVector& Offset, FName BranchId, const TCHAR* Verb, const FLinearColor& Color, float Scale)
	{
		AQuestInteractActor* Actor = World->SpawnActor<AQuestInteractActor>(
			AQuestInteractActor::StaticClass(), Origin + Offset, FRotator::ZeroRotator, Params);
		if (Actor)
		{
			Actor->Configure(Chapter, Quest, BranchId, FText::FromString(Verb), Color, Scale);
		}
	};

	SpawnInteract(Forward * 420.f + Right * 220.f, FName(TEXT("Stones")), TEXT("拾取"), FLinearColor(0.82f, 0.62f, 0.28f, 1.f), 0.45f);
	SpawnInteract(Forward * 420.f - Right * 220.f, FName(TEXT("Stones")), TEXT("拾取"), FLinearColor(0.82f, 0.62f, 0.28f, 1.f), 0.45f);
	SpawnInteract(Forward * 820.f, FName(TEXT("Stele")), TEXT("交谈"), FLinearColor(0.55f, 0.42f, 0.22f, 1.f), 0.85f);

	AQuestReachVolume* Gate = World->SpawnActor<AQuestReachVolume>(
		AQuestReachVolume::StaticClass(), Origin + Forward * 1250.f, FRotator::ZeroRotator, Params);
	if (Gate)
	{
		Gate->Configure(Chapter, Quest, FName(TEXT("Gate")), FVector(200.f, 200.f, 140.f));
	}

	UE_LOG(LogSlimeFable, Log, TEXT("QuestSubsystem: Spawned SlimeLab test quest actors."));
}

FString UQuestSubsystem::MakeSaveSlot() const
{
	return FString::Printf(TEXT("Quest_%s"), *ActiveDayId.ToString());
}

void UQuestSubsystem::PersistProgress() const
{
	if (!Book || Book->bDoNotSave || ActiveDayId.IsNone())
	{
		return;
	}

	UDayQuestSaveGame* Save = Cast<UDayQuestSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UDayQuestSaveGame::StaticClass()));
	if (!Save)
	{
		return;
	}
	Save->CompletedChapters = CompletedChapters.Array();
	Save->CompletedSideQuestIds = CompletedSideQuestIds.Array();
	Save->SideBranchCounts = SideBranchCounts;
	Save->TrackedChapterId = TrackedChapterId;
	Save->TrackedQuestId = TrackedQuestId;
	Save->TrackedBranchId = TrackedBranchId;
	Save->bTrackingSide = bTrackingSide;
	if (const FQuestChapter* Chapter = GetActiveChapter())
	{
		Save->ActiveChapterId = Chapter->ChapterId;
	}
	if (const FQuestMain* Main = GetActiveMain())
	{
		Save->ActiveQuestId = Main->QuestId;
	}
	if (const FQuestBranch* Branch = GetActiveBranch())
	{
		Save->ActiveBranchId = Branch->BranchId;
	}
	Save->BranchCount = BranchCount;
	UGameplayStatics::SaveGameToSlot(Save, MakeSaveSlot(), 0);
}

void UQuestSubsystem::RestoreProgress()
{
	if (!Book || Book->bDoNotSave || ActiveDayId.IsNone())
	{
		return;
	}
	if (!UGameplayStatics::DoesSaveGameExist(MakeSaveSlot(), 0))
	{
		return;
	}

	UDayQuestSaveGame* Save = Cast<UDayQuestSaveGame>(UGameplayStatics::LoadGameFromSlot(MakeSaveSlot(), 0));
	if (!Save)
	{
		return;
	}

	CompletedChapters.Reset();
	for (const FName& ChapterId : Save->CompletedChapters)
	{
		CompletedChapters.Add(ChapterId);
	}
	CompletedSideQuestIds.Reset();
	for (const FName& SideId : Save->CompletedSideQuestIds)
	{
		CompletedSideQuestIds.Add(SideId);
	}
	SideBranchCounts = Save->SideBranchCounts;

	for (int32 C = 0; C < Book->Chapters.Num(); ++C)
	{
		if (Book->Chapters[C].ChapterId != Save->ActiveChapterId)
		{
			continue;
		}
		for (int32 M = 0; M < Book->Chapters[C].MainQuests.Num(); ++M)
		{
			if (Book->Chapters[C].MainQuests[M].QuestId != Save->ActiveQuestId)
			{
				continue;
			}
			for (int32 B = 0; B < Book->Chapters[C].MainQuests[M].Branches.Num(); ++B)
			{
				if (Book->Chapters[C].MainQuests[M].Branches[B].BranchId == Save->ActiveBranchId)
				{
					ChapterIndex = C;
					MainIndex = M;
					BranchIndex = B;
					BranchCount = Save->BranchCount;
					if (Save->bTrackingSide && Book->FindSide(Save->TrackedChapterId, Save->TrackedQuestId)
						&& !IsSideComplete(Save->TrackedChapterId, Save->TrackedQuestId))
					{
						bTrackingSide = true;
						TrackedChapterId = Save->TrackedChapterId;
						TrackedQuestId = Save->TrackedQuestId;
						TrackedBranchId = Save->TrackedBranchId;
					}
					return;
				}
			}
		}
	}
}
