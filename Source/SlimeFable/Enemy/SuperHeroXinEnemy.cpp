// Copyright Epic Games, Inc. All Rights Reserved.

#include "SuperHeroXinEnemy.h"

#include "AIController.h"
#include "AnimNode_SlimeArmIK.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BoneControllers/AnimNode_ModifyBone.h"
#include "BrainComponent.h"
#include "Camera/CameraComponent.h"
#include "Combat/SlimeDevourComponent.h"
#include "Combat/SlimeDodgeComponent.h"
#include "Combat/SlimeHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "EnemyCombatComponent.h"
#include "SlimeCombatTypes.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Blueprint/UserWidget.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "SlimeFable.h"
#include "Slime/SlimeMorphComponent.h"
#include "SlimeLockOnComponent.h"
#include "SlimeStatusComponent.h"
#include "SlimeWorldHealthBar.h"
#include "SuperHeroXinAIController.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

ASuperHeroXinEnemy::ASuperHeroXinEnemy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = ASuperHeroXinAIController::StaticClass();
	bUseControllerRotationYaw = false;

	DisplayName = NSLOCTEXT("SuperHeroXin", "DisplayName", "心月狐（飞行）");

	VisualBodyMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(
		TEXT("/Game/_Slime/Models/Xin2/Xin_Form2_UE5.Xin_Form2_UE5")));
	VisualAnimClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(
		TEXT("/Game/_Slime/Enemies/Lyra/Visual/ABP_XinForm2_Retarget.ABP_XinForm2_Retarget_C")));
	VisualOverlayMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
		TEXT("/Game/Models/Phoebe/Materials/MI_PhoebeOutline_Overlay.MI_PhoebeOutline_Overlay")));
	FlightMapping = TSoftObjectPtr<UInputMappingContext>(FSoftObjectPath(
		TEXT("/Game/SuperheroFlight/Input/IMC_SuperheroFlight.IMC_SuperheroFlight")));
	PunchMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(
		TEXT("/Game/Characters/Heroes/Mannequin/Animations/Actions/AM_MM_Rifle_Melee.AM_MM_Rifle_Melee")));
	SourceFlightAnimClass = TSoftClassPtr<UAnimInstance>(FSoftObjectPath(
		TEXT("/Game/SuperheroFlight/Characters/Mannequins/Animations/ABP_Player_UE5.ABP_Player_UE5_C")));

	JumpMaxCount = 2;

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	if (USkeletalMeshComponent* Source = GetMesh())
	{
		Source->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Source->bEnableUpdateRateOptimizations = false;
	}

	VisualMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(GetMesh());
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisualMesh->SetSimulatePhysics(false);
	VisualMesh->SetVisibility(true, false);
	VisualMesh->SetHiddenInGame(false, false);
	VisualMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	Health = CreateDefaultSubobject<USlimeHealthComponent>(TEXT("Health"));
	Health->Team = ESlimeTeam::Enemy;
	Health->bDestroyOnDeath = false;
	Health->bRegenOnDeath = false;
	Health->MaxHP = MaxHP;
	Status = CreateDefaultSubobject<USlimeStatusComponent>(TEXT("Status"));
	Combat = CreateDefaultSubobject<UEnemyCombatComponent>(TEXT("Combat"));
	Combat->GlobalHitDelay = 0.f;
	Combat->MaxMeleeHitDistance = 220.f;

	HealthBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBar->SetupAttachment(RootComponent);
	HealthBar->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBar->SetDrawAtDesiredSize(false);
	HealthBar->SetDrawSize(FVector2D(72.f, 8.f));
	HealthBar->SetPivot(FVector2D(0.5f, 1.f));
	HealthBar->SetWidgetClass(USlimeWorldHealthBar::StaticClass());
	HealthBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = ChaseSpeed;
		Move->MaxStepHeight = 60.f;
		Move->bOrientRotationToMovement = true;
		Move->RotationRate = FRotator(0.f, 480.f, 0.f);
	}
}

void ASuperHeroXinEnemy::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EnsureVisuals();
#if WITH_EDITOR
	if (UWorld* World = GetWorld(); World && !World->IsGameWorld() && VisualMesh && VisualMesh->GetAnimClass())
	{
		VisualMesh->SetUpdateAnimationInEditor(true);
		if (!VisualMesh->GetAnimInstance())
		{
			VisualMesh->InitAnim(true);
		}
	}
#endif
}

void ASuperHeroXinEnemy::PostInitializeComponents()
{
	// APawn::PostInitializeComponents spawns the AI controller -> PossessedBy runs before the
	// flight component's own BeginPlay. Bind its Character/CharacterMovement/AnimInstance caches
	// first so anything that touches them during possession is not reading None.
	EnsureSourceFlightAnim();
	BindFlightComponentCaches();
	Super::PostInitializeComponents();
	EnsureSourceFlightAnim();
	BindFlightComponentCaches();
}

void ASuperHeroXinEnemy::BeginPlay()
{
	Super::BeginPlay();
	SpawnOrigin = GetActorLocation();
	EnsurePunchMove();
	EnsureVisuals();
	EnsureSourceFlightAnim();
	BindFlightComponentCaches();
	if (Health)
	{
		Health->OnDied.AddDynamic(this, &ASuperHeroXinEnemy::HandleDied);
		Health->MaxHP = FMath::Max(MaxHP, 1.f);
		if (bMorphTarget)
		{
			Health->ResetHP();
		}
		else if (DebugStartHealthPercent > KINDA_SMALL_NUMBER)
		{
			Health->CurrentHP = Health->MaxHP * FMath::Clamp(DebugStartHealthPercent, 0.01f, 1.f);
		}
		Health->OnHealthChanged.Broadcast(Health->CurrentHP, Health->MaxHP);
	}
	BindWorldHealthBar();
	RefreshHealthBarAnchor();
	StripTutorialHud();
	StripFlightMappingIfNotPlayer();
	BindFlightComponentCaches();
	if (!IsPlayerMorphBody())
	{
		StopFlightIfAny();
		ApplyGroundMovement();
	}
}

void ASuperHeroXinEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SourceAttackRestoreTimer);
	}
	RestoreSourceAttackAnim();
	ApplyFlightArmLengths(true);
	RemoveFlightMapping(GetController());
	if (AActor* Master = MorphMaster.Get())
	{
		if (APlayerController* MasterPC = Cast<APlayerController>(Master->GetInstigatorController()))
		{
			RemoveFlightMapping(MasterPC);
		}
	}
	if (Health)
	{
		Health->OnDied.RemoveDynamic(this, &ASuperHeroXinEnemy::HandleDied);
	}
	Super::EndPlay(EndPlayReason);
}

void ASuperHeroXinEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	BindFlightComponentCaches();
	RefreshWorldHealthBarVisibility();

	if (IsPlayerMorphBody())
	{
		DisableVisualHandIK();
		if (WantsCombatDodge())
		{
			CancelFlightDodgeIfAny();
		}
		TickMorphCameraZoom(DeltaSeconds);
		// Same as the AI: a punch plants the feet (ground only; never yank a flying player).
		if (Combat && Combat->IsMovementLocked())
		{
			if (UCharacterMovementComponent* Move = GetCharacterMovement())
			{
				if (Move->IsMovingOnGround())
				{
					Move->StopMovementImmediately();
				}
			}
		}
		return;
	}

	if (bDeathSequence || bDevourLocked || bMorphTarget)
	{
		return;
	}

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		const bool bLocked = Combat && (Combat->IsMovementLocked() || Combat->IsAttacking());
		if (!bLocked && !AiMoveIntent.IsNearlyZero())
		{
			AddMovementInput(AiMoveIntent.GetSafeNormal2D(), 1.f);
		}
		else if (bLocked)
		{
			Move->StopMovementImmediately();
		}
	}
}

void ASuperHeroXinEnemy::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	RefreshWorldHealthBarVisibility();
	if (APlayerController* PC = Cast<APlayerController>(NewController))
	{
		EnsureSourceFlightAnim();
		BindFlightComponentCaches();
		SetFlightLogicEnabled(true);
		AddFlightMapping(PC);
		CaptureFlightArmBase();
		MorphZoomOffset = 0.f;
	}
	else
	{
		StripFlightMappingIfNotPlayer();
		StopFlightIfAny();
		ApplyGroundMovement();
		EnsureSourceFlightAnim();
		BindFlightComponentCaches();
		SetFlightLogicEnabled(true);
	}
}

void ASuperHeroXinEnemy::UnPossessed()
{
	AController* Previous = GetController();
	Super::UnPossessed();
	RemoveFlightMapping(Previous);
	ApplyFlightArmLengths(true);
	MorphZoomOffset = 0.f;
	StopFlightIfAny();
	EnsureSourceFlightAnim();
	BindFlightComponentCaches();
	RefreshWorldHealthBarVisibility();
}

void ASuperHeroXinEnemy::Jump()
{
	if (IsPlayerMorphBody())
	{
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			if (!Move->IsMovingOnGround() && Move->MovementMode != MOVE_Flying)
			{
				static const TArray<FName> StartNames = {
					FName(TEXT("StartFlight")),
					FName(TEXT("Start_Flight")),
				};
				CallFlightFunctions(StartNames);
				return;
			}
		}
	}
	Super::Jump();
}

void ASuperHeroXinEnemy::EnsurePunchMove()
{
	const FSoftObjectPath RifleMelee(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Actions/AM_MM_Rifle_Melee.AM_MM_Rifle_Melee"));
	const FString PunchPath = PunchMontage.ToSoftObjectPath().ToString();
	if (!PunchMontage.ToSoftObjectPath().IsValid() || PunchPath.Contains(TEXT("Flight/Dodge")))
	{
		PunchMontage = TSoftObjectPtr<UAnimMontage>(RifleMelee);
	}

	if (Moves.Num() > 0)
	{
		for (FEnemyMoveDef& Existing : Moves)
		{
			const FString ExistingPath = Existing.Skill.AttackMontage.ToSoftObjectPath().ToString();
			if (!Existing.Skill.AttackMontage.ToSoftObjectPath().IsValid()
				|| ExistingPath.Contains(TEXT("Flight/Dodge")))
			{
				Existing.Skill.AttackMontage = PunchMontage;
			}
		}
		return;
	}

	FEnemyMoveDef Move;
	Move.MoveId = TEXT("SuperHeroPunch");
	Move.Skill.DisplayName = NSLOCTEXT("SuperHeroXin", "Punch", "拳击");
	Move.Skill.Exec = EEnemySkillExec::Melee;
	Move.Skill.Windup = 0.12f;
	Move.Skill.HitStart = 0.18f;
	Move.Skill.HitEnd = 0.38f;
	Move.Skill.Recovery = 0.4f;
	Move.Skill.Damage = 16.f;
	Move.Skill.Knockback = 280.f;
	Move.Skill.Hit.Shape = ESlimeHitShape::Sphere;
	Move.Skill.Hit.Radius = 70.f;
	Move.Skill.Hit.Range = 120.f;
	Move.Skill.Hit.OriginForwardOffset = 45.f;
	Move.Skill.Hit.OriginZOffset = -20.f;
	Move.Skill.AttackMontage = PunchMontage;
	Move.MinRange = 0.f;
	Move.MaxRange = 210.f;
	Move.Weight = 1.f;
	Move.TelegraphTime = 0.2f;
	Move.Cooldown = 1.1f;
	Moves.Add(Move);
}

void ASuperHeroXinEnemy::EnsureVisuals()
{
	HideSourceMeshKeepPose();
	ApplyVisualMesh();
}

void ASuperHeroXinEnemy::HideSourceMeshKeepPose()
{
	if (USkeletalMeshComponent* Source = GetMesh())
	{
		Source->SetVisibility(false, false);
		Source->SetHiddenInGame(true, false);
		Source->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Source->bEnableUpdateRateOptimizations = false;
		Source->bPauseAnims = false;
	}
	EnsureSourceFlightAnim();
}

void ASuperHeroXinEnemy::ApplyVisualMesh()
{
	if (!VisualMesh)
	{
		return;
	}

	VisualMesh->ComponentTags.Reset();
	if (!VisualRetargetTag.IsNone())
	{
		VisualMesh->ComponentTags.Add(VisualRetargetTag);
	}

	if (USkeletalMesh* Body = VisualBodyMesh.LoadSynchronous())
	{
		if (VisualMesh->GetSkeletalMeshAsset() != Body)
		{
			VisualMesh->SetAnimInstanceClass(nullptr);
			VisualMesh->SetSkeletalMeshAsset(Body);
			VisualMesh->EmptyOverrideMaterials();
		}
	}

	if (UClass* AnimClass = VisualAnimClass.LoadSynchronous())
	{
		VisualMesh->SetAnimInstanceClass(AnimClass);
	}

	VisualMesh->SetVisibility(true, false);
	VisualMesh->SetHiddenInGame(false, false);
	VisualMesh->SetLeaderPoseComponent(nullptr);
	VisualMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	if (VisualMesh->GetAnimClass())
	{
		VisualMesh->InitAnim(true);
	}
	if (UMaterialInterface* Overlay = VisualOverlayMaterial.LoadSynchronous())
	{
		VisualMesh->SetOverlayMaterial(Overlay);
	}
	StopLivingVisualPhysics();
	DisableVisualHandIK();
}

void ASuperHeroXinEnemy::StopLivingVisualPhysics()
{
	if (!VisualMesh || bDeathSequence)
	{
		return;
	}
	if (UPhysicsAsset* PhysAsset = VisualMesh->GetPhysicsAsset())
	{
		for (USkeletalBodySetup* Body : PhysAsset->SkeletalBodySetups)
		{
			if (Body && Body->PhysicsType == PhysType_Simulated)
			{
				Body->PhysicsType = PhysType_Default;
			}
		}
	}
	VisualMesh->RecreatePhysicsState();
	VisualMesh->SetSimulatePhysics(false);
	VisualMesh->SetAllBodiesSimulatePhysics(false);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ASuperHeroXinEnemy::DisableVisualHandIK()
{
	auto ZeroIk = [](UAnimInstance* AnimInstance)
	{
		if (!AnimInstance)
		{
			return;
		}
		for (TFieldIterator<FStructProperty> It(AnimInstance->GetClass()); It; ++It)
		{
			if (It->Struct == FAnimNode_SlimeArmIK::StaticStruct())
			{
				if (FAnimNode_SlimeArmIK* Node = It->ContainerPtrToValuePtr<FAnimNode_SlimeArmIK>(AnimInstance))
				{
					Node->Alpha = 0.f;
				}
			}
			else if (It->Struct == FAnimNode_ModifyBone::StaticStruct())
			{
				if (FAnimNode_ModifyBone* Node = It->ContainerPtrToValuePtr<FAnimNode_ModifyBone>(AnimInstance))
				{
					Node->Alpha = 0.f;
				}
			}
		}
	};

	if (VisualMesh)
	{
		ZeroIk(VisualMesh->GetAnimInstance());
		ZeroIk(VisualMesh->GetPostProcessInstance());
	}
}

void ASuperHeroXinEnemy::EnsureSourceFlightAnim()
{
	USkeletalMeshComponent* Source = GetMesh();
	if (!Source)
	{
		return;
	}
	UClass* FlightClass = SourceFlightAnimClass.LoadSynchronous();
	if (!FlightClass)
	{
		return;
	}
	Source->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	if (Source->GetAnimClass() != FlightClass)
	{
		Source->SetAnimInstanceClass(FlightClass);
	}
	Source->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Source->bEnableUpdateRateOptimizations = false;
	Source->bPauseAnims = false;
	if (Source->GetAnimClass() && !Source->GetAnimInstance())
	{
		Source->InitAnim(true);
	}
}

void ASuperHeroXinEnemy::ForEachFlightComponent(TFunctionRef<void(UActorComponent*)> Fn) const
{
	TArray<UActorComponent*> Comps;
	GetComponents(Comps);
	for (UActorComponent* Comp : Comps)
	{
		if (!Comp)
		{
			continue;
		}
		if (!Comp->GetClass()->GetName().Contains(TEXT("SuperheroFlight")))
		{
			continue;
		}
		Fn(Comp);
	}
}

void ASuperHeroXinEnemy::CallFlightFunctions(const TArray<FName>& Names)
{
	// The flight BP fills its own caches in BeginPlay. Calling StopFlight / SetIsSprintFunc
	// before that (AI possession during PostInitializeComponents) spams Accessed None.
	// BeginPlay re-issues StopFlightIfAny for non-players, so nothing is lost by skipping.
	// AActor::BeginPlay runs component BeginPlay before our own body, so "beginning" is fine.
	if (!HasActorBegunPlay() && !IsActorBeginningPlay())
	{
		return;
	}
	BindFlightComponentCaches();
	ForEachFlightComponent([&Names](UActorComponent* Comp)
	{
		if (!Comp->HasBegunPlay())
		{
			return;
		}
		for (const FName& FnName : Names)
		{
			if (UFunction* Fn = Comp->FindFunction(FnName))
			{
				if (Fn->NumParms == 0)
				{
					Comp->ProcessEvent(Fn, nullptr);
				}
			}
		}
	});
}

void ASuperHeroXinEnemy::BindFlightComponentCaches()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	USkeletalMeshComponent* Source = GetMesh();
	UAnimInstance* Anim = Source ? Source->GetAnimInstance() : nullptr;
	ForEachFlightComponent([this, Move, Anim](UActorComponent* Comp)
	{
		for (TFieldIterator<FObjectProperty> It(Comp->GetClass()); It; ++It)
		{
			FObjectProperty* Prop = *It;
			UClass* PropClass = Prop ? Prop->PropertyClass : nullptr;
			if (!PropClass)
			{
				continue;
			}
			if (PropClass->IsChildOf(ACharacter::StaticClass()))
			{
				Prop->SetObjectPropertyValue_InContainer(Comp, const_cast<ASuperHeroXinEnemy*>(this));
			}
			else if (Move && PropClass->IsChildOf(UCharacterMovementComponent::StaticClass()))
			{
				Prop->SetObjectPropertyValue_InContainer(Comp, Move);
			}
			else if (Anim && PropClass->IsChildOf(UAnimInstance::StaticClass()))
			{
				Prop->SetObjectPropertyValue_InContainer(Comp, Anim);
			}
		}
	});
}

void ASuperHeroXinEnemy::SetFlightLogicEnabled(bool bEnabled)
{
	BindFlightComponentCaches();
	ForEachFlightComponent([bEnabled](UActorComponent* Comp)
	{
		Comp->SetComponentTickEnabled(true);
		(void)bEnabled;
	});
}

bool ASuperHeroXinEnemy::PlaySourceAttackMontage(UAnimMontage* Montage)
{
	USkeletalMeshComponent* Source = GetMesh();
	if (!Source || !Montage)
	{
		return false;
	}

	EnsureSourceFlightAnim();
	Source->bPauseAnims = false;
	Source->SetComponentTickEnabled(true);
	Source->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	UAnimMontage* SlotMontage = ResolveSourceSlotMontage(Montage);
	if (!SlotMontage)
	{
		SlotMontage = Montage;
	}

	if (UAnimInstance* Anim = Source->GetAnimInstance())
	{
		const float Played = Anim->Montage_Play(SlotMontage);
		if (Played > 0.f)
		{
			UE_LOG(LogSlimeFable, Log, TEXT("SuperHeroXin punch montage %s (%s) len=%.2f"),
				*GetNameSafe(Montage), *GetNameSafe(SlotMontage), Played);
			return true;
		}
	}
	const float Played = PlayAnimMontage(SlotMontage);
	UE_LOG(LogSlimeFable, Log, TEXT("SuperHeroXin punch PlayAnimMontage %s len=%.2f"),
		*GetNameSafe(SlotMontage), Played);
	return Played > 0.f;
}

UAnimMontage* ASuperHeroXinEnemy::ResolveSourceSlotMontage(UAnimMontage* Montage)
{
	if (!Montage)
	{
		return nullptr;
	}
	static const FName DefaultSlot(TEXT("DefaultSlot"));

	// Already on the slot the flight ABP consumes? Use as-is.
	for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
	{
		if (Track.SlotName == DefaultSlot)
		{
			return Montage;
		}
	}
	if (TObjectPtr<UAnimMontage>* Cached = DefaultSlotMontageCache.Find(Montage))
	{
		if (*Cached)
		{
			return *Cached;
		}
	}

	// ABP_Player_UE5 only has DefaultSlot / Dodge / HoverStart. Lyra montages sit on
	// UpperBody(+Additive): Montage_Play succeeds but nothing consumes the pose.
	// Copy once, keep the first non-additive track, retarget it to DefaultSlot, drop
	// Lyra's GameplayEvent notifies (no ASC here -> LogAbilitySystem errors).
	const FName CopyName = MakeUniqueObjectName(GetTransientPackage(), UAnimMontage::StaticClass(),
		FName(*FString::Printf(TEXT("%s_DefaultSlot"), *Montage->GetName())));
	UAnimMontage* Copy = DuplicateObject<UAnimMontage>(Montage, GetTransientPackage(), CopyName);
	if (!Copy)
	{
		return nullptr;
	}
	Copy->ClearFlags(RF_Public | RF_Standalone);
	Copy->SetFlags(RF_Transient);

	int32 KeepIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Copy->SlotAnimTracks.Num(); ++Index)
	{
		if (!Copy->SlotAnimTracks[Index].AnimTrack.IsAdditive())
		{
			KeepIndex = Index;
			break;
		}
	}
	if (KeepIndex == INDEX_NONE && Copy->SlotAnimTracks.Num() > 0)
	{
		KeepIndex = 0;
	}
	if (KeepIndex == INDEX_NONE)
	{
		return nullptr;
	}
	FSlotAnimationTrack Kept = Copy->SlotAnimTracks[KeepIndex];
	Kept.SlotName = DefaultSlot;
	Copy->SlotAnimTracks.Reset();
	Copy->SlotAnimTracks.Add(Kept);
	Copy->Notifies.Empty();
	Copy->RefreshCacheData();

	DefaultSlotMontageCache.Add(Montage, Copy);
	UE_LOG(LogSlimeFable, Log, TEXT("SuperHeroXin remapped montage %s -> DefaultSlot copy %s"),
		*GetNameSafe(Montage), *GetNameSafe(Copy));
	return Copy;
}

namespace
{
	FNumericProperty* FindFlightNumericProp(UActorComponent* Comp, const TCHAR* Name)
	{
		FProperty* Prop = Comp ? Comp->GetClass()->FindPropertyByName(FName(Name)) : nullptr;
		FNumericProperty* Num = CastField<FNumericProperty>(Prop);
		return (Num && Num->IsFloatingPoint()) ? Num : nullptr;
	}

	bool ReadFlightFloat(UActorComponent* Comp, const TCHAR* Name, float& Out)
	{
		if (FNumericProperty* Num = FindFlightNumericProp(Comp, Name))
		{
			Out = static_cast<float>(Num->GetFloatingPointPropertyValue(Num->ContainerPtrToValuePtr<void>(Comp)));
			return true;
		}
		return false;
	}

	void WriteFlightFloat(UActorComponent* Comp, const TCHAR* Name, float Value)
	{
		if (FNumericProperty* Num = FindFlightNumericProp(Comp, Name))
		{
			Num->SetFloatingPointPropertyValue(Num->ContainerPtrToValuePtr<void>(Comp), static_cast<double>(Value));
		}
	}

	bool ReadFlightBool(UActorComponent* Comp, const TCHAR* Name)
	{
		FBoolProperty* Prop = Comp ? CastField<FBoolProperty>(Comp->GetClass()->FindPropertyByName(FName(Name))) : nullptr;
		return Prop && Prop->GetPropertyValue_InContainer(Comp);
	}
}

void ASuperHeroXinEnemy::CaptureFlightArmBase()
{
	if (bFlightArmBaseCaptured)
	{
		return;
	}
	ForEachFlightComponent([this](UActorComponent* Comp)
	{
		if (bFlightArmBaseCaptured)
		{
			return;
		}
		float DefaultArm = 0.f;
		float AimingArm = 0.f;
		if (ReadFlightFloat(Comp, TEXT("DefaultArmLength"), DefaultArm))
		{
			ReadFlightFloat(Comp, TEXT("AimingArmLength"), AimingArm);
			FlightBaseDefaultArm = DefaultArm;
			FlightBaseAimingArm = AimingArm > 0.f ? AimingArm : DefaultArm;
			bFlightArmBaseCaptured = true;
		}
	});
	if (!bFlightArmBaseCaptured)
	{
		if (const USpringArmComponent* Boom = FindComponentByClass<USpringArmComponent>())
		{
			FlightBaseDefaultArm = Boom->TargetArmLength;
			FlightBaseAimingArm = Boom->TargetArmLength;
			bFlightArmBaseCaptured = true;
		}
	}
}

void ASuperHeroXinEnemy::ApplyFlightArmLengths(bool bRestoreBase)
{
	if (!bFlightArmBaseCaptured)
	{
		return;
	}
	const float Offset = bRestoreBase ? 0.f : MorphZoomOffset;
	ForEachFlightComponent([this, Offset](UActorComponent* Comp)
	{
		// The flight BP FInterpTo's SpringArm.TargetArmLength toward these every flight tick,
		// so zoom has to move the targets rather than fight the arm directly.
		WriteFlightFloat(Comp, TEXT("DefaultArmLength"), FlightBaseDefaultArm + Offset);
		WriteFlightFloat(Comp, TEXT("AimingArmLength"), FlightBaseAimingArm + Offset);
	});
}

bool ASuperHeroXinEnemy::IsOwnerWheelOpen() const
{
	const AActor* Master = MorphMaster.Get();
	if (!Master)
	{
		return false;
	}
	if (const USlimeMorphComponent* Morph = Master->FindComponentByClass<USlimeMorphComponent>())
	{
		if (Morph->IsMorphWheelOpen())
		{
			return true;
		}
	}
	if (const USlimeDevourComponent* Devour = Master->FindComponentByClass<USlimeDevourComponent>())
	{
		if (Devour->IsPhantomWheelOpen())
		{
			return true;
		}
	}
	return false;
}

void ASuperHeroXinEnemy::AdjustMorphCameraZoom(int32 WheelSteps)
{
	CaptureFlightArmBase();
	if (!bFlightArmBaseCaptured || WheelSteps == 0)
	{
		return;
	}
	const float MinArm = FMath::Max(MorphCameraArmLengthMin, 10.f);
	const float MaxArm = FMath::Max(MorphCameraArmLengthMax, MinArm);
	const float Desired = FMath::Clamp(
		FlightBaseDefaultArm + MorphZoomOffset + MorphCameraZoomStep * WheelSteps,
		MinArm,
		MaxArm);
	MorphZoomOffset = Desired - FlightBaseDefaultArm;
	ApplyFlightArmLengths(false);
}

void ASuperHeroXinEnemy::TickMorphCameraZoom(float DeltaSeconds)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}
	CaptureFlightArmBase();

	// Morph / phantom wheels own the scroll while open.
	if (!IsOwnerWheelOpen())
	{
		if (PC->WasInputKeyJustPressed(EKeys::MouseScrollUp))
		{
			AdjustMorphCameraZoom(-1);
		}
		else if (PC->WasInputKeyJustPressed(EKeys::MouseScrollDown))
		{
			AdjustMorphCameraZoom(1);
		}
	}

	if (!bFlightArmBaseCaptured)
	{
		return;
	}
	// On the ground the flight BP does not touch the arm; drive it ourselves toward the same
	// target the BP uses while flying so both writers agree.
	bool bAiming = false;
	ForEachFlightComponent([&bAiming](UActorComponent* Comp)
	{
		bAiming = bAiming || ReadFlightBool(Comp, TEXT("IsAiming"));
	});
	const float Target = (bAiming ? FlightBaseAimingArm : FlightBaseDefaultArm) + MorphZoomOffset;
	if (USpringArmComponent* Boom = FindComponentByClass<USpringArmComponent>())
	{
		Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, Target, DeltaSeconds, MorphCameraZoomInterpSpeed);
	}
}

void ASuperHeroXinEnemy::RestoreSourceAttackAnim()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SourceAttackRestoreTimer);
	}
	bSourceAttackPlaying = false;
	EnsureSourceFlightAnim();
}

void ASuperHeroXinEnemy::CancelFlightDodgeIfAny()
{
	static const TArray<FName> DodgeStops = {
		FName(TEXT("StopDodgeMontageFunc")),
		FName(TEXT("StopDodge")),
		FName(TEXT("CancelDodge")),
		FName(TEXT("Stop_Dodge")),
	};
	CallFlightFunctions(DodgeStops);
}

void ASuperHeroXinEnemy::BindWorldHealthBar()
{
	if (!HealthBar)
	{
		return;
	}
	HealthBar->InitWidget();
	HealthBar->SetDrawSize(FVector2D(72.f, 8.f));
	if (USlimeWorldHealthBar* Bar = Cast<USlimeWorldHealthBar>(HealthBar->GetWidget()))
	{
		Bar->SetHealth(Health);
	}
	RefreshWorldHealthBarVisibility();
}

void ASuperHeroXinEnemy::RefreshWorldHealthBarVisibility()
{
	if (!HealthBar)
	{
		return;
	}
	bool bShow = Health && Health->IsAlive() && !bDeathSequence && !bMorphTarget && !IsPlayerMorphBody();
	if (bShow)
	{
		if (APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			bShow = FVector::DistSquared(GetActorLocation(), Player->GetActorLocation())
				<= FMath::Square(HealthBarVisibleRange);
		}
	}
	HealthBar->SetHiddenInGame(!bShow);
	HealthBar->SetVisibility(bShow);
}

void ASuperHeroXinEnemy::RefreshHealthBarAnchor()
{
	if (!HealthBar)
	{
		return;
	}
	const float Half = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 96.f;
	HealthBar->SetRelativeLocation(FVector(0.f, 0.f, Half + HealthBarZOffset));
}

void ASuperHeroXinEnemy::AddFlightMapping(APlayerController* PC)
{
	if (bFlightMappingAdded || !PC || !PC->IsLocalController())
	{
		return;
	}
	ULocalPlayer* LP = PC->GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	UInputMappingContext* IMC = FlightMapping.LoadSynchronous();
	if (!Subsystem || !IMC)
	{
		return;
	}
	if (!Subsystem->HasMappingContext(IMC))
	{
		Subsystem->AddMappingContext(IMC, 2);
	}
	bFlightMappingAdded = true;
}

void ASuperHeroXinEnemy::RemoveFlightMapping(AController* OldController)
{
	if (!bFlightMappingAdded)
	{
		return;
	}
	bFlightMappingAdded = false;
	APlayerController* PC = Cast<APlayerController>(OldController);
	ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Subsystem)
	{
		return;
	}
	if (UInputMappingContext* IMC = FlightMapping.Get())
	{
		Subsystem->RemoveMappingContext(IMC);
	}
}

void ASuperHeroXinEnemy::StripTutorialHud()
{
	if (FObjectProperty* Prop = FindFProperty<FObjectProperty>(GetClass(), TEXT("WBP_ControlsGuide")))
	{
		if (UUserWidget* Guide = Cast<UUserWidget>(Prop->GetObjectPropertyValue_InContainer(this)))
		{
			Guide->RemoveFromParent();
			Prop->SetObjectPropertyValue_InContainer(this, nullptr);
		}
	}
}

void ASuperHeroXinEnemy::StripFlightMappingIfNotPlayer()
{
	if (IsPlayerMorphBody())
	{
		return;
	}
	if (APlayerController* PC = Cast<APlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
	{
		if (UInputMappingContext* IMC = FlightMapping.LoadSynchronous())
		{
			if (ULocalPlayer* LP = PC->GetLocalPlayer())
			{
				if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
				{
					Subsystem->RemoveMappingContext(IMC);
				}
			}
		}
	}
	bFlightMappingAdded = false;
}

void ASuperHeroXinEnemy::StopFlightIfAny()
{
	ApplyGroundMovement();
	static const TArray<FName> StopNames = {
		FName(TEXT("StopFlight")),
		FName(TEXT("Stop_Flight")),
	};
	CallFlightFunctions(StopNames);
}

void ASuperHeroXinEnemy::ApplyGroundMovement()
{
	if (IsPlayerMorphBody())
	{
		return;
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = ChaseSpeed;
		if (Move->MovementMode == MOVE_Flying || Move->MovementMode == MOVE_None)
		{
			Move->SetDefaultMovementMode();
		}
	}
}

bool ASuperHeroXinEnemy::IsPlayerMorphBody() const
{
	return Cast<APlayerController>(GetController()) != nullptr;
}

bool ASuperHeroXinEnemy::IsInCombatThreat() const
{
	if (const USlimeDodgeComponent* Dodge = FindComponentByClass<USlimeDodgeComponent>())
	{
		return Dodge->IsInEnemyThreatRange();
	}
	return false;
}

bool ASuperHeroXinEnemy::WantsCombatDodge() const
{
	return IsPlayerMorphBody() && IsInCombatThreat();
}

void ASuperHeroXinEnemy::SetAiMoveIntent(const FVector& WorldIntent)
{
	AiMoveIntent = WorldIntent;
	AiMoveIntent.Z = 0.f;
}

void ASuperHeroXinEnemy::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	if (bDeathSequence)
	{
		return;
	}
	if (Health)
	{
		Health->ApplyDamage(Damage, DamageCauser, DamageLocation, DamageImpulse);
	}
}

void ASuperHeroXinEnemy::ApplyHealing(float Healing, AActor* Healer)
{
	(void)Healer;
	if (Health)
	{
		Health->ApplyHealing(Healing);
	}
}

void ASuperHeroXinEnemy::NotifyDanger(const FVector& DangerLocation, AActor* DangerSource)
{
	(void)DangerLocation;
	(void)DangerSource;
}

void ASuperHeroXinEnemy::HandleDeath()
{
	if (bDeathSequence)
	{
		return;
	}
	if (Health && Health->IsAlive())
	{
		ApplyDamage(FMath::Max(Health->CurrentHP, 1.f), this, GetActorLocation(), FVector::ZeroVector);
	}
	else
	{
		HandleDied();
	}
}

void ASuperHeroXinEnemy::HandleDied()
{
	if (bMorphTarget)
	{
		if (AActor* Master = MorphMaster.Get())
		{
			if (USlimeMorphComponent* Morph = Master->FindComponentByClass<USlimeMorphComponent>())
			{
				Morph->ForceUnmorph(true);
			}
		}
		return;
	}
	if (bDeathSequence)
	{
		return;
	}

	bDeathSequence = true;
	ClearAiMoveIntent();
	if (HealthBar)
	{
		HealthBar->SetVisibility(false);
		HealthBar->SetHiddenInGame(true);
	}
	if (Combat)
	{
		Combat->InterruptCombat();
	}
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	if (AController* AI = GetController())
	{
		AI->StopMovement();
		if (AAIController* AIC = Cast<AAIController>(AI))
		{
			if (UBrainComponent* Brain = AIC->GetBrainComponent())
			{
				Brain->StopLogic(TEXT("Death"));
			}
		}
	}

	if (bDevouredDeath)
	{
		SetActorEnableCollision(false);
		SetActorHiddenInGame(true);
		return;
	}

	SetLifeSpan(2.5f);
}

bool ASuperHeroXinEnemy::IsDevourableNow() const
{
	return bDevourable && !bMorphTarget && !bPhantomInstance && !bDeathSequence;
}

float ASuperHeroXinEnemy::GetHealthPercent() const
{
	return Health ? Health->GetHealthPercent() : 0.f;
}

FText ASuperHeroXinEnemy::GetResolvedDisplayName() const
{
	return DisplayName.IsEmpty() ? NSLOCTEXT("SuperHeroXin", "FallbackName", "心月狐（飞行）") : DisplayName;
}

FLinearColor ASuperHeroXinEnemy::ResolveDevourWheelTint() const
{
	return FLinearColor(0.45f, 0.62f, 0.95f);
}

USkeletalMeshComponent* ASuperHeroXinEnemy::GetDevourPreviewMesh() const
{
	return VisualMesh ? VisualMesh.Get() : GetMesh();
}

void ASuperHeroXinEnemy::ForEachVisualMesh(TFunctionRef<void(UMeshComponent*)> Fn) const
{
	if (VisualMesh && VisualMesh->GetSkeletalMeshAsset() && !VisualMesh->IsVisualizationComponent())
	{
		Fn(VisualMesh);
		return;
	}
	if (USkeletalMeshComponent* Source = GetMesh())
	{
		if (!Source->IsVisualizationComponent())
		{
			Fn(Source);
		}
	}
}

void ASuperHeroXinEnemy::InitAsMorphTarget(AActor* Master)
{
	bMorphTarget = true;
	bDevourable = false;
	MorphMaster = Master;
	AutoPossessAI = EAutoPossessAI::Disabled;
	AIControllerClass = nullptr;
	ClearAiMoveIntent();
	if (Health)
	{
		Health->Team = ESlimeTeam::Player;
		Health->bDestroyOnDeath = false;
		Health->ResetHP();
	}
	EnsureVisuals();
	if (!MorphLockOn)
	{
		MorphLockOn = NewObject<USlimeLockOnComponent>(this, TEXT("MorphLockOn"));
		MorphLockOn->bPollLockOnKey = true;
		MorphLockOn->RegisterComponent();
	}
	RefreshWorldHealthBarVisibility();
}

void ASuperHeroXinEnemy::InitAsPhantom(float LifeSeconds, AActor* Master)
{
	bPhantomInstance = true;
	bDevourable = false;
	MorphMaster = Master;
	(void)LifeSeconds;
	if (Health)
	{
		Health->Team = ESlimeTeam::Player;
		Health->bDestroyOnDeath = false;
	}
}

void ASuperHeroXinEnemy::BeginDevouredDeath(AActor* Devourer)
{
	(void)Devourer;
	bDevouredDeath = true;
	bDeathSequence = true;
	bDevourable = false;
}

void ASuperHeroXinEnemy::FreezeForDevour()
{
	ClearAiMoveIntent();
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->Velocity = FVector::ZeroVector;
		Move->GravityScale = 0.f;
		Move->SetMovementMode(MOVE_None);
		Move->DisableMovement();
	}
	if (Combat)
	{
		Combat->InterruptCombat();
	}
}

void ASuperHeroXinEnemy::RestoreFromDevour()
{
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->GravityScale = 1.f;
		Move->SetDefaultMovementMode();
	}
}

void ASuperHeroXinEnemy::SetMorphGameplayEnabled(bool bEnabled)
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		if (!bEnabled)
		{
			Movement->StopMovementImmediately();
			Movement->Velocity = FVector::ZeroVector;
			Movement->DisableMovement();
			SetFlightLogicEnabled(false);
			return;
		}
		if (Movement->MovementMode != MOVE_Flying)
		{
			Movement->SetDefaultMovementMode();
		}
	}
	if (bEnabled)
	{
		SetFlightLogicEnabled(true);
	}
}

void ASuperHeroXinEnemy::StopMeshAnimation()
{
	RestoreSourceAttackAnim();
	auto StopOn = [](USkeletalMeshComponent* Skel)
	{
		if (UAnimInstance* Anim = Skel ? Skel->GetAnimInstance() : nullptr)
		{
			Anim->StopAllMontages(0.1f);
		}
	};
	StopOn(GetMesh());
	StopOn(VisualMesh);
}

FVector ASuperHeroXinEnemy::GetVisualBoundsCenter() const
{
	if (USkeletalMeshComponent* Skel = GetDevourPreviewMesh())
	{
		return Skel->Bounds.Origin;
	}
	return GetActorLocation();
}

FVector ASuperHeroXinEnemy::GetHudAnchorLocation() const
{
	const float Half = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 96.f;
	return GetActorLocation() + FVector(0.f, 0.f, Half + HealthBarZOffset);
}

bool ASuperHeroXinEnemy::GetStableMeshBounds(FBox& OutBox) const
{
	if (USkeletalMeshComponent* Skel = GetDevourPreviewMesh())
	{
		OutBox = Skel->Bounds.GetBox();
		return true;
	}
	return false;
}

bool ASuperHeroXinEnemy::CanBeLockedOn() const
{
	return Health && Health->IsAlive() && !bDeathSequence;
}

FVector ASuperHeroXinEnemy::GetLockOnLocation() const
{
	return GetHudAnchorLocation();
}
