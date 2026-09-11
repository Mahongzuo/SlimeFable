// Copyright Epic Games, Inc. All Rights Reserved.

#include "GaspCostumeEnemy.h"

AGaspCostumeEnemy::AGaspCostumeEnemy()
{
	CostumeKind = EGaspCostumeKind::Meituan;
}

void AGaspCostumeEnemy::EnsureMoveKit()
{
	Super::EnsureMoveKit();
}
