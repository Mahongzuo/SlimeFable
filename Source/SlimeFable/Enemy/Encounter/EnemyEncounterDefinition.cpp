#include "EnemyEncounterDefinition.h"

const FEnemyEncounterPhaseDef* UEnemyEncounterDefinition::FindPhase(int32 PhaseIndex) const
{
	for (const FEnemyEncounterPhaseDef& Phase : Phases)
	{
		if (Phase.PhaseIndex == PhaseIndex)
		{
			return &Phase;
		}
	}
	return nullptr;
}
