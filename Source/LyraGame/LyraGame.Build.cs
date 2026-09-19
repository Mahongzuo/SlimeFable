// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LyraGame : ModuleRules
{
	public LyraGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				"LyraGame"
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreOnline",
				"CoreUObject",
				"ApplicationCore",
				"Engine",
				"PhysicsCore",
				"GameplayTags",
				"GameplayTasks",
				"GameplayAbilities",
				"AIModule",
				"ModularGameplay",
				"ModularGameplayActors",
				"DataRegistry",
				"ReplicationGraph",
				"GameFeatures",
				"SignificanceManager",
				"CommonLoadingScreen",
				"Niagara",
				"AsyncMixin",
				"ControlFlows",
				"PropertyPath",
				"CommonGame",
				"CommonUser",
				"CommonUI"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"InputCore",
				"Slate",
				"SlateCore",
				"RenderCore",
				"DeveloperSettings",
				"EnhancedInput",
				"NetCore",
				"RHI",
				"Projects",
				"Gauntlet",
				"UMG",
				"CommonInput",
				"GameSettings",
				"GameSubtitles",
				"GameplayMessageRuntime",
				"AudioMixer",
				"NetworkReplayStreaming",
				"UIExtension",
				"AudioModulation",
				"EngineSettings",
				"DTLSHandlerComponent",
				"Json",
				"PlatformDLC",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
			}
		);

		// Generate compile errors if using DrawDebug functions in test/shipping builds.
		PublicDefinitions.Add("SHIPPING_DRAW_DEBUG_ERROR=1");

		// Installed engine: ExternalRpcRegistry / HTTPServer / ClientPilot / Hotfix are source-only.
		PublicDefinitions.Add("WITH_RPC_REGISTRY=0");
		PublicDefinitions.Add("WITH_HTTPSERVER_LISTENERS=0");
		PublicDefinitions.Add("WITH_AUTOMATION_DRIVER=0");
		PublicDefinitions.Add("UE_WITH_EXTERNAL_RPC=0");

		SetupGameplayDebuggerSupport(Target);
		SetupIrisSupport(Target);
	}
}
