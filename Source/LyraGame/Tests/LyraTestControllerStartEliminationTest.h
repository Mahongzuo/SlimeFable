// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "GauntletTestControllerBootTest.h"

#include "LyraTestControllerStartEliminationTest.generated.h"

UCLASS()
class ULyraTestControllerStartEliminationTest : public UGauntletTestControllerBootTest
{
	GENERATED_BODY()

protected:
	virtual bool IsBootProcessComplete() const override;
};
