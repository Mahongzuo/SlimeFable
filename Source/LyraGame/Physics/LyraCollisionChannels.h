// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


/**
 * when you modify this, please note that this information can be saved with instances
 * also DefaultEngine.ini [/Script/Engine.CollisionProfile] should match with this list
 *
 * SlimeFable: GameTraceChannel1-4 are already taken by GASP/Slime
 * (Traversable / FluidTrace / Obstacle / Slicable), so Lyra's channels are shifted to 5-9.
 * Must stay in sync with SlimeFable/Config/DefaultEngine.ini +DefaultChannelResponses.
 **/

// Trace against Actors/Components which provide interactions.
#define Lyra_TraceChannel_Interaction					ECC_GameTraceChannel5

// Trace used by weapons, will hit physics assets instead of capsules
#define Lyra_TraceChannel_Weapon						ECC_GameTraceChannel6

// Trace used by by weapons, will hit pawn capsules instead of physics assets
#define Lyra_TraceChannel_Weapon_Capsule				ECC_GameTraceChannel7

// Trace used by by weapons, will trace through multiple pawns rather than stopping on the first hit
#define Lyra_TraceChannel_Weapon_Multi					ECC_GameTraceChannel8

// Allocated to aim assist by the ShooterCore game feature (ShooterCoreRuntimeSettings.AimAssistCollisionChannel)
// ECC_GameTraceChannel9
