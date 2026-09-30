#include "SlimeMuseumNpc.h"
#include "SlimeNpcRetargetAnim.h"
#include "Retargeter/IKRetargeter.h"
#include "AIController.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/BlendSpace.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Farm/SlimeFarmPlot.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Hub/HomeBuild/SlimeHomeBuildSubsystem.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Navigation/PathFollowingComponent.h"
#include "SlimeFable.h"
#include "Kismet/GameplayStatics.h"

ASlimeMuseumNpc::ASlimeMuseumNpc()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.2f;
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::Disabled;
	bUseControllerRotationYaw = false;
	SetCanBeDamaged(false);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetCapsuleComponent()->SetGenerateOverlapEvents(false);
	GetCapsuleComponent()->SetCanEverAffectNavigation(false);
	GetMesh()->SetCanEverAffectNavigation(false);
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0, 300, 0);
	GetCharacterMovement()->MaxStepHeight = 60.f;
	GetCharacterMovement()->bEnablePhysicsInteraction = false;
}

bool ASlimeMuseumNpc::Configure(const FSlimeNpcSpecies& InSpecies, bool bPreview)
{
	Definition = InSpecies;
	bIsPreview = bPreview;
	if (Definition.Layers.IsEmpty() || Definition.SpeciesId.IsNone()) return false;
	GetCapsuleComponent()->SetCapsuleSize(Definition.Radius, FMath::Max(Definition.HalfHeight, Definition.Radius));
	GetCharacterMovement()->MaxWalkSpeed = FMath::Max(Definition.WalkSpeed, 10.f);
	GetCharacterMovement()->MaxAcceleration = FMath::Max(Definition.WalkSpeed * 2.f, 200.f);
	for (int32 I = 0; I < Definition.Layers.Num(); ++I)
	{
		const auto& Layer = Definition.Layers[I];
		USkeletalMesh* BodyMesh = Layer.Mesh.LoadSynchronous();
		if (!BodyMesh || Layer.ParentIndex >= I || Layer.LeaderIndex >= I) return false;
		USkeletalMeshComponent* Part = I == 0 ? GetMesh() : NewObject<USkeletalMeshComponent>(this);
		if (I > 0)
		{
			Part->SetupAttachment(Visuals.IsValidIndex(Layer.ParentIndex) ? static_cast<USceneComponent*>(Visuals[Layer.ParentIndex].Get()) : GetCapsuleComponent());
			AddInstanceComponent(Part);
			Part->RegisterComponent();
		}
		Part->SetRelativeTransform(Layer.Transform);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		Part->ComponentTags = Layer.Tags;
		Part->SetSkeletalMeshAsset(BodyMesh);
		for (int32 M = 0; M < Layer.Materials.Num(); ++M)
			if (auto* Material = Layer.Materials[M].LoadSynchronous()) Part->SetMaterial(M, Material);
		Part->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Part->SetVisibility(Layer.bVisible, false);
		if (Visuals.IsValidIndex(Layer.LeaderIndex)) Part->SetLeaderPoseComponent(Visuals[Layer.LeaderIndex]);
		else if (!Layer.Retargeter.IsNull())
		{
			auto* Asset = Layer.Retargeter.LoadSynchronous();
			if (!Asset) return false;
			Part->SetAnimInstanceClass(USlimeNpcRetargetAnim::StaticClass());
			auto* Anim = Cast<USlimeNpcRetargetAnim>(Part->GetAnimInstance());
			if (!Anim) return false;
			Anim->Retargeter = Asset;
		}
		else
		{
			UBlendSpace* Locomotion = Layer.Locomotion.LoadSynchronous();
			if (!Locomotion) return false;
			Part->PlayAnimation(Locomotion, true);
		}
		if (Visuals.IsValidIndex(Layer.ParentIndex)) Part->AddTickPrerequisiteComponent(Visuals[Layer.ParentIndex]);
		Visuals.Add(Part);
	}
	if (bPreview)
	{
		SetActorEnableCollision(false);
		GetCharacterMovement()->DisableMovement();
	}
	return true;
}

void ASlimeMuseumNpc::BeginPlay()
{
	Super::BeginPlay();
	Anchor = GetActorLocation();
	if (!bIsPreview)
	{
		const auto* Home = GetWorld()->GetSubsystem<USlimeHomeBuildSubsystem>();
		if (!Home || !Home->IsMuseum()) { SetActorTickEnabled(false); return; }
		SpawnDefaultController();
	}
}

void ASlimeMuseumNpc::EndPlay(const EEndPlayReason::Type Reason)
{
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
		AI->UnPossess();
		AI->Destroy();
	}
	Super::EndPlay(Reason);
}

bool ASlimeMuseumNpc::ValidateLocation(UWorld* World, const FSlimeNpcSpecies& S, const FVector& Feet, FVector& OutCenter, const AActor* Ignore, FString* OutReason)
{
	if (!World || S.Layers.IsEmpty())
	{
		if (OutReason) *OutReason = TEXT("NPC外观配置缺失，无法放置");
		return false;
	}
	FVector Stand = Feet;
	if (auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		FNavLocation Projected;
		if (Nav->ProjectPointToNavigation(Feet, Projected, FVector(40, 40, 100))
			&& FVector::DistSquared2D(Feet, Projected.Location) <= FMath::Square(40.f)
			&& FMath::Abs(Feet.Z - Projected.Location.Z) <= 25.f)
		{
			Stand = Projected.Location;
		}
	}
	const float Half = FMath::Max(S.HalfHeight, S.Radius);
	OutCenter = Stand + FVector(0, 0, Half + 3.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NpcPlacement), false, Ignore);
	if (World->OverlapBlockingTestByChannel(OutCenter, FQuat::Identity, ECC_Pawn,
		FCollisionShape::MakeCapsule(S.Radius, Half), Params))
	{
		if (OutReason) *OutReason = TEXT("这里太挤了");
		return false;
	}
	return true;
}

void ASlimeMuseumNpc::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	for (USkeletalMeshComponent* Part : Visuals)
		if (auto* Anim = Part->GetSingleNodeInstance()) Anim->SetBlendSpacePosition(FVector(GetVelocity().Size2D(), 0, 0));
	if (bIsPreview) return;
	UpdateWander(DeltaSeconds);
	UpdateCrops(DeltaSeconds);
}

void ASlimeMuseumNpc::BeginWalk()
{
	bMoving = true;
	MoveSeconds = 0.f;
	StuckSeconds = 0.f;
	WalkLimitSeconds = FMath::FRandRange(2.f, 4.f);
	WanderLastPos = GetActorLocation();
	SetActorTickInterval(0.f);
}

void ASlimeMuseumNpc::StopWander()
{
	bMoving = false;
	bDirectWalk = false;
	MoveSeconds = 0.f;
	StuckSeconds = 0.f;
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
	}
	RestSeconds = FMath::FRandRange(3.f, 6.f);
	SetActorTickInterval(0.2f);
}

bool ASlimeMuseumNpc::TryStartWander()
{
	const float Radius = FMath::Max(Definition.WanderRadius, 100.f);
	AAIController* AI = Cast<AAIController>(GetController());
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (AI && Nav)
	{
		FNavLocation Goal;
		if (Nav->GetRandomReachablePointInRadius(Anchor, Radius, Goal))
		{
			UNavigationPath* Path = Nav->FindPathToLocationSynchronously(GetWorld(), GetActorLocation(), Goal.Location, this);
			bool bInside = Path && Path->IsValid() && !Path->IsPartial();
			if (bInside)
			{
				for (const FVector& Point : Path->PathPoints)
				{
					if (FVector::DistSquared2D(Point, Anchor) > FMath::Square(Radius))
					{
						bInside = false;
						break;
					}
				}
			}
			if (bInside && AI->MoveToLocation(Goal.Location, 35.f, true, true, false, false, nullptr, false) == EPathFollowingRequestResult::RequestSuccessful)
			{
				bDirectWalk = false;
				BeginWalk();
				return true;
			}
		}
	}

	const float Hop = FMath::Min(Radius, 450.f);
	const FVector2D Offset = FMath::RandPointInCircle(Hop);
	if (Offset.SizeSquared() < FMath::Square(80.f))
	{
		return false;
	}
	FVector Dest = Anchor + FVector(Offset.X, Offset.Y, 0.f);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NpcWanderGround), false, this);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Dest + FVector(0.f, 0.f, 250.f), Dest - FVector(0.f, 0.f, 800.f), ECC_WorldStatic, Params))
	{
		Dest.Z = Hit.ImpactPoint.Z;
	}
	else
	{
		Dest.Z = GetActorLocation().Z;
	}
	DirectDest = Dest;
	bDirectWalk = true;
	BeginWalk();
	return true;
}

void ASlimeMuseumNpc::UpdateWander(float Dt)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move)
	{
		return;
	}
	const float WalkSpeed = FMath::Max(Definition.WalkSpeed, 10.f);
	Move->MaxWalkSpeed = WalkSpeed;
	Move->MaxAcceleration = FMath::Max(WalkSpeed * 2.f, 200.f);

	if (!bMoving)
	{
		RestSeconds -= Dt;
		if (RestSeconds > 0.f)
		{
			return;
		}
		if (!TryStartWander())
		{
			RestSeconds = 1.f;
		}
		return;
	}

	MoveSeconds += Dt;
	const float MovedSq = FVector::DistSquared2D(GetActorLocation(), WanderLastPos);
	if (MovedSq < FMath::Square(30.f))
	{
		StuckSeconds += Dt;
	}
	else
	{
		StuckSeconds = 0.f;
		WanderLastPos = GetActorLocation();
	}

	const bool bTimeUp = MoveSeconds >= WalkLimitSeconds;
	const bool bStuck = StuckSeconds >= 1.5f;
	if (bDirectWalk)
	{
		FVector To = DirectDest - GetActorLocation();
		To.Z = 0.f;
		if (bTimeUp || bStuck || To.SizeSquared() < FMath::Square(80.f))
		{
			StopWander();
			return;
		}
		AddMovementInput(To.GetSafeNormal(), 1.f);
		return;
	}

	const AAIController* AI = Cast<AAIController>(GetController());
	const bool bArrived = !AI || (MoveSeconds > 0.25f && AI->GetMoveStatus() == EPathFollowingStatus::Idle);
	if (bTimeUp || bStuck || bArrived)
	{
		StopWander();
	}
}

void ASlimeMuseumNpc::UpdateCrops(float Dt)
{
	if (!Definition.bDamagesCrops || UGameplayStatics::IsGamePaused(GetWorld())) return;
	CooldownSeconds = FMath::Max(0.f, CooldownSeconds - Dt);
	if (CooldownSeconds > 0.f) { CropTarget.Reset(); DwellSeconds = 0.f; return; }
	auto Reachable = [this](ASlimeFarmPlot* Plot)
	{
		FVector Edge;
		if (!Plot || !Plot->GetNpcDamagePoint(GetActorLocation(), Definition.CropDistance, Edge)) return false;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(NpcCropSight), false, this);
		Params.AddIgnoredActor(Plot);
		FHitResult Hit;
		const FVector Start(GetActorLocation().X, GetActorLocation().Y, Edge.Z);
		return !GetWorld()->LineTraceSingleByChannel(Hit, Start, Edge, ECC_Visibility, Params);
	};
	if (!Reachable(CropTarget.Get()))
	{
		CropTarget.Reset(); DwellSeconds = 0.f;
		for (TActorIterator<ASlimeFarmPlot> It(GetWorld()); It; ++It)
			if (Reachable(*It)) { CropTarget = *It; break; }
	}
	if (!CropTarget.IsValid()) return;
	DwellSeconds += Dt;
	if (DwellSeconds >= Definition.CropDwellSeconds)
	{
		if (CropTarget->DestroyCropByNpc()) CooldownSeconds = Definition.CropCooldownSeconds;
		CropTarget.Reset(); DwellSeconds = 0.f;
	}
}

void ASlimeMuseumNpc::SetHighlighted(bool bHighlighted)
{
	for (USkeletalMeshComponent* Part : Visuals) Part->SetRenderCustomDepth(bHighlighted);
}
