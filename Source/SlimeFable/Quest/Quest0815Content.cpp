#include "Quest/Quest0815Content.h"
#include "Quest/QuestInteractActor.h"
#include "Quest/QuestReachVolume.h"
#include "Quest/QuestObjectiveComponent.h"
#include "Quest/QuestChapterGate.h"
#include "Quest/SlimeElementLock.h"
#include "Quest/SlimeSplitPad.h"
#include "Quest/SlimeReactionHearth.h"
#include "Enemy/EnemyFighter.h"
#include "Enemy/EnemyTower.h"
#include "Enemy/EnemyCombatTypes.h"
#include "SlimeFable.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Misc/PackageName.h"

namespace
{
	FString ShortMapName(const UWorld* World)
	{
		if (!World)
		{
			return FString();
		}
		FString Name = World->GetMapName();
		const FString Prefix = World->StreamingLevelsPrefix;
		if (!Prefix.IsEmpty() && Name.StartsWith(Prefix))
		{
			Name.RightChopInline(Prefix.Len());
		}
		return FPackageName::GetShortName(Name);
	}

	const FName TagSpawned(TEXT("Quest0815Spawned"));
	const FName TagHub(TEXT("Quest0815Hub"));
	const FName TagYear(TEXT("Quest0815Year"));

	bool HasHub(UWorld* World)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (IsValid(*It) && It->ActorHasTag(TagHub))
			{
				return true;
			}
		}
		return false;
	}

	void ClearYearActors(UWorld* World)
	{
		TArray<AActor*> ToDestroy;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (IsValid(*It) && It->ActorHasTag(TagYear))
			{
				ToDestroy.Add(*It);
			}
		}
		for (AActor* Actor : ToDestroy)
		{
			Actor->Destroy();
		}
	}

	void MarkHub(AActor* Actor)
	{
		if (Actor)
		{
			Actor->Tags.AddUnique(TagSpawned);
			Actor->Tags.AddUnique(TagHub);
		}
	}

	void MarkYear(AActor* Actor)
	{
		if (Actor)
		{
			Actor->Tags.AddUnique(TagSpawned);
			Actor->Tags.AddUnique(TagYear);
		}
	}

	FVector ResolveHubOrigin(UWorld* World)
	{
		if (APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0))
		{
			FVector Loc = Player->GetActorLocation();
			Loc.Z = FMath::Max(Loc.Z, 80.f);
			return Loc;
		}
		return FVector(0.f, 0.f, 80.f);
	}

	FVector ResolveYearOrigin(UWorld* World)
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (IsValid(*It) && It->ActorHasTag(TagHub))
			{
				return It->GetActorLocation() + FVector(1400.f, 0.f, 0.f);
			}
		}
		return ResolveHubOrigin(World) + FVector(1400.f, 0.f, 0.f);
	}

	UClass* LoadOr(const TCHAR* Path, UClass* Fallback)
	{
		if (UClass* Loaded = LoadClass<AActor>(nullptr, Path))
		{
			return Loaded;
		}
		return Fallback;
	}

	FActorSpawnParameters MakeParams()
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		return Params;
	}

	AQuestInteractActor* SpawnPickup(UWorld* World, const FVector& Loc, FName Chapter, FName Quest, FName Branch, const TCHAR* Verb, const FLinearColor& Color, float Scale = 0.4f, bool bHub = false)
	{
		AQuestInteractActor* Actor = World->SpawnActor<AQuestInteractActor>(
			AQuestInteractActor::StaticClass(), Loc, FRotator::ZeroRotator, MakeParams());
		if (Actor)
		{
			Actor->Configure(Chapter, Quest, Branch, FText::FromString(Verb), Color, Scale);
			if (Chapter.IsNone() && Actor->Objective)
			{
				Actor->Objective->SetConsumed(true);
			}
			if (bHub)
			{
				MarkHub(Actor);
			}
			else
			{
				MarkYear(Actor);
			}
		}
		return Actor;
	}

	void AttachObjective(AActor* Actor, FName Chapter, FName Quest, FName Branch, const TCHAR* Verb)
	{
		if (!Actor)
		{
			return;
		}
		UQuestObjectiveComponent* Objective = Actor->FindComponentByClass<UQuestObjectiveComponent>();
		if (!Objective)
		{
			Objective = NewObject<UQuestObjectiveComponent>(Actor, TEXT("Objective"));
			Objective->RegisterComponent();
		}
		Objective->ChapterId = Chapter;
		Objective->QuestId = Quest;
		Objective->BranchId = Branch;
		Objective->PromptVerb = FText::FromString(Verb);
	}

	FEnemyMoveDef MakeMelee(FName Id, const TCHAR* Name, float MinR, float MaxR, float Damage, float Windup, float Dash = 0.f, bool bGap = false, FName Next = NAME_None)
	{
		FEnemyMoveDef Move;
		Move.MoveId = Id;
		Move.MinRange = MinR;
		Move.MaxRange = MaxR;
		Move.Weight = 1.f;
		Move.TelegraphTime = Windup;
		Move.Cooldown = 1.6f;
		Move.bGapCloser = bGap;
		Move.NextMoveId = Next;
		Move.Skill.DisplayName = FText::FromString(Name);
		Move.Skill.Exec = Dash > 0.f && bGap ? EEnemySkillExec::Dash : EEnemySkillExec::Melee;
		Move.Skill.Hit.Shape = ESlimeHitShape::Cone;
		Move.Skill.Hit.Range = 180.f;
		Move.Skill.Hit.ConeHalfAngle = 40.f;
		Move.Skill.Windup = Windup;
		Move.Skill.HitStart = Windup + 0.05f;
		Move.Skill.HitEnd = Windup + 0.18f;
		Move.Skill.Recovery = 0.4f;
		Move.Skill.Damage = Damage;
		Move.Skill.DashDistance = Dash;
		return Move;
	}

	void ConfigureSamurai(AEnemyFighter* Fighter)
	{
		if (!Fighter)
		{
			return;
		}
		Fighter->DisplayName = FText::FromString(TEXT("武士"));
		Fighter->MaxHP = 180.f;
		Fighter->PreferredDistance = 220.f;
		Fighter->Moves.Reset();
		Fighter->Moves.Add(MakeMelee(TEXT("Iai"), TEXT("居合"), 80.f, 420.f, 22.f, 0.55f, 360.f, false));
		FEnemyMoveDef Combo = MakeMelee(TEXT("Combo"), TEXT("二连斩"), 0.f, 280.f, 16.f, 0.22f, 0.f, false, TEXT("Combo2"));
		Fighter->Moves.Add(Combo);
		Fighter->Moves.Add(MakeMelee(TEXT("Combo2"), TEXT("二连斩·二"), 0.f, 260.f, 18.f, 0.16f));
		Fighter->Moves.Add(MakeMelee(TEXT("Dash"), TEXT("踏込斩"), 200.f, 700.f, 18.f, 0.28f, 480.f, true));
	}

	void ConfigureEmperor(AEnemyFighter* Fighter)
	{
		if (!Fighter)
		{
			return;
		}
		Fighter->DisplayName = FText::FromString(TEXT("天皇"));
		Fighter->MaxHP = 420.f;
		Fighter->PreferredDistance = 260.f;
		Fighter->Moves.Reset();
		Fighter->Moves.Add(MakeMelee(TEXT("GuardSlash"), TEXT("禁卫斩"), 0.f, 320.f, 28.f, 0.4f));
		FEnemyMoveDef Bolt = MakeMelee(TEXT("EdictBolt"), TEXT("诏弹"), 200.f, 1200.f, 16.f, 0.35f);
		Bolt.Skill.Exec = EEnemySkillExec::Projectile;
		Bolt.Skill.Hit.Shape = ESlimeHitShape::ProjectileSweep;
		Bolt.Skill.ProjectileSpeed = 900.f;
		Bolt.Skill.HomingTurnRate = 9.f;
		Fighter->Moves.Add(Bolt);
		FEnemyMoveDef Shock = MakeMelee(TEXT("EdictShock"), TEXT("御诏冲击"), 0.f, 360.f, 32.f, 0.7f);
		Shock.Skill.Exec = EEnemySkillExec::AoE;
		Shock.Skill.Hit.Shape = ESlimeHitShape::Sphere;
		Shock.Skill.Hit.Radius = 280.f;
		Shock.Skill.LaunchZ = 420.f;
		Fighter->Moves.Add(Shock);
	}

	void ConfigureGunner(AEnemyTower* Tower)
	{
		if (!Tower)
		{
			return;
		}
		Tower->DisplayName = FText::FromString(TEXT("机枪手"));
		Tower->MaxHP = 220.f;
		Tower->AttackRange = 1400.f;
		Tower->FireInterval = 0.12f;
		Tower->FireMode = EEnemyTowerFireMode::Projectile;
		Tower->MissileSkill = EnemyCombat::MakeDefaultMissileSkill();
		Tower->MissileSkill.DisplayName = FText::FromString(TEXT("点射"));
		Tower->MissileSkill.Damage = 6.f;
		Tower->MissileSkill.ProjectileSpeed = 1800.f;
	}

	void SpawnHub(UWorld* World, const FVector& Origin, const FVector& Forward, const FVector& Right)
	{
		const TArray<FName> Years = { TEXT("1920"), TEXT("1945"), TEXT("1962"), TEXT("1985"), TEXT("2004"), TEXT("2021") };
		for (int32 Index = 0; Index < Years.Num(); ++Index)
		{
			const float Angle = (Index / 6.f) * 2.f * PI;
			const FVector Loc = Origin + Forward * 500.f + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * 420.f + FVector(0.f, 0.f, 40.f);
			AQuestChapterGate* Gate = World->SpawnActor<AQuestChapterGate>(
				LoadOr(TEXT("/Game/_Slime/Quest/Actors/BP_QuestChapterGate.BP_QuestChapterGate_C"), AQuestChapterGate::StaticClass()),
				Loc, FRotator::ZeroRotator, MakeParams());
			if (Gate)
			{
				Gate->TargetChapterId = Years[Index];
				Gate->DayId = FName(TEXT("0815"));
				Gate->Configure(NAME_None, NAME_None, NAME_None, FText::FromString(TEXT("进入")), FLinearColor(0.55f, 0.42f, 0.22f, 1.f), 0.9f);
				MarkHub(Gate);
			}
		}

		SpawnPickup(World, Origin + Forward * 180.f + FVector(0.f, 0.f, 40.f),
			FName(TEXT("2026")), FName(TEXT("Today")), FName(TEXT("Write")),
			TEXT("写下今天"), FLinearColor(0.78f, 0.62f, 0.28f, 1.f), 0.7f, true);
	}

	void Spawn1920(UWorld* World, const FVector& Origin, const FVector& Forward, const FVector& Right)
	{
		const FName Chapter(TEXT("1920"));
		const FName Quest(TEXT("Print"));
		SpawnPickup(World, Origin + Forward * 380.f - Right * 180.f, Chapter, Quest, TEXT("Manuscripts"), TEXT("拾取"), FLinearColor(0.82f, 0.62f, 0.28f), 0.4f);
		SpawnPickup(World, Origin + Forward * 380.f + Right * 180.f, Chapter, Quest, TEXT("Manuscripts"), TEXT("拾取"), FLinearColor(0.82f, 0.62f, 0.28f), 0.4f);

		AActor* Door = SpawnPickup(World, Origin + Forward * 920.f, NAME_None, NAME_None, NAME_None, TEXT(""), FLinearColor(0.25f, 0.2f, 0.14f), 1.2f);
		ASlimeSplitPad* PadA = World->SpawnActor<ASlimeSplitPad>(
			LoadOr(TEXT("/Game/_Slime/Quest/Actors/BP_SlimeSplitPad.BP_SlimeSplitPad_C"), ASlimeSplitPad::StaticClass()),
			Origin + Forward * 700.f - Right * 160.f, FRotator::ZeroRotator, MakeParams());
		ASlimeSplitPad* PadB = World->SpawnActor<ASlimeSplitPad>(
			LoadOr(TEXT("/Game/_Slime/Quest/Actors/BP_SlimeSplitPad.BP_SlimeSplitPad_C"), ASlimeSplitPad::StaticClass()),
			Origin + Forward * 700.f + Right * 160.f, FRotator::ZeroRotator, MakeParams());
		if (PadA && PadB)
		{
			PadA->PartnerPad = PadB;
			PadB->PartnerPad = PadA;
			PadA->DoorActor = Door;
			PadB->DoorActor = Door;
			PadA->bLatchFragment = true;
			PadB->bLatchFragment = true;
			MarkYear(PadA);
			MarkYear(PadB);
		}
		SpawnPickup(World, Origin + Forward * 1080.f, Chapter, Quest, TEXT("Manuscripts"), TEXT("拾取"), FLinearColor(0.82f, 0.62f, 0.28f), 0.4f);

		ASlimeElementLock* Lock = World->SpawnActor<ASlimeElementLock>(
			LoadOr(TEXT("/Game/_Slime/Quest/Actors/BP_SlimeElementLock.BP_SlimeElementLock_C"), ASlimeElementLock::StaticClass()),
			Origin + Forward * 1280.f, FRotator::ZeroRotator, MakeParams());
		if (Lock)
		{
			Lock->RequiredElement = ESlimeElement::Water;
			Lock->Configure(NAME_None, NAME_None, NAME_None, FText::FromString(TEXT("开印")), FLinearColor(0.25f, 0.45f, 0.7f), 0.8f);
			MarkYear(Lock);
		}

		SpawnPickup(World, Origin + Forward * 1500.f, Chapter, Quest, TEXT("Editor"), TEXT("交给陈编"), FLinearColor(0.55f, 0.38f, 0.2f), 0.7f);
		SpawnPickup(World, Origin + Forward * 1780.f - Right * 200.f, Chapter, Quest, TEXT("Workers"), TEXT("交给工人"), FLinearColor(0.45f, 0.4f, 0.28f), 0.55f);
		SpawnPickup(World, Origin + Forward * 1780.f, Chapter, Quest, TEXT("Workers"), TEXT("交给工人"), FLinearColor(0.45f, 0.4f, 0.28f), 0.55f);
		SpawnPickup(World, Origin + Forward * 1780.f + Right * 200.f, Chapter, Quest, TEXT("Workers"), TEXT("交给工人"), FLinearColor(0.45f, 0.4f, 0.28f), 0.55f);

		SpawnPickup(World, Origin + Forward * 520.f + Right * 360.f, Chapter, TEXT("Souvenir1920"), TEXT("Pick"), TEXT("拾取"), FLinearColor(0.7f, 0.55f, 0.2f), 0.35f);

		AEnemyFighter* Foreman = World->SpawnActor<AEnemyFighter>(
			AEnemyFighter::StaticClass(), Origin + Forward * 1600.f + Right * 380.f, FRotator::ZeroRotator, MakeParams());
		if (Foreman)
		{
			Foreman->DisplayName = FText::FromString(TEXT("工头"));
			Foreman->MaxHP = 140.f;
			MarkYear(Foreman);
		}
	}

	void Spawn1945(UWorld* World, const FVector& Origin, const FVector& Forward, const FVector& Right)
	{
		const FName Chapter(TEXT("1945"));
		const FName Quest(TEXT("Broadcast"));

		AEnemyFighter* Samurai = World->SpawnActor<AEnemyFighter>(
			LoadOr(TEXT("/Game/_Slime/Days/08/0815/Y1945/Enemies/BP_0815_Samurai.BP_0815_Samurai_C"), AEnemyFighter::StaticClass()),
			Origin + Forward * 700.f - Right * 80.f, FRotator::ZeroRotator, MakeParams());
		ConfigureSamurai(Samurai);
		AttachObjective(Samurai, Chapter, Quest, TEXT("Samurai"), TEXT("打败"));
		MarkYear(Samurai);

		AEnemyTower* Gunner = World->SpawnActor<AEnemyTower>(
			LoadOr(TEXT("/Game/_Slime/Days/08/0815/Y1945/Enemies/BP_0815_Gunner.BP_0815_Gunner_C"), AEnemyTower::StaticClass()),
			Origin + Forward * 1100.f + Right * 220.f, FRotator::ZeroRotator, MakeParams());
		ConfigureGunner(Gunner);
		AttachObjective(Gunner, Chapter, Quest, TEXT("Gunner"), TEXT("打败"));
		MarkYear(Gunner);

		AEnemyFighter* Emperor = World->SpawnActor<AEnemyFighter>(
			LoadOr(TEXT("/Game/_Slime/Days/08/0815/Y1945/Enemies/BP_0815_Emperor.BP_0815_Emperor_C"), AEnemyFighter::StaticClass()),
			Origin + Forward * 1500.f, FRotator::ZeroRotator, MakeParams());
		ConfigureEmperor(Emperor);
		AttachObjective(Emperor, Chapter, Quest, TEXT("Emperor"), TEXT("打败"));
		MarkYear(Emperor);

		SpawnPickup(World, Origin + Forward * 1750.f, Chapter, Quest, TEXT("Play"), TEXT("按下播放"), FLinearColor(0.7f, 0.2f, 0.18f), 0.65f);
		SpawnPickup(World, Origin + Forward * 480.f + Right * 360.f, Chapter, TEXT("Souvenir1945"), TEXT("Pick"), TEXT("拾取"), FLinearColor(0.55f, 0.45f, 0.25f), 0.35f);
	}

	void Spawn1962(UWorld* World, const FVector& Origin, const FVector& Forward, const FVector& Right)
	{
		const FName Chapter(TEXT("1962"));
		const FName Quest(TEXT("Help"));
		SpawnPickup(World, Origin + Forward * 420.f - Right * 220.f, Chapter, Quest, TEXT("Neighbors"), TEXT("帮助"), FLinearColor(0.55f, 0.35f, 0.22f), 0.5f);
		SpawnPickup(World, Origin + Forward * 520.f, Chapter, Quest, TEXT("Neighbors"), TEXT("帮助"), FLinearColor(0.55f, 0.35f, 0.22f), 0.5f);
		SpawnPickup(World, Origin + Forward * 420.f + Right * 220.f, Chapter, Quest, TEXT("Neighbors"), TEXT("帮助"), FLinearColor(0.55f, 0.35f, 0.22f), 0.5f);
		SpawnPickup(World, Origin + Forward * 980.f, Chapter, Quest, TEXT("Spirit"), TEXT("交谈"), FLinearColor(0.75f, 0.25f, 0.2f), 0.7f);
		SpawnPickup(World, Origin + Forward * 300.f + Right * 360.f, Chapter, TEXT("Souvenir1962"), TEXT("Pick"), TEXT("拾取"), FLinearColor(0.6f, 0.5f, 0.3f), 0.35f);

		AActor* Crate = SpawnPickup(World, Origin + Forward * 780.f, NAME_None, NAME_None, NAME_None, TEXT(""), FLinearColor(0.3f, 0.22f, 0.14f), 1.1f);
		ASlimeSplitPad* PadA = World->SpawnActor<ASlimeSplitPad>(ASlimeSplitPad::StaticClass(), Origin + Forward * 640.f - Right * 140.f, FRotator::ZeroRotator, MakeParams());
		ASlimeSplitPad* PadB = World->SpawnActor<ASlimeSplitPad>(ASlimeSplitPad::StaticClass(), Origin + Forward * 640.f + Right * 140.f, FRotator::ZeroRotator, MakeParams());
		if (PadA && PadB)
		{
			PadA->PartnerPad = PadB;
			PadB->PartnerPad = PadA;
			PadA->DoorActor = Crate;
			PadB->DoorActor = Crate;
			MarkYear(PadA);
			MarkYear(PadB);
		}
	}

	void Spawn1985(UWorld* World, const FVector& Origin, const FVector& Forward, const FVector& Right)
	{
		const FName Chapter(TEXT("1985"));
		const FName Quest(TEXT("Peace"));
		for (int32 Index = 0; Index < 6; ++Index)
		{
			const FVector Loc = Origin + Forward * (360.f + Index * 70.f) + Right * ((Index % 2 == 0) ? -160.f : 160.f);
			SpawnPickup(World, Loc, Chapter, Quest, TEXT("Flowers"), TEXT("拾取"), FLinearColor(0.92f, 0.9f, 0.85f), 0.28f);
		}
		SpawnPickup(World, Origin + Forward * 900.f, Chapter, Quest, TEXT("Altar"), TEXT("安放"), FLinearColor(0.4f, 0.35f, 0.28f), 0.8f);
		SpawnPickup(World, Origin + Forward * 1150.f, Chapter, Quest, TEXT("Bell"), TEXT("敲钟"), FLinearColor(0.72f, 0.62f, 0.28f), 0.7f);
		SpawnPickup(World, Origin + Forward * 280.f + Right * 360.f, Chapter, TEXT("Souvenir1985"), TEXT("Pick"), TEXT("拾取"), FLinearColor(0.65f, 0.55f, 0.3f), 0.35f);

		ASlimeElementLock* Lamp = World->SpawnActor<ASlimeElementLock>(ASlimeElementLock::StaticClass(), Origin + Forward * 780.f + Right * 240.f, FRotator::ZeroRotator, MakeParams());
		if (Lamp)
		{
			Lamp->RequiredElement = ESlimeElement::Fire;
			Lamp->Configure(NAME_None, NAME_None, NAME_None, FText::FromString(TEXT("点灯")), FLinearColor(0.8f, 0.35f, 0.12f), 0.5f);
			MarkYear(Lamp);
		}
	}

	void Spawn2004(UWorld* World, const FVector& Origin, const FVector& Forward, const FVector& Right)
	{
		const FName Chapter(TEXT("2004"));
		const FName Quest(TEXT("Cards"));
		SpawnPickup(World, Origin + Forward * 380.f - Right * 160.f, Chapter, Quest, TEXT("Papers"), TEXT("拾取"), FLinearColor(0.85f, 0.82f, 0.7f), 0.35f);
		SpawnPickup(World, Origin + Forward * 380.f + Right * 160.f, Chapter, Quest, TEXT("Papers"), TEXT("拾取"), FLinearColor(0.85f, 0.82f, 0.7f), 0.35f);

		AActor* CopierDoor = SpawnPickup(World, Origin + Forward * 720.f, NAME_None, NAME_None, NAME_None, TEXT(""), FLinearColor(0.2f, 0.28f, 0.32f), 1.0f);
		ASlimeReactionHearth* Hearth = World->SpawnActor<ASlimeReactionHearth>(
			LoadOr(TEXT("/Game/_Slime/Quest/Actors/BP_SlimeReactionHearth.BP_SlimeReactionHearth_C"), ASlimeReactionHearth::StaticClass()),
			Origin + Forward * 600.f, FRotator::ZeroRotator, MakeParams());
		if (Hearth)
		{
			Hearth->FirstElement = ESlimeElement::Water;
			Hearth->SecondElement = ESlimeElement::Lightning;
			Hearth->DoorActor = CopierDoor;
			MarkYear(Hearth);
		}

		SpawnPickup(World, Origin + Forward * 980.f, Chapter, Quest, TEXT("Officer"), TEXT("盖章"), FLinearColor(0.3f, 0.4f, 0.5f), 0.7f);
		SpawnPickup(World, Origin + Forward * 1280.f - Right * 180.f, Chapter, Quest, TEXT("Deliver"), TEXT("交卡"), FLinearColor(0.35f, 0.55f, 0.4f), 0.5f);
		SpawnPickup(World, Origin + Forward * 1280.f + Right * 180.f, Chapter, Quest, TEXT("Deliver"), TEXT("交卡"), FLinearColor(0.35f, 0.55f, 0.4f), 0.5f);
		SpawnPickup(World, Origin + Forward * 300.f + Right * 360.f, Chapter, TEXT("Souvenir2004"), TEXT("Pick"), TEXT("拾取"), FLinearColor(0.4f, 0.6f, 0.45f), 0.35f);
	}

	void Spawn2021(UWorld* World, const FVector& Origin, const FVector& Forward, const FVector& Right)
	{
		const FName Chapter(TEXT("2021"));
		const FName Quest(TEXT("Airlift"));
		SpawnPickup(World, Origin + Forward * 360.f - Right * 140.f, Chapter, Quest, TEXT("Docs"), TEXT("拾取"), FLinearColor(0.75f, 0.7f, 0.55f), 0.35f);
		SpawnPickup(World, Origin + Forward * 360.f + Right * 140.f, Chapter, Quest, TEXT("Docs"), TEXT("拾取"), FLinearColor(0.75f, 0.7f, 0.55f), 0.35f);

		AActor* GateDoor = SpawnPickup(World, Origin + Forward * 780.f, NAME_None, NAME_None, NAME_None, TEXT(""), FLinearColor(0.2f, 0.2f, 0.22f), 1.3f);
		ASlimeSplitPad* PadA = World->SpawnActor<ASlimeSplitPad>(ASlimeSplitPad::StaticClass(), Origin + Forward * 640.f - Right * 150.f, FRotator::ZeroRotator, MakeParams());
		ASlimeSplitPad* PadB = World->SpawnActor<ASlimeSplitPad>(ASlimeSplitPad::StaticClass(), Origin + Forward * 640.f + Right * 150.f, FRotator::ZeroRotator, MakeParams());
		if (PadA && PadB)
		{
			PadA->PartnerPad = PadB;
			PadB->PartnerPad = PadA;
			PadA->DoorActor = GateDoor;
			PadB->DoorActor = GateDoor;
			MarkYear(PadA);
			MarkYear(PadB);
		}

		AQuestReachVolume* Gate = World->SpawnActor<AQuestReachVolume>(
			AQuestReachVolume::StaticClass(), Origin + Forward * 1100.f, FRotator::ZeroRotator, MakeParams());
		if (Gate)
		{
			Gate->Configure(Chapter, Quest, FName(TEXT("Gate")), FVector(220.f, 220.f, 140.f));
			MarkYear(Gate);
		}
		SpawnPickup(World, Origin + Forward * 1380.f, Chapter, Quest, TEXT("Board"), TEXT("登机"), FLinearColor(0.45f, 0.4f, 0.28f), 0.6f);
		SpawnPickup(World, Origin + Forward * 280.f + Right * 360.f, Chapter, TEXT("Souvenir2021"), TEXT("Pick"), TEXT("拾取"), FLinearColor(0.55f, 0.5f, 0.3f), 0.35f);
	}
}

void FQuest0815Content::EnterChapter(UWorld* World, FName ChapterId)
{
	if (!World || ChapterId.IsNone())
	{
		return;
	}

	ClearYearActors(World);

	const FVector Origin = ResolveYearOrigin(World);
	const FVector Forward = FVector::ForwardVector;
	const FVector Right = FVector::RightVector;

	if (ChapterId == FName(TEXT("1920")))
	{
		Spawn1920(World, Origin, Forward, Right);
	}
	else if (ChapterId == FName(TEXT("1945")))
	{
		Spawn1945(World, Origin, Forward, Right);
	}
	else if (ChapterId == FName(TEXT("1962")))
	{
		Spawn1962(World, Origin, Forward, Right);
	}
	else if (ChapterId == FName(TEXT("1985")))
	{
		Spawn1985(World, Origin, Forward, Right);
	}
	else if (ChapterId == FName(TEXT("2004")))
	{
		Spawn2004(World, Origin, Forward, Right);
	}
	else if (ChapterId == FName(TEXT("2021")))
	{
		Spawn2021(World, Origin, Forward, Right);
	}

	UE_LOG(LogSlimeFable, Log, TEXT("Quest0815: Entered chapter %s on %s"),
		*ChapterId.ToString(), *ShortMapName(World));
}

void FQuest0815Content::SpawnForWorld(UWorld* World, FName ChapterId)
{
	if (!World)
	{
		return;
	}

	if (!HasHub(World))
	{
		const FVector Origin = ResolveHubOrigin(World);
		APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
		const FVector Forward = Player ? Player->GetActorForwardVector() : FVector::ForwardVector;
		const FVector Right = Player ? Player->GetActorRightVector() : FVector::RightVector;
		SpawnHub(World, Origin, Forward, Right);
	}

	if (!ChapterId.IsNone() && ChapterId != FName(TEXT("2026")))
	{
		EnterChapter(World, ChapterId);
	}

	UE_LOG(LogSlimeFable, Log, TEXT("Quest0815: Spawned hub/year actors for %s chapter %s"),
		*ShortMapName(World), *ChapterId.ToString());
}
