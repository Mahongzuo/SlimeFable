// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "SlimeCharacter.h"
#include "SlimeCharacterMovementComponent.h"

namespace
{
	class FSlimeMovementTestWorld
	{
	public:
		FSlimeMovementTestWorld()
		{
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, GetTransientPackage());
			World->AddToRoot();
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}

		~FSlimeMovementTestWorld()
		{
			if (!World)
			{
				return;
			}
			if (World->AreActorsInitialized())
			{
				for (AActor* Actor : FActorRange(World))
				{
					if (Actor)
					{
						Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
					}
				}
			}
			GEngine->ShutdownWorldNetDriver(World);
			World->DestroyWorld(true);
			World->SetPhysicsScene(nullptr);
			GEngine->DestroyWorldContext(World);
			World->RemoveFromRoot();
		}

		UWorld* Get() const { return World; }

	private:
		UWorld* World = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeMovementComponentContractTest,
	"SlimeFable.Slime.Movement.CustomComponentExposesSurfaceModes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeMovementComponentContractTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Slime movement derives from CharacterMovementComponent"),
		TIsDerivedFrom<USlimeCharacterMovementComponent, UCharacterMovementComponent>::Value);
	TestEqual(TEXT("Climbing has a stable custom mode value"),
		static_cast<uint8>(ESlimeCustomMovementMode::Climbing), uint8(0));
	TestEqual(TEXT("Mantling has a stable custom mode value"),
		static_cast<uint8>(ESlimeCustomMovementMode::Mantling), uint8(1));

	FSlimeSurfaceContact Contact;
	Contact.Point = FVector(10.0, 20.0, 30.0);
	Contact.Normal = -FVector::UpVector;
	TestEqual(TEXT("A contact stores its point"), Contact.Point, FVector(10.0, 20.0, 30.0));
	TestEqual(TEXT("A contact stores a downward ceiling normal"), Contact.Normal, -FVector::UpVector);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeCapsuleSupportDistanceTest,
	"SlimeFable.Slime.Movement.CapsuleSupportDistanceHandlesWallsAndCeilings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeCapsuleSupportDistanceTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("A vertical wall uses capsule radius"),
		SlimeMovementPolicies::CapsuleSupportDistance(32.f, 48.f, FVector::ForwardVector), 32.f);
	TestEqual(TEXT("A ceiling uses capsule half height"),
		SlimeMovementPolicies::CapsuleSupportDistance(32.f, 48.f, -FVector::UpVector), 48.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeSurfaceFrameTransportTest,
	"SlimeFable.Slime.Movement.SurfaceFrameTransportsWallUpOntoEave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeSurfaceFrameTransportTest::RunTest(const FString& Parameters)
{
	const FVector Transported = SlimeMovementPolicies::TransportTangent(
		FVector::UpVector,
		FVector::ForwardVector,
		-FVector::UpVector);

	TestTrue(TEXT("Forward input remains tangent to the ceiling"),
		FMath::Abs(Transported | FVector::UpVector) < KINDA_SMALL_NUMBER);
	TestTrue(TEXT("Climbing up continues outward under an eave"),
		(Transported | FVector::ForwardVector) > 0.99f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeCeilingGrabGateTest,
	"SlimeFable.Slime.Movement.CeilingGrabRequiresUpwardImpact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeCeilingGrabGateTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Fast upward contact with a downward normal can grab"),
		SlimeMovementPolicies::CanGrabCeiling(120.f, -FVector::UpVector, 30.f));
	TestFalse(TEXT("Falling contact cannot grab"),
		SlimeMovementPolicies::CanGrabCeiling(-20.f, -FVector::UpVector, 30.f));
	TestFalse(TEXT("A side wall is not a direct ceiling grab"),
		SlimeMovementPolicies::CanGrabCeiling(120.f, FVector::ForwardVector, 30.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeClingJumpDirectionTest,
	"SlimeFable.Slime.Movement.CeilingJumpHasNonZeroDetachVelocity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeClingJumpDirectionTest::RunTest(const FString& Parameters)
{
	const FVector WallJump = SlimeMovementPolicies::ClingJumpDirection(FVector::ForwardVector);
	TestTrue(TEXT("A wall jump retains its upward and outward direction"),
		WallJump.Z > 0.f && WallJump.X > 0.f);
	const FVector CeilingJump = SlimeMovementPolicies::ClingJumpDirection(-FVector::UpVector);
	TestTrue(TEXT("A ceiling jump detaches along the downward surface normal"),
		CeilingJump.Equals(-FVector::UpVector));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeMovementBudgetTest,
	"SlimeFable.Slime.Movement.DisplacementBudgetRejectsTeleport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeMovementBudgetTest::RunTest(const FString& Parameters)
{
	const float Budget = SlimeMovementPolicies::DisplacementBudget(110.f, 1.f / 60.f, 32.f);
	TestTrue(TEXT("Normal climb displacement fits"),
		SlimeMovementPolicies::IsWithinDisplacementBudget(1.5f, Budget));
	TestFalse(TEXT("A one-frame jump to the roof is rejected"),
		SlimeMovementPolicies::IsWithinDisplacementBudget(120.f, Budget));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeSurfaceTransitionPathTest,
	"SlimeFable.Slime.Movement.SurfaceTransitionPathIsContinuous",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeSurfaceTransitionPathTest::RunTest(const FString& Parameters)
{
	const FVector Start(32.0, 0.0, 80.0);
	const FVector Target(24.0, 0.0, 48.0);
	TArray<FVector> Points;
	SlimeMovementPolicies::BuildSurfaceTransitionPath(
		Start, Target, FVector::ForwardVector, -FVector::UpVector, 4.f, 10.f, Points);

	TestTrue(TEXT("A wall-to-eave transition is split into multiple sweeps"), Points.Num() > 2);
	TestEqual(TEXT("The transition ends at the requested legal capsule center"), Points.Last(), Target);
	FVector Previous = Start;
	for (const FVector& Point : Points)
	{
		TestTrue(TEXT("Every transition sweep is at most four centimetres"),
			FVector::Distance(Previous, Point) <= 4.f + KINDA_SMALL_NUMBER);
		Previous = Point;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeBlockedSurfaceTransitionTest,
	"SlimeFable.Slime.Movement.BlockedSurfaceTransitionDoesNotMoveCapsule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeBlockedSurfaceTransitionTest::RunTest(const FString& Parameters)
{
	FSlimeMovementTestWorld TestWorld;
	UWorld* World = TestWorld.Get();
	ASlimeCharacter* Slime = World->SpawnActor<ASlimeCharacter>(FVector(0.0, 0.0, 150.0), FRotator::ZeroRotator);
	USlimeCharacterMovementComponent* Movement = Slime
		? Cast<USlimeCharacterMovementComponent>(Slime->GetCharacterMovement())
		: nullptr;
	if (!TestNotNull(TEXT("The test slime uses the custom movement component"), Movement))
	{
		return false;
	}

	AActor* Obstacle = World->SpawnActor<AActor>();
	UBoxComponent* Box = NewObject<UBoxComponent>(Obstacle, TEXT("TransitionBlocker"));
	Obstacle->SetRootComponent(Box);
	Box->SetBoxExtent(FVector(8.0, 100.0, 200.0));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->RegisterComponent();
	Box->SetWorldLocation(FVector(50.0, 0.0, 150.0));

	const FVector StartLocation = Slime->GetActorLocation();
	FSlimeSurfaceContact WallContact;
	WallContact.Normal = FVector::ForwardVector;
	WallContact.Point = StartLocation - WallContact.Normal * 50.f;
	TestTrue(TEXT("The slime can enter climbing for the collision-path test"),
		Movement->RequestClimbStart(WallContact, FVector::UpVector));

	FSlimeSurfaceContact EaveContact;
	EaveContact.Normal = -FVector::UpVector;
	EaveContact.Point = FVector(100.0, 0.0,
		StartLocation.Z + Slime->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + EaveContact.Skin);
	TestFalse(TEXT("A blocking obstacle rejects the entire wall-to-eave path"),
		Movement->RequestSurfaceTransition(EaveContact, 110.f));
	TestEqual(TEXT("Rejected transition leaves the gameplay capsule at its legal start"),
		Slime->GetActorLocation(), StartLocation);

	ASlimeCharacter* DynamicSlime = World->SpawnActor<ASlimeCharacter>(
		FVector(0.0, 400.0, 150.0), FRotator::ZeroRotator);
	USlimeCharacterMovementComponent* DynamicMovement = DynamicSlime
		? Cast<USlimeCharacterMovementComponent>(DynamicSlime->GetCharacterMovement())
		: nullptr;
	if (!TestNotNull(TEXT("The dynamic obstruction slime has custom movement"), DynamicMovement))
	{
		return false;
	}
	DynamicMovement->bRunPhysicsWithNoController = true;
	const FVector DynamicStart = DynamicSlime->GetActorLocation();
	WallContact.Point = DynamicStart - WallContact.Normal * 50.f;
	TestTrue(TEXT("Dynamic obstruction test enters climbing"),
		DynamicMovement->RequestClimbStart(WallContact, FVector::UpVector));
	EaveContact.Point = FVector(100.0, 400.0,
		DynamicStart.Z + DynamicSlime->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + EaveContact.Skin);
	TestTrue(TEXT("The clear path is accepted before a dynamic obstacle appears"),
		DynamicMovement->RequestSurfaceTransition(EaveContact, 110.f));

	AActor* DynamicObstacle = World->SpawnActor<AActor>();
	UBoxComponent* DynamicBox = NewObject<UBoxComponent>(DynamicObstacle, TEXT("DynamicTransitionBlocker"));
	DynamicObstacle->SetRootComponent(DynamicBox);
	DynamicBox->SetBoxExtent(FVector(8.0, 100.0, 200.0));
	DynamicBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	DynamicBox->SetCollisionResponseToAllChannels(ECR_Block);
	DynamicBox->RegisterComponent();
	DynamicBox->SetWorldLocation(FVector(52.0, 400.0, 150.0));
	DynamicMovement->TickComponent(1.f / 30.f, LEVELTICK_All, nullptr);

	TestTrue(TEXT("A dynamic obstruction reports transition failure"),
		DynamicMovement->ConsumeSurfaceTransitionFailure());
	TestEqual(TEXT("A dynamic obstruction restores the source surface normal"),
		DynamicMovement->GetSurfaceContact().Normal, FVector::ForwardVector);
	TestEqual(TEXT("The obstructed movement sub-step rolls back its capsule transform"),
		DynamicSlime->GetActorLocation(), DynamicStart);
	return true;
}

#endif
