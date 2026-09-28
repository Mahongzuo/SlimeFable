// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LyraShooterEnemy.h"
#include "LyraXinShooterEnemy.generated.h"

class UMaterialInterface;

/**
 * Lyra shooter that plays Manny + weapon layers on a hidden source mesh and shows
 * 心月狐 via IK retarget. First HP bar emptying swaps Form1 → Form2 instead of dying.
 */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ALyraXinShooterEnemy : public ALyraShooterEnemy
{
	GENERATED_BODY()

public:
	ALyraXinShooterEnemy(const FObjectInitializer& ObjectInitializer);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual bool IsDevourableNow() const override;
	virtual USkeletalMeshComponent* GetDevourPreviewMesh() const override;
	virtual USkeletalMeshComponent* GetMorphVisualMesh() const override;
	virtual void ForEachVisualMesh(TFunctionRef<void(UMeshComponent*)> Fn) const override;
	virtual FVector GetVisualBoundsCenter() const override;
	virtual bool GetStableMeshBounds(FBox& OutBox) const override;
	virtual void StopMeshAnimation() override;
	virtual bool UsesDualHealthBars() const override { return true; }
	virtual void GetDualHealthPercents(float& OutPhase1, float& OutPhase2) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ToolTip = "第一阶段外观网格。默认 /Game/_Slime/Models/Xin/Xin_Form1_UE5。"))
	TSoftObjectPtr<USkeletalMesh> Phase1Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ToolTip = "第二阶段外观网格。默认 /Game/_Slime/Models/Xin2/Xin_Form2_UE5。第一管血打空后换成它。"))
	TSoftObjectPtr<USkeletalMesh> Phase2Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ToolTip = "第一阶段重定向 ABP。默认 ABP_XinForm1_Retarget。软引用，禁止构造函数硬加载。"))
	TSoftClassPtr<UAnimInstance> Phase1AnimClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ToolTip = "第二阶段重定向 ABP。默认 ABP_XinForm2_Retarget。"))
	TSoftClassPtr<UAnimInstance> Phase2AnimClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ToolTip = "第一阶段 ABP_GenericRetarget 查表用的 ComponentTag，必须等于 RTG 资产名。默认 RTG_Manny_to_XinForm1。"))
	FName Phase1RetargetTag = FName(TEXT("RTG_Manny_to_XinForm1"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ToolTip = "第二阶段 ComponentTag。默认 RTG_Manny_to_XinForm2。"))
	FName Phase2RetargetTag = FName(TEXT("RTG_Manny_to_XinForm2"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ClampMin = "1.0", ClampMax = "9999.0",
			ToolTip = "第一管血上限。默认 120，和普通射击兵一样。打空后换 Form2 并回满第二管。"))
	float Phase1MaxHP = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ClampMin = "1.0", ClampMax = "9999.0",
			ToolTip = "第二管血上限。默认 120。这管打空才死亡。"))
	float Phase2MaxHP = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ClampMin = "0.0", ClampMax = "5.0", Units = "s",
			ToolTip = "切阶段后的无敌秒数，避免同一发打穿两管。默认 0.6。"))
	float PhaseTransitionInvulnSeconds = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ToolTip = "外观网格相对隐藏 Manny 的位移。Manny 已经在胶囊底 (0,0,-90)。默认 0。"))
	FVector VisualRelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ToolTip = "外观网格相对隐藏 Manny 的旋转。默认 0。枪偏了或站姿拧了再调。"))
	FRotator VisualRelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ToolTip = "枪相对当前姿态的位移（厘米），跟着枪一起转。正 X 沿枪自身 X。把手对到握把时优先调这个。选关卡里的 BP_LyraXinEnemy 或打开 BP 即可，不用改代码。默认 0。"))
	FVector WeaponHandOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ToolTip = "枪相对当前姿态的旋转（度）。握把角度歪了再拧。默认 0。"))
	FRotator WeaponHandRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ToolTip = "额外的世界位移（厘米）。Z 上移为正。只做整体抬高/降低时用；对着手调请用上面的 WeaponHandOffset。默认 0。"))
	FVector WeaponWorldOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ToolTip = "开启后，举枪时用心月狐自己的上臂/前臂/手做 IK，拉到 Manny 手位，只补位置、不拷旋转。待机垂手会自动关掉。默认开。"))
	bool bPullHandsToSource = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ClampMin = "0.0", ClampMax = "1.0",
			ToolTip = "举枪时的手 IK 强度。1=手跟 Manny 手位。0=只用重定向。默认 1。"))
	float HandIKAlpha = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ClampMin = "0.0", ClampMax = "100.0", Units = "cm",
			ToolTip = "Manny 右手比 pelvis 至少高出这么多厘米才开手 IK。待机垂手关掉，避免手腕被拧。默认 25。"))
	float HandIKMinRaiseAbovePelvis = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ToolTip = "右手目标相对 Manny hand_r 的本地偏移（厘米）。手已经贴到枪附近但手腕还差一截时再调。默认 0。"))
	FVector RightHandAlignOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ToolTip = "叠在重定向右手腕朝向上的额外旋转（度），不复制 Manny。默认 0。手腕还拧就在这里微调。"))
	FRotator RightHandAlignRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ToolTip = "左手目标相对 Manny hand_l 的本地偏移（厘米）。默认 0。"))
	FVector LeftHandAlignOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ToolTip = "叠在重定向左手腕朝向上的额外旋转（度），不复制 Manny。默认 0。"))
	FRotator LeftHandAlignRotation = FRotator::ZeroRotator;

	void ApplyHandIKToAnimInstance(UAnimInstance* AnimInstance) const;

	/** Swap Form1 / Form2 look without changing HealthPhase (morph Tab hold). */
	void CycleVisualSkin();

	virtual void HandleDeath() override;

protected:
	virtual USkeletalMeshComponent* GetDeathRagdollMesh() const override;
	virtual void EnableDeathRagdoll(USkeletalMeshComponent* MeshComp) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components")
	TObjectPtr<USkeletalMeshComponent> VisualMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "0_Config|Xin",
		meta = (ToolTip = "战斗阶段。1=第一管血，2=第二管血。只读，和外观皮肤分开。"))
	int32 HealthPhase = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "0_Config|Xin",
		meta = (ToolTip = "当前外观。1=Form1，2=Form2。幻形后按住 Tab 切换，不改血条。"))
	int32 VisualForm = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Xin",
		meta = (ToolTip = "外观描边 Overlay。默认菲比 MI_PhoebeOutline_Overlay。"))
	TSoftObjectPtr<UMaterialInterface> VisualOverlayMaterial;

	virtual void HandleSlimeHealthChanged(float CurrentHP, float MaxHP) override;

	void EnsureXinVisuals();
	void StopLivingVisualPhysics();
	void HideSourceMeshKeepChildren();
	void ApplyVisualForPhase(int32 Phase);
	void RefreshDualHealthBars();
	bool AbsorbLethalDamage(AActor* DamageCauser);
	void BeginPhase2();
	void SnapWeaponsToVisual();

	UPROPERTY(Transient)
	TMap<TObjectPtr<AActor>, FTransform> WeaponSpawnRelative;
};
