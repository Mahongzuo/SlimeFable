#include "MiniGame/SlimeTrackDirector.h"
#include "MiniGame/SlimeTrackActors.h"
#include "MiniGame/SlimeTrackHUDWidget.h"
#include "Quest/QuestSubsystem.h"
#include "SlimeFable.h"
#include "Algo/Reverse.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/PointLight.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const int32 DX[4] = {0, 1, 0, -1};
	const int32 DY[4] = {-1, 0, 1, 0};

	void Ports(ESlimeTrackKind Kind, int32 Quarter, int32& OutA, int32& OutB)
	{
		Quarter &= 3;
		if (Kind == ESlimeTrackKind::Curve)
		{
			OutA = Quarter;
			OutB = (Quarter + 1) & 3;
			return;
		}
		OutA = (Quarter & 1) ? 0 : 1;
		OutB = (OutA + 2) & 3;
	}
}

ASlimeTrackDirector::ASlimeTrackDirector()
{
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	DisabledComponentClasses.AddUnique(TEXT("SlimeMorphComponent"));
	DisabledComponentClasses.AddUnique(TEXT("SlimeVehicleComponent"));
	DisabledComponentClasses.AddUnique(TEXT("SlimeCombatComponent"));
	ChapterId = TEXT("1969");
	QuestId = TEXT("Metro");
	FinishBranchId = TEXT("Open");
	FinaleKicker = FText::FromString(TEXT("1969年10月1日"));
	FinaleText = FText::FromString(TEXT("北京地铁一期工程建成通车，线路从苹果园到北京站"));
}

void ASlimeTrackDirector::EnterMode(ACharacter* Character, APlayerController* PC)
{
	PC->SetIgnoreMoveInput(true);
	PC->SetIgnoreLookInput(true);
	bPawnInputWasEnabled = Character->InputEnabled();
	Character->DisableInput(PC);
	bInputLocked = true;
	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}

	Gather();
	if (Boards.Num() == 0)
	{
		UE_LOG(LogSlimeFable, Error, TEXT("Track: no boards in the level"));
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	Cam = GetWorld()->SpawnActor<ACameraActor>(Character->GetActorLocation(), FRotator(-75.f, -90.f, 0.f), Params);
	if (Cam)
	{
		Cam->GetCameraComponent()->SetFieldOfView(55.f);
		Cam->GetCameraComponent()->bConstrainAspectRatio = false;
		PreviousView = PC->GetViewTarget();
		PC->SetViewTargetWithBlend(Cam, 0.35f);
	}

	HUD = CreateWidget<USlimeTrackHUDWidget>(PC, USlimeTrackHUDWidget::StaticClass());
	if (HUD)
	{
		HUD->AddToViewport(5);
	}

	TimeLeft = Week3TimeLimit;
	DimLamps();
	StartBoardIndex(0, false);
	UE_LOG(LogSlimeFable, Log, TEXT("Track: %d boards, week %d"), Boards.Num(), GetWeekIndex());
	if (GetWeekIndex() == 3)
	{
		ShowLine(FText::FromString(TEXT("三周目")),
			FText::FromString(FString::Printf(TEXT("%d 秒内接通五段线路"), FMath::RoundToInt(Week3TimeLimit))), 3.f);
	}
}

void ASlimeTrackDirector::ExitMode(ACharacter* Character, APlayerController* PC)
{
	if (bInputLocked && PC)
	{
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
		bInputLocked = false;
	}
	if (Character)
	{
		if (bPawnInputWasEnabled) Character->EnableInput(PC);
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			Move->SetMovementMode(MOVE_Walking);
		}
	}
	if (PC && PreviousView.IsValid())
	{
		PC->SetViewTarget(PreviousView.Get());
	}
	if (HUD)
	{
		HUD->RemoveFromParent();
		HUD = nullptr;
	}
	if (Cam)
	{
		Cam->Destroy();
		Cam = nullptr;
	}
	DestroyBlob();
}

void ASlimeTrackDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsModeActive() || IsFinished() || Boards.Num() == 0)
	{
		return;
	}
	if (Phase != EPhase::Finale && GetWeekIndex() == 3)
	{
		TimeLeft -= DeltaSeconds;
		if (TimeLeft <= 0.f)
		{
			ShowLine(FText::FromString(TEXT("超时")), FText::FromString(TEXT("再铺一次")), 2.f);
			FailMiniGame();
			return;
		}
	}
	switch (Phase)
	{
	case EPhase::Play: TickPlay(DeltaSeconds); break;
	case EPhase::Ride: TickRide(DeltaSeconds); break;
	case EPhase::Finale: TickFinale(DeltaSeconds); break;
	}
	UpdateCamera(DeltaSeconds);
	UpdateHUD();
}

void ASlimeTrackDirector::Gather()
{
	Boards.Reset();
	for (TActorIterator<ASlimeTrackBoard> It(GetWorld()); It; ++It)
	{
		It->EnsureParsed();
		Boards.Add(*It);
	}
	Boards.Sort([](const ASlimeTrackBoard& A, const ASlimeTrackBoard& B)
	{
		return A.BoardIndex < B.BoardIndex;
	});
	AllPieces.Reset();
	for (TActorIterator<ASlimeTrackPiece> It(GetWorld()); It; ++It)
	{
		AllPieces.Add(*It);
	}
	Train = nullptr;
	for (TActorIterator<ASlimeTrackTrain> It(GetWorld()); It; ++It)
	{
		Train = *It;
		break;
	}
	Lamps.Reset();
	LampBase.Reset();
	for (TActorIterator<APointLight> It(GetWorld()); It; ++It)
	{
		if (!It->ActorHasTag(TEXT("1969Lamp")) || !It->GetLightComponent())
		{
			continue;
		}
		Lamps.Add(*It);
		LampBase.Add(It->GetLightComponent()->Intensity);
	}
}

void ASlimeTrackDirector::StartBoardIndex(int32 Index, bool bFromRide)
{
	BoardIdx = Index;
	Phase = EPhase::Play;
	Steps = 0;
	InputQueue.Reset();
	UndoHistory.Reset();
	Facing = 1;
	bPending = false;
	bHopping = false;
	HopActor = nullptr;
	bControlBlob = false;
	DestroyBlob();

	ASlimeTrackBoard* Board = CurrentBoard();
	if (!Board)
	{
		return;
	}
	int32 X = 0;
	int32 Y = 0;
	if (!Board->FindCell(TEXT('P'), X, Y))
	{
		UE_LOG(LogSlimeFable, Error, TEXT("Track: board %d has no P"), BoardIdx);
	}
	SlimeX = X;
	SlimeY = Y;
	if (ACharacter* Character = GetPlayerCharacter())
	{
		Character->SetActorLocation(Board->Stand(X, Y), false, nullptr, ETeleportType::TeleportPhysics);
	}
	for (ASlimeTrackPiece* Piece : AllPieces)
	{
		if (Piece && Piece->BoardIndex == BoardIdx)
		{
			Piece->ResetHome(Board->CellCenter(Piece->HomeX, Piece->HomeY));
		}
	}
	ParkTrain();
	FVector Location;
	FRotator Rotation;
	BoardCamera(Board, Location, Rotation);
	BlendCamera(Location, Rotation, bFromRide ? CameraBlendSeconds : 0.45f);
	RefreshHints();
}

void ASlimeTrackDirector::ResetBoard(bool bOutOfSteps)
{
	const int32 Index = BoardIdx;
	StartBoardIndex(Index, false);
	ShowLine(
		FText::FromString(bOutOfSteps ? TEXT("步数用完") : TEXT("重来")),
		FText::FromString(TEXT("这一屏重新铺")), 1.4f);
}

void ASlimeTrackDirector::TickPlay(float DeltaSeconds)
{
	ASlimeTrackBoard* Board = CurrentBoard();
	if (!Board)
	{
		return;
	}
	for (ASlimeTrackPiece* Piece : AllPieces)
	{
		if (Piece && Piece->BoardIndex == BoardIdx)
		{
			Piece->Advance(DeltaSeconds);
		}
	}
	AdvanceHop(DeltaSeconds);

	const bool bW = Consume(EKeys::W, WasW);
	const bool bA = Consume(EKeys::A, WasA);
	const bool bS = Consume(EKeys::S, WasS);
	const bool bD = Consume(EKeys::D, WasD);
	const bool bF = Consume(EKeys::F, WasF);
	const bool bQ = Consume(EKeys::Q, WasQ);
	const bool bTab = Consume(EKeys::Tab, WasTab);
	const bool bR = Consume(EKeys::R, WasR);
	const bool bE = Consume(EKeys::E, WasE);
	const bool bZ = Consume(EKeys::Z, WasZ);
	if (bR)
	{
		ResetBoard(false);
		return;
	}
	if (bZ)
	{
		Undo();
		return;
	}
	if (bW) QueueAction(DirForKey(0));
	else if (bD) QueueAction(DirForKey(1));
	else if (bS) QueueAction(DirForKey(2));
	else if (bA) QueueAction(DirForKey(3));
	else if (bF) QueueAction(4);
	else if (bQ) QueueAction(5);
	else if (bTab) QueueAction(6);
	else if (bE) QueueAction(7);
	if (AnyBusy())
	{
		RefreshHints();
		return;
	}
	if (bPending)
	{
		bPending = false;
		if (BuildPath().Num() >= 2)
		{
			StartRide();
			return;
		}
	}
	if (!InputQueue.IsEmpty())
	{
		const int32 Action = InputQueue[0];
		InputQueue.RemoveAt(0);
		if (Action <= 4 && Steps >= StepLimit())
		{
			ShowLine(FText::FromString(TEXT("步数用完")), FText::FromString(TEXT("Z 撤销一步 · R 重来这一段")), 2.f);
		}
		else if (Action < 4) TryMove(Action);
		else if (Action == 4) TryRotate();
		else if (Action == 5) TrySplit();
		else if (Action == 6) TrySwitch();
		else if (Action == 7) CycleTarget();
	}
	RefreshHints();
}

void ASlimeTrackDirector::TickRide(float DeltaSeconds)
{
	RideT += DeltaSeconds;
	if (Train && RidePoints.Num() >= 2)
	{
		const float Along = FMath::Clamp(RideT / RideDur, 0.f, 1.f) * RideDistances.Last();
		int32 Index = 0;
		while (Index + 2 < RidePoints.Num() && RideDistances[Index + 1] < Along) ++Index;
		const float Alpha = (Along - RideDistances[Index]) / FMath::Max(0.01f, RideDistances[Index + 1] - RideDistances[Index]);
		Train->SetActorLocation(FMath::Lerp(RidePoints[Index], RidePoints[Index + 1], Alpha));
		const FVector Delta = RidePoints[Index + 1] - RidePoints[Index];
		if (!Delta.IsNearlyZero())
		{
			Train->SetActorRotation(Delta.GetSafeNormal().Rotation());
		}
	}
	if (RideT < RideDur)
	{
		return;
	}
	if (BoardIdx + 1 < Boards.Num())
	{
		StartBoardIndex(BoardIdx + 1, true);
	}
	else
	{
		StartFinale();
	}
}

void ASlimeTrackDirector::TickFinale(float DeltaSeconds)
{
	FinaleT += DeltaSeconds;
	const float Along = FMath::Clamp(FinaleT / FinaleSeconds, 0.f, 1.f) * 2600.f;
	const FVector Travel = FinaleForward * Along;
	if (Train)
	{
		Train->SetActorLocation(FinaleOrigin + Travel);
		Train->SetActorRotation(FinaleForward.Rotation());
	}
	if (Cam)
	{
		const FVector Eye = FinaleOrigin + Travel + FVector(0.f, 0.f, 150.f) + FinaleForward * 160.f;
		Cam->SetActorLocation(Eye);
		Cam->SetActorRotation(FinaleForward.Rotation() + FRotator(-8.f, 0.f, 0.f));
	}
	FlickerLamps();
	if (!bFinaleBanner && FinaleT >= 2.f)
	{
		bFinaleBanner = true;
		ShowLine(FinaleKicker, FinaleText, FMath::Max(4.f, FinaleSeconds - 2.f));
	}
	if (FinaleT >= FinaleSeconds)
	{
		FinishMiniGame();
	}
}

void ASlimeTrackDirector::UpdateCamera(float DeltaSeconds)
{
	if (Phase == EPhase::Finale || !Cam || CamAlpha >= 1.f)
	{
		return;
	}
	CamAlpha = FMath::Min(1.f, CamAlpha + DeltaSeconds / CamDur);
	const float Alpha = FMath::InterpEaseInOut(0.f, 1.f, CamAlpha, 2.f);
	Cam->SetActorLocation(FMath::Lerp(CamFrom, CamTo, Alpha));
	Cam->SetActorRotation(FMath::Lerp(CamRotFrom, CamRotTo, Alpha));
}

void ASlimeTrackDirector::UpdateHUD()
{
	if (!HUD)
	{
		return;
	}
	const ASlimeTrackBoard* Board = CurrentBoard();
	FText Route = FText::FromString(TEXT("首班车开往北京站"));
	if (Phase != EPhase::Finale && Board)
	{
		Route = FText::FromString(FString::Printf(TEXT("%s → %s"),
			*Board->StartName.ToString(), *Board->EndName.ToString()));
	}
	const int32 Lit = FMath::Clamp(BoardIdx + 1 + (Phase != EPhase::Play ? 1 : 0), 1, 6);
	const float ShownTime = (GetWeekIndex() == 3 && Phase != EPhase::Finale) ? TimeLeft : -1.f;
	const bool bBlob = Board && Board->bAllowBlob;
	const FText Keys = FText::FromString(bBlob
		? TEXT("WASD 走一格，撞上就推  ·  F 旋转金框轨道 · E 换目标 · Z 撤销 · Q 分团/收回 · Tab 切换 · R 重来")
		: TEXT("WASD 走一格，撞上就推  ·  F 旋转金框轨道 · E 换目标 · Z 撤销 · R 重来"));
	HUD->SetStats(Route, Steps, StepLimit(), Lit, ShownTime, Keys);
}

void ASlimeTrackDirector::ShowLine(const FText& Kicker, const FText& Line, float Seconds)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UQuestSubsystem* Quests = GameInstance->GetSubsystem<UQuestSubsystem>())
		{
			Quests->ShowCenterBanner(Kicker, Line, Seconds);
		}
	}
}

bool ASlimeTrackDirector::Consume(const FKey& Key, bool& WasDown)
{
	const APlayerController* PC = GetPlayerController();
	const bool bDown = PC && PC->IsInputKeyDown(Key);
	const bool bEdge = bDown && !WasDown;
	WasDown = bDown;
	return bEdge;
}

int32 ASlimeTrackDirector::DirForKey(int32 KeyIndex) const
{
	float Yaw = -90.f;
	if (const ASlimeTrackBoard* Board = CurrentBoard())
	{
		if (Board->bUseCamera)
		{
			Yaw = Board->CameraRotation.Yaw;
		}
	}
	const int32 ScreenUp = FMath::RoundToInt((Yaw + 90.f) / 90.f) & 3;
	return (ScreenUp + KeyIndex) & 3;
}

bool ASlimeTrackDirector::TryMove(int32 Dir)
{
	ASlimeTrackBoard* Board = CurrentBoard();
	if (!Board)
	{
		return false;
	}
	const bool bBlob = bControlBlob && bBlobOut;
	const int32 X = bBlob ? BlobX : SlimeX;
	const int32 Y = bBlob ? BlobY : SlimeY;
	Facing = Dir; // A blocked push still aims at that track.
	const int32 NX = X + DX[Dir];
	const int32 NY = Y + DY[Dir];
	if (!Board->InBounds(NX, NY))
	{
		return false;
	}
	if (ASlimeTrackPiece* Piece = PieceAt(NX, NY))
	{
		if (Piece->Kind == ESlimeTrackKind::Fixed)
		{
			return false;
		}
		const int32 BeyondX = NX + DX[Dir];
		const int32 BeyondY = NY + DY[Dir];
		if (!CanHoldPiece(BeyondX, BeyondY))
		{
			return false;
		}
		SaveUndo();
		Piece->MoveTo(BeyondX, BeyondY, Board->CellCenter(BeyondX, BeyondY), false);
		MoveActive(NX, NY, Dir);
		return true;
	}
	const TCHAR Cell = Board->GetCell(NX, NY);
	const bool bFloor = Cell == TEXT('.') || Cell == TEXT('P');
	if (bBlob)
	{
		if (!bFloor && Cell != TEXT('d'))
		{
			return false;
		}
	}
	else if (!bFloor)
	{
		return false;
	}
	if (OtherOccupies(NX, NY))
	{
		return false;
	}
	SaveUndo();
	MoveActive(NX, NY, Dir);
	return true;
}

ASlimeTrackPiece* ASlimeTrackDirector::RotationTarget() const
{
	const bool bBlob = bControlBlob && bBlobOut;
	const int32 X = bBlob ? BlobX : SlimeX;
	const int32 Y = bBlob ? BlobY : SlimeY;
	// Facing first, then the stable N/E/S/W fallback.
	for (int32 Index = -1; Index < 4; ++Index)
	{
		const int32 Dir = Index < 0 ? Facing : Index;
		ASlimeTrackPiece* Piece = PieceAt(X + DX[Dir], Y + DY[Dir]);
		if (Piece && Piece->Kind != ESlimeTrackKind::Fixed) return Piece;
	}
	return nullptr;
}

void ASlimeTrackDirector::CycleTarget()
{
	const ASlimeTrackPiece* Current = RotationTarget();
	if (!Current) return;
	const int32 X = bControlBlob && bBlobOut ? BlobX : SlimeX;
	const int32 Y = bControlBlob && bBlobOut ? BlobY : SlimeY;
	int32 Start = Facing;
	for (int32 Dir = 0; Dir < 4; ++Dir)
	{
		if (PieceAt(X + DX[Dir], Y + DY[Dir]) == Current) Start = Dir;
	}
	for (int32 Offset = 1; Offset <= 4; ++Offset)
	{
		const int32 Dir = (Start + Offset) & 3;
		ASlimeTrackPiece* Piece = PieceAt(X + DX[Dir], Y + DY[Dir]);
		if (Piece && Piece->Kind != ESlimeTrackKind::Fixed)
		{
			Facing = Dir;
			return;
		}
	}
}

bool ASlimeTrackDirector::TryRotate()
{
	ASlimeTrackPiece* Piece = RotationTarget();
	if (!Piece) return false;
	SaveUndo();
	Piece->SetQuarter((Piece->QuarterTurns + 1) & 3, false);
	Steps += 1;
	bPending = true;
	return true;
}

void ASlimeTrackDirector::QueueAction(int32 Action)
{
	// A short bounded queue keeps quick taps through the movement/rotation animation.
	if (InputQueue.Num() < 4) InputQueue.Add(Action);
}

void ASlimeTrackDirector::SaveUndo()
{
	FUndoState State{SlimeX, SlimeY, BlobX, BlobY, Facing, Steps, bBlobOut, bControlBlob, {}};
	for (ASlimeTrackPiece* Piece : AllPieces)
	{
		if (Piece && Piece->BoardIndex == BoardIdx)
			State.Pieces.Add(FIntVector(Piece->GridX, Piece->GridY, Piece->QuarterTurns));
	}
	UndoHistory.Add(MoveTemp(State));
}

void ASlimeTrackDirector::Undo()
{
	if (UndoHistory.IsEmpty()) return;
	const FUndoState State = UndoHistory.Pop();
	InputQueue.Reset();
	bPending = false;
	bHopping = false;
	HopActor = nullptr;
	DestroyBlob();
	SlimeX = State.X; SlimeY = State.Y;
	Facing = State.Direction; Steps = State.StepCount;
	ASlimeTrackBoard* Board = CurrentBoard();
	if (!Board) return;
	if (ACharacter* Character = GetPlayerCharacter())
		Character->SetActorLocation(Board->Stand(SlimeX, SlimeY), false, nullptr, ETeleportType::TeleportPhysics);
	int32 Index = 0;
	for (ASlimeTrackPiece* Piece : AllPieces)
	{
		if (!Piece || Piece->BoardIndex != BoardIdx) continue;
		const FIntVector At = State.Pieces[Index++];
		Piece->MoveTo(At.X, At.Y, Board->CellCenter(At.X, At.Y), true);
		Piece->SetQuarter(At.Z, true);
	}
	if (State.bBlobOut)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		Blob = GetWorld()->SpawnActor<ASlimeTrackBlob>(Board->Stand(State.BX, State.BY), FRotator::ZeroRotator, Params);
		if (Blob)
		{
			BlobX = State.BX; BlobY = State.BY;
			bBlobOut = true; bControlBlob = State.bControlBlob;
			Blob->SetActorScale3D(FVector(0.42f));
		}
	}
	RefreshHints();
	UpdateHUD();
}

void ASlimeTrackDirector::TrySplit()
{
	ASlimeTrackBoard* Board = CurrentBoard();
	if (!Board || !Board->bAllowBlob)
	{
		return;
	}
	if (bBlobOut)
	{
		SaveUndo();
		DestroyBlob();
		return;
	}
	const int32 Order[4] = {Facing, (Facing + 1) & 3, (Facing + 3) & 3, (Facing + 2) & 3};
	for (int32 Dir : Order)
	{
		const int32 X = SlimeX + DX[Dir];
		const int32 Y = SlimeY + DY[Dir];
		if (!CanBlobStand(X, Y))
		{
			continue;
		}
		SaveUndo();
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		Blob = GetWorld()->SpawnActor<ASlimeTrackBlob>(Board->Stand(X, Y), FRotator::ZeroRotator, Params);
		if (Blob)
		{
			BlobX = X;
			BlobY = Y;
			bBlobOut = true;
			bControlBlob = false;
			Blob->SetActorScale3D(FVector(0.42f));
		}
		return;
	}
}

void ASlimeTrackDirector::TrySwitch()
{
	if (!bBlobOut || !Blob)
	{
		return;
	}
	SaveUndo();
	bControlBlob = !bControlBlob;
	Blob->SetActorScale3D(FVector(0.42f));
}

void ASlimeTrackDirector::MoveActive(int32 X, int32 Y, int32 Dir)
{
	Facing = Dir;
	Steps += 1;
	bPending = true;
	ASlimeTrackBoard* Board = CurrentBoard();
	const FVector Dest = Board->Stand(X, Y);
	if (bControlBlob && bBlobOut)
	{
		BlobX = X;
		BlobY = Y;
		BeginHop(Blob, Dest);
	}
	else
	{
		SlimeX = X;
		SlimeY = Y;
		BeginHop(GetPlayerCharacter(), Dest);
	}
}

void ASlimeTrackDirector::BeginHop(AActor* Actor, const FVector& Dest)
{
	if (!Actor)
	{
		return;
	}
	HopActor = Actor;
	HopFrom = Actor->GetActorLocation();
	HopTo = Dest;
	HopT = 0.f;
	bHopping = true;
}

void ASlimeTrackDirector::AdvanceHop(float DeltaSeconds)
{
	if (!bHopping)
	{
		return;
	}
	AActor* Actor = HopActor.Get();
	if (!Actor)
	{
		bHopping = false;
		return;
	}
	HopT += DeltaSeconds;
	const float Alpha = FMath::Clamp(HopT / FMath::Max(HopSeconds, 0.01f), 0.f, 1.f);
	FVector Pos = FMath::Lerp(HopFrom, HopTo, Alpha);
	Pos.Z += FMath::Sin(Alpha * UE_PI) * 36.f;
	Actor->SetActorLocation(Pos, false, nullptr, ETeleportType::TeleportPhysics);
	if (Alpha >= 1.f)
	{
		Actor->SetActorLocation(HopTo, false, nullptr, ETeleportType::TeleportPhysics);
		bHopping = false;
		HopActor = nullptr;
	}
}

void ASlimeTrackDirector::DestroyBlob()
{
	if (HopActor.Get() == Blob)
	{
		bHopping = false;
		HopActor = nullptr;
	}
	if (Blob)
	{
		Blob->Destroy();
		Blob = nullptr;
	}
	bBlobOut = false;
	bControlBlob = false;
	BlobX = -1;
	BlobY = -1;
}

bool ASlimeTrackDirector::CanHoldPiece(int32 X, int32 Y) const
{
	const ASlimeTrackBoard* Board = CurrentBoard();
	if (!Board || !Board->InBounds(X, Y))
	{
		return false;
	}
	const TCHAR Cell = Board->GetCell(X, Y);
	if (Cell != TEXT('.') && Cell != TEXT('P'))
	{
		return false;
	}
	if (PieceAt(X, Y))
	{
		return false;
	}
	if (SlimeX == X && SlimeY == Y)
	{
		return false;
	}
	return !(bBlobOut && BlobX == X && BlobY == Y);
}

bool ASlimeTrackDirector::OtherOccupies(int32 X, int32 Y) const
{
	if (bControlBlob && bBlobOut)
	{
		return SlimeX == X && SlimeY == Y;
	}
	return bBlobOut && BlobX == X && BlobY == Y;
}

bool ASlimeTrackDirector::CanBlobStand(int32 X, int32 Y) const
{
	const ASlimeTrackBoard* Board = CurrentBoard();
	if (!Board || !Board->InBounds(X, Y) || PieceAt(X, Y))
	{
		return false;
	}
	if (SlimeX == X && SlimeY == Y)
	{
		return false;
	}
	const TCHAR Cell = Board->GetCell(X, Y);
	return Cell == TEXT('.') || Cell == TEXT('P') || Cell == TEXT('d');
}

ASlimeTrackPiece* ASlimeTrackDirector::PieceAt(int32 X, int32 Y) const
{
	for (ASlimeTrackPiece* Piece : AllPieces)
	{
		if (Piece && Piece->BoardIndex == BoardIdx && Piece->GridX == X && Piece->GridY == Y)
		{
			return Piece;
		}
	}
	return nullptr;
}

ASlimeTrackBoard* ASlimeTrackDirector::CurrentBoard() const
{
	return Boards.IsValidIndex(BoardIdx) ? Boards[BoardIdx].Get() : nullptr;
}

bool ASlimeTrackDirector::Emits(int32 X, int32 Y, int32 Dir) const
{
	if (const ASlimeTrackPiece* Piece = PieceAt(X, Y))
	{
		int32 PortA = 0;
		int32 PortB = 0;
		Ports(Piece->Kind, Piece->QuarterTurns, PortA, PortB);
		return Dir == PortA || Dir == PortB;
	}
	const ASlimeTrackBoard* Board = CurrentBoard();
	if (!Board)
	{
		return false;
	}
	const TCHAR Cell = Board->GetCell(X, Y);
	if (Cell == TEXT('A')) return Dir == Board->StartFacing;
	if (Cell == TEXT('B')) return Dir == Board->EndFacing;
	return false;
}

bool ASlimeTrackDirector::Linked(int32 X, int32 Y, int32 Dir) const
{
	const ASlimeTrackBoard* Board = CurrentBoard();
	if (!Board)
	{
		return false;
	}
	const int32 NX = X + DX[Dir];
	const int32 NY = Y + DY[Dir];
	if (!Board->InBounds(NX, NY))
	{
		return false;
	}
	return Emits(X, Y, Dir) && Emits(NX, NY, (Dir + 2) & 3);
}

TArray<FIntPoint> ASlimeTrackDirector::BuildPath() const
{
	TArray<FIntPoint> Path;
	const ASlimeTrackBoard* Board = CurrentBoard();
	if (!Board)
	{
		return Path;
	}
	int32 StartX = 0;
	int32 StartY = 0;
	int32 EndX = 0;
	int32 EndY = 0;
	if (!Board->FindCell(TEXT('A'), StartX, StartY) || !Board->FindCell(TEXT('B'), EndX, EndY))
	{
		return Path;
	}
	TMap<FIntPoint, FIntPoint> Parent;
	TArray<FIntPoint> Queue;
	const FIntPoint Start(StartX, StartY);
	Queue.Add(Start);
	Parent.Add(Start, Start);
	bool bFound = false;
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const FIntPoint At = Queue[Head];
		if (At.X == EndX && At.Y == EndY)
		{
			bFound = true;
			break;
		}
		for (int32 Dir = 0; Dir < 4; ++Dir)
		{
			const FIntPoint Next(At.X + DX[Dir], At.Y + DY[Dir]);
			if (Parent.Contains(Next) || !Linked(At.X, At.Y, Dir))
			{
				continue;
			}
			Parent.Add(Next, At);
			Queue.Add(Next);
		}
	}
	if (!bFound)
	{
		return Path;
	}
	for (FIntPoint At(EndX, EndY); ; At = Parent[At])
	{
		Path.Add(At);
		if (At == Start)
		{
			break;
		}
	}
	Algo::Reverse(Path);
	return Path;
}

bool ASlimeTrackDirector::AnyBusy() const
{
	if (bHopping)
	{
		return true;
	}
	for (const ASlimeTrackPiece* Piece : AllPieces)
	{
		if (Piece && Piece->BoardIndex == BoardIdx && Piece->IsBusy())
		{
			return true;
		}
	}
	return false;
}

int32 ASlimeTrackDirector::StepLimit() const
{
	const ASlimeTrackBoard* Board = CurrentBoard();
	return Board ? Board->StepLimitFor(GetWeekIndex()) : 0;
}

void ASlimeTrackDirector::BoardCamera(const ASlimeTrackBoard* Board, FVector& Location, FRotator& Rotation) const
{
	if (Board->bUseCamera)
	{
		Location = Board->CameraLocation;
		Rotation = Board->CameraRotation;
		// Fit the entire board at the actual viewport aspect, including widescreen.
		int32 ViewX = 1920, ViewY = 1080;
		if (const APlayerController* PC = GetPlayerController()) PC->GetViewportSize(ViewX, ViewY);
		const float Aspect = ViewY > 0 && ViewX > 0 ? float(ViewX) / ViewY : 16.f / 9.f;
		const float TanH = FMath::Tan(FMath::DegreesToRadians(55.f * 0.5f)) * 0.85f;
		const float TanV = TanH / Aspect;
		const FRotationMatrix Basis(Rotation);
		const FVector Forward = Basis.GetUnitAxis(EAxis::X);
		const FVector Right = Basis.GetUnitAxis(EAxis::Y);
		const FVector Up = Basis.GetUnitAxis(EAxis::Z);
		float Retreat = 0.f;
		for (int32 X : {0, Board->Width})
			for (int32 Y : {0, Board->Height})
			{
				const FVector Delta = Board->GetActorLocation() + FVector(X * Board->CellSize, Y * Board->CellSize, 80.f) - Location;
				const float Depth = FVector::DotProduct(Delta, Forward);
				Retreat = FMath::Max(Retreat, FMath::Abs(FVector::DotProduct(Delta, Right)) / TanH - Depth);
				Retreat = FMath::Max(Retreat, FMath::Abs(FVector::DotProduct(Delta, Up)) / TanV - Depth);
			}
		Location -= Forward * Retreat;
		return;
	}
	const float SpanX = Board->Width * Board->CellSize;
	const float SpanY = Board->Height * Board->CellSize;
	const FVector Origin = Board->GetActorLocation();
	const float Span = FMath::Max(SpanX, SpanY);
	Location = Origin + FVector(SpanX * 0.5f, SpanY * 0.72f, Span * 0.95f);
	Rotation = FRotator(-72.f, -90.f, 0.f);
}

void ASlimeTrackDirector::BlendCamera(const FVector& Location, const FRotator& Rotation, float Duration)
{
	if (!Cam)
	{
		return;
	}
	CamFrom = Cam->GetActorLocation();
	CamRotFrom = Cam->GetActorRotation();
	CamTo = Location;
	CamRotTo = Rotation;
	CamAlpha = 0.f;
	CamDur = FMath::Max(Duration, 0.01f);
}

void ASlimeTrackDirector::ParkTrain()
{
	if (!Train)
	{
		return;
	}
	ASlimeTrackBoard* Board = CurrentBoard();
	if (!Board)
	{
		return;
	}
	int32 X = 0;
	int32 Y = 0;
	if (!Board->FindCell(TEXT('A'), X, Y))
	{
		return;
	}
	const int32 Face = Board->StartFacing & 3;
	FVector Pos = Board->CellCenter(X, Y) - DirVector(Face) * (Board->CellSize * 0.45f);
	Pos.Z += 8.f;
	Train->SetActorLocation(Pos);
	Train->SetActorRotation(DirVector(Face).Rotation());
}

void ASlimeTrackDirector::StartRide()
{
	Phase = EPhase::Ride;
	const TArray<FIntPoint> Path = BuildPath();
	RidePoints.Reset();
	if (const ASlimeTrackBoard* Board = CurrentBoard())
	{
		for (int32 Index = 0; Index < Path.Num(); ++Index)
		{
			const FIntPoint Cell = Path[Index];
			FVector Pos = Board->CellCenter(Cell.X, Cell.Y);
			Pos.Z += 8.f;
			const ASlimeTrackPiece* Piece = PieceAt(Cell.X, Cell.Y);
			if (Piece && Piece->Kind == ESlimeTrackKind::Curve && Index > 0 && Index + 1 < Path.Num())
			{
				const FIntPoint Before = Path[Index - 1], After = Path[Index + 1];
				const FVector In = Pos + FVector(Before.X - Cell.X, Before.Y - Cell.Y, 0.f) * Board->CellSize * 0.5f;
				const FVector Out = Pos + FVector(After.X - Cell.X, After.Y - Cell.Y, 0.f) * Board->CellSize * 0.5f;
				const FVector Center = In + Out - Pos;
				const float StartAngle = (In - Center).Rotation().Yaw;
				const float Turn = FMath::FindDeltaAngleDegrees(StartAngle, (Out - Center).Rotation().Yaw);
				for (int32 Segment = 0; Segment <= 12; ++Segment)
				{
					const float Angle = FMath::DegreesToRadians(StartAngle + Turn * Segment / 12.f);
					RidePoints.Add(Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Board->CellSize * 0.5f);
				}
			}
			else RidePoints.Add(Pos);
		}
	}
	// Adjacent curves share their boundary point; keep only one copy for arc-length motion.
	for (int32 Index = RidePoints.Num() - 1; Index > 0; --Index)
	{
		if (RidePoints[Index].Equals(RidePoints[Index - 1], 0.1f)) RidePoints.RemoveAt(Index);
	}
	RideDistances.Reset();
	float Distance = 0.f;
	for (int32 Index = 0; Index < RidePoints.Num(); ++Index)
	{
		if (Index > 0) Distance += FVector::Distance(RidePoints[Index - 1], RidePoints[Index]);
		RideDistances.Add(Distance);
	}
	RideT = 0.f;
	RideDur = FMath::Max(1.6f, Distance / 300.f);
	UE_LOG(LogSlimeFable, Log, TEXT("Track: board %d connected in %d steps"), BoardIdx, Steps);
	RefreshHints();
}

void ASlimeTrackDirector::StartFinale()
{
	Phase = EPhase::Finale;
	FinaleT = 0.f;
	bFinaleBanner = false;
	FinaleForward = FVector(1.f, 0.f, 0.f);
	if (RidePoints.Num() >= 2)
	{
		FinaleForward = (RidePoints.Last() - RidePoints[RidePoints.Num() - 2]).GetSafeNormal();
	}
	else if (const ASlimeTrackBoard* Board = CurrentBoard())
	{
		FinaleForward = DirVector((Board->EndFacing + 2) & 3);
	}
	FinaleOrigin = Train ? Train->GetActorLocation() : (RidePoints.Num() ? RidePoints.Last() : FVector::ZeroVector);
	RefreshHints();
}

void ASlimeTrackDirector::DimLamps()
{
	for (int32 Index = 0; Index < Lamps.Num(); ++Index)
	{
		if (Lamps[Index] && Lamps[Index]->GetLightComponent())
		{
			Lamps[Index]->GetLightComponent()->SetIntensity(LampBase[Index] * 0.2f);
		}
	}
}

void ASlimeTrackDirector::FlickerLamps()
{
	const FVector Eye = Cam ? Cam->GetActorLocation() : FinaleOrigin;
	for (int32 Index = 0; Index < Lamps.Num(); ++Index)
	{
		APointLight* Lamp = Lamps[Index];
		if (!Lamp || !Lamp->GetLightComponent())
		{
			continue;
		}
		const float Dist = FVector::Dist(Lamp->GetActorLocation(), Eye);
		const float Pass = 1.f - FMath::Clamp(FMath::Abs(Dist - 350.f) / 700.f, 0.f, 1.f);
		const float Blink = 0.55f + 0.45f * FMath::Abs(FMath::Sin(FinaleT * 9.f + Index * 1.7f));
		Lamp->GetLightComponent()->SetIntensity(LampBase[Index] * (0.08f + Pass * Blink));
	}
}

void ASlimeTrackDirector::RefreshHints()
{
	const bool bShow = Phase == EPhase::Play && GetWeekIndex() == 1;
	const ASlimeTrackBoard* Board = CurrentBoard();
	for (ASlimeTrackPiece* Piece : AllPieces)
	{
		if (!Piece)
		{
			continue;
		}
		Piece->RefreshHint(Board, bShow && Piece->BoardIndex == BoardIdx);
		Piece->TargetMarker->SetHiddenInGame(Phase != EPhase::Play || Piece != RotationTarget());
	}
}

FVector ASlimeTrackDirector::DirVector(int32 Dir) const
{
	const int32 Index = Dir & 3;
	return FVector(static_cast<float>(DX[Index]), static_cast<float>(DY[Index]), 0.f);
}
