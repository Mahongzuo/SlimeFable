#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SlimeNpcCatalog.generated.h"

class USkeletalMesh;
class UBlendSpace;
class UAnimInstance;
class UMaterialInterface;
class UTexture2D;
class UIKRetargeter;

/** Visual-only layers, ordered parent first. Never instantiate the source enemy. */
USTRUCT(BlueprintType)
struct FSlimeNpcVisualLayer
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ToolTip="外观骨骼网格，必填；不生成敌人角色。"))
	TSoftObjectPtr<USkeletalMesh> Mesh;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ToolTip="相对父层的变换；首层相对胶囊。默认单位变换。"))
	FTransform Transform;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ToolTip="父层索引，必须小于当前层；-1 表示胶囊。"))
	int32 ParentIndex = -1;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ToolTip="同骨架跟随层索引；默认-1独立播放。"))
	int32 LeaderIndex = -1;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ToolTip="默认显示；重定向源骨架关闭显示但继续刷新骨骼。"))
	bool bVisible = true;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ToolTip="材质槽覆盖，按原网格顺序；空槽沿用网格材质。"))
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ToolTip="待机/行走混合空间，横轴为速度厘米/秒；仅驱动源骨架。"))
	TSoftObjectPtr<UBlendSpace> Locomotion;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ToolTip="外观重定向资产；从父层源骨架取姿势，不运行原敌人动画蓝图逻辑。"))
	TSoftObjectPtr<UIKRetargeter> Retargeter;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ToolTip="原重定向组件标签，顺序保留；普通网格留空。"))
	TArray<FName> Tags;
};

USTRUCT(BlueprintType)
struct FSlimeNpcSpecies
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ToolTip="永久种类键，不随蓝图或日期改名；猪Pig、狗Dog共享各自额度。"))
	FName SpeciesId;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ToolTip="NPC目录中的中文名称。"))
	FText DisplayName;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ToolTip="动物每种最多5只，关闭则每角色1个。默认关闭。"))
	bool bAnimal = false;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ToolTip="仅猪狗开启靠近作物破坏，默认关闭。"))
	bool bDamagesCrops = false;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ToolTip="对应正式敌人蓝图及其派生类；最具体匹配优先。"))
	TArray<TSoftClassPtr<APawn>> EnemyClasses;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ToolTip="固定和平外观，各层必须按父层优先排列。"))
	TArray<FSlimeNpcVisualLayer> Layers;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ToolTip="目录图标，可留空显示名称。"))
	TSoftObjectPtr<UTexture2D> Icon;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ClampMin="10", ToolTip="胶囊半径厘米，默认34；与放置净空检查一致。"))
	float Radius = 34.f;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ClampMin="10", ToolTip="胶囊半高厘米，默认88，必须不小于半径。"))
	float HalfHeight = 88.f;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ClampMin="10", ToolTip="行走速度厘米/秒，默认150；匹配行走混合空间速度。"))
	float WalkSpeed = 150.f;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ClampMin="100", ToolTip="放置锚点周围闲逛半径厘米，默认1000（10米）。"))
	float WanderRadius = 1000.f;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ClampMin="0", ToolTip="距地块边缘的破坏距离厘米，默认100（1米），需无障碍。"))
	float CropDistance = 100.f;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ClampMin="0.1", ToolTip="连续靠近同一地块多久毁坏，默认3秒，离开重计。"))
	float CropDwellSeconds = 3.f;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ClampMin="0.1", ToolTip="每次破坏后冷却秒数，默认10秒；离线不推进。"))
	float CropCooldownSeconds = 10.f;
	int32 Limit() const { return bAnimal ? 5 : 1; }
};

UCLASS(BlueprintType)
class SLIMEFABLE_API USlimeNpcCatalog : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ToolTip="可收录NPC种类；SpeciesId必须唯一，按此顺序显示。"))
	TArray<FSlimeNpcSpecies> Species;
	UPROPERTY(EditAnywhere, Category="0_Config", meta=(ToolTip="正式探索地图白名单；年份地图自动读取DayLevelRegistry，测试图不加入。"))
	TArray<TSoftObjectPtr<UWorld>> ExplorationMaps;
	const FSlimeNpcSpecies* Find(FName Id) const;
	const FSlimeNpcSpecies* Match(UClass* EnemyClass) const;
	static USlimeNpcCatalog* Load();
	static FName EntryId(FName SpeciesId) { return FName(*(TEXT("NPC_") + SpeciesId.ToString())); }
};
