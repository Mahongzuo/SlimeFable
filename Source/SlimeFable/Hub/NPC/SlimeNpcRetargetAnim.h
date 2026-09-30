#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SlimeNpcRetargetAnim.generated.h"

class UIKRetargeter;
/** Visual-only retarget graph, with no dependency on the enemy's Blueprint owner. */
UCLASS(Transient)
class USlimeNpcRetargetAnim : public UAnimInstance
{
	GENERATED_BODY()
public:
	UPROPERTY(Transient) TObjectPtr<UIKRetargeter> Retargeter;
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
