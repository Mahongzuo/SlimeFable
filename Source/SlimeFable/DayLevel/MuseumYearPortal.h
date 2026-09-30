#pragma once

#include "CoreMinimal.h"
#include "Quest/DayChapterPortal.h"
#include "MuseumYearPortal.generated.h"

/** Numbered, reusable physical slot; the selected date supplies its story. */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API AMuseumYearPortal : public ADayChapterPortal
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "0_Config|Museum", meta = (ClampMin = "1", ToolTip = "门位编号从 1 开始，必须唯一。当天 ChapterOrder 的第 N 项分配到 N 号门；增加门位即可扩容。"))
	int32 SlotNumber = 1;
};
