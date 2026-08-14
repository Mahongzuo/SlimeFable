// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SlimeFable : ModuleRules
{
	public SlimeFable(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"GameplayTags",
			"UMG",
			"Slate",
			"SlateCore",
			"ProceduralMeshComponent",
			"Niagara",
			"MoviePlayer",
			"MediaAssets"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"SlimeFable",
			"SlimeFable/DayLevel",
			"SlimeFable/Slime",
			"SlimeFable/Combat",
			"SlimeFable/Inventory",
			"SlimeFable/Settings",
			"SlimeFable/UI",
			"SlimeFable/Variant_Platforming",
			"SlimeFable/Variant_Platforming/Animation",
			"SlimeFable/Variant_Combat",
			"SlimeFable/Variant_Combat/AI",
			"SlimeFable/Variant_Combat/Animation",
			"SlimeFable/Variant_Combat/Gameplay",
			"SlimeFable/Variant_Combat/Interfaces",
			"SlimeFable/Variant_Combat/UI",
			"SlimeFable/Variant_SideScrolling",
			"SlimeFable/Variant_SideScrolling/AI",
			"SlimeFable/Variant_SideScrolling/Gameplay",
			"SlimeFable/Variant_SideScrolling/Interfaces",
			"SlimeFable/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
