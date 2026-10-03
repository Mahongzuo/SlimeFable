#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimeTrackActors.generated.h"

class UStaticMeshComponent;

/** 0 北、1 东、2 南、3 西。北是格子的上一行，东是右一列。直轨偶数朝向为东西，奇数朝向为南北。弯轨朝向 Q 接通 Q 与 Q+1。 */
UENUM(BlueprintType)
enum class ESlimeTrackKind : uint8
{
	Straight UMETA(DisplayName = "直轨"),
	Curve UMETA(DisplayName = "弯轨"),
	Fixed UMETA(DisplayName = "固定轨"),
};

/** 一屏区间。格子行用字符描述，轨道块是单独摆的 Actor。 */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeTrackBoard : public AActor
{
	GENERATED_BODY()

public:
	ASlimeTrackBoard();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Track",
		meta = (ToolTip = "第几屏，从 0 起。0 是苹果园到五棵松，4 是前门到北京站。"))
	int32 BoardIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Track",
		meta = (ToolTip = "一格的边长，厘米。格子原点是这个 Actor 的位置，X 向东，Y 向南。"))
	float CellSize = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Track",
		meta = (ToolTip = "史莱姆站在格子上时，胶囊中心比地面高出多少厘米。胶囊半高约 32。"))
	float StandZ = 32.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Track",
		meta = (ToolTip = "从上到下的格子行。# 墙，. 地面，d 检修通道，A 起点站口，B 终点站口，P 史莱姆起点。"))
	TArray<FString> Rows;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Track",
		meta = (ToolTip = "这一屏的起点站名，例如苹果园。"))
	FText StartName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Track",
		meta = (ToolTip = "这一屏的终点站名，例如五棵松。"))
	FText EndName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Track",
		meta = (ClampMin = "0", ClampMax = "3", ToolTip = "起点站口往哪边接轨道。0 北，1 东，2 南，3 西。"))
	int32 StartFacing = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Track",
		meta = (ClampMin = "0", ClampMax = "3", ToolTip = "终点站口往哪边接轨道。0 北，1 东，2 南，3 西。"))
	int32 EndFacing = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Track",
		meta = (ToolTip = "第 1 周目这一屏的步数上限。用完可按 Z 撤销或 R 重来这一屏。"))
	int32 StepLimitWeek1 = 20;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Track",
		meta = (ToolTip = "第 2 周目这一屏的步数上限。"))
	int32 StepLimitWeek2 = 14;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Track",
		meta = (ToolTip = "第 3 周目这一屏的步数上限。第 3 周目另外还有全章限时。"))
	int32 StepLimitWeek3 = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Track",
		meta = (ToolTip = "勾上后这一屏可以按 Q 分出小团。只有小团能走进检修通道。"))
	bool bAllowBlob = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Camera",
		meta = (ToolTip = "勾上后使用下面的俯视机位。不勾则自动架在棋盘南侧上方。"))
	bool bUseCamera = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Camera",
		meta = (ToolTip = "这一屏的相机位置，厘米。"))
	FVector CameraLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Camera",
		meta = (ToolTip = "这一屏的相机朝向。俯视看向北方时，俯仰约 -75，偏航 -90。"))
	FRotator CameraRotation = FRotator(-75.f, -90.f, 0.f);

	int32 Width = 0;
	int32 Height = 0;

	void EnsureParsed();
	bool InBounds(int32 X, int32 Y) const;
	TCHAR GetCell(int32 X, int32 Y) const;
	bool FindCell(TCHAR Mark, int32& OutX, int32& OutY) const;
	FVector CellCenter(int32 X, int32 Y) const;
	FVector Stand(int32 X, int32 Y) const;
	int32 StepLimitFor(int32 Week) const;

private:
	bool bParsed = false;
};

/** 一块轨道。直轨和弯轨可以推、旋转 90 度；固定轨不参与推转。 */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeTrackPiece : public AActor
{
	GENERATED_BODY()

public:
	ASlimeTrackPiece();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Track")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Z_Components", AdvancedDisplay)
	TObjectPtr<UStaticMeshComponent> TargetMarker;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Track")
	TObjectPtr<UStaticMeshComponent> HintArrow;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Track",
		meta = (ToolTip = "属于第几屏，和棋盘的屏号一致。"))
	int32 BoardIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Track",
		meta = (ToolTip = "直轨可推，弯轨可推并且可以按 F 转 90 度，固定轨是这一屏原来就铺好的。"))
	ESlimeTrackKind Kind = ESlimeTrackKind::Straight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Track",
		meta = (ClampMin = "0", ClampMax = "3", ToolTip = "朝向，0 到 3。每加 1 是逆时针 90 度。直轨 0 为东西，1 为南北。"))
	int32 QuarterTurns = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Track",
		meta = (ToolTip = "所在列，从左往右，0 起。"))
	int32 GridX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Track",
		meta = (ToolTip = "所在行，从上往下，0 起。"))
	int32 GridY = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Hint",
		meta = (ToolTip = "第 1 周目前两屏才显示。勾上后，这块轨道上方出现指向目标格的箭头。"))
	bool bHasHint = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Hint",
		meta = (ToolTip = "提示箭头指向的列。"))
	int32 HintX = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Hint",
		meta = (ToolTip = "提示箭头指向的行。"))
	int32 HintY = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Hint",
		meta = (ToolTip = "若目标就在当前格，箭头改用这个朝向。"))
	int32 HintQuarter = 0;

	virtual void BeginPlay() override;

	void MoveTo(int32 X, int32 Y, const FVector& World, bool bSnap);
	void SetQuarter(int32 Quarter, bool bSnap);
	void ResetHome(const FVector& World);
	void Advance(float DeltaSeconds);
	bool IsBusy() const;
	void RefreshHint(const ASlimeTrackBoard* Board, bool bShow);

	int32 HomeX = 0;
	int32 HomeY = 0;
	int32 HomeQuarter = 0;

private:
	FVector SlideFrom = FVector::ZeroVector;
	FVector SlideTo = FVector::ZeroVector;
	float SlideAlpha = 1.f;
	float YawFrom = 0.f;
	float YawTo = 0.f;
	float YawAlpha = 1.f;
	bool bYawBusy = false;
};

/** 首班车。导演让它沿接通的轨道开。 */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeTrackTrain : public AActor
{
	GENERATED_BODY()

public:
	ASlimeTrackTrain();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Track")
	TObjectPtr<UStaticMeshComponent> Mesh;

	virtual void BeginPlay() override;
};

/** 分裂出去的小团。只有它能走进检修通道。由导演生成，不放进关卡。 */
UCLASS()
class SLIMEFABLE_API ASlimeTrackBlob : public AActor
{
	GENERATED_BODY()

public:
	ASlimeTrackBlob();

	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;
};
