#include "DayLevel/MuseumTravelLibrary.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

bool UMuseumTravelLibrary::FindSafeGround(UObject* Context, FVector Desired, FVector Reference, FVector& Ground)
{
	UWorld* World = Context ? Context->GetWorld() : nullptr;
	auto* Nav = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (!Nav) return false;
	FNavLocation Start;
	if (!Nav->ProjectPointToNavigation(Reference, Start, FVector(200,200,300))) return false;
	for (int32 Ring = 0; Ring <= 3; ++Ring)
		for (int32 I = 0; I < (Ring ? 8 : 1); ++I)
		{
			const float Angle = I * PI / 4.f;
			FNavLocation P;
			const FVector Candidate = Desired + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * Ring * 100.f;
			if (!Nav->ProjectPointToNavigation(Candidate, P, FVector(60,60,250))) continue;
			if (World->OverlapBlockingTestByChannel(P.Location + FVector(0,0,103), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(60,100))) continue;
			const auto* Path = Nav->FindPathToLocationSynchronously(World, Start.Location, P.Location);
			if (!Path || !Path->IsValid() || Path->IsPartial()) continue;
			Ground = P.Location;
			return true;
		}
	return false;
}
