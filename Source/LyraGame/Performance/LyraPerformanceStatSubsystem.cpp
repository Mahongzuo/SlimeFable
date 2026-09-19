// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraPerformanceStatSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "GameModes/LyraGameState.h"
#include "Performance/LyraPerformanceStatTypes.h"
#include "Performance/LatencyMarkerModule.h"
#include "ProfilingDebugging/CsvProfiler.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraPerformanceStatSubsystem)

CSV_DEFINE_CATEGORY(LyraPerformance, /*bIsEnabledByDefault=*/false);

class FSubsystemCollectionBase;

//////////////////////////////////////////////////////////////////////
// FLyraPerformanceStatCache

void FLyraPerformanceStatCache::StartCharting()
{
}

void FLyraPerformanceStatCache::ProcessFrame(const FFrameData& FrameData)
{
	// Record stats about the frame data
	{
		RecordStat(
			ELyraDisplayablePerformanceStat::ClientFPS,
			(FrameData.TrueDeltaSeconds != 0.0) ?
			1.0 / FrameData.TrueDeltaSeconds :
			0.0);

		RecordStat(ELyraDisplayablePerformanceStat::IdleTime, FrameData.IdleSeconds);
		RecordStat(ELyraDisplayablePerformanceStat::FrameTime, FrameData.TrueDeltaSeconds);
		RecordStat(ELyraDisplayablePerformanceStat::FrameTime_GameThread, FrameData.GameThreadTimeSeconds);
		RecordStat(ELyraDisplayablePerformanceStat::FrameTime_RenderThread, FrameData.RenderThreadTimeSeconds);
		RecordStat(ELyraDisplayablePerformanceStat::FrameTime_RHIThread, FrameData.RHIThreadTimeSeconds);
		RecordStat(ELyraDisplayablePerformanceStat::FrameTime_GPU, FrameData.GPUTimeSeconds);	
	}

	if (UWorld* World = MySubsystem->GetGameInstance()->GetWorld())
	{
		// Record some networking related stats
		if (const ALyraGameState* GameState = World->GetGameState<ALyraGameState>())
		{
			RecordStat(ELyraDisplayablePerformanceStat::ServerFPS, GameState->GetServerFPS());
		}

		if (APlayerController* LocalPC = GEngine->GetFirstLocalPlayerController(World))
		{
			if (APlayerState* PS = LocalPC->GetPlayerState<APlayerState>())
			{
				RecordStat(ELyraDisplayablePerformanceStat::Ping, PS->GetPingInMilliseconds());
			}

			if (UNetConnection* NetConnection = LocalPC->GetNetConnection())
			{
				const UNetConnection::FNetConnectionPacketLoss& InLoss = NetConnection->GetInLossPercentage();
				RecordStat(ELyraDisplayablePerformanceStat::PacketLoss_Incoming, InLoss.GetAvgLossPercentage());
				
				const UNetConnection::FNetConnectionPacketLoss& OutLoss = NetConnection->GetOutLossPercentage();
				RecordStat(ELyraDisplayablePerformanceStat::PacketLoss_Outgoing, OutLoss.GetAvgLossPercentage());
				
				RecordStat(ELyraDisplayablePerformanceStat::PacketRate_Incoming, NetConnection->InPacketsPerSecond);
				RecordStat(ELyraDisplayablePerformanceStat::PacketRate_Outgoing, NetConnection->OutPacketsPerSecond);

				RecordStat(ELyraDisplayablePerformanceStat::PacketSize_Incoming, (NetConnection->InPacketsPerSecond != 0) ? (NetConnection->InBytesPerSecond / (float)NetConnection->InPacketsPerSecond) : 0.0f);
				RecordStat(ELyraDisplayablePerformanceStat::PacketSize_Outgoing, (NetConnection->OutPacketsPerSecond != 0) ? (NetConnection->OutBytesPerSecond / (float)NetConnection->OutPacketsPerSecond) : 0.0f);
			}
			
			// Finally, record some input latency related stats if they are enabled
			TArray<ILatencyMarkerModule*> LatencyMarkerModules = IModularFeatures::Get().GetModularFeatureImplementations<ILatencyMarkerModule>(ILatencyMarkerModule::GetModularFeatureName());
			for (ILatencyMarkerModule* LatencyMarkerModule : LatencyMarkerModules)
			{
				if (LatencyMarkerModule->GetEnabled())
				{
					const float TotalLatencyMs = LatencyMarkerModule->GetTotalLatencyInMs();
					if (TotalLatencyMs > 0.0f)
					{
						// Record some stats about the latency of the game
						RecordStat(ELyraDisplayablePerformanceStat::Latency_Total, TotalLatencyMs);
						RecordStat(ELyraDisplayablePerformanceStat::Latency_Game, LatencyMarkerModule->GetGameLatencyInMs());
						RecordStat(ELyraDisplayablePerformanceStat::Latency_Render, LatencyMarkerModule->GetRenderLatencyInMs());

						// Record some CSV profile stats.
						// You can see these by using the following commands
						// Start and stop the profile:
						//	CsvProfile Start
						//	CsvProfile Stop
						//
						// Or, you can profile for a certain number of frames:
						// CsvProfile Frames=10
						//
						// And this will output a .csv file to the Saved\Profiling\CSV folder
#if CSV_PROFILER
						if (FCsvProfiler* Profiler = FCsvProfiler::Get())
						{
							static const FName TotalLatencyStatName = TEXT("Lyra_Latency_Total");
							Profiler->RecordCustomStat(TotalLatencyStatName, CSV_CATEGORY_INDEX(LyraPerformance), TotalLatencyMs, ECsvCustomStatOp::Set);

							static const FName GameLatencyStatName = TEXT("Lyra_Latency_Game");
							Profiler->RecordCustomStat(GameLatencyStatName, CSV_CATEGORY_INDEX(LyraPerformance), LatencyMarkerModule->GetGameLatencyInMs(), ECsvCustomStatOp::Set);

							static const FName RenderLatencyStatName = TEXT("Lyra_Latency_Render");
							Profiler->RecordCustomStat(RenderLatencyStatName, CSV_CATEGORY_INDEX(LyraPerformance), LatencyMarkerModule->GetRenderLatencyInMs(), ECsvCustomStatOp::Set);
						}
#endif

						// Some more fine grain latency numbers can be found on the marker module if desired
						//LatencyMarkerModule->GetRenderLatencyInMs()));
						//LatencyMarkerModule->GetDriverLatencyInMs()));
						//LatencyMarkerModule->GetOSRenderQueueLatencyInMs()));
						//LatencyMarkerModule->GetGPURenderLatencyInMs()));
						break;	
					}
				}
			}
		}
	}
}

void FLyraPerformanceStatCache::StopCharting()
{
}

void FLyraPerformanceStatCache::RecordStat(const ELyraDisplayablePerformanceStat Stat, const double Value)
{
	PerfStateCache.FindOrAdd(Stat).RecordSample(Value);
}

double FLyraPerformanceStatCache::GetCachedStat(ELyraDisplayablePerformanceStat Stat) const
{
	static_assert((int32)ELyraDisplayablePerformanceStat::Count == 18, "Need to update this function to deal with new performance stats");

	if (const FSampledStatCache* Cache = GetCachedStatData(Stat))
	{
		return Cache->GetLastCachedStat();
	}

	return 0.0;
}

const FSampledStatCache* FLyraPerformanceStatCache::GetCachedStatData(const ELyraDisplayablePerformanceStat Stat) const
{
	static_assert((int32)ELyraDisplayablePerformanceStat::Count == 18, "Need to update this function to deal with new performance stats");
	
	return PerfStateCache.Find(Stat);
}

//////////////////////////////////////////////////////////////////////
// ULyraPerformanceStatSubsystem

void ULyraPerformanceStatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Tracker = MakeShared<FLyraPerformanceStatCache>(this);
	GEngine->AddPerformanceDataConsumer(Tracker);
}

void ULyraPerformanceStatSubsystem::Deinitialize()
{
	GEngine->RemovePerformanceDataConsumer(Tracker);
	Tracker.Reset();
}

double ULyraPerformanceStatSubsystem::GetCachedStat(ELyraDisplayablePerformanceStat Stat) const
{
	return Tracker->GetCachedStat(Stat);
}

const FSampledStatCache* ULyraPerformanceStatSubsystem::GetCachedStatData(const ELyraDisplayablePerformanceStat Stat) const
{
	return Tracker->GetCachedStatData(Stat);
}

