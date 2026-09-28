// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SlimeFableEditor : ModuleRules
{
	public SlimeFableEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"SlimeFable",
			"AnimGraph",
			"AnimGraphRuntime",
			"BlueprintGraph"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"UnrealEd",
			"Slate",
			"SlateCore"
		});

		PublicIncludePaths.AddRange(new string[] {
			"SlimeFableEditor"
		});
	}
}
