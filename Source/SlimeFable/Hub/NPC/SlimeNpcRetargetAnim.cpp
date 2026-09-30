#include "SlimeNpcRetargetAnim.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_RetargetPoseFromMesh.h"

struct FSlimeNpcRetargetProxy : FAnimInstanceProxy
{
	FAnimNode_RetargetPoseFromMesh Retarget;
	explicit FSlimeNpcRetargetProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
	virtual FAnimNode_Base* GetCustomRootNode() override { return &Retarget; }
	virtual void PreUpdate(UAnimInstance* Instance, float Dt) override
	{
		FAnimInstanceProxy::PreUpdate(Instance, Dt);
		Retarget.IKRetargeterAsset = CastChecked<USlimeNpcRetargetAnim>(Instance)->Retargeter;
		Retarget.PreUpdate(Instance);
	}
};

FAnimInstanceProxy* USlimeNpcRetargetAnim::CreateAnimInstanceProxy() { return new FSlimeNpcRetargetProxy(this); }
void USlimeNpcRetargetAnim::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
