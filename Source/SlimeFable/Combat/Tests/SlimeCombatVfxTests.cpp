// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SlimeCombatVfxTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSlimeCombatVfxPolicyTest,
	"SlimeFable.Combat.Vfx.PolicyMapsElementsAndComboStages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlimeCombatVfxPolicyTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Water uses element id 0"), SlimeCombatVfx::ElementId(ESlimeElement::Water), 0);
	TestEqual(TEXT("Physical uses element id 5"), SlimeCombatVfx::ElementId(ESlimeElement::Physical), 5);
	TestEqual(TEXT("Combo 1 is the first stage"), SlimeCombatVfx::ComboIndex(ESlimeSkillSlot::Combo1), 0);
	TestEqual(TEXT("Combo 4 is the last stage"), SlimeCombatVfx::ComboIndex(ESlimeSkillSlot::Combo4), 3);
	TestEqual(TEXT("Combo 1 and 3 sweep right"), SlimeCombatVfx::SwingSign(ESlimeSkillSlot::Combo1), 1.f);
	TestEqual(TEXT("Combo 2 sweeps left"), SlimeCombatVfx::SwingSign(ESlimeSkillSlot::Combo2), -1.f);
	TestEqual(TEXT("Combo 4 is enlarged"), SlimeCombatVfx::ComboScale(ESlimeSkillSlot::Combo4), 1.35f);
	TestFalse(TEXT("Skill 1 cannot emit combo hit feedback"),
		SlimeCombatVfx::IsComboHitVfx(ESlimeSkillSlot::Skill1));
	TestTrue(TEXT("Combo 3 can emit combo hit feedback"),
		SlimeCombatVfx::IsComboHitVfx(ESlimeSkillSlot::Combo3));
	return true;
}

#endif
