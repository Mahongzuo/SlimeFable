#include "DayLevel/MuseumTravelVerification.h"
#include "DayLevel/DayLevelSubsystem.h"
#include "DayLevel/MuseumPortalSubsystem.h"
#include "DayLevel/MuseumYearPortal.h"
#include "Quest/QuestSubsystem.h"
#include "Farm/SlimeFarmField.h"
#include "Farm/SlimeFarmSubsystem.h"
#include "Exploration/SlimeEncounterDirector.h"
#include "Exploration/SlimeEncounterMember.h"
#include "Combat/SlimeHealthComponent.h"
#include "Enemy/LyraShooterEnemy.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformMisc.h"
#include "SlimeFable.h"

bool UMuseumTravelVerification::ShouldCreateSubsystem(UObject* Outer) const
{
#if WITH_EDITOR
	return FParse::Param(FCommandLine::Get(), TEXT("MuseumTravelVerify"));
#else
	return false;
#endif
}

void UMuseumTravelVerification::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Started = FPlatformTime::Seconds();
	if (FParse::Param(FCommandLine::Get(), TEXT("MuseumTravelDirectVerify"))) Phase = 100;
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick), 0.5f);
}

void UMuseumTravelVerification::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	Super::Deinitialize();
}

void UMuseumTravelVerification::Check(bool bPassed, const FString& Message)
{
	bFailed |= !bPassed;
	const FString Line = FString::Printf(TEXT("%s phase=%d: %s\n"), bPassed ? TEXT("PASS") : TEXT("FAIL"), Phase, *Message);
	Report += Line;
	UE_LOG(LogSlimeFable, Display, TEXT("MuseumVerify %s"), *Line);
}

void UMuseumTravelVerification::Finish()
{
	Report += bFailed ? TEXT("FAILED\n") : TEXT("PASSED\n");
	FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("MuseumTravelVerification.txt")));
	FPlatformMisc::RequestExitWithStatus(false, bFailed ? 1 : 0);
}

bool UMuseumTravelVerification::Tick(float Delta)
{
	if (FPlatformTime::Seconds() - Started > 900) { Check(false, TEXT("Travel timeout")); Finish(); return false; }
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || World->GetTimeSeconds() < 3.f) return true;
	UDayLevelSubsystem* Days = GetGameInstance()->GetSubsystem<UDayLevelSubsystem>();
	UQuestSubsystem* Quests = GetGameInstance()->GetSubsystem<UQuestSubsystem>();
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!Pawn || !PC || Days->IsTravelPending()) return true;
	const bool bMuseum = Days->IsMuseumHubWorld(World);
	const bool bRuin = World->GetMapName().Contains(TEXT("L_ReclaimedCity_100m"));
	auto CountFarm = [&]() { int32 N = 0; for (TActorIterator<ASlimeFarmField> It(World); It; ++It) ++N; return N; };
	auto CheckArrival = [&](FName Tag)
	{
		APlayerStart* Start = nullptr;
		for (TActorIterator<APlayerStart> It(World); It; ++It) if (It->PlayerStartTag == Tag) { Start = *It; break; }
		Check(Start && FVector::Dist2D(Pawn->GetActorLocation(), Start->GetActorLocation()) < 150, TEXT("Correct arrival: ") + Tag.ToString());
		for (TActorIterator<ADayChapterPortal> It(World); It; ++It)
		{
			const FVector Local = It->GetActorTransform().InverseTransformPosition(Pawn->GetActorLocation());
			Check(!It->IsPortalEnabled() || FMath::Abs(Local.X) > It->OverlapExtent.X || FMath::Abs(Local.Y) > It->OverlapExtent.Y,
				TEXT("Arrival outside portal: ") + It->GetName());
		}
		Check(!UGameplayStatics::IsGamePaused(World) && !PC->IsMoveInputIgnored() && !PC->IsLookInputIgnored(), TEXT("Pause cleared and input restored"));
	};
	auto FindFixed = [&](EPortalDestination Mode) -> ADayChapterPortal*
	{
		for (TActorIterator<ADayChapterPortal> It(World); It; ++It)
			if (!It->IsA<AMuseumYearPortal>() && It->DestinationMode == Mode) return *It;
		return nullptr;
	};
	if (Phase == 0 && bMuseum)
	{
		Check(FPaths::ProjectSavedDir().Contains(TEXT("RuntimeProfile")), TEXT("Isolated save directory"));
		if (bFailed) { Finish(); return false; }
		Check(Days->GetSelectedDayId() == Days->GetTodayDayId().Id, TEXT("Direct museum start selects today"));
		FarmCount = CountFarm();
		Check(FarmCount == 1, TEXT("One shared farm field"));
		UDayLevelRegistry* Original = Days->GetRegistry();
		UDayLevelRegistry* Test = DuplicateObject<UDayLevelRegistry>(Original, this);
		Days->SetRegistry(Test);
		FDayLevelEntry* Entry = Test->Entries.FindByPredicate([](const FDayLevelEntry& E) { return E.DayId == FName(TEXT("0929")); });
		const TArray<FName> Order = {TEXT("1910"),TEXT("1938"),TEXT("2020"),TEXT("2015"),TEXT("1988"),TEXT("1900"),TEXT("2100")};
		for (int32 Count : {0,1,5,6,7})
		{
			Entry->ChapterOrder.Reset(); Entry->SubLevels.Reset();
			for (int32 I=0; I<Count; ++I) { Entry->ChapterOrder.Add(Order[I]); Entry->SubLevels.Add(Order[I], Days->GetMuseumHubLevel()); }
			FString Error;
			Check(UMuseumPortalSubsystem::ValidateLayout(*Entry, {1,2,3,4,5,6}, Error) == (Count <= 6), FString::Printf(TEXT("Capacity %d; %s"), Count, *Error));
			Check(Days->TravelToDayId(World, TEXT("0929")), TEXT("Date refresh accepted"));
			Check(GetWorld() == World && CountFarm() == FarmCount, TEXT("Date change preserves world/farm actors"));
			for (TActorIterator<AMuseumYearPortal> It(World); It; ++It)
			{
				const bool bExpected = Count <= 6 && It->SlotNumber <= Count;
				Check(It->IsPortalEnabled() == bExpected, TEXT("Unused/overflow slot disabled"));
				if (bExpected) Check(It->TargetChapterId == Order[It->SlotNumber-1], TEXT("Explicit unsorted story order preserved"));
			}
		}
		FString Error;
		Check(!UMuseumPortalSubsystem::ValidateLayout(*Entry, {1,1,2,3,4,5,6,7}, Error), TEXT("Duplicate slot rejected"));
		Entry->ChapterOrder = {TEXT("Missing")}; Entry->SubLevels.Reset();
		Check(!UMuseumPortalSubsystem::ValidateLayout(*Entry, {1}, Error), TEXT("Missing mapping rejected"));
		Days->SetRegistry(Original);
		Days->TravelToDayId(World, TEXT("0815"));
		Check(Quests->GetBook() && Quests->GetHostDayId() == FName(TEXT("0815")), TEXT("Museum loads selected day's quest progress"));
		Check(Quests->IsChapterUnlocked(TEXT("1920")) && !Quests->IsChapterUnlocked(TEXT("1945")), TEXT("Initial chapter lock preserved"));
		ADayChapterPortal* Portal = FindFixed(EPortalDestination::Map);
		Check(Portal != nullptr, TEXT("User Ruin portal preserved"));
		if (bFailed || !Portal) { Finish(); return false; }
		Phase = 1;
		Check(Portal->TryInteract(Pawn), TEXT("F interaction starts Ruin travel"));
		Check(!Portal->TryInteract(Pawn), TEXT("Duplicate trigger rejected"));
	}
	else if (Phase == 1 && bRuin)
	{
		ASlimeEncounterDirector* Director = nullptr;
		for (TActorIterator<ASlimeEncounterDirector> It(World); It; ++It) Director = *It;
		AActor* Victim = nullptr;
		for (TActorIterator<APawn> It(World); It; ++It) if (It->FindComponentByClass<USlimeEncounterMember>() && !It->IsA<ALyraShooterEnemy>()) { Victim = *It; break; }
		if (!Director || !Victim) return true;
		CheckArrival(TEXT("RuinFromMuseum"));
		Check(!Quests->GetBook() && Days->GetSelectedDayId() == FName(TEXT("0815")), TEXT("Ruin retains date but has no story quests"));
		if (auto* Health = Victim->FindComponentByClass<USlimeHealthComponent>())
		{
			Health->InvulnerableUntil = -1;
			Health->ApplyDamage(Health->MaxHP * 100, Pawn, Victim->GetActorLocation(), FVector::ZeroVector);
		}
		Check(Director->KillLevel == 1, TEXT("Defeat increases Ruin progression"));
		ADayChapterPortal* Portal = FindFixed(EPortalDestination::Museum);
		Check(Portal != nullptr, TEXT("Ruin return portal exists"));
		if (bFailed || !Portal) { Finish(); return false; }
		Phase = 2;
		Pawn->SetActorLocation(Portal->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	}
	else if (Phase == 2 && bMuseum)
	{
		CheckArrival(TEXT("MuseumRuin"));
		Check(CountFarm() == FarmCount, TEXT("Round trip does not duplicate farm field"));
		ADayChapterPortal* Portal = FindFixed(EPortalDestination::Map);
		Phase = 3;
		Pawn->SetActorLocation(Portal->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	}
	else if (Phase == 3 && bRuin)
	{
		for (TActorIterator<ASlimeEncounterDirector> It(World); It; ++It) Check(It->KillLevel == 1, TEXT("Same-day Ruin strength survives actual map round trip"));
		Phase = 4;
		UGameplayStatics::SetGamePaused(World, true);
		PC->SetIgnoreMoveInput(true);
		Check(Days->TravelToMuseumHub(World), TEXT("Pause-menu return route accepted"));
	}
	else if (Phase == 4 && bMuseum)
	{
		CheckArrival(TEXT("MuseumRuin"));
		Days->TravelToDayId(World, TEXT("0812"));
		AMuseumYearPortal* Portal = nullptr;
		for (TActorIterator<AMuseumYearPortal> It(World); It; ++It) if (It->SlotNumber == 1) Portal = *It;
		Check(Portal && Portal->DestinationMode == EPortalDestination::Map && Portal->IsPortalEnabled(), TEXT("0812 compatibility door bound"));
		if (!Portal) { Finish(); return false; }
		Phase = 5;
		Portal->TryInteract(Pawn);
	}
	else if (Phase == 5 && World->GetMapName().EndsWith(TEXT("0812")))
	{
		Check(Days->GetSelectedDayId() == FName(TEXT("0812")), TEXT("Original 0812 world reached without copying"));
		Phase = 6;
		Days->TravelToMuseumHub(World);
	}
	else if (Phase == 6 && bMuseum)
	{
		CheckArrival(TEXT("MuseumYear_1"));
		Days->TravelToDayId(World, TEXT("0815"));
		UDayQuestSaveGame* Save = NewObject<UDayQuestSaveGame>();
		Save->CompletedChapters.Add(TEXT("1920"));
		Save->HighestWeekByChapter.Add(TEXT("1920"), 2);
		Save->HighestWeekByChapter.Add(TEXT("1945"), 2);
		UGameplayStatics::SaveGameToSlot(Save, TEXT("0815"), 0);
		Quests->RefreshMuseumDate(World);
		Check(Quests->GetHighestWeek(TEXT("1945")) == 2 && Quests->GetHighestWeek(TEXT("1962")) == 1, TEXT("Per-year week unlock remains independent"));
		for (TActorIterator<AMuseumYearPortal> It(World); It; ++It)
			if (It->TargetChapterId == FName(TEXT("1945"))) { It->EnterDelaySeconds = 0; It->RequestEnter(Pawn); }
		Check(Quests->IsWeekSelectOpen(), TEXT("Existing week selector opens from numbered door"));
		Phase = 7;
		Check(Quests->TravelToChapter(TEXT("0815"), TEXT("1945"), 2), TEXT("Selected 1945 week 2 travel"));
	}
	else if (Phase == 7 && World->GetMapName().Contains(TEXT("SL_0815_1945")))
	{
		Check(Quests->GetActiveDayId() == FName(TEXT("0815")) && Quests->GetActiveChapterId() == FName(TEXT("1945")), TEXT("1945 quest and date restored"));
		Phase = 8;
		Quests->TravelToHub(TEXT("0815"));
	}
	else if (Phase == 8 && bMuseum)
	{
		CheckArrival(TEXT("MuseumYear_2"));
		Days->TravelToDayId(World, TEXT("0929"));
		Check(!Quests->GetBook(), TEXT("Empty date clears previous book"));
		Days->TravelToDayId(World, TEXT("0815"));
		Check(Quests->GetHighestWeek(TEXT("1945")) == 2, TEXT("Date switch does not mix MMDD progress"));
		Days->TravelToToday(World);
		Check(Days->GetSelectedDayId() == Days->GetTodayDayId().Id, TEXT("Today action reselects real date"));
		Finish(); return false;
	}
	else if (Phase == 100 && bRuin)
	{
		Check(!Quests->GetBook(), TEXT("Direct Ruin has no story book"));
		Phase = 101;
		Days->TravelToMuseumHub(World);
	}
	else if (Phase == 101 && bMuseum)
	{
		CheckArrival(NAME_None);
		Check(Days->GetSelectedDayId() == Days->GetTodayDayId().Id, TEXT("Direct Ruin return defaults to today's museum"));
		Finish(); return false;
	}
	return true;
}
