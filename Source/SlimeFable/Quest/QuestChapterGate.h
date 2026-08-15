#pragma once

#include "CoreMinimal.h"
#include "Quest/QuestInteractActor.h"
#include "QuestChapterGate.generated.h"

UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API AQuestChapterGate : public AQuestInteractActor
{
	GENERATED_BODY()

public:
	AQuestChapterGate();

	virtual bool TryInteract(APawn* Interactor) override;
	virtual FText GetInteractPromptVerb() const override;
	virtual bool CanBeFocused() const override;

	UFUNCTION(BlueprintCallable, Category = "Quest")
	virtual bool RequestEnter(APawn* Interactor);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Gate",
		meta = (GetOptions = "GetTargetChapterIdOptions",
			ToolTip = "大厅：下拉选当天 Registry 里的年份/故事。关末回大厅：选 Hub。列表来自 DA_DayLevelRegistry 的 SubLevels。"))
	FName TargetChapterId;

	/** When true, travel uses the open day map (0815/0816), not the baked DayId. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Gate",
		meta = (ToolTip = "勾选后进门用当前打开的日关卡（0815/0816），不用手填日期。一般保持勾选。"))
	bool bUseHostDayId = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Gate",
		meta = (EditCondition = "!bUseHostDayId", EditConditionHides,
			ToolTip = "仅取消「用宿主日」时手填 MMDD，例如 0815。勾选宿主日时隐藏。"))
	FName DayId = FName(TEXT("0815"));

	UFUNCTION()
	TArray<FString> GetTargetChapterIdOptions() const;

protected:
	bool IsUnlocked() const;
	FName ResolveTravelDayId() const;
	FName ResolveOptionsDayId() const;
};
