# Slime Basic Attack VFX Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task with verification checkpoints.

**Goal:** Replace the six slime variants' normal-attack Ribbon trail with a directional Mesh blade, expose a stable six-element Niagara parameter contract, and add combo-only hit-point and enemy-color feedback.

**Architecture:** Keep `USlimeSwordTrailComponent` as the only runtime VFX owner. A shared slash Niagara system is configured by canonical User Parameters and spawned at the resolved attack direction; `USlimeHitProbe` returns confirmed hit records so the same owner can spawn one impact system per target. `AEnemyCharacter` receives an optional element color for the existing overlay pulse, while all other damage paths retain their current behavior.

**Tech Stack:** Unreal Engine 5.8, C++ runtime module, Niagara Toolset/MCP editor operations, Unreal Editor Python for idempotent validation, UE automation tests.

---

## File Map

Runtime files:

- Create `Source/SlimeFable/Combat/SlimeCombatVfxTypes.h` for pure, testable element/combo parameter policy.
- Create `Source/SlimeFable/Combat/Tests/SlimeCombatVfxTests.cpp` for automation coverage.
- Modify `Source/SlimeFable/Combat/SlimeSwordTrailComponent.h/.cpp` to own Mesh slash and impact instances.
- Modify `Source/SlimeFable/Combat/SlimeCombatComponent.cpp` to pass the policy values, collect combo hit records, and dispatch feedback only for confirmed combo hits.
- Modify `Source/SlimeFable/Combat/SlimeHitProbe.cpp` to resolve a useful collision point without changing damage eligibility.
- Modify `Source/SlimeFable/Enemy/EnemyCharacter.h/.cpp` to accept an optional overlay color and write `HitColor`.

Asset/validation files:

- Create or update `Content/Python/create_slime_basic_attack_vfx.py` as an idempotent asset existence/parameter validation runner; it must not use unavailable emitter-handle reflection. This script and all `.uasset` files are intentionally local-only because the repository ignores `/Content/`; verify them in the shared workspace but do not include them in Git commits.
- Create `/Game/Characters/Slime/FX/Skills/Meshes/SM_Slime_Combo_Crescent` by duplicating `/Game/RPGEffects/Meshes/SM_Swipe`.
- Create `/Game/Characters/Slime/FX/Skills/Materials/M_Slime_Combo_Blade` by duplicating `/Game/RPGEffects/Materials/M_Swipe` and wiring the canonical color/core controls.
- Create/update `/Game/Characters/Slime/FX/Skills/NS_Slime_Combo_Slash` through `NiagaraToolset_System` using the dedicated mesh/material and no Ribbon renderer.
- Create `/Game/Characters/Slime/FX/Skills/NS_Slime_Combo_Impact` through the same toolset with the one-shot flash and element-gated secondary emitters.
- Modify `Config/DefaultGame.ini` only if the new dedicated paths are not already covered by the existing `/Game/Characters/Slime/FX/Skills` and `/Game/_Slime/FX` cook rules.

---

### Task 1: Define and Test the VFX Parameter Policy

**Files:**
- Create: `Source/SlimeFable/Combat/Tests/SlimeCombatVfxTests.cpp`
- Create: `Source/SlimeFable/Combat/SlimeCombatVfxTypes.h`

- [ ] **Step 1: Write the failing automation test first**

Add a test that uses the intended policy API before the header exists:

```cpp
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
```

- [ ] **Step 2: Run the focused test/build and confirm the expected red failure**

Run:

```powershell
& "D:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" SlimeFableEditor Win64 Development -Project="E:\UE\SlimeFable\SlimeFable.uproject" -WaitMutex
```

Expected: compilation fails because `SlimeCombatVfxTypes.h` and its policy functions do not exist yet. Do not treat unrelated pre-existing worktree edits as part of this failure.

- [ ] **Step 3: Add the minimal policy implementation**

Create the header with inline, dependency-free functions:

```cpp
#pragma once

#include "SlimeCombatTypes.h"

namespace SlimeCombatVfx
{
    FORCEINLINE int32 ElementId(ESlimeElement Element)
    {
        return FMath::Clamp(static_cast<int32>(Element), 0, SlimeElement::Count - 1);
    }

    FORCEINLINE int32 ComboIndex(ESlimeSkillSlot Slot)
    {
        return FMath::Clamp(static_cast<int32>(Slot), 0, 3);
    }

    FORCEINLINE float SwingSign(ESlimeSkillSlot Slot)
    {
        return (ComboIndex(Slot) % 2) == 0 ? 1.f : -1.f;
    }

    FORCEINLINE float ComboScale(ESlimeSkillSlot Slot)
    {
        return Slot == ESlimeSkillSlot::Combo4 ? 1.35f : 1.f;
    }

    FORCEINLINE bool IsComboHitVfx(ESlimeSkillSlot Slot)
    {
        return SlimeCombat::IsComboSlot(Slot);
    }
}
```

- [ ] **Step 4: Re-run the test/build and verify green**

Run the same UBT command, then run the editor automation filter:

```powershell
& "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "E:\UE\SlimeFable\SlimeFable.uproject" -unattended -nop4 -nullrhi -nosound -ExecCmds="Automation RunTests SlimeFable.Combat.Vfx.PolicyMapsElementsAndComboStages; Quit"
```

Expected: compile exit code `0`; automation reports one passing test and no failures.

- [ ] **Step 5: Commit the policy/test slice**

```powershell
git add Source/SlimeFable/Combat/SlimeCombatVfxTypes.h Source/SlimeFable/Combat/Tests/SlimeCombatVfxTests.cpp
git commit -m "test: define slime combo vfx policy"
```

---

### Task 2: Replace Ribbon Ownership with Directional Blade Ownership

**Files:**
- Modify: `Source/SlimeFable/Combat/SlimeSwordTrailComponent.h`
- Modify: `Source/SlimeFable/Combat/SlimeSwordTrailComponent.cpp`

- [ ] **Step 1: Replace the public component contract**

Change the public calls to:

```cpp
UFUNCTION(BlueprintCallable, Category = "Combat")
void PlayComboSwing(ESlimeSkillSlot ComboSlot, float Duration, const FVector& Forward,
    ESlimeElement Element, FLinearColor ElementColor);

UFUNCTION(BlueprintCallable, Category = "Combat")
void PlayImpact(const FVector& Location, const FVector& Normal,
    ESlimeElement Element, FLinearColor ElementColor, float Scale = 1.f);
```

Rename the `RibbonFx`/`EnsureRibbon`/`KillRibbon` state to `BladeFx`/`EnsureBlade`/`KillBlade`; remove all fallback paths to `NS_SimpleRibbonTrail` and `NS_Jump_Trail`.

- [ ] **Step 2: Add designer-facing config with Chinese tooltips**

Use `/Game/Characters/Slime/FX/Skills/NS_Slime_Combo_Slash` and `/Game/Characters/Slime/FX/Skills/NS_Slime_Combo_Impact` as defaults. Keep the fields under `0_Config|Sword`, add `ImpactOffset`, `SlashLength`, `SlashWidth`, `CoreIntensity`, `SecondaryIntensity`, and a `Combo4Scale` defaulting to `1.35f`. Every new `EditAnywhere` field gets a Chinese tooltip describing units, default, and linkage.

- [ ] **Step 3: Implement one-shot blade spawning and parameter writes**

Spawn at the owner blob center plus the forward offset with `SpawnSystemAtLocation`. Set the component rotation from the flattened normalized `Forward`, use the policy swing sign for mirrored yaw, and write only:

```cpp
BladeFx->SetVariableLinearColor(TEXT("User.ElementColor"), ElementColor);
BladeFx->SetVariableInt(TEXT("User.ElementId"), SlimeCombatVfx::ElementId(Element));
BladeFx->SetVariableInt(TEXT("User.ComboIndex"), SlimeCombatVfx::ComboIndex(ComboSlot));
BladeFx->SetVariableFloat(TEXT("User.ComboScale"), SlimeCombatVfx::ComboScale(ComboSlot));
BladeFx->SetVariableFloat(TEXT("User.SlashLength"), SlashLength);
BladeFx->SetVariableFloat(TEXT("User.SlashWidth"), SlashWidth);
BladeFx->SetVariableFloat(TEXT("User.CoreIntensity"), CoreIntensity);
BladeFx->SetVariableFloat(TEXT("User.SecondaryIntensity"), SecondaryIntensity);
BladeFx->SetVariableVec3(TEXT("User.Forward"), Forward);
```

Set `AutoDestroy` true and schedule a weak-pointer hard cleanup at the configured lifetime. `StopSwing` must immediately deactivate/destroy the active component.

- [ ] **Step 4: Implement one-shot impact spawning**

Spawn the impact system at `Location + Normal * ImpactOffset`, rotate it so its local forward points along `-Normal`, set `User.ElementColor`, `User.ElementId`, `User.ImpactScale`, `User.ImpactNormal`, and `User.CoreIntensity`, then let it auto-destroy under a hard lifetime.

- [ ] **Step 5: Build and run the policy test**

Run the documented UBT command and the focused automation command from Task 1. Expected: exit code `0`, policy test passes, and no C++ compile errors.

---

### Task 3: Collect Real Combo Hits and Scope Feedback

**Files:**
- Modify: `Source/SlimeFable/Combat/SlimeCombatComponent.cpp`
- Modify: `Source/SlimeFable/Combat/SlimeHitProbe.cpp`
- Modify: `Source/SlimeFable/Enemy/EnemyCharacter.h`
- Modify: `Source/SlimeFable/Enemy/EnemyCharacter.cpp`

- [ ] **Step 1: Add a closest-point helper to `USlimeHitProbe`**

In `PerformHit`, after overlap filtering and before `ApplyToActor`, resolve `HitLocation` using `Overlap.GetComponent()->GetClosestPointOnCollision(Origin, Closest)` when it returns a valid point; otherwise use the target visual bounds center for `AEnemyCharacter`, then actor location. Keep `OutHits` population and damage calls unchanged for all existing callers.

- [ ] **Step 2: Make `FireHit` collect only combo results**

For non-projectile, non-chain combo actions, call:

```cpp
TArray<FSlimeHitResult> ComboHits;
const bool bComboHitFeedback = Cast<ASlimeCharacter>(GetOwner())
    && SlimeCombatVfx::IsComboHitVfx(ActiveDef.Slot);
Hits = USlimeHitProbe::PerformHit(
    GetOwner(), HitDef, Origin, ActiveForward, AlreadyHit, Restrict,
    bComboHitFeedback ? &ComboHits : nullptr);
```

After `PerformHit`, iterate `ComboHits` and call the sword component's `PlayImpact` once per result. The existing `AwardResources` path remains based on `Hits`. Skills, projectiles, chains, and non-slime owners must not call `PlayImpact`.

- [ ] **Step 3: Pass element ID/color into slash start**

Update the existing `StartAction` sword call to pass `Element->CurrentElement` plus its canonical color. Do not change attack timing or hit shapes.

- [ ] **Step 4: Add an optional colored enemy flash**

Keep the existing no-argument enemy API and add a separate element-colored entry point:

```cpp
UFUNCTION(BlueprintCallable, Category = "Combat")
void PlayHitFlash();

UFUNCTION(BlueprintCallable, Category = "Combat")
void PlayElementHitFlash(FLinearColor FlashColor);
```

Keep `PlayHitFlash()` behavior unchanged for generic damage, with its existing default red/white material behavior. Implement `PlayElementHitFlash` through the same pulse timer but set `HitColor` before starting it. For each combo hit result whose actor is `AEnemyCharacter`, call `PlayElementHitFlash(CurrentElementColor)` immediately after the damage probe returns.

- [ ] **Step 5: Write and run a regression check**

Extend `SlimeCombatVfxTests.cpp` with a pure eligibility assertion that `Skill1`, `Skill2`, and `Skill3` are false while all four combo slots are true. Run the focused automation command and verify all assertions pass.

- [ ] **Step 6: Commit the runtime slice**

```powershell
git add Source/SlimeFable/Combat/SlimeSwordTrailComponent.h Source/SlimeFable/Combat/SlimeSwordTrailComponent.cpp Source/SlimeFable/Combat/SlimeCombatComponent.cpp Source/SlimeFable/Combat/SlimeHitProbe.cpp Source/SlimeFable/Enemy/EnemyCharacter.h Source/SlimeFable/Enemy/EnemyCharacter.cpp Source/SlimeFable/Combat/Tests/SlimeCombatVfxTests.cpp
git commit -m "feat: drive slime combo blade and hit feedback"
```

---

### Task 4: Create Dedicated Blade Mesh and Material Assets

**Files/assets:**
- Create: `/Game/Characters/Slime/FX/Skills/Meshes/SM_Slime_Combo_Crescent`
- Create: `/Game/Characters/Slime/FX/Skills/Materials/M_Slime_Combo_Blade`
- Create/modify: `Content/Python/create_slime_basic_attack_vfx.py`

- [ ] **Step 1: Duplicate and isolate the existing swipe mesh/material**

Use the editor asset tools to duplicate `SM_Swipe` and `M_Swipe` into the dedicated paths. Keep the mesh pivot at its attack origin, set a shallow crescent silhouette, and make the material unlit/additive, two-sided, Niagara Mesh compatible, with vector parameters `ElementColor` and `CoreColor` and scalar parameters `CoreIntensity`, `Opacity`, and `EdgePower`.

- [ ] **Step 2: Add the idempotent validator script**

The script must load both dedicated assets, assert the two Niagara systems exist, and print one `[slime-basic-vfx]` line per asset/parameter. It must not inspect emitter handles. Its failure mode is a non-zero exception when a required asset or canonical parameter is missing.

- [ ] **Step 3: Run the validator before Niagara authoring**

Run:

```powershell
& "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "E:\UE\SlimeFable\SlimeFable.uproject" -ExecutePythonScript="E:/UE/SlimeFable/Content/Python/create_slime_basic_attack_vfx.py" -unattended -nop4 -nullrhi -nosound
```

Expected at this checkpoint: the script reports the two mesh/material assets present and explicitly reports the Niagara systems as pending/missing; it must not silently succeed.

---

### Task 5: Author Slash and Impact Niagara Systems with Toolset

**Assets:**
- `/Game/Characters/Slime/FX/Skills/NS_Slime_Combo_Slash`
- `/Game/Characters/Slime/FX/Skills/NS_Slime_Combo_Impact`

- [ ] **Step 1: Inspect the template and schemas through serial MCP calls**

Start the GUI editor, execute `ModelContextProtocol.StartServer 8010`, then use `NiagaraToolset_System.GetSystemSummary`, `GetEmitterTopology`, `GetRendererSchema`, and `GetUserVariables` on the current `NS_Slime_Combo_Slash`. Record the actual emitter references and renderer property names before mutation.

- [ ] **Step 2: Build the slash system from a valid template**

Use `CreateNiagaraSystem` or duplicate the existing dedicated system, remove any Ribbon renderer, add one Mesh renderer using `SM_Slime_Combo_Crescent`, and configure a one-shot burst with fixed bounds. Add/update all nine slash User Variables with `AddUserVariables`; wire `ElementColor`, `ElementId`, `ComboIndex`, and intensity/size values into the particle/material parameter modules. Keep the system CPU/simple and disable collision, lights, loops, and persistent simulation.

- [ ] **Step 3: Build the impact system**

Create the impact system from a valid burst template. Add a central Mesh/Sprite flash, short directional streaks, and one low-count secondary emitter whose branch reads `User.ElementId`. Add the five impact User Variables and fixed bounds. Use the same `ElementColor` material contract.

- [ ] **Step 4: Compile and inspect after every structural mutation**

Call `GetSystemCompileState` and `GetStackIssues` after each system is edited. Apply only concrete fix IDs returned by the toolset. Do not claim asset completion while any Niagara stack error remains.

- [ ] **Step 5: Run the validator after authoring**

Update the validator's expected assets and run the headless command from Task 4. Expected: both systems load, all canonical User Variables are listed, slash renderer classes contain `UNiagaraMeshRendererProperties` and contain no Ribbon renderer, and compile state has no errors.

- [ ] **Step 6: Verify the local-only asset/script slice without committing ignored files**

```powershell
git status --ignored --short --untracked-files=all -- Content/Python Content/Characters/Slime/FX/Skills Config/DefaultGame.ini
```

Expected: the dedicated `.uasset` files and validation script exist in the workspace. Because `/Content/` is ignored by project policy, do not force-add them and do not add unrelated generated cache files. Commit only a necessary `Config/DefaultGame.ini` change if a new cook directory is required.

---

### Task 6: Full Verification and Visual QA

**Files:**
- No new production files; use the focused test, validator, UBT, and editor session.

- [ ] **Step 1: Run the complete editor build**

```powershell
& "D:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" SlimeFableEditor Win64 Development -Project="E:\UE\SlimeFable\SlimeFable.uproject" -WaitMutex
```

Expected: exit code `0` and no new compile errors.

- [ ] **Step 2: Run focused and full automation**

Run the focused VFX test command, then:

```powershell
& "D:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "E:\UE\SlimeFable\SlimeFable.uproject" -unattended -nop4 -nullrhi -nosound -ExecCmds="Automation RunTests SlimeFable; Quit"
```

Expected: both commands exit `0`; report any pre-existing unrelated failures separately.

- [ ] **Step 3: Run the asset validator one final time**

Expected: all dedicated assets and canonical parameters pass; no missing asset, Ribbon renderer, or Niagara stack error is reported.

- [ ] **Step 4: Run editor visual QA**

Launch the GUI editor and test a combat scene with all six elements. Verify:

- empty-space attacks show a broad forward-facing Mesh blade and never spawn impact feedback;
- Combo 1/3 and 2 mirror directions, Combo 4 is visibly larger and has cross-light;
- Water/Wind/Fire/Lightning/Dark/Physical secondary particles are distinct but share the blade silhouette;
- confirmed hits spawn one impact at each target point and flash the enemy in the current element color;
- skills, projectiles, reactions, enemy attacks, and whiffs do not spawn the combo impact system;
- interrupted attacks leave no active slash instance.

- [ ] **Step 5: Capture output evidence and inspect the final diff**

Use the editor Output Log and `git diff --check`; inspect `git status --short` to ensure only planned files/assets changed. Do not revert or stage unrelated user edits.

- [ ] **Step 6: Commit verification metadata only if needed**

Do not commit screenshots, logs, cached asset registries, or generated temporary files. Commit only a necessary final source/config adjustment with a focused message.

---

## Self-Review

- Spec coverage: Tasks 1-3 cover the parameter contract, direction, combo progression, hit scoping, hit location, and colored enemy flash. Tasks 4-5 cover the dedicated mesh/material, slash/impact Niagara systems, canonical User Variables, fixed bounds, no Ribbon, and cooking. Task 6 covers build, tests, asset validation, and six-element visual QA. Content assets/scripts are explicitly local-only per repository ignore policy.
- Placeholder scan: no `TBD`, `TODO`, or unspecified implementation steps are used; each mutation names a file, asset, API, or command.
- Type consistency: the policy API names are consistent across test, component plan, and FireHit plan: `ElementId`, `ComboIndex`, `SwingSign`, `ComboScale`, and `IsComboHitVfx`.
- Scope check: no skill, projectile, reaction, enemy attack, or damage-balance behavior is included.
