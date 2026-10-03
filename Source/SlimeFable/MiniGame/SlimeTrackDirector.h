#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "MiniGame/SlimeMiniGameDirector.h"
#include "SlimeTrackDirector.generated.h"

class ACameraActor;
class APointLight;
class ASlimeTrackBlob;
class ASlimeTrackBoard;
class ASlimeTrackPiece;
class ASlimeTrackTrain;
class USlimeTrackHUDWidget;

/** 1969 俯视推轨。五屏区间，接通后首班车开过，最后一屏穿隧道到北京站。 */
UCLASS(meta = (PrioritizeCategories = "0_Config"))
class SLIMEFABLE_API ASlimeTrackDirector : public ASlimeMiniGameDirector
{
	GENERATED_BODY()

public:
	ASlimeTrackDirector();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Rules",
		meta = (ToolTip = "第 3 周目全章限时，秒。超时按失败重来本章。前两个周目不限时。"))
	float Week3TimeLimit = 240.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Rules",
		meta = (ToolTip = "史莱姆跳一格用的时间，秒。"))
	float HopSeconds = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Rules",
		meta = (ToolTip = "换到下一屏时机位混合用的时间，秒。"))
	float CameraBlendSeconds = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Finale",
		meta = (ToolTip = "第五屏接通后，车头视角穿隧道的时长，秒。"))
	float FinaleSeconds = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Finale",
		meta = (ToolTip = "结尾横幅的上排小字。"))
	FText FinaleKicker;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "0_Config|Finale",
		meta = (ToolTip = "结尾横幅正文。史实：北京地铁一期从苹果园到北京站。"))
	FText FinaleText;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void EnterMode(ACharacter* Character, APlayerController* PC) override;
	virtual void ExitMode(ACharacter* Character, APlayerController* PC) override;

private:
	friend class FSlimeTrackRulesTest;
	friend class FSlimeTrackReplayTest;
	struct FUndoState
	{
		int32 X, Y, BX, BY, Direction, StepCount;
		bool bBlobOut, bControlBlob;
		TArray<FIntVector> Pieces;
	};
	void SaveUndo();
	void Undo();
	void CycleTarget();
	ASlimeTrackPiece* RotationTarget() const;
	void QueueAction(int32 Action);
	TArray<int32> InputQueue;
	TArray<FUndoState> UndoHistory;
	enum class EPhase : uint8
	{
		Play,
		Ride,
		Finale
	};

	void Gather();
	void StartBoardIndex(int32 Index, bool bFromRide);
	void ResetBoard(bool bOutOfSteps);
	void TickPlay(float DeltaSeconds);
	void TickRide(float DeltaSeconds);
	void TickFinale(float DeltaSeconds);
	void UpdateCamera(float DeltaSeconds);
	void UpdateHUD();
	void ShowLine(const FText& Kicker, const FText& Line, float Seconds);

	bool Consume(const FKey& Key, bool& WasDown);
	int32 DirForKey(int32 KeyIndex) const;
	bool TryMove(int32 Dir);
	bool TryRotate();
	void TrySplit();
	void TrySwitch();
	void MoveActive(int32 X, int32 Y, int32 Dir);
	void BeginHop(AActor* Actor, const FVector& Dest);
	void AdvanceHop(float DeltaSeconds);
	void DestroyBlob();

	bool CanHoldPiece(int32 X, int32 Y) const;
	bool OtherOccupies(int32 X, int32 Y) const;
	bool CanBlobStand(int32 X, int32 Y) const;
	ASlimeTrackPiece* PieceAt(int32 X, int32 Y) const;
	ASlimeTrackBoard* CurrentBoard() const;
	bool Emits(int32 X, int32 Y, int32 Dir) const;
	bool Linked(int32 X, int32 Y, int32 Dir) const;
	TArray<FIntPoint> BuildPath() const;
	bool AnyBusy() const;
	int32 StepLimit() const;
	void BoardCamera(const ASlimeTrackBoard* Board, FVector& Location, FRotator& Rotation) const;
	void BlendCamera(const FVector& Location, const FRotator& Rotation, float Duration);
	void ParkTrain();
	void StartRide();
	void StartFinale();
	void DimLamps();
	void FlickerLamps();
	void RefreshHints();
	FVector DirVector(int32 Dir) const;

	EPhase Phase = EPhase::Play;
	int32 BoardIdx = 0;
	int32 Steps = 0;
	int32 SlimeX = 0;
	int32 SlimeY = 0;
	int32 BlobX = -1;
	int32 BlobY = -1;
	int32 Facing = 1;
	bool bBlobOut = false;
	bool bControlBlob = false;
	bool bPending = false;
	bool bHopping = false;
	bool bInputLocked = false;
	bool bPawnInputWasEnabled = false;
	bool bFinaleBanner = false;

	float HopT = 0.f;
	FVector HopFrom = FVector::ZeroVector;
	FVector HopTo = FVector::ZeroVector;
	TWeakObjectPtr<AActor> HopActor;

	float RideT = 0.f;
	float RideDur = 1.f;
	TArray<FVector> RidePoints;
	TArray<float> RideDistances;

	float FinaleT = 0.f;
	float TimeLeft = 0.f;
	FVector FinaleOrigin = FVector::ZeroVector;
	FVector FinaleForward = FVector(1.f, 0.f, 0.f);

	float CamAlpha = 1.f;
	float CamDur = 0.45f;
	FVector CamFrom = FVector::ZeroVector;
	FVector CamTo = FVector::ZeroVector;
	FRotator CamRotFrom = FRotator::ZeroRotator;
	FRotator CamRotTo = FRotator::ZeroRotator;

	bool WasW = false;
	bool WasA = false;
	bool WasS = false;
	bool WasD = false;
	bool WasF = false;
	bool WasQ = false;
	bool WasTab = false;
	bool WasR = false;
	bool WasE = false;
	bool WasZ = false;

	TArray<TObjectPtr<ASlimeTrackBoard>> Boards;
	TArray<TObjectPtr<ASlimeTrackPiece>> AllPieces;
	TObjectPtr<ASlimeTrackTrain> Train;
	TObjectPtr<ASlimeTrackBlob> Blob;
	TObjectPtr<ACameraActor> Cam;
	TObjectPtr<USlimeTrackHUDWidget> HUD;
	TWeakObjectPtr<AActor> PreviousView;
	TArray<TObjectPtr<APointLight>> Lamps;
	TArray<float> LampBase;
};
