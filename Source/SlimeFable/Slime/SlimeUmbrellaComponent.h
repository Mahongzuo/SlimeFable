// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlimeUmbrellaComponent.generated.h"

class USkeletalMeshComponent;
class USkeletalMesh;
class UAnimSequence;
class ASlimeCharacter;
class USlimeBodyComponent;

UCLASS(ClassGroup=(Slime), meta=(BlueprintSpawnableComponent, PrioritizeCategories="0_Config"))
class SLIMEFABLE_API USlimeUmbrellaComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 USlimeUmbrellaComponent();
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void TickComponent(float Dt, ELevelTick Tick, FActorComponentTickFunction* Function) override;
 UFUNCTION(BlueprintCallable, Category="Slime|Umbrella") bool ToggleUmbrella();
 UFUNCTION(BlueprintCallable, Category="Slime|Umbrella") void CancelUmbrella(bool bImmediate=false);
 UFUNCTION(BlueprintPure, Category="Slime|Umbrella") bool IsUmbrellaOpen() const { return bOpen; }
 void ResetUmbrellas();
 float LimitFallVelocity(float NormalZ) const;

 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Umbrella", meta=(ClampMin="30", ToolTip="撑伞稳定下降速度，默认180厘米/秒，不产生向上推力"))
 float FallSpeed=180.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Umbrella", meta=(ClampMin="0.01", ToolTip="开伞后高速下降减速时间，默认0.28秒；上升阶段不减小重力"))
 float BrakeSeconds=0.28f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Umbrella", meta=(ClampMin="0", ClampMax="1", ToolTip="撑伞空中转向控制，默认0.65，水平速度仍受当前移动速度限制"))
 float GlideAirControl=0.65f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Umbrella", meta=(ClampMin="0.01", ToolTip="展开伞动画时间，默认0.28秒；反向切换保持当前开合进度"))
 float OpenSeconds=0.28f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Umbrella", meta=(ClampMin="0.01", ToolTip="收伞动画时间，默认0.22秒，落地收完后隐藏"))
 float CloseSeconds=0.22f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Umbrella", meta=(ClampMin="0.5", ToolTip="伞面半径与身体半径之比，默认1.2；主体和子球共用"))
 float CanopyRadiusRatio=1.2f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Umbrella", meta=(ClampMin="0", ToolTip="回收子球距离地面超过此高度时可自动撑伞，默认120厘米；下方无地面也允许"))
 float ShotOpenHeight=120.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Umbrella", meta=(ClampMin="10", ToolTip="撑伞子球水平追随最大速度，默认420厘米/秒"))
 float ShotFollowSpeed=420.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Umbrella", meta=(ClampMin="30", ToolTip="撑伞子球下降速度下限，默认180厘米/秒"))
 float ShotFallSpeed=180.f;
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="0_Config|Umbrella", meta=(ClampMin="30", ToolTip="子球落后母体较高时的下降速度上限，默认260厘米/秒"))
 float ShotMaxFallSpeed=260.f;
private:
 bool CanUseUmbrella() const;
 void UpdateVisual(uint8 Id, bool bWanted, const FVector& Base, float Radius, float Dt);
 UPROPERTY(Transient) TObjectPtr<ASlimeCharacter> Character;
 UPROPERTY(Transient) TObjectPtr<USlimeBodyComponent> Body;
 UPROPERTY(Transient) TArray<TObjectPtr<USkeletalMesh>> Styles;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> OpenAnimation;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> CloseAnimation;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> GlideAnimation;
 UPROPERTY(Transient) TMap<uint8,TObjectPtr<USkeletalMeshComponent>> Visuals;
 TMap<uint8,float> OpenAmounts;
 bool bOpen=false;
 double BrakeStart=-1;
 float BrakeInitialSpeed=180.f;
 uint64 LastToggleFrame=MAX_uint64;
};
