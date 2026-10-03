#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "MiniGame/SlimeTrackDirector.h"
#include "MiniGame/SlimeTrackActors.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
struct FTrackTestWorld
{
	UWorld* World;
	FTrackTestWorld()
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
	}
	~FTrackTestWorld()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlimeTrackRulesTest, "SlimeFable.Track.RotationUndoAndInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlimeTrackRulesTest::RunTest(const FString& Parameters)
{
	FTrackTestWorld Scope;
	auto* D = Scope.World->SpawnActor<ASlimeTrackDirector>();
	auto* B = Scope.World->SpawnActor<ASlimeTrackBoard>();
	B->Rows = {TEXT("#####"), TEXT("#...#"), TEXT("#.P.#"), TEXT("#...#"), TEXT("#####")};
	B->EnsureParsed(); D->Boards.Add(B); D->SlimeX = 2; D->SlimeY = 2;
	auto* East = Scope.World->SpawnActor<ASlimeTrackPiece>();
	East->GridX = 3; East->GridY = 2; East->Kind = ESlimeTrackKind::Curve;
	auto* North = Scope.World->SpawnActor<ASlimeTrackPiece>();
	North->GridX = 2; North->GridY = 1; North->Kind = ESlimeTrackKind::Straight;
	D->AllPieces = {East, North}; D->Facing = 1;
	TestTrue(TEXT("Facing curve selected"), D->RotationTarget() == East);
	D->TryRotate();
	TestEqual(TEXT("First track rotates"), East->QuarterTurns, 1);
	D->CycleTarget();
	TestTrue(TEXT("E selects the other adjacent track"), D->RotationTarget() == North);
	D->TryRotate();
	TestEqual(TEXT("Straight track also rotates"), North->QuarterTurns, 1);
	D->Undo();
	TestEqual(TEXT("Undo restores second rotation"), North->QuarterTurns, 0);
	TestEqual(TEXT("Undo refunds step"), D->Steps, 1);
	D->TryMove(1);
	TestEqual(TEXT("Blocked push updates facing"), D->Facing, 1);
	TestEqual(TEXT("Blocked push costs no step"), D->Steps, 1);
	East->SetQuarter(1, false);
	D->QueueAction(4); D->QueueAction(7);
	TestEqual(TEXT("Busy animation retains both taps"), D->InputQueue.Num(), 2);
	D->TickPlay(0.01f);
	TestEqual(TEXT("Tick does not consume queued actions while rail animates"), D->InputQueue.Num(), 2);
	D->TickPlay(1.f);
	TestEqual(TEXT("Next ready tick executes first buffered action"), D->InputQueue.Num(), 1);
	TestEqual(TEXT("Buffered F rotates the aimed rail"), East->QuarterTurns, 2);
	North->Kind = ESlimeTrackKind::Fixed; D->Facing = 0;
	TestTrue(TEXT("Fixed rail is not a rotation target"), D->RotationTarget() == East);
	B->bAllowBlob = true;
	D->TrySplit();
	if (!TestNotNull(TEXT("Split creates a blob"), D->Blob.Get())) return false;
	const FVector BlobScale(0.42f);
	TestTrue(TEXT("Spawned blob keeps its small scale"), D->Blob->GetActorScale3D().Equals(BlobScale));
	for (int32 Switch = 0; Switch < 4; ++Switch)
	{
		D->TrySwitch();
		TestTrue(TEXT("Switching control never enlarges the blob"), D->Blob->GetActorScale3D().Equals(BlobScale));
	}
	D->Undo();
	TestTrue(TEXT("Undo restores blob control"), D->bControlBlob);
	TestTrue(TEXT("Undo respawns the blob at the same small scale"), D->Blob && D->Blob->GetActorScale3D().Equals(BlobScale));
	D->TrySplit();
	D->Undo();
	TestTrue(TEXT("Undo recall preserves small scale"), D->Blob && D->Blob->GetActorScale3D().Equals(BlobScale));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlimeTrackReplayTest, "SlimeFable.Track.FiveBoardSolutions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlimeTrackReplayTest::RunTest(const FString& Parameters)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *(FPaths::ProjectSavedDir() / TEXT("MiniGame/1969_solutions.json"))))
	{
		AddError(TEXT("Generate Saved/MiniGame/1969_solutions.json with metro_1969_solver.py first")); return false;
	}
	TArray<TSharedPtr<FJsonValue>> Specs;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Specs)) return false;
	TestEqual(TEXT("Five boards exported"), Specs.Num(), 5);
	for (const auto& Value : Specs)
	{
		const auto S = Value->AsObject();
		FTrackTestWorld Scope;
		auto* D = Scope.World->SpawnActor<ASlimeTrackDirector>();
		auto* B = Scope.World->SpawnActor<ASlimeTrackBoard>();
		for (const auto& Row : S->GetArrayField(TEXT("rows"))) B->Rows.Add(Row->AsString());
		B->StartFacing = S->GetIntegerField(TEXT("start_facing"));
		B->EndFacing = S->GetIntegerField(TEXT("end_facing"));
		B->bAllowBlob = S->GetBoolField(TEXT("allow_blob"));
		B->EnsureParsed(); D->Boards.Add(B);
		B->FindCell(TEXT('P'), D->SlimeX, D->SlimeY); D->Facing = 1;
		for (const auto& P : S->GetArrayField(TEXT("pieces")))
		{
			const auto& A = P->AsArray();
			auto* Piece = Scope.World->SpawnActor<ASlimeTrackPiece>();
			Piece->Kind = A[0]->AsString() == TEXT("C") ? ESlimeTrackKind::Curve :
				(A[0]->AsString() == TEXT("F") ? ESlimeTrackKind::Fixed : ESlimeTrackKind::Straight);
			Piece->GridX = A[1]->AsNumber(); Piece->GridY = A[2]->AsNumber();
			Piece->SetQuarter(A[3]->AsNumber(), true);
			D->AllPieces.Add(Piece);
		}
		TestTrue(TEXT("Board is initially disconnected"), D->BuildPath().IsEmpty());
		for (TCHAR Action : S->GetStringField(TEXT("solution")))
		{
			int32 Dir = FString(TEXT("NESW")).Find(FString::Chr(Action));
			if (Dir != INDEX_NONE) TestTrue(TEXT("Solution move is legal"), D->TryMove(Dir));
			else if (Action == TEXT('F')) TestTrue(TEXT("Solution rotation is legal"), D->TryRotate());
			else if (Action == TEXT('C')) D->CycleTarget();
			else if (Action == TEXT('Q')) D->TrySplit();
			else if (Action == TEXT('T')) D->TrySwitch();
			for (ASlimeTrackPiece* Piece : D->AllPieces) Piece->Advance(1.f);
			D->AdvanceHop(1.f);
		}
		TestTrue(TEXT("Runtime connects start to destination"), D->BuildPath().Num() >= 2);
		TestEqual(TEXT("Runtime step cost matches solver"), D->Steps, S->GetIntegerField(TEXT("optimal")));
		for (const auto& Limit : S->GetArrayField(TEXT("limits")))
			TestTrue(TEXT("Solution fits each week's budget"), D->Steps <= Limit->AsNumber());
		D->StartRide();
		TestEqual(TEXT("Every train waypoint has a distance"), D->RideDistances.Num(), D->RidePoints.Num());
		for (int32 Index = 1; Index < D->RideDistances.Num(); ++Index)
			TestTrue(TEXT("Train route advances continuously"), D->RideDistances[Index] > D->RideDistances[Index - 1]);
		D->Undo();
		TestTrue(TEXT("Undo final action disconnects track again"), D->BuildPath().IsEmpty());
	}
	return true;
}
#endif
