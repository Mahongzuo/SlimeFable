#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "MiniGame/SlimeArchiveRules.h"
#include "Slime/SlimeAbilityComponent.h"
#include "Settings/SlimeCheatComponent.h"
#include "Slime/SlimeElementComponent.h"
#include "MiniGame/SlimeArchiveDirector.h"
#include "Combat/SlimeDevourComponent.h"
#include "Quest/QuestInteractActor.h"
#include "Quest/QuestObjectiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/TargetPoint.h"
#include "Engine/StaticMesh.h"
#include "Camera/CameraActor.h"
#include "GameFramework/Character.h"
#include "Materials/Material.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Quest/QuestSubsystem.h"
#include "Quest/QuestTypes.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlimeArchiveRulesTest, "SlimeFable.Archive.ComboAndShift",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlimeArchiveRulesTest::RunTest(const FString& Parameters)
{
	FSlimeArchiveRules Rules;
	for (int32 I = 0; I < 12; ++I)
	{
		TestTrue(TEXT("Matching parcel accepted"), Rules.Deposit(I % 4, I % 4));
		Rules.Tick(3.f);
	}
	TestEqual(TEXT("Four deliveries per multiplier tier"), Rules.Score, 240);
	TestTrue(TEXT("First shift passes with all destinations represented"), Rules.Passed(12, 2));
	TestFalse(TEXT("Second shift has a higher target"), Rules.Passed(16, 3));
	const int32 Previous = Rules.Correct;
	TestFalse(TEXT("Wrong destination rejected"), Rules.Deposit(0, 1));
	TestEqual(TEXT("Wrong cabinet cannot count as a delivery"), Rules.Correct, Previous);
	TestEqual(TEXT("Wrong cabinet resets combo"), Rules.Chain, 0);
	Rules.Deposit(0, 0); Rules.Tick(14.1f);
	TestEqual(TEXT("Combo expires"), Rules.Chain, 0);
	Rules.Deposit(0, 0); Rules.Miss();
	TestEqual(TEXT("Miss resets combo"), Rules.Chain, 0);
	for (int32 Week = 1; Week <= 3; ++Week)
	{
		FSlimeArchiveRules Shift;
		for (int32 I = 0; I < 8 + Week * 4; ++I) Shift.Deposit(I % 4, I % 4);
		TestTrue(TEXT("Each week's balanced delivery target can pass"), Shift.Passed(8 + Week * 4, Week + 1));
	}
	FSlimeArchiveRules Unbalanced;
	for (int32 I = 0; I < 40; ++I) Unbalanced.Deposit(0, 0);
	TestFalse(TEXT("One destination cannot satisfy the shift"), Unbalanced.Passed(12, 2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlimeArchiveRuntimeTest, "SlimeFable.Archive.SwallowDepositRestart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlimeArchiveRuntimeTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	auto* SlimePawn = World->SpawnActor<ACharacter>();
	auto* D = World->SpawnActor<ASlimeArchiveDirector>();
	D->Player = SlimePawn;
	D->Devour = NewObject<USlimeDevourComponent>(SlimePawn);
	SlimePawn->AddInstanceComponent(D->Devour); D->Devour->RegisterComponent();
	D->Devour->SwallowSound.Reset();
	D->RoomCamera = World->SpawnActor<ACameraActor>();
	D->BeltStart = World->SpawnActor<ATargetPoint>();
	D->BeltEnd = World->SpawnActor<ATargetPoint>();
	D->FolderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	D->EquipmentMesh = D->FolderMesh;
	D->PaperClass = AQuestInteractActor::StaticClass();
	for (int32 I = 0; I < 4; ++I)
	{
		D->CategoryMaterials.Add(UMaterial::GetDefaultMaterial(MD_Surface));
		auto* Target = World->SpawnActor<ATargetPoint>(FVector(I * 600.f, 600.f, 0.f), FRotator::ZeroRotator);
		D->CabinetTargets.Add(Target);
	}
	D->Random.Initialize(1958);
	D->SpawnParcel();
	TestEqual(TEXT("One parcel enters belt"), D->Parcels.Num(), 1);
	auto* Parcel = D->Parcels[0].Actor.Get();
	const int32 Category = D->Parcels[0].Category;
	if (auto* Paper = Cast<AQuestInteractActor>(Parcel))
		TestFalse(TEXT("Reused paper cannot fire its former quest"), Paper->CanBeFocused());
	const FVector Scale(.7f, .8f, .9f); Parcel->SetActorScale3D(Scale);
	SlimePawn->SetActorLocation(FVector(5000.f, 0.f, 0.f)); D->Swallow();
	TestNull(TEXT("Cannot swallow from across the room"), D->Devour->GetStoredProp());
	SlimePawn->SetActorLocation(Parcel->GetActorLocation()); D->Swallow();
	TestTrue(TEXT("Uses devour pocket"), D->Devour->GetStoredProp() == Parcel);
	D->AdvanceTransfers(0.3f);
	TestFalse(TEXT("Swallowed parcel is visible inside slime"), Parcel->IsHidden());
	TestTrue(TEXT("Belly model is scaled down"), Parcel->GetActorScale3D().X < .5f);
	const FVector BellyPosition = Parcel->GetActorLocation();
	const FRotator BellyRotation = Parcel->GetActorRotation();
	SlimePawn->AddActorWorldOffset(FVector(60.f, 0.f, 0.f)); D->AdvanceTransfers(.5f);
	TestTrue(TEXT("Belly model follows moving slime"), Parcel->GetActorLocation().X > BellyPosition.X + 50.f);
	TestFalse(TEXT("Belly model spins"), Parcel->GetActorRotation().Equals(BellyRotation));
	D->SpawnParcel(); D->Swallow();
	TestTrue(TEXT("Only one item can be swallowed"), D->Devour->GetStoredProp() == Parcel);
	SlimePawn->SetActorLocation(D->CabinetTargets[(Category + 1) % 4]->GetActorLocation()); D->Spit();
	TestEqual(TEXT("Wrong cabinet counts as a mistake"), D->Rules.Mistakes, 1);
	TestTrue(TEXT("Wrong cabinet preserves held item for correction"), D->Devour->GetStoredProp() == Parcel);
	SlimePawn->SetActorLocation(D->CabinetTargets[Category]->GetActorLocation()); D->Spit();
	TestNull(TEXT("Correct delivery frees belly"), D->Devour->GetStoredProp());
	TestEqual(TEXT("Correct delivery scores once"), D->Rules.Correct, 1);
	TestFalse(TEXT("Spit shows the existing prop"), Parcel->IsHidden());
	TestTrue(TEXT("Spit preserves original scale"), Parcel->GetActorScale3D().Equals(Scale));
	D->Spit(); TestEqual(TEXT("Repeated E cannot duplicate score"), D->Rules.Correct, 1);
	D->ResetShift();
	TestEqual(TEXT("Restart clears score"), D->Rules.Score, 0);
	TestTrue(TEXT("Restart removes parcels and flight"), D->Parcels.IsEmpty() && !D->FlyingParcel.IsValid());
	int32 Counts[4] = {0, 0, 0, 0};
	for (int32 I = 0; I < 20; ++I) { D->SpawnParcel(); ++Counts[D->Parcels.Last().Category]; }
	for (int32 Count : Counts) TestEqual(TEXT("Balanced arrivals cannot starve a cabinet"), Count, 5);
	D->ClearParcels();
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlimeArchivePIETest, "SlimeFable.Archive.RoomPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlimeArchivePIETest::RunTest(const FString& Parameters)
{
	// Keep smoke testing from writing to the player's real chapter progress.
	auto* Book = LoadObject<UDayQuestBook>(nullptr, TEXT("/Game/_Slime/Days/10/1001/Quests/DA_Quest_1001.DA_Quest_1001"));
	if (!TestNotNull(TEXT("October quest book exists"), Book)) return false;
	const bool SavedNoSave = Book->bDoNotSave;
	Book->bDoNotSave = true;
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Maps/Days/10/SL_1001_1958")));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	struct FState { int32 Stage = 0; double Time = FPlatformTime::Seconds(); FVector Start; };
	auto State = MakeShared<FState>();
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State]()
	{
		UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
		ASlimeArchiveDirector* D = nullptr;
		if (World) for (TActorIterator<ASlimeArchiveDirector> It(World); It; ++It) { D = *It; break; }
		if (!D || !D->IsModeActive())
		{
			if (FPlatformTime::Seconds() - State->Time < 30.0) return false;
			AddError(TEXT("Archive did not enter PIE within 30 seconds")); return true;
		}
		auto* PC = World->GetFirstPlayerController();
		auto* SlimePawn = D->GetPlayerCharacter();
		if (!PC || !SlimePawn) { AddError(TEXT("No playable slime in archive")); return true; }
		if (State->Stage == 0)
		{
			TestTrue(TEXT("Real map passes configuration validation"), D->Shift == ASlimeArchiveDirector::EShift::Ready);
			TestTrue(TEXT("Room HUD created"), D->HUD != nullptr);
			auto* Ability = SlimePawn->FindComponentByClass<USlimeAbilityComponent>();
			TestTrue(TEXT("Attribute and body style input remains enabled"), Ability && Ability->IsComponentTickEnabled());
			TestTrue(TEXT("Adventure camera remains attached to slime"), PC->GetViewTarget() == SlimePawn);
			TestTrue(TEXT("Normal pawn movement and mouse look stay enabled"), SlimePawn->InputEnabled() && !PC->IsLookInputIgnored());
			auto* Quests = World->GetGameInstance()->GetSubsystem<UQuestSubsystem>();
			TestTrue(TEXT("Quest accepts new shift completion branch"), Quests && Quests->CanContribute(TEXT("1958"), TEXT("Archive"), TEXT("Shift")));
			State->Start = SlimePawn->GetActorLocation();
			PC->InputKey(FInputKeyParams(EKeys::D, IE_Pressed, 1.0));
			State->Time = FPlatformTime::Seconds(); State->Stage = 1;
			return false;
		}
		if (State->Stage == 1)
		{
			if (FPlatformTime::Seconds() - State->Time < 1.0) return false;
			PC->InputKey(FInputKeyParams(EKeys::D, IE_Released, 0.0));
			TestTrue(TEXT("WASD moves real slime across floor"), FVector::Dist2D(State->Start, SlimePawn->GetActorLocation()) > 60.f);
			TestTrue(TEXT("Slime remains on room floor"), SlimePawn->GetActorLocation().Z > -20.f && SlimePawn->GetActorLocation().Z < 120.f);
			PC->InputKey(FInputKeyParams(EKeys::Enter, IE_Pressed, 1.0));
			State->Stage = 2; State->Time = FPlatformTime::Seconds(); return false;
		}
		if (State->Stage == 2)
		{
			if (FPlatformTime::Seconds() - State->Time < 0.5) return false;
			PC->InputKey(FInputKeyParams(EKeys::Enter, IE_Released, 0.0));
			TestFalse(TEXT("Starting shift keeps mouse captured for free look"), PC->bShowMouseCursor);
			if (auto* Cheat = SlimePawn->FindComponentByClass<USlimeCheatComponent>())
				TestFalse(TEXT("Enter does not open global cheat console in archive"), Cheat->IsConsoleOpen());
			TestTrue(TEXT("Enter starts conveyor and timer"), D->Shift == ASlimeArchiveDirector::EShift::Running && D->Parcels.Num() > 0);
			// Validate navigation to all four floor positions in front of intake mouths.
			for (ATargetPoint* Cabinet : D->CabinetTargets)
			{
				const FVector Destination = Cabinet->GetActorLocation() + FVector(0.f, -118.f, -90.f);
				auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, State->Start, Destination);
				TestTrue(TEXT("Each cabinet is reachable around the belt"), Path && Path->IsValid() && !Path->IsPartial());
			}
			// Finish a shift through actual runtime swallowing / delivery using the placed mouths.
			D->ResetShift();
			for (int32 I = 0; I < D->TargetCounts[D->GetWeekIndex() - 1]; ++I)
			{
				D->SpawnParcel();
				auto Parcel = D->Parcels.Last();
				SlimePawn->SetActorLocation(Parcel.Actor->GetActorLocation() + FVector(0.f, -140.f, -50.f));
				D->Swallow(); D->AdvanceTransfers(.3f);
				if (auto* Element = SlimePawn->FindComponentByClass<USlimeElementComponent>()) Element->CycleElement(1);
				TestTrue(TEXT("Real configured parcel reaches belly"), D->Devour->GetStoredProp() == Parcel.Actor);
				SlimePawn->SetActorLocation(D->CabinetTargets[Parcel.Category]->GetActorLocation() + FVector(0.f, -118.f, -90.f));
				D->Spit(); D->AdvanceTransfers(.4f);
			}
			TestTrue(TEXT("Real room shift meets completion requirements"), D->Rules.Passed(D->TargetCounts[D->GetWeekIndex() - 1], D->GetWeekIndex() + 1));
			D->Shift = ASlimeArchiveDirector::EShift::Running;
			D->Tick(.1f);
			TestTrue(TEXT("Qualification continues the running shift"), D->bQualified && D->Shift == ASlimeArchiveDirector::EShift::Running);
			TestTrue(TEXT("Timer continues after qualification"), D->TimeLeft < 180.f && D->TimeLeft > 170.f);
			const int32 ScoreAtQualification = D->Rules.Score;
			D->ClearParcels();
			D->SpawnParcel();
			auto Extra = D->Parcels.Last();
			SlimePawn->SetActorLocation(Extra.Actor->GetActorLocation()); D->Swallow(); D->AdvanceTransfers(.3f);
			SlimePawn->SetActorLocation(D->CabinetTargets[Extra.Category]->GetActorLocation()); D->Spit();
			TestTrue(TEXT("Additional deliveries continue scoring after qualification"), D->Rules.Score > ScoreAtQualification);
			D->TimeLeft = .05f; D->Tick(.1f);
			TestTrue(TEXT("Only zero time ends a qualified shift"), D->Shift == ASlimeArchiveDirector::EShift::Results && D->bSuccess && D->TimeLeft == 0.f);
			D->ResetShift();
			TestTrue(TEXT("Restart returns to ready without held items"), D->Shift == ASlimeArchiveDirector::EShift::Ready && !D->Devour->GetStoredProp());
			return true;
		}
		return true;
	}));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([Book, SavedNoSave]()
	{
		if (GEditor && GEditor->PlayWorld) return false;
		Book->bDoNotSave = SavedNoSave; return true;
	}));
	return true;
}
#endif
