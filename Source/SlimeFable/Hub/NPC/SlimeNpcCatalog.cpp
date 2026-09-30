#include "SlimeNpcCatalog.h"
#include "GameFramework/Pawn.h"

USlimeNpcCatalog* USlimeNpcCatalog::Load()
{
	return LoadObject<USlimeNpcCatalog>(nullptr, TEXT("/Game/_Slime/Hub/NPC/DA_NpcCatalog.DA_NpcCatalog"));
}

const FSlimeNpcSpecies* USlimeNpcCatalog::Find(FName Id) const
{
	return Species.FindByPredicate([Id](const FSlimeNpcSpecies& S) { return S.SpeciesId == Id; });
}

const FSlimeNpcSpecies* USlimeNpcCatalog::Match(UClass* EnemyClass) const
{
	// Ruin blueprints are children of the catalog classes. Soft paths sometimes omit _C.
	auto SameClass = [](const FSoftObjectPath& Soft, const UClass* Class)
	{
		if (!Class || !Soft.IsValid())
		{
			return false;
		}
		const FSoftObjectPath Live(Class);
		if (Soft == Live)
		{
			return true;
		}
		const FString SoftStr = Soft.ToString();
		const FString LiveStr = Live.ToString();
		return SoftStr + TEXT("_C") == LiveStr || LiveStr + TEXT("_C") == SoftStr;
	};
	for (UClass* Class = EnemyClass; Class; Class = Class->GetSuperClass())
	{
		for (const FSlimeNpcSpecies& S : Species)
		{
			for (const auto& Source : S.EnemyClasses)
			{
				if (SameClass(Source.ToSoftObjectPath(), Class))
				{
					return &S;
				}
			}
		}
	}
	return nullptr;
}
