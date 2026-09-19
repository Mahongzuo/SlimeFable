// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraReplicationGraphSettings.h"
#include "Misc/App.h"
#include "System/LyraReplicationGraph.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraReplicationGraphSettings)

ULyraReplicationGraphSettings::ULyraReplicationGraphSettings()
{
	CategoryName = TEXT("Game");
	DefaultReplicationGraphClass = ULyraReplicationGraph::StaticClass();
}
