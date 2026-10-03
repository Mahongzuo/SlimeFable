#pragma once

#include "CoreMinimal.h"
#include "MiniGame/SlimeMiniGameDirector.h"
#include "MiniGame/SlimeArchiveRules.h"
#include "SlimeArchiveDirector.generated.h"

class ACameraActor;
class ATargetPoint;
class UStaticMesh;
class UMaterialInterface;
class USlimeArchiveHUDWidget;
class USlimeDevourComponent;
class UTextRenderComponent;

USTRUCT()
struct FSlimeArchiveParcel
{
	GENERATED_BODY()
	UPROPERTY(Transient) TObjectPtr<AActor> Actor;
	int32 Category = 0;
	float Age = 0.f;
};

/** Third-person NACA reception shift, using the normal slime pawn and follow camera. */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeArchiveDirector : public ASlimeMiniGameDirector
{
	GENERATED_BODY()
public:
	ASlimeArchiveDirector();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "0_Config|Room", meta = (ToolTip = "仅供编辑器预览房间。游戏始终使用史莱姆原有第三人称跟随镜头。"))
	TObjectPtr<ACameraActor> RoomCamera;
	UPROPERTY(EditAnywhere, Category = "0_Config|Room", meta = (ToolTip = "传送带起点，物品从此位置出现。"))
	TObjectPtr<ATargetPoint> BeltStart;
	UPROPERTY(EditAnywhere, Category = "0_Config|Room", meta = (ToolTip = "传送带终点；未吞下的物品在此进入退件箱。"))
	TObjectPtr<ATargetPoint> BeltEnd;
	UPROPERTY(EditAnywhere, Category = "0_Config|Room", meta = (ToolTip = "四个投递口，按兰利、艾姆斯、刘易斯、新 NASA 排序。"))
	TArray<TObjectPtr<ATargetPoint>> CabinetTargets;
	UPROPERTY(EditAnywhere, Category = "0_Config|Assets", meta = (ToolTip = "从 BP_0815_2004_Paper 复制的本关纸张蓝图，旧任务绑定须清空。"))
	TSubclassOf<AActor> PaperClass;
	UPROPERTY(EditAnywhere, Category = "0_Config|Assets", meta = (ToolTip = "文件夹网格，原点在底面；默认使用本关 Blender 档案模型。"))
	TObjectPtr<UStaticMesh> FolderMesh;
	UPROPERTY(EditAnywhere, Category = "0_Config|Assets", meta = (ToolTip = "设备箱网格；与文件夹交替出现，按编号分类。"))
	TObjectPtr<UStaticMesh> EquipmentMesh;
	UPROPERTY(EditAnywhere, Category = "0_Config|Assets", meta = (ToolTip = "四种标签材质，顺序与柜子相同。标签兼有编号，不只靠颜色。"))
	TArray<TObjectPtr<UMaterialInterface>> CategoryMaterials;
	UPROPERTY(EditAnywhere, Category = "0_Config|Rules", meta = (ToolTip = "各周目合格件数。默认 12 / 16 / 20，且每柜至少 2 / 3 / 4 件。"))
	FIntVector TargetCounts = FIntVector(12, 16, 20);
	UPROPERTY(EditAnywhere, Category = "0_Config|Rules", meta = (ToolTip = "每隔多少秒送来一件；各周目默认 6 / 5 / 4.5 秒。"))
	FVector ArrivalSeconds = FVector(6.f, 5.f, 4.5f);
	UPROPERTY(EditAnywhere, Category = "0_Config|Rules", meta = (ToolTip = "物品完整通过传送带所需秒数，默认 28 秒。"))
	float BeltTravelSeconds = 28.f;
	UPROPERTY(EditAnywhere, Category = "0_Config|Rules", meta = (ToolTip = "吞下及柜子投递的最大距离，厘米，默认 210。"))
	float InteractionRange = 210.f;

protected:
	virtual void EnterMode(ACharacter* Character, APlayerController* PC) override;
	virtual void ExitMode(ACharacter* Character, APlayerController* PC) override;
private:
	friend class FSlimeArchiveRuntimeTest;
	friend class FSlimeArchivePIETest;
	enum class EShift : uint8 { Ready, Running, Results, Invalid };
	void ResetShift();
	void SpawnParcel();
	void Swallow();
	void Spit();
	void ClearParcels();
	void AdvanceTransfers(float Dt);
	void RefreshHUD();
	void SetMessage(const FString& Text);
	int32 NearestCabinet() const;
	int32 HeldCategory() const;
	void FinishShift(bool bSuccess);

	UPROPERTY(Transient) TArray<FSlimeArchiveParcel> Parcels;
	UPROPERTY(Transient) TObjectPtr<USlimeArchiveHUDWidget> HUD;
	UPROPERTY(Transient) TObjectPtr<USlimeDevourComponent> Devour;
	UPROPERTY(Transient) TObjectPtr<UTextRenderComponent> CargoBadge;
	float CargoPhase = 0.f;
	TWeakObjectPtr<AActor> FlyingParcel;
	TWeakObjectPtr<AActor> SwallowingParcel;
	FVector SwallowStart, SwallowScale;
	float SwallowAlpha = 1.f;
	FVector FlightStart, FlightEnd;
	float FlightAlpha = 1.f;
	FTransform PlayerStart;
	float TimeLeft = 0.f;
	float ArrivalLeft = 0.f;
	float MessageLeft = 0.f;
	int32 Serial = 0;
	int32 Bag[4] = {0, 1, 2, 3};
	FRandomStream Random;
	FSlimeArchiveRules Rules;
	EShift Shift = EShift::Ready;
	FString Message;
	bool bSuccess = false;
	bool bQualified = false;
};
