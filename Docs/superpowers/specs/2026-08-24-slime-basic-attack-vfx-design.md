# Slime Basic Attack VFX Design

Date: 2026-08-24

## Goal

Upgrade the six slime variants' normal-attack feedback from a generic Ribbon trail to a directional sword-light effect. The slash must face the resolved attack direction, expose a stable Niagara User Parameter contract for all six elements, and produce a matching hit reaction only when a slime normal attack actually hits a valid target.

## Scope

In scope:

- Combo slots `Combo1` through `Combo4` for Water, Wind, Fire, Lightning, Dark, and Physical slimes.
- One shared slash silhouette with element color and element-specific secondary particles.
- A separate hit-point Niagara effect and a short element-colored enemy overlay flash.
- Accurate per-hit placement, fixed lifetimes, cooking, asset verification, and runtime visual QA.

Out of scope:

- Skills, projectiles, elemental reactions, enemy attacks, and generic damage feedback.
- Combat damage, hit shape, targeting, combo timing, or movement balance changes.
- Six independently authored slash systems.

## Visual Direction

The reference is a broad crescent blade projected in front of the attacker rather than a thin historical trail. The effect has three layers:

1. A wide, readable crescent blade surface that expands quickly along the attack direction.
2. A white-hot core and edge highlight that remains legible for every element palette.
3. Lightweight element-specific secondary particles.

The slash is a one-shot Niagara Mesh renderer with explicit transform and lifetime. A shallow curved crescent mesh gives the blade stable three-dimensional direction from side and oblique camera angles. It must not contain a Ribbon renderer, camera-facing Sprite renderer, or sampled movement history.

Element treatments:

| Element ID | Element | Secondary treatment |
| --- | --- | --- |
| 0 | Water | Droplets and short curved splashes |
| 1 | Wind | Thin directional stream lines |
| 2 | Fire | Sparks and short-lived embers |
| 3 | Lightning | Small forked arcs |
| 4 | Dark | Angular fragments and restrained dark haze |
| 5 | Physical | Gold impact chips |

Combo rhythm:

- Combo 1: rightward slash, base scale.
- Combo 2: mirrored leftward slash, base scale.
- Combo 3: rightward slash, base scale.
- Combo 4: enlarged slash at `1.35` scale plus a short cross-light secondary blade.

Target lifetimes are approximately `0.18s` for Combo 1-3 and `0.24s` for Combo 4. Runtime code enforces a hard cleanup deadline.

## Asset Design

### Slash system

`/Game/Characters/Slime/FX/Skills/NS_Slime_Combo_Slash`

- Rebuild the existing dedicated asset as a custom one-shot blade system.
- Use a Mesh blade renderer, never Ribbon or camera-facing Sprite.
- Use fixed bounds sized for the Combo 4 variant.
- Do not use Niagara lights, collision, or persistent GPU simulation.
- Keep the white-hot core visually neutral; multiply the body and secondary emitters by element parameters.

Supporting assets:

- `/Game/Characters/Slime/FX/Skills/Meshes/SM_Slime_Combo_Crescent`: shallow curved crescent blade mesh with a forward-facing pivot and stable UVs.
- `/Game/Characters/Slime/FX/Skills/Materials/M_Slime_Combo_Blade`: additive unlit blade material with separate body, white-hot core, edge glow, and opacity controls.

### Impact system

`/Game/Characters/Slime/FX/Skills/NS_Slime_Combo_Impact`

- Spawn only for confirmed slime combo hits.
- Combine a central white flash, short directional streaks, and the selected element's secondary particles.
- Orient from the impact normal and use a fixed short lifetime.

### Enemy overlay

`/Game/_Slime/FX/M_EnemyHitFlash`

- Continue using the existing overlay material and pulse lifecycle.
- Ensure the material consumes `HitColor` as a vector parameter.
- Preserve the existing no-argument generic flash path for unrelated damage.
- Add a separate element-colored entry point used only by slime combo hits.

## Niagara User Parameter Contract

The slash system exposes:

| Parameter | Type | Meaning |
| --- | --- | --- |
| `User.ElementColor` | Linear Color | Primary element tint |
| `User.ElementId` | Integer | Water `0`, Wind `1`, Fire `2`, Lightning `3`, Dark `4`, Physical `5` |
| `User.ComboIndex` | Integer | Combo stage `0..3` |
| `User.ComboScale` | Float | Stage multiplier; Combo 4 defaults to `1.35` |
| `User.SlashLength` | Float | Blade length in effect-local units |
| `User.SlashWidth` | Float | Blade width in effect-local units |
| `User.CoreIntensity` | Float | White-hot core emissive multiplier |
| `User.SecondaryIntensity` | Float | Element particle spawn/intensity multiplier |
| `User.Forward` | Vector | Normalized world attack direction for emitter logic |

The impact system exposes:

| Parameter | Type | Meaning |
| --- | --- | --- |
| `User.ElementColor` | Linear Color | Primary element tint |
| `User.ElementId` | Integer | Same mapping as the slash system |
| `User.ImpactScale` | Float | Per-hit size multiplier |
| `User.ImpactNormal` | Vector | World-space normal facing away from the attack |
| `User.CoreIntensity` | Float | Central white flash emissive multiplier |

C++ writes only these canonical names. It will no longer guess multiple aliases such as `User.Color`, `User.Tint`, or unscoped parameter names.

## Runtime Integration

`USlimeSwordTrailComponent` remains the single runtime owner to avoid broad Blueprint or character changes. Its implementation changes from tracking a moving Ribbon to spawning one-shot slash and impact systems.

### Attack start

1. `USlimeCombatComponent::StartAction` verifies that the owner is an `ASlimeCharacter` and the slot is a combo slot.
2. It resolves the current element, canonical element color, combo index, and normalized attack direction.
3. It calls the slash component once.
4. The slash component places the effect in front of the slime, rotates it from the attack direction, applies mirrored stage orientation, writes all User Parameters, activates the system, and schedules hard cleanup.

### Confirmed hit

1. `USlimeCombatComponent::FireHit` requests `FSlimeHitResult` output only for slime combo attacks.
2. `USlimeHitProbe::PerformHit` keeps the existing damage flow but returns the closest valid collision point when the overlap component supports it. It falls back to target bounds center or actor location without changing damage behavior.
3. For each confirmed hostile living target, the combat component spawns `NS_Slime_Combo_Impact` at the returned point and writes the current element parameters.
4. If the target is `AEnemyCharacter`, the combat component calls the dedicated element-colored overlay flash entry point.
5. Whiffed attacks produce no impact system and no element-colored target flash.

The existing generic health-component flash behavior remains unchanged for skills and other damage. A combo hit immediately supplies the element color to the dedicated overlay path, so the special layered response remains scoped to normal attacks.

## C++ and Asset Changes

- Refactor `USlimeSwordTrailComponent` fields and private names from Ribbon terminology to blade/impact terminology while preserving the component's public class identity.
- Set the slash asset default to `NS_Slime_Combo_Slash`; remove example Ribbon and jump-trail fallbacks.
- Add a soft reference for `NS_Slime_Combo_Impact` and designer-facing size, offset, lifetime, and intensity settings under `0_Config`, each with Chinese tooltips.
- Pass `ESlimeElement` or its stable integer mapping into the component instead of passing only color.
- Collect combo `OutHits` in `FireHit` and spawn one impact per valid target.
- Add an element-colored enemy overlay entry point without changing the generic no-argument path.
- Update the idempotent editor asset script to build or validate both systems and their canonical exposed parameters.
- Ensure `/Game/Characters/Slime/FX/Skills` and `/Game/_Slime/FX` remain included in cooking through the existing configuration.

## Failure Handling

- Missing slash asset: log one clear warning, skip visual output, and leave combat functional.
- Missing impact asset: preserve damage and body flash; skip only the impact particles.
- Invalid element value: fall back to Physical ID and color.
- Missing collision point: use component bounds center, then actor location.
- Target destroyed during feedback: Niagara instances remain auto-destroying; weak references prevent callbacks from accessing invalid objects.
- Interrupted combo: immediately deactivate and destroy the active slash instance.

## Performance Budget

- At most one slash system per combo stage.
- At most one impact system per confirmed target for that stage.
- No Ribbon renderers, dynamic lights, Niagara collision, or persistent looping emitters.
- Fixed bounds and short hard lifetimes for both systems.
- Secondary particles use low counts and element-gated spawn so inactive element branches do not emit.

## Verification

### Automated and headless checks

- Add focused C++ automation coverage for element ID/color mapping, combo stage mirroring/scaling, and combo-only impact eligibility.
- Run the new tests red before implementation and green afterward.
- Run the idempotent asset script in `UnrealEditor-Cmd` and verify both assets load, save, and expose every canonical User Parameter.
- Verify the slash system contains no Ribbon renderer.
- Build `SlimeFableEditor Win64 Development` with the documented UBT command.

### Runtime visual QA

In an editor play session:

1. Cycle through all six slime elements.
2. Execute all four combo stages against empty space and confirm no impact feedback.
3. Execute all four stages against one enemy and confirm attack-facing blade orientation, alternating direction, Combo 4 scale/cross-light, per-element secondary particles, hit-point burst, and element-colored enemy flash.
4. Repeat with multiple nearby targets to confirm one impact per valid hit and no lingering systems.
5. Capture representative screenshots for all six elements and inspect the Output Log for missing assets or Niagara parameter warnings.

## Acceptance Criteria

- Every slime normal attack shows a broad directional blade instead of a Ribbon trail.
- Combo 1-3 alternate direction and Combo 4 is visibly larger with cross-light.
- All six elements share the blade silhouette while retaining distinct color and secondary particles.
- Only confirmed slime combo hits spawn the new impact Niagara and element-colored enemy flash.
- Whiffs, skills, projectiles, reactions, and enemy attacks do not spawn the new combo impact effect.
- Runtime assets cook correctly, C++ builds, focused tests pass, and no visual instance survives its hard lifetime.
