#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SlimeMiniGameDirector.generated.h"

class ACharacter;
class APlayerController;
class UActorComponent;

/**
 * Per-map mini-game rules. Story sublevels always run SlimePlayGameMode, so each year map
 * drops one director that switches the slime into its mode on BeginPlay and back on EndPlay.
 */
UCLASS(Abstract, meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeMiniGameDirector : public AActor
{
	GENERATED_BODY()

public:
	ASlimeMiniGameDirector();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Quest",
		meta = (ToolTip = "任务书里的章 Id，等于年份，如 1949。完成小游戏时推进这一章。"))
	FName ChapterId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Quest",
		meta = (ToolTip = "任务书里该章主线的 QuestId，如 Run。"))
	FName QuestId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Quest",
		meta = (ToolTip = "小游戏通关时推进的分支 Id，如 Arrive。该分支满后本章完成并回博物馆。"))
	FName FinishBranchId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Player",
		meta = (ToolTip = "进入小游戏时停用的玩家组件类名（不带 U 前缀），如 SlimeAbilityComponent、SlimeClingComponent。离开时恢复。"))
	TArray<FName> DisabledComponentClasses;

	UFUNCTION(BlueprintPure, Category = "MiniGame")
	ACharacter* GetPlayerCharacter() const { return Player.Get(); }

	UFUNCTION(BlueprintPure, Category = "MiniGame")
	bool IsModeActive() const { return bModeActive; }

	UFUNCTION(BlueprintPure, Category = "MiniGame")
	bool IsFinished() const { return bFinished; }

	/** 1–3, from the week the player entered this chapter with. */
	UFUNCTION(BlueprintPure, Category = "MiniGame")
	int32 GetWeekIndex() const;

	/** Report the finish branch to the quest system; the chapter then completes and returns to the museum. */
	UFUNCTION(BlueprintCallable, Category = "MiniGame")
	void FinishMiniGame();

	/** Restart the chapter through the normal death flow. */
	UFUNCTION(BlueprintCallable, Category = "MiniGame")
	void FailMiniGame();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void EnterMode(ACharacter* Character, APlayerController* PC);
	virtual void ExitMode(ACharacter* Character, APlayerController* PC);

	APlayerController* GetPlayerController() const;

private:
	friend class FSlimeArchiveRuntimeTest;
	void TryEnter();

	TWeakObjectPtr<ACharacter> Player;
	TArray<TWeakObjectPtr<UActorComponent>> DisabledComponents;
	FTimerHandle EnterHandle;
	int32 EnterAttempts = 0;
	bool bModeActive = false;
	bool bFinished = false;
};
