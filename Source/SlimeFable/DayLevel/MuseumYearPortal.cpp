#include "DayLevel/MuseumYearPortal.h"

void AMuseumYearPortal::BeginPlay()
{
	// Do not expose yesterday's editor assignment before the manager refreshes.
	SetPortalEnabled(false);
	Super::BeginPlay();
}
