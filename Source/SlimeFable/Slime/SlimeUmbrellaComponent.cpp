// Copyright Epic Games, Inc. All Rights Reserved.
#include "SlimeUmbrellaComponent.h"
#include "SlimeCharacter.h"
#include "SlimeBodyComponent.h"
#include "SlimeCharacterMovementComponent.h"
#include "SlimeElementComponent.h"
#include "SlimeHealthComponent.h"
#include "SlimeDevourComponent.h"
#include "SlimeMorphComponent.h"
#include "SlimeVehicleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "SlimeFable.h"

USlimeUmbrellaComponent::USlimeUmbrellaComponent()
{
 PrimaryComponentTick.bCanEverTick=true;
 PrimaryComponentTick.TickGroup=TG_PrePhysics;
}
void USlimeUmbrellaComponent::BeginPlay()
{
 Super::BeginPlay();
 Character=Cast<ASlimeCharacter>(GetOwner());
 Body=GetOwner()->FindComponentByClass<USlimeBodyComponent>();
 if (Character) Character->GetCharacterMovement()->AddTickPrerequisiteComponent(this);
 const TCHAR* Names[]={TEXT("Water"),TEXT("Wind"),TEXT("Fire"),TEXT("Lightning"),TEXT("Dark"),TEXT("Physical")};
 for (const TCHAR* Name:Names)
  Styles.Add(LoadObject<USkeletalMesh>(nullptr,*FString::Printf(TEXT("/Game/Characters/Slime/Umbrella/SK_Umbrella_%s.SK_Umbrella_%s"),Name,Name)));
 OpenAnimation=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Slime/Umbrella/A_Umbrella_Open.A_Umbrella_Open"));
 CloseAnimation=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Slime/Umbrella/A_Umbrella_Close.A_Umbrella_Close"));
 GlideAnimation=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Slime/Umbrella/A_Umbrella_Glide.A_Umbrella_Glide"));
 if (!OpenAnimation || Styles.Contains(nullptr)) UE_LOG(LogSlimeFable,Warning,TEXT("Umbrella assets missing: run import_slime_umbrella.py"));
}
bool USlimeUmbrellaComponent::CanUseUmbrella() const
{
 if (!Character || !Character->GetCharacterMovement()->IsFalling() || Character->IsHidden()) return false;
 if (const USlimeHealthComponent* Health=GetOwner()->FindComponentByClass<USlimeHealthComponent>()) if (!Health->IsAlive()) return false;
 if (const USlimeDevourComponent* Devour=GetOwner()->FindComponentByClass<USlimeDevourComponent>())
  if (Devour->IsCombatLocked() || Devour->IsPhantomWheelOpen()) return false;
 if (const USlimeVehicleComponent* Vehicle=GetOwner()->FindComponentByClass<USlimeVehicleComponent>()) if (Vehicle->IsUsingVehicle()) return false;
 if (const USlimeMorphComponent* Morph=GetOwner()->FindComponentByClass<USlimeMorphComponent>()) if (Morph->IsMorphed()) return false;
 return true;
}
bool USlimeUmbrellaComponent::ToggleUmbrella()
{
 if (LastToggleFrame==GFrameCounter) return true;
 LastToggleFrame=GFrameCounter;
 const APlayerController* PC=Character ? Cast<APlayerController>(Character->GetController()) : nullptr;
 if (!PC || UGameplayStatics::IsGamePaused(this) || PC->IsMoveInputIgnored() || PC->bShowMouseCursor || !CanUseUmbrella()) return false;
 if (bOpen) CancelUmbrella();
 else { bOpen=true; BrakeStart=-1; }
 return true;
}
void USlimeUmbrellaComponent::CancelUmbrella(bool bImmediate)
{
 if (bOpen && Character) Character->ResetUmbrellaFallOrigin();
 bOpen=false; BrakeStart=-1;
 if (bImmediate)
 {
  if (TObjectPtr<USkeletalMeshComponent>* Visual=Visuals.Find(0)) if (*Visual) (*Visual)->SetVisibility(false);
  OpenAmounts.Remove(0);
 }
}
float USlimeUmbrellaComponent::LimitFallVelocity(float NormalZ) const
{
 if (!bOpen || NormalZ>=0.f || BrakeStart<0 || !GetWorld()) return NormalZ;
 const float T=FMath::Clamp(float(GetWorld()->GetTimeSeconds()-BrakeStart)/FMath::Max(BrakeSeconds,0.01f),0.f,1.f);
 const float Limit=FMath::Lerp(BrakeInitialSpeed,FMath::Max(FallSpeed,30.f),FMath::SmoothStep(0.f,1.f,T));
 return FMath::Max(NormalZ,-Limit);
}
void USlimeUmbrellaComponent::TickComponent(float Dt,ELevelTick Tick,FActorComponentTickFunction* Function)
{
 Super::TickComponent(Dt,Tick,Function);
 if (!Character || !Body) return;
 if (const USlimeHealthComponent* Health=GetOwner()->FindComponentByClass<USlimeHealthComponent>())
  if (!Health->IsAlive()) { ResetUmbrellas(); return; }
 if (bOpen && !CanUseUmbrella()) CancelUmbrella();
 if (bOpen && Character->GetVelocity().Z<=0.f && BrakeStart<0)
 {
  BrakeStart=GetWorld()->GetTimeSeconds();
  BrakeInitialSpeed=FMath::Max(FallSpeed,float(-Character->GetVelocity().Z));
 }
 Body->ConfigureShotUmbrellas(ShotOpenHeight,ShotFollowSpeed,ShotFallSpeed,ShotMaxFallSpeed);
 const float Radius=Body->SolverParams.RestRadius*Body->GetAppliedBodyScale();
 UpdateVisual(0,bOpen,Body->GetShellCenter()+FVector(0,0,Radius*0.6f),Radius,Dt);
 TSet<uint8> Alive; Alive.Add(0);
 for (const FSlimeSolver::FShotState& Shot:Body->GetShotStates())
 {
  Alive.Add(Shot.Id);
  UpdateVisual(Shot.Id,Shot.bUmbrellaOpen,FVector(Shot.Center)+FVector(0,0,Body->GetMiniMembraneRadius()*0.65f),Body->GetMiniMembraneRadius(),Dt);
 }
 for (auto It=Visuals.CreateIterator();It;++It) if (!Alive.Contains(It.Key()))
 { if (It.Value()) It.Value()->DestroyComponent(); OpenAmounts.Remove(It.Key()); It.RemoveCurrent(); }
}
void USlimeUmbrellaComponent::UpdateVisual(uint8 Id,bool bWanted,const FVector& Base,float Radius,float Dt)
{
 float& Amount=OpenAmounts.FindOrAdd(Id);
 Amount=FMath::FInterpConstantTo(Amount,bWanted?1.f:0.f,Dt,1.f/FMath::Max(bWanted?OpenSeconds:CloseSeconds,0.01f));
 if (Amount<=0.f)
 {
  if (auto* Existing=Visuals.Find(Id)) if (*Existing) (*Existing)->SetVisibility(false);
  return;
 }
 const USlimeElementComponent* Element=GetOwner()->FindComponentByClass<USlimeElementComponent>();
 const int32 Style=Element ? static_cast<int32>(Element->CurrentElement) : 0;
 if (!Styles.IsValidIndex(Style) || !Styles[Style] || !OpenAnimation) return;
 TObjectPtr<USkeletalMeshComponent>& Mesh=Visuals.FindOrAdd(Id);
 if (!Mesh)
 {
  Mesh=NewObject<USkeletalMeshComponent>(GetOwner(),NAME_None,RF_Transient);
  Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Mesh->SetGenerateOverlapEvents(false);
  Mesh->SetCastShadow(true);
  Mesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
  Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
  Mesh->RegisterComponent();
 }
 if (Mesh->GetSkeletalMeshAsset()!=Styles[Style]) Mesh->SetSkeletalMesh(Styles[Style]);
 Mesh->SetVisibility(true);
 const FVector Velocity=Id==0 ? Character->GetVelocity() : FVector::ZeroVector;
 const FRotator Tilt(FMath::Clamp(-Velocity.X/42.f,-10.f,10.f),0,FMath::Clamp(Velocity.Y/42.f,-10.f,10.f));
 Mesh->SetWorldLocationAndRotation(Base,Tilt);
 // Blender reference canopy radius is 50cm. All styles and animations share this rig.
 Mesh->SetWorldScale3D(FVector(FMath::Max(Radius,1.f)*CanopyRadiusRatio/50.f));
 UAnimSequence* Animation=Amount>=1.f && GlideAnimation ? GlideAnimation.Get() : (!bWanted && CloseAnimation ? CloseAnimation.Get() : OpenAnimation.Get());
 if (!Mesh->GetSingleNodeInstance() || Mesh->GetSingleNodeInstance()->GetCurrentAsset()!=Animation) Mesh->SetAnimation(Animation);
 const float Position=Amount>=1.f && Animation==GlideAnimation
  ? FMath::Fmod(float(GetWorld()->GetTimeSeconds())+Id*0.17f,Animation->GetPlayLength())
  : (Animation==CloseAnimation ? 1.f-Amount : Amount)*Animation->GetPlayLength();
 Mesh->SetPosition(Position,false);
 Mesh->SetPlayRate(0.f);
}
void USlimeUmbrellaComponent::ResetUmbrellas()
{
 CancelUmbrella(true);
 for (auto& Pair:Visuals) if (Pair.Value) Pair.Value->DestroyComponent();
 Visuals.Reset(); OpenAmounts.Reset();
}
void USlimeUmbrellaComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 ResetUmbrellas();
 Super::EndPlay(Reason);
}
