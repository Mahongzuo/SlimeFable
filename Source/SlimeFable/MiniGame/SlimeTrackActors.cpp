#include "MiniGame/SlimeTrackActors.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

ASlimeTrackBoard::ASlimeTrackBoard()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
}

void ASlimeTrackBoard::EnsureParsed()
{
	Height = Rows.Num();
	Width = 0;
	for (const FString& Row : Rows)
	{
		Width = FMath::Max(Width, Row.Len());
	}
	bParsed = true;
}

bool ASlimeTrackBoard::InBounds(int32 X, int32 Y) const
{
	return X >= 0 && Y >= 0 && X < Width && Y < Height;
}

TCHAR ASlimeTrackBoard::GetCell(int32 X, int32 Y) const
{
	if (Y < 0 || Y >= Rows.Num() || X < 0)
	{
		return TEXT('#');
	}
	const FString& Row = Rows[Y];
	if (X >= Row.Len())
	{
		return TEXT('#');
	}
	return Row[X];
}

bool ASlimeTrackBoard::FindCell(TCHAR Mark, int32& OutX, int32& OutY) const
{
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			if (GetCell(X, Y) == Mark)
			{
				OutX = X;
				OutY = Y;
				return true;
			}
		}
	}
	return false;
}

FVector ASlimeTrackBoard::CellCenter(int32 X, int32 Y) const
{
	const FVector Origin = GetActorLocation();
	return Origin + FVector((X + 0.5f) * CellSize, (Y + 0.5f) * CellSize, 0.f);
}

FVector ASlimeTrackBoard::Stand(int32 X, int32 Y) const
{
	FVector Center = CellCenter(X, Y);
	Center.Z += StandZ;
	return Center;
}

int32 ASlimeTrackBoard::StepLimitFor(int32 Week) const
{
	if (Week <= 1) return StepLimitWeek1;
	if (Week == 2) return StepLimitWeek2;
	return StepLimitWeek3;
}

ASlimeTrackPiece::ASlimeTrackPiece()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(true);
	TargetMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetMarker"));
	TargetMarker->SetupAttachment(RootComponent);
	TargetMarker->SetMobility(EComponentMobility::Movable);
	TargetMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TargetMarker->SetCastShadow(false);
	TargetMarker->SetHiddenInGame(true);

	HintArrow = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HintArrow"));
	HintArrow->SetupAttachment(RootComponent);
	HintArrow->SetMobility(EComponentMobility::Movable);
	HintArrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HintArrow->SetCastShadow(false);
	HintArrow->SetHiddenInGame(true);
	HintArrow->SetRelativeLocation(FVector(70.f, 0.f, 80.f));
	HintArrow->SetRelativeScale3D(FVector(0.7f, 0.12f, 0.08f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		HintArrow->SetStaticMesh(Cube.Object);
		HintArrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ASlimeTrackPiece::BeginPlay()
{
	Super::BeginPlay();
	HomeX = GridX;
	HomeY = GridY;
	HomeQuarter = QuarterTurns & 3;
	QuarterTurns = HomeQuarter;
	SetActorRotation(FRotator(0.f, HomeQuarter * 90.f, 0.f));
	SlideAlpha = 1.f;
	bYawBusy = false;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HintArrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (UMaterialInterface* Mat = HintArrow->GetMaterial(0))
	{
		if (UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Mat, this))
		{
			Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.95f, 0.75f, 0.2f, 1.f));
			HintArrow->SetMaterial(0, Mid);
		}
	}
}

void ASlimeTrackPiece::MoveTo(int32 X, int32 Y, const FVector& World, bool bSnap)
{
	GridX = X;
	GridY = Y;
	if (bSnap)
	{
		SetActorLocation(World);
		SlideFrom = SlideTo = World;
		SlideAlpha = 1.f;
		return;
	}
	SlideFrom = GetActorLocation();
	SlideTo = World;
	SlideAlpha = 0.f;
}

void ASlimeTrackPiece::SetQuarter(int32 Quarter, bool bSnap)
{
	QuarterTurns = Quarter & 3;
	const float Target = QuarterTurns * 90.f;
	if (bSnap)
	{
		SetActorRotation(FRotator(0.f, Target, 0.f));
		bYawBusy = false;
		return;
	}
	YawFrom = GetActorRotation().Yaw;
	YawTo = YawFrom + FMath::FindDeltaAngleDegrees(YawFrom, Target);
	YawAlpha = 0.f;
	bYawBusy = true;
}

void ASlimeTrackPiece::ResetHome(const FVector& World)
{
	GridX = HomeX;
	GridY = HomeY;
	QuarterTurns = HomeQuarter;
	SetActorLocation(World);
	SetActorRotation(FRotator(0.f, HomeQuarter * 90.f, 0.f));
	SlideFrom = SlideTo = World;
	SlideAlpha = 1.f;
	bYawBusy = false;
}

void ASlimeTrackPiece::Advance(float DeltaSeconds)
{
	if (SlideAlpha < 1.f)
	{
		SlideAlpha = FMath::Min(1.f, SlideAlpha + DeltaSeconds / 0.16f);
		const float Alpha = FMath::InterpEaseOut(0.f, 1.f, SlideAlpha, 2.f);
		SetActorLocation(FMath::Lerp(SlideFrom, SlideTo, Alpha));
	}
	if (bYawBusy)
	{
		YawAlpha = FMath::Min(1.f, YawAlpha + DeltaSeconds / 0.16f);
		SetActorRotation(FRotator(0.f, FMath::Lerp(YawFrom, YawTo, YawAlpha), 0.f));
		if (YawAlpha >= 1.f)
		{
			bYawBusy = false;
		}
	}
}

bool ASlimeTrackPiece::IsBusy() const
{
	return SlideAlpha < 1.f || bYawBusy;
}

void ASlimeTrackPiece::RefreshHint(const ASlimeTrackBoard* Board, bool bShow)
{
	const bool bVisible = bShow && bHasHint && Board;
	HintArrow->SetHiddenInGame(!bVisible);
	if (!bVisible)
	{
		return;
	}
	const FVector Here = GetActorLocation();
	const FVector There = Board->CellCenter(HintX, HintY);
	FVector Dir = There - Here;
	Dir.Z = 0.f;
	const float Yaw = Dir.SizeSquared() > 100.f ? Dir.Rotation().Yaw : HintQuarter * 90.f;
	const FVector Offset = FRotator(0.f, Yaw, 0.f).Vector() * 60.f;
	HintArrow->SetWorldLocation(Here + FVector(0.f, 0.f, 70.f) + Offset);
	HintArrow->SetWorldRotation(FRotator(0.f, Yaw, 0.f));
}

ASlimeTrackTrain::ASlimeTrackTrain()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASlimeTrackTrain::BeginPlay()
{
	Super::BeginPlay();
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

ASlimeTrackBlob::ASlimeTrackBlob()
{
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.42f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Mesh->SetStaticMesh(Sphere.Object);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ASlimeTrackBlob::BeginPlay()
{
	Super::BeginPlay();
	// Spawn transforms can replace the root mesh's constructor scale.
	SetActorScale3D(FVector(0.42f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (UMaterialInterface* Mat = Mesh->GetMaterial(0))
	{
		if (UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Mat, this))
		{
			Mid->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.45f, 0.75f, 0.38f, 1.f));
			Mesh->SetMaterial(0, Mid);
		}
	}
}
