// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hub/HomeBuild/SlimeHomeFluidPad.h"

#include "Hub/HomeBuild/SlimeHomeBuildTypes.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodyInstance.h"
#include "SlimeFable.h"
#include "TimerManager.h"
#include "UnrealClient.h"

ASlimeHomeFluidPad::ASlimeHomeFluidPad()
{
	PrimaryActorTick.bCanEverTick = false;
	PadRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PadRoot"));
	SetRootComponent(PadRoot);
	PadRoot->SetMobility(EComponentMobility::Movable);
	AimQuery = CreateDefaultSubobject<UBoxComponent>(TEXT("AimQuery"));
	AimQuery->SetupAttachment(PadRoot);
	AimQuery->SetMobility(EComponentMobility::Movable);
	AimQuery->SetBoxExtent(FVector(200.f, 200.f, 2.f));
	AimQuery->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AimQuery->SetCollisionResponseToAllChannels(ECR_Ignore);
	AimQuery->SetGenerateOverlapEvents(false);
	AimQuery->SetHiddenInGame(true);

	Surface = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Surface"));
	Surface->SetupAttachment(PadRoot);
	Surface->SetMobility(EComponentMobility::Movable);
	Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Surface->SetGenerateOverlapEvents(false);
	Surface->SetCastShadow(false);
	Surface->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		Surface->SetStaticMesh(PlaneMesh.Object);
	}
}

namespace SlimeFluidLook
{
	struct FLook
	{
		const TCHAR* Token;
		/** Water boxes hang below their pivot; lift the preset until the box bottom rests on the floor. */
		bool bSitOnFloor;
		/** Drawn on the preset TraceMesh (tessellated). A one-quad stand-in cannot show MeshDistortion. */
		const TCHAR* SurfaceMaterial;
		float SurfaceLift;
		/** Centimeters. Zero keeps the blueprint's authored size. */
		float WidthCm;
		float LengthCm;
		float DepthCm;
	};

	const FLook Looks[] = {
		{TEXT("Pool"), true, nullptr, 0.f, 600.f, 400.f, 80.f},
		{TEXT("Sea"), false, TEXT("/Game/FluidNinjaLive/UseCases/007_SmallWater/MaterialsMisc/MI_TraceMesh_Pool_Fluorescent.MI_TraceMesh_Pool_Fluorescent"), 2.f, 0.f, 0.f, 0.f},
		{TEXT("River"), false, TEXT("/Game/FluidNinjaLive/UseCases/017_RiverAndLandscape/MaterialsWaterLocalSpace/MI_Water_Local_Whitewater_Heterogen.MI_Water_Local_Whitewater_Heterogen"), 2.f, 0.f, 0.f, 0.f},
		{TEXT("Sand"), false, TEXT("/Game/_Slime/Hub/Build/Fluid/MI_HomeFluid_SandGround.MI_HomeFluid_SandGround"), 1.f, 0.f, 0.f, 0.f},
		{TEXT("Snow"), false, TEXT("/Game/_Slime/Hub/Build/Fluid/MI_HomeFluid_SnowGround.MI_HomeFluid_SnowGround"), 1.f, 0.f, 0.f, 0.f},
	};

	const FLook* Find(const UClass* FluidClass)
	{
		const FString Name = FluidClass ? FluidClass->GetName() : FString();
		for (const FLook& Look : Looks)
		{
			if (Name.Contains(Look.Token))
			{
				return &Look;
			}
		}
		return nullptr;
	}

	const FName SimBuffer(TEXT("VelocityDensityBuffer"));

	UMaterialInstanceDynamic* AsSimMID(UObject* Object)
	{
		UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Object);
		UTexture* Buffer = nullptr;
		return MID && MID->GetTextureParameterValue(FHashedMaterialParameterInfo(SimBuffer), Buffer) && Buffer ? MID : nullptr;
	}

	UStaticMeshComponent* FindTraceMesh(AActor* Fluid)
	{
		if (!Fluid)
		{
			return nullptr;
		}
		TArray<UStaticMeshComponent*> Meshes;
		Fluid->GetComponents<UStaticMeshComponent>(Meshes);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (Mesh && Mesh->GetName().Contains(TEXT("TraceMesh")))
			{
				return Mesh;
			}
		}
		return nullptr;
	}

	bool IsVolumeSmoke(const UMaterialInstanceDynamic* MID)
	{
		if (!MID)
		{
			return false;
		}
		if (MID->GetName().Contains(TEXT("VolumeSmoke")))
		{
			return true;
		}
		return MID->Parent && MID->Parent->GetName().Contains(TEXT("VolumeSmoke"));
	}

	UMaterialInstanceDynamic* ConsiderSimMID(UObject* Object, bool bSkipVolumeSmoke, const UMaterialInstanceDynamic* Exclude)
	{
		UMaterialInstanceDynamic* MID = AsSimMID(Object);
		if (!MID || MID == Exclude || (bSkipVolumeSmoke && IsVolumeSmoke(MID)))
		{
			return nullptr;
		}
		return MID;
	}

	/** NinjaLive keeps its output material instances in Blueprint variables on the actor and its components. */
	UMaterialInstanceDynamic* FindSimMID(AActor* Fluid, bool bSkipVolumeSmoke, const UMaterialInstanceDynamic* Exclude)
	{
		TArray<UObject*> Holders;
		Holders.Add(Fluid);
		for (UActorComponent* Component : Fluid->GetComponents())
		{
			Holders.Add(Component);
		}
		for (UObject* Holder : Holders)
		{
			if (!Holder)
			{
				continue;
			}
			for (TFieldIterator<FProperty> It(Holder->GetClass()); It; ++It)
			{
				if (const FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(*It))
				{
					if (UMaterialInstanceDynamic* MID = ConsiderSimMID(ObjectProp->GetObjectPropertyValue_InContainer(Holder), bSkipVolumeSmoke, Exclude))
					{
						return MID;
					}
				}
				else if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(*It))
				{
					const FObjectPropertyBase* Inner = CastField<FObjectPropertyBase>(ArrayProp->Inner);
					if (!Inner)
					{
						continue;
					}
					FScriptArrayHelper Array(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Holder));
					for (int32 Index = 0; Index < Array.Num(); ++Index)
					{
						if (UMaterialInstanceDynamic* MID = ConsiderSimMID(Inner->GetObjectPropertyValue(Array.GetRawPtr(Index)), bSkipVolumeSmoke, Exclude))
						{
							return MID;
						}
					}
				}
			}
			if (const UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Holder))
			{
				for (int32 Slot = 0; Slot < Prim->GetNumMaterials(); ++Slot)
				{
					if (UMaterialInstanceDynamic* MID = ConsiderSimMID(Prim->GetMaterial(Slot), bSkipVolumeSmoke, Exclude))
					{
						return MID;
					}
				}
			}
		}
		return nullptr;
	}
}

void ASlimeHomeFluidPad::ApplySize(float WidthCm, float LengthCm, float DepthCm)
{
	UStaticMeshComponent* Trace = SlimeFluidLook::FindTraceMesh(FluidActor);
	if (!FluidActor || !Trace || !Trace->GetStaticMesh() || WidthCm <= 0.f || LengthCm <= 0.f)
	{
		return;
	}

	const FBox Local = Trace->GetStaticMesh()->GetBoundingBox();
	const float UnitX = FMath::Max(Local.Max.X - Local.Min.X, 1.f);
	const float UnitY = FMath::Max(Local.Max.Y - Local.Min.Y, 1.f);
	const float UnitZ = FMath::Max(Local.Max.Z - Local.Min.Z, 1.f);
	const FVector Scale(WidthCm / UnitX, LengthCm / UnitY, DepthCm > 0.f ? DepthCm / UnitZ : Trace->GetRelativeScale3D().Z);

	auto WriteVector = [this](const TCHAR* Name, const FVector& Value, bool bKeepZ)
	{
		const FStructProperty* Prop = FindFProperty<FStructProperty>(FluidActor->GetClass(), Name);
		if (!Prop || Prop->Struct != TBaseStructure<FVector>::Get())
		{
			return;
		}
		FVector& Stored = *Prop->ContainerPtrToValuePtr<FVector>(FluidActor);
		Stored = bKeepZ ? FVector(Value.X, Value.Y, Stored.Z) : Value;
	};

	WriteVector(TEXT("TraceMeshSize"), Scale, false);
	WriteVector(TEXT("InteractionVolumeSize"), FVector(Scale.X, Scale.Y, DepthCm * 2.5f / 100.f), DepthCm <= 0.f);
	WriteVector(TEXT("ActivationVolumeSize"), Scale, true);
	Trace->SetRelativeScale3D(Scale);

	TArray<UPrimitiveComponent*> Prims;
	FluidActor->GetComponents<UPrimitiveComponent>(Prims);
	for (UPrimitiveComponent* Prim : Prims)
	{
		if (!Prim || Prim == Trace)
		{
			continue;
		}
		const FString Name = Prim->GetName();
		const FVector Rel = Prim->GetRelativeScale3D();
		if (Name.Contains(TEXT("Interaction")))
		{
			Prim->SetRelativeScale3D(FVector(Scale.X, Scale.Y, DepthCm > 0.f ? DepthCm * 2.5f / 100.f : Rel.Z));
		}
		else if (Name.Contains(TEXT("Activation")))
		{
			Prim->SetRelativeScale3D(FVector(Scale.X, Scale.Y, Rel.Z));
		}
	}
	Trace->UpdateBounds();
}

void ASlimeHomeFluidPad::SitOnFloor()
{
	TArray<UPrimitiveComponent*> Prims;
	FluidActor->GetComponents<UPrimitiveComponent>(Prims);
	for (UPrimitiveComponent* Prim : Prims)
	{
		if (Prim && Prim->GetName().Contains(TEXT("TraceMesh")))
		{
			Prim->UpdateBounds();
			const float Bottom = Prim->Bounds.GetBox().Min.Z;
			FluidActor->AddActorWorldOffset(FVector(0.f, 0.f, GetActorLocation().Z - Bottom));
			return;
		}
	}
}

void ASlimeHomeFluidPad::SetupSurface(const TCHAR* MaterialPath, float Lift)
{
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, MaterialPath);
	UStaticMeshComponent* Trace = SlimeFluidLook::FindTraceMesh(FluidActor);
	if (!Material || !Trace)
	{
		UE_LOG(LogSlimeFable, Warning, TEXT("[HomeFluid] surface skipped for %s (material %d, trace %d)"),
			*FluidActor->GetClass()->GetName(), Material != nullptr, Trace != nullptr);
		return;
	}

	if (FBoolProperty* Invisible = FindFProperty<FBoolProperty>(FluidActor->GetClass(), TEXT("TraceMeshInvisible")))
	{
		Invisible->SetPropertyValue_InContainer(FluidActor, false);
	}
	Trace->SetVisibility(true);
	Trace->SetHiddenInGame(false);
	const FVector Rel = Trace->GetRelativeLocation();
	Trace->SetRelativeLocation(FVector(Rel.X, Rel.Y, Rel.Z + Lift));
	SurfaceMID = UMaterialInstanceDynamic::Create(Material, this);
	Trace->SetMaterial(0, SurfaceMID);
	SyncSurface();
	GetWorldTimerManager().SetTimer(SurfaceTimer, this, &ASlimeHomeFluidPad::SyncSurface, 0.25f, true);
}

void ASlimeHomeFluidPad::SyncSurface()
{
	if (!SurfaceMID || !FluidActor)
	{
		return;
	}
	if (UStaticMeshComponent* Trace = SlimeFluidLook::FindTraceMesh(FluidActor))
	{
		if (FBoolProperty* Invisible = FindFProperty<FBoolProperty>(FluidActor->GetClass(), TEXT("TraceMeshInvisible")))
		{
			Invisible->SetPropertyValue_InContainer(FluidActor, false);
		}
		if (Trace->GetMaterial(0) != SurfaceMID)
		{
			Trace->SetMaterial(0, SurfaceMID);
		}
		Trace->SetVisibility(true);
		Trace->SetHiddenInGame(false);
	}
	if (!SimMID.IsValid())
	{
		SimMID = SlimeFluidLook::FindSimMID(FluidActor, true, SurfaceMID);
		if (!SimMID.IsValid())
		{
			SimMID = SlimeFluidLook::FindSimMID(FluidActor, false, SurfaceMID);
		}
		if (!SimMID.IsValid())
		{
			return;
		}
		UE_LOG(LogSlimeFable, Log, TEXT("[HomeFluid] %s surface reads %s"), *FluidActor->GetClass()->GetName(), *SimMID->GetName());
	}
	UMaterialInterface* Parent = SurfaceMID->Parent;
	for (const FTextureParameterValue& Value : SimMID->TextureParameterValues)
	{
		UTexture* Existing = nullptr;
		if (Value.ParameterValue && Parent && Parent->GetTextureParameterValue(Value.ParameterInfo, Existing))
		{
			SurfaceMID->SetTextureParameterValueByInfo(Value.ParameterInfo, Value.ParameterValue);
		}
	}
	for (const FVectorParameterValue& Value : SimMID->VectorParameterValues)
	{
		FLinearColor Existing;
		if (Parent && Parent->GetVectorParameterValue(Value.ParameterInfo, Existing))
		{
			SurfaceMID->SetVectorParameterValueByInfo(Value.ParameterInfo, Value.ParameterValue);
		}
	}
}

void ASlimeHomeFluidPad::Configure(TSubclassOf<AActor> InFluidClass, int32 InHomePieceId, float InPlanScale, float InDepthCm, bool bInSwapPlanAxes)
{
	FluidClass = InFluidClass;
	HomePieceId = InHomePieceId;
	PlanScale = InPlanScale;
	RequestedDepthCm = InDepthCm;
	bSwapPlanAxes = bInSwapPlanAxes;
}

namespace
{
	UEnum* EnumOf(const FProperty* Inner)
	{
		if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Inner))
		{
			return EnumProp->GetEnum();
		}
		if (const FByteProperty* ByteProp = CastField<FByteProperty>(Inner))
		{
			return ByteProp->Enum;
		}
		return nullptr;
	}

	uint8 ReadEnumByte(const FProperty* Inner, void* Value)
	{
		if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Inner))
		{
			return static_cast<uint8>(EnumProp->GetUnderlyingProperty()->GetUnsignedIntPropertyValue(Value));
		}
		if (const FByteProperty* ByteProp = CastField<FByteProperty>(Inner))
		{
			return ByteProp->GetPropertyValue(Value);
		}
		return MAX_uint8;
	}

	bool WriteEnumByte(FProperty* Inner, void* Value, uint8 Byte)
	{
		if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Inner))
		{
			EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(Value, static_cast<int64>(Byte));
			return true;
		}
		if (FByteProperty* ByteProp = CastField<FByteProperty>(Inner))
		{
			ByteProp->SetPropertyValue(Value, Byte);
			return true;
		}
		return false;
	}
}

void ASlimeHomeFluidPad::IncludeWorldDynamicBrush() const
{
	if (!FluidActor)
	{
		return;
	}

	TArray<UObject*> Holders;
	Holders.Add(FluidActor);
	for (UActorComponent* Component : FluidActor->GetComponents())
	{
		Holders.Add(Component);
	}
	for (UObject* Holder : Holders)
	{
		if (!Holder)
		{
			continue;
		}
		FArrayProperty* ArrayProp = FindFProperty<FArrayProperty>(Holder->GetClass(), TEXT("OverlapFilterInclusiveObjType"));
		if (!ArrayProp || (!CastField<FEnumProperty>(ArrayProp->Inner) && !CastField<FByteProperty>(ArrayProp->Inner)))
		{
			continue;
		}
		const UEnum* Enum = EnumOf(ArrayProp->Inner);
		const uint8 Wanted = (Enum && Enum->GetName().Contains(TEXT("CollisionChannel")))
			? static_cast<uint8>(ECC_WorldDynamic)
			: static_cast<uint8>(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));

		FScriptArrayHelper Array(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Holder));
		bool bFound = false;
		for (int32 Index = 0; Index < Array.Num(); ++Index)
		{
			if (ReadEnumByte(ArrayProp->Inner, Array.GetRawPtr(Index)) == Wanted)
			{
				bFound = true;
				break;
			}
		}
		if (bFound)
		{
			continue;
		}
		const int32 NewIndex = Array.AddValue();
		if (!WriteEnumByte(ArrayProp->Inner, Array.GetRawPtr(NewIndex), Wanted))
		{
			Array.RemoveValues(NewIndex, 1);
			continue;
		}
		UE_LOG(LogSlimeFable, Log, TEXT("[HomeFluid] %s overlap filter accepts WorldDynamic"), *FluidActor->GetClass()->GetName());
	}
}

void ASlimeHomeFluidPad::DropDemoBalls() const
{
	if (!FluidActor)
	{
		return;
	}

	TArray<UNiagaraComponent*> Niagaras;
	FluidActor->GetComponents<UNiagaraComponent>(Niagaras);
	for (UNiagaraComponent* Niagara : Niagaras)
	{
		const UNiagaraSystem* System = Niagara ? Niagara->GetAsset() : nullptr;
		if (!System || !System->GetPathName().Contains(TEXT("NS_Demo_Balls")))
		{
			continue;
		}
		Niagara->SetAutoDestroy(false);
		Niagara->DeactivateImmediate();
		Niagara->SetVisibility(false);
		UE_LOG(LogSlimeFable, Log, TEXT("[HomeFluid] hid demo balls %s on %s"),
			*Niagara->GetName(), *FluidActor->GetClass()->GetName());
	}
}

void ASlimeHomeFluidPad::LetPawnPassThroughFluid() const
{
	if (!FluidActor)
	{
		return;
	}

	TArray<UPrimitiveComponent*> Prims;
	FluidActor->GetComponents<UPrimitiveComponent>(Prims);
	for (UPrimitiveComponent* Prim : Prims)
	{
		if (!Prim)
		{
			continue;
		}
		const FString Name = Prim->GetName();
		const bool bInteraction = Name.Contains(TEXT("Interaction"));
		const bool bSimVolume = Name.Contains(TEXT("Activation"))
			|| bInteraction
			|| Name.Contains(TEXT("TraceMesh"));
		if (!bSimVolume)
		{
			continue;
		}
		Prim->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		if (bInteraction)
		{
			Prim->SetGenerateOverlapEvents(true);
			Prim->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
		}
	}
	IncludeWorldDynamicBrush();
}

void ASlimeHomeFluidPad::FitAimQuery()
{
	if (!AimQuery || !FluidActor)
	{
		return;
	}

	FVector Origin = FVector::ZeroVector;
	FVector Extent = FVector::ZeroVector;
	UPrimitiveComponent* Trace = nullptr;
	TArray<UPrimitiveComponent*> Prims;
	FluidActor->GetComponents<UPrimitiveComponent>(Prims);
	for (UPrimitiveComponent* Prim : Prims)
	{
		if (Prim && Prim->GetName().Contains(TEXT("TraceMesh")))
		{
			Trace = Prim;
			break;
		}
	}
	if (Trace)
	{
		const FBox Box = Trace->Bounds.GetBox();
		Origin = Box.GetCenter();
		Extent = Box.GetExtent();
	}
	else
	{
		FluidActor->GetActorBounds(false, Origin, Extent);
	}
	const FVector LocalTop = GetActorTransform().InverseTransformPosition(Origin + FVector(0.f, 0.f, Extent.Z));
	AimQuery->SetBoxExtent(FVector(FMath::Max(Extent.X, 25.f), FMath::Max(Extent.Y, 25.f), 2.f));
	AimQuery->SetRelativeLocation(FVector(LocalTop.X, LocalTop.Y, FMath::Max(LocalTop.Z, 0.f)));
}

namespace
{
	bool CopyUnderwaterTemplate(FPostProcessSettings& OutSettings, float& OutWeight, float& OutPriority, float& OutRadius, bool& bOutEnabled)
	{
		static bool bTried = false;
		static bool bOk = false;
		static FPostProcessSettings Settings;
		static float Weight = 1.f;
		static float Priority = 0.f;
		static float Radius = 0.f;
		static bool bEnabled = true;
		if (!bTried)
		{
			bTried = true;
			UPackage* Package = LoadPackage(nullptr, TEXT("/Game/FluidNinjaLive/UseCases/Levels/Usecase_016_Caustics_LIVE17"), LOAD_None);
			APostProcessVolume* Found = nullptr;
			if (Package)
			{
				ForEachObjectWithPackage(Package, [&Found](UObject* Object)
				{
					if (APostProcessVolume* Volume = Cast<APostProcessVolume>(Object))
					{
						if (Volume->GetName().Contains(TEXT("UnderWater")))
						{
							Found = Volume;
						}
					}
					return true;
				});
			}
			if (!Found)
			{
				UE_LOG(LogSlimeFable, Warning, TEXT("[HomeFluid] PostProcess_UnderWater was not found in Usecase_016_Caustics_LIVE17"));
			}
			else
			{
				Settings = Found->Settings;
				Weight = Found->BlendWeight;
				Priority = Found->Priority;
				Radius = Found->BlendRadius;
				bEnabled = Found->bEnabled;
				bOk = true;
				UE_LOG(LogSlimeFable, Log, TEXT("[HomeFluid] copied underwater post from %s (%d blends)"),
					*Found->GetName(), Settings.WeightedBlendables.Array.Num());
			}
		}
		if (!bOk)
		{
			return false;
		}
		OutSettings = Settings;
		OutWeight = Weight;
		OutPriority = Priority;
		OutRadius = Radius;
		bOutEnabled = bEnabled;
		return true;
	}
}

void ASlimeHomeFluidPad::AddUnderwaterPost()
{
	if (UnderwaterPost || !FluidActor || !GetWorld())
	{
		return;
	}
	if (!FluidClass || !FluidClass->GetName().Contains(TEXT("Pool")))
	{
		return;
	}

	FPostProcessSettings Settings;
	float Weight = 1.f;
	float Priority = 0.f;
	float Radius = 0.f;
	bool bEnabled = true;
	if (!CopyUnderwaterTemplate(Settings, Weight, Priority, Radius, bEnabled))
	{
		return;
	}
	(void)Radius;

	UStaticMeshComponent* Trace = SlimeFluidLook::FindTraceMesh(FluidActor);
	if (!Trace || !Trace->GetStaticMesh())
	{
		return;
	}
	Trace->UpdateBounds();
	const FBox MeshLocal = Trace->GetStaticMesh()->GetBoundingBox();
	const FTransform TraceXform = Trace->GetComponentTransform();
	const FVector Half = MeshLocal.GetExtent() * TraceXform.GetScale3D().GetAbs();
	if (Half.GetMin() <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector WaterCenter = TraceXform.TransformPosition(MeshLocal.GetCenter());
	WaterSurfaceZ = WaterCenter.Z + Half.Z;
	PoolMinXY = FVector2D(WaterCenter.X - Half.X, WaterCenter.Y - Half.Y);
	PoolMaxXY = FVector2D(WaterCenter.X + Half.X, WaterCenter.Y + Half.Y);

	// The rendered surface is the TraceMesh's real top face, which can sit a few centimeters off its padded bounds.
	// A few centimeters matter: with the camera at the surface they move the waterline across a third of the screen.
	{
		FHitResult Hit;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(SlimePoolSurface), true);
		const FVector Start(WaterCenter.X, WaterCenter.Y, WaterSurfaceZ + 200.f);
		const FVector End(WaterCenter.X, WaterCenter.Y, WaterCenter.Z - Half.Z);
		const bool bHit = Trace->LineTraceComponent(Hit, Start, End, Query);
		UE_LOG(LogSlimeFable, Log, TEXT("[HomeFluid] water surface: bounds top %.1f, traced top %s"),
			WaterSurfaceZ, bHit ? *FString::Printf(TEXT("%.1f"), Hit.ImpactPoint.Z) : TEXT("miss"));
		if (bHit && Hit.ImpactPoint.Z > WaterCenter.Z)
		{
			WaterSurfaceZ = Hit.ImpactPoint.Z;
		}
	}

	// The trace mesh still wears the inactive gray material here. Ninja maps the visible water on later, from
	// OutputMaterials on the live component. That material's MeshDistortion pushes the top face up by as much as
	// MeshDistortClampMax, so the drawn surface sits well above the collision top.
	// A late split leaves an original-color band between the drawn surface and the mask; the full ceiling tints a
	// strip of wall above the water. UnderwaterMaskTrimCm pulls the mask down from that ceiling. Whole-screen tint stays on the constant part of that lift: it cannot be masked per pixel.
	TArray<UMaterialInterface*> WaterCandidates;
	auto ConsiderMaterial = [&WaterCandidates](UObject* Object)
	{
		if (UMaterialInterface* Material = Cast<UMaterialInterface>(Object))
		{
			WaterCandidates.AddUnique(Material);
		}
	};
	auto ConsiderHolder = [&ConsiderMaterial](UObject* Holder)
	{
		if (!Holder)
		{
			return;
		}
		if (const FObjectPropertyBase* Output = FindFProperty<FObjectPropertyBase>(Holder->GetClass(), TEXT("MI_Output")))
		{
			ConsiderMaterial(Output->GetObjectPropertyValue_InContainer(Holder));
		}
		const FArrayProperty* Materials = FindFProperty<FArrayProperty>(Holder->GetClass(), TEXT("OutputMaterials"));
		const FObjectPropertyBase* Inner = Materials ? CastField<FObjectPropertyBase>(Materials->Inner) : nullptr;
		if (!Materials || !Inner)
		{
			return;
		}
		FScriptArrayHelper Helper(Materials, Materials->ContainerPtrToValuePtr<void>(Holder));
		for (int32 Index = 0; Index < Helper.Num(); ++Index)
		{
			ConsiderMaterial(Inner->GetObjectPropertyValue(Helper.GetRawPtr(Index)));
		}
	};
	ConsiderHolder(FluidActor);
	TArray<UActorComponent*> FluidComponents;
	FluidActor->GetComponents(FluidComponents);
	for (UActorComponent* Component : FluidComponents)
	{
		ConsiderHolder(Component);
	}
	TArray<UPrimitiveComponent*> MaterialPrims;
	FluidActor->GetComponents<UPrimitiveComponent>(MaterialPrims);
	for (UPrimitiveComponent* Prim : MaterialPrims)
	{
		if (!Prim)
		{
			continue;
		}
		for (int32 Slot = 0; Slot < Prim->GetNumMaterials(); ++Slot)
		{
			ConsiderMaterial(Prim->GetMaterial(Slot));
		}
	}

	const UMaterialInterface* WaterMat = nullptr;
	float BestClamp = -1.f;
	for (UMaterialInterface* Candidate : WaterCandidates)
	{
		bool bCandidateDistort = false;
		FGuid CandidateGuid;
		float CandidateClamp = 0.f;
		Candidate->GetStaticSwitchParameterValue(FHashedMaterialParameterInfo(TEXT("MeshDistortion")), bCandidateDistort, CandidateGuid);
		if (!bCandidateDistort || !Candidate->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("MeshDistortClampMax")), CandidateClamp))
		{
			continue;
		}
		if (!WaterMat || CandidateClamp > BestClamp)
		{
			WaterMat = Candidate;
			BestClamp = CandidateClamp;
		}
	}
	if (!WaterMat && Trace->GetNumMaterials() > 0)
	{
		WaterMat = Trace->GetMaterial(0);
	}

	float MaskSurfaceZ = WaterSurfaceZ;
	if (WaterMat)
	{
		bool bDistort = false;
		FGuid Unused;
		float Offset = 0.f, Mult = 0.f, ClampMin = -100.f, ClampMax = 0.f;
		WaterMat->GetStaticSwitchParameterValue(FHashedMaterialParameterInfo(TEXT("MeshDistortion")), bDistort, Unused);
		WaterMat->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("AdditionalOffset")), Offset);
		WaterMat->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("MeshDistortMult")), Mult);
		WaterMat->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("MeshDistortClampMin")), ClampMin);
		const bool bHasClamp = WaterMat->GetScalarParameterValue(FHashedMaterialParameterInfo(TEXT("MeshDistortClampMax")), ClampMax);
		const float FadeLift = bDistort ? FMath::Clamp(Offset + 0.25f * Mult, ClampMin, bHasClamp ? ClampMax : Offset + Mult) : 0.f;
		const float CeilingLift = (bDistort && bHasClamp) ? FMath::Max(ClampMax, FadeLift) : FadeLift;
		UnderwaterMaskCeilingZ = WaterSurfaceZ + CeilingLift;
		UnderwaterMaskFloorZ = WaterSurfaceZ + FadeLift;
		const float MaskLift = FMath::Max(CeilingLift - FMath::Clamp(UnderwaterMaskTrimCm, 0.f, 80.f), FadeLift);
		UE_LOG(LogSlimeFable, Log, TEXT("[HomeFluid] water WPO fade lift %.1f, mask lift %.1f trim %.1f (distort %d, offset %.1f, mult %.1f, clamp %.1f..%.1f) via %s -> mask z %.1f"),
			FadeLift, MaskLift, UnderwaterMaskTrimCm, bDistort ? 1 : 0, Offset, Mult, ClampMin, ClampMax, *WaterMat->GetName(), WaterSurfaceZ + MaskLift);
		WaterSurfaceZ += FadeLift;
		MaskSurfaceZ += MaskLift;
	}

	TArray<UPrimitiveComponent*> FluidPrims;
	FluidActor->GetComponents<UPrimitiveComponent>(FluidPrims);
	for (UPrimitiveComponent* Prim : FluidPrims)
	{
		if (Prim && Prim != Trace && Prim->IsVisible() && !Prim->bHiddenInGame)
		{
			Prim->UpdateBounds();
			const FBox PrimBox = Prim->Bounds.GetBox();
			UE_LOG(LogSlimeFable, Log, TEXT("[HomeFluid] pool prim %s (%s) z %.1f..%.1f"),
				*Prim->GetName(), *Prim->GetClass()->GetName(), PrimBox.Min.Z, PrimBox.Max.Z);
		}
	}

	UMaterialInterface* PoolLook = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/_Slime/Hub/Build/Fluid/PP/MI_SlimePool_Cyan_v2.MI_SlimePool_Cyan_v2"));
	UMaterialInterface* Refract = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/_Slime/Hub/Build/Fluid/PP/M_SlimePoolWaterlineRefract.M_SlimePoolWaterlineRefract"));
	auto MakeWaterlineMID = [this, MaskSurfaceZ](UMaterialInterface* Base) -> UMaterialInstanceDynamic*
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Base, this);
		MID->SetScalarParameterValue(TEXT("WaterZ"), MaskSurfaceZ);
		MID->SetVectorParameterValue(TEXT("PoolMin"), FLinearColor(PoolMinXY.X, PoolMinXY.Y, 0.f, 0.f));
		MID->SetVectorParameterValue(TEXT("PoolMax"), FLinearColor(PoolMaxXY.X, PoolMaxXY.Y, 0.f, 0.f));
		UnderwaterMIDs.Add(MID);
		return MID;
	};
	TArray<FString> Overrides;
	for (TFieldIterator<FBoolProperty> It(FPostProcessSettings::StaticStruct()); It; ++It)
	{
		if (It->GetName().StartsWith(TEXT("bOverride_")) && It->GetPropertyValue_InContainer(&Settings))
		{
			Overrides.Add(It->GetName().RightChop(10));
		}
	}
	UE_LOG(LogSlimeFable, Log, TEXT("[HomeFluid] underwater source overrides: %s"), *FString::Join(Overrides, TEXT(", ")));

	// SceneColorTint stays a whole-screen setting: the masked material runs after tonemapping, where the same tint reads far darker.
	FPostProcessSettings Masked;
	FPostProcessSettings Global = Settings;
	Global.WeightedBlendables.Array.Reset();
	for (const FWeightedBlendable& Blend : Settings.WeightedBlendables.Array)
	{
		UMaterialInterface* Source = Cast<UMaterialInterface>(Blend.Object);
		const UMaterial* Base = Source ? Source->GetBaseMaterial() : nullptr;
		const bool bMaskable = PoolLook && Base && Base->GetName() == TEXT("M_UnderWaterPostProcess");
		UE_LOG(LogSlimeFable, Log, TEXT("[HomeFluid] underwater blendable %s weight %.2f base %s -> %s"),
			Blend.Object ? *Blend.Object->GetName() : TEXT("None"), Blend.Weight,
			Base ? *Base->GetName() : TEXT("None"), Blend.Weight <= 0.f ? TEXT("off") : bMaskable ? TEXT("masked") : TEXT("depth fade"));
		if (Blend.Weight <= 0.f)
		{
			continue;
		}
		if (!bMaskable)
		{
			Global.WeightedBlendables.Array.Add(Blend);
			continue;
		}
		UMaterialInstanceDynamic* LookMID = MakeWaterlineMID(PoolLook);
		if (UMaterialInstance* SourceInstance = Cast<UMaterialInstance>(Source))
		{
			LookMID->CopyParameterOverrides(SourceInstance);
			LookMID->SetScalarParameterValue(TEXT("WaterZ"), MaskSurfaceZ);
			LookMID->SetVectorParameterValue(TEXT("PoolMin"), FLinearColor(PoolMinXY.X, PoolMinXY.Y, 0.f, 0.f));
			LookMID->SetVectorParameterValue(TEXT("PoolMax"), FLinearColor(PoolMaxXY.X, PoolMaxXY.Y, 0.f, 0.f));
		}
		Masked.WeightedBlendables.Array.Add(FWeightedBlendable(Blend.Weight, LookMID));
	}
	if (Refract)
	{
		Masked.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, MakeWaterlineMID(Refract)));
	}
	UE_LOG(LogSlimeFable, Log, TEXT("[HomeFluid] waterline materials %d (pool look %d, refract %d), depth-fade blendables %d"),
		UnderwaterMIDs.Num(), PoolLook != nullptr, Refract != nullptr, Global.WeightedBlendables.Array.Num());
	GlobalWeight = Weight;

	// The slime camera boom is ~260cm, so the eye often sits in the pool wall or just outside it while still below the surface.
	// The top has to clear the early mask plane, otherwise the split is clipped before the camera reaches it.
	const float MaskAbove = FMath::Max(0.f, FMath::Max(MaskSurfaceZ, UnderwaterMaskCeilingZ) - (WaterCenter.Z + Half.Z));
	const FVector Margin(100.f, 100.f, FMath::Max(30.f, (MaskAbove + 40.f) * 0.5f));
	UnderwaterBox = NewObject<UBoxComponent>(this, TEXT("UnderwaterBox"));
	UnderwaterBox->SetupAttachment(PadRoot);
	UnderwaterBox->SetMobility(EComponentMobility::Movable);
	UnderwaterBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	UnderwaterBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	UnderwaterBox->SetGenerateOverlapEvents(false);
	UnderwaterBox->SetHiddenInGame(true);
	UnderwaterBox->SetVisibility(false);
	UnderwaterBox->SetCanEverAffectNavigation(false);
	UnderwaterBox->SetBoxExtent(Half + Margin);
	UnderwaterBox->SetWorldScale3D(FVector::OneVector);
	UnderwaterBox->SetWorldRotation(TraceXform.Rotator());
	UnderwaterBox->SetWorldLocation(WaterCenter + FVector(0.f, 0.f, Margin.Z));
	UnderwaterBox->RegisterComponent();

	UnderwaterPost = NewObject<UPostProcessComponent>(this, TEXT("UnderwaterPost"));
	UnderwaterPost->SetupAttachment(UnderwaterBox);
	UnderwaterPost->bUnbound = false;
	UnderwaterPost->bEnabled = bEnabled;
	UnderwaterPost->BlendWeight = 1.f;
	UnderwaterPost->Priority = Priority;
	UnderwaterPost->BlendRadius = 0.f;
	UnderwaterPost->Settings = Masked;
	UnderwaterPost->RegisterComponent();

	UnderwaterGlobalPost = NewObject<UPostProcessComponent>(this, TEXT("UnderwaterGlobalPost"));
	UnderwaterGlobalPost->SetupAttachment(UnderwaterBox);
	UnderwaterGlobalPost->bUnbound = false;
	UnderwaterGlobalPost->bEnabled = bEnabled;
	UnderwaterGlobalPost->BlendWeight = 0.f;
	UnderwaterGlobalPost->Priority = Priority;
	UnderwaterGlobalPost->BlendRadius = 0.f;
	UnderwaterGlobalPost->Settings = Global;
	UnderwaterGlobalPost->RegisterComponent();
	const FBodyInstance* Body = UnderwaterBox->GetBodyInstance();
	UE_LOG(LogSlimeFable, Log, TEXT("[HomeFluid] underwater box %s extent %s shape %d"),
		*UnderwaterBox->GetComponentLocation().ToCompactString(), *Half.ToCompactString(),
		Body && Body->IsValidBodyInstance());
	AppliedMaskTrimCm = FMath::Clamp(UnderwaterMaskTrimCm, 0.f, 80.f);
	GetWorldTimerManager().SetTimer(CameraTimer, this, &ASlimeHomeFluidPad::UpdateUnderwaterCamera, 0.25f, true);
}

void ASlimeHomeFluidPad::ApplyMaskTrim()
{
	const float Trim = FMath::Clamp(UnderwaterMaskTrimCm, 0.f, 80.f);
	if (UnderwaterMIDs.Num() == 0 || UnderwaterMaskCeilingZ <= 0.f || FMath::IsNearlyEqual(Trim, AppliedMaskTrimCm))
	{
		return;
	}
	AppliedMaskTrimCm = Trim;
	const float MaskZ = FMath::Max(UnderwaterMaskCeilingZ - Trim, UnderwaterMaskFloorZ);
	for (UMaterialInstanceDynamic* MID : UnderwaterMIDs)
	{
		if (MID)
		{
			MID->SetScalarParameterValue(TEXT("WaterZ"), MaskZ);
		}
	}
	UE_LOG(LogSlimeFable, Log, TEXT("[HomeFluid] waterline trim %.1f -> mask z %.1f"), Trim, MaskZ);
}

void ASlimeHomeFluidPad::UpdateUnderwaterCamera()
{
	ApplyMaskTrim();
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Camera || !UnderwaterBox || !UnderwaterGlobalPost)
	{
		return;
	}
	const FVector Eye = Camera->GetCameraLocation();
	const FVector Local = UnderwaterBox->GetComponentTransform().InverseTransformPosition(Eye);
	const FVector Extent = UnderwaterBox->GetUnscaledBoxExtent();
	const bool bNear = FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
	// Whole-screen settings only start once the eye itself is below the surface, so the above-water half stays untinted.
	const float Under = FMath::SmoothStep(0.f, 1.f, FMath::Clamp(static_cast<float>(WaterSurfaceZ - Eye.Z) / 20.f, 0.f, 1.f));
	UnderwaterGlobalPost->BlendWeight = bNear ? GlobalWeight * Under : 0.f;
	const bool bEyeUnder = Eye.Z < WaterSurfaceZ;
	if (bNear && bEyeUnder != bEyeWasUnder)
	{
		const bool bInsidePoolXY = Eye.X >= PoolMinXY.X && Eye.X <= PoolMaxXY.X && Eye.Y >= PoolMinXY.Y && Eye.Y <= PoolMaxXY.Y;
		UE_LOG(LogSlimeFable, Log, TEXT("[HomeFluid] camera %s water: eye %s, surface z %.1f, %s pool plan"),
			bEyeUnder ? TEXT("entered") : TEXT("left"), *Eye.ToCompactString(), WaterSurfaceZ,
			bInsidePoolXY ? TEXT("inside") : TEXT("outside"));
	}
	bEyeWasUnder = bEyeUnder;
	if (bNear != bCameraNear)
	{
		bCameraNear = bNear;
		GetWorldTimerManager().SetTimer(CameraTimer, this, &ASlimeHomeFluidPad::UpdateUnderwaterCamera, bNear ? 0.033f : 0.25f, true);
	}
}

void ASlimeHomeFluidPad::BeginPlay()
{
	Super::BeginPlay();
	if (!FluidClass || !GetWorld() || FluidActor)
	{
		return;
	}

	const SlimeFluidLook::FLook* Look = SlimeFluidLook::Find(FluidClass);
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform Xform(GetActorRotation(), GetActorLocation());
	FluidActor = GetWorld()->SpawnActor<AActor>(FluidClass, Xform, Params);
	if (!FluidActor)
	{
		return;
	}
	LetPawnPassThroughFluid();
	DropDemoBalls();
	FluidActor->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);
	if (Look && Look->WidthCm > 0.f)
	{
		float Width = Look->WidthCm;
		float Length = Look->LengthCm;
		float Depth = Look->DepthCm;
		if (FluidClass && FluidClass->GetName().Contains(TEXT("Pool")))
		{
			const float Plan = SlimeHomePoolPlanScale(PlanScale);
			Width = SlimeHomePoolCells(FMath::RoundToInt(Width / 50.f), Plan) * 50.f;
			Length = SlimeHomePoolCells(FMath::RoundToInt(Length / 50.f), Plan) * 50.f;
			Depth = SlimeHomePoolDepthCm(RequestedDepthCm);
			if (bSwapPlanAxes)
			{
				Swap(Width, Length);
			}
		}
		ApplySize(Width, Length, Depth);
	}
	if (Look && Look->bSitOnFloor)
	{
		SitOnFloor();
	}
	FitAimQuery();
	if (Look && Look->SurfaceMaterial)
	{
		SetupSurface(Look->SurfaceMaterial, Look->SurfaceLift);
	}
	AddUnderwaterPost();
}

void ASlimeHomeFluidPad::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SurfaceTimer);
	GetWorldTimerManager().ClearTimer(CameraTimer);
	UnderwaterMIDs.Reset();
	if (UnderwaterPost)
	{
		UnderwaterPost->DestroyComponent();
		UnderwaterPost = nullptr;
	}
	if (UnderwaterGlobalPost)
	{
		UnderwaterGlobalPost->DestroyComponent();
		UnderwaterGlobalPost = nullptr;
	}
	if (UnderwaterBox)
	{
		UnderwaterBox->DestroyComponent();
		UnderwaterBox = nullptr;
	}
	if (FluidActor)
	{
		FluidActor->Destroy();
		FluidActor = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void ASlimeHomeFluidPad::SetHighlighted(bool bEnabled)
{
	bHighlighted = bEnabled;
}

namespace SlimeFluidProbe
{
	const TCHAR* const Classes[] = {
		TEXT("/Game/_Slime/Hub/Build/Fluid/BP_HomeFluid_Pool.BP_HomeFluid_Pool_C"),
		TEXT("/Game/_Slime/Hub/Build/Fluid/BP_HomeFluid_Sand.BP_HomeFluid_Sand_C"),
		TEXT("/Game/_Slime/Hub/Build/Fluid/BP_HomeFluid_Snow.BP_HomeFluid_Snow_C"),
		TEXT("/Game/_Slime/Hub/Build/Fluid/BP_HomeFluid_Sea.BP_HomeFluid_Sea_C"),
		TEXT("/Game/_Slime/Hub/Build/Fluid/BP_HomeFluid_River.BP_HomeFluid_River_C"),
	};

	void Dump(TWeakObjectPtr<ASlimeHomeFluidPad> Pad, float Time)
	{
		AActor* Fluid = Pad.IsValid() ? Pad->GetFluidActor() : nullptr;
		if (!Fluid)
		{
			UE_LOG(LogSlimeFable, Warning, TEXT("[FluidProbe] t=%.1f no fluid actor"), Time);
			return;
		}
		UE_LOG(LogSlimeFable, Log, TEXT("[FluidProbe] t=%.1f %s actorHidden=%d loc=%s"), Time, *Fluid->GetClass()->GetName(),
			Fluid->IsHidden(), *Fluid->GetActorLocation().ToCompactString());
		for (TFieldIterator<FProperty> It(Fluid->GetClass()); It; ++It)
		{
			const FString Name = It->GetName();
			if (Name.Contains(TEXT("Activ")) || Name.Contains(TEXT("Inside")) || Name.Contains(TEXT("Visib"))
				|| Name.Contains(TEXT("Invisible")) || Name.Contains(TEXT("Inactive")) || Name.Contains(TEXT("Activator")))
			{
				FString Value;
				It->ExportTextItem_InContainer(Value, Fluid, nullptr, Fluid, PPF_None);
				UE_LOG(LogSlimeFable, Log, TEXT("[FluidProbe]    var %s = %s"), *Name, *Value.Left(120));
			}
		}
		TArray<UPrimitiveComponent*> Prims;
		Fluid->GetComponents<UPrimitiveComponent>(Prims);
		for (const UPrimitiveComponent* Prim : Prims)
		{
			const UMaterialInterface* Mat = Prim->GetNumMaterials() > 0 ? Prim->GetMaterial(0) : nullptr;
			UE_LOG(LogSlimeFable, Log, TEXT("[FluidProbe]    %s <%s> visible=%d hiddenInGame=%d mat=%s bounds=%s coll=%d type=%d overlapEvents=%d vsPawn=%d"),
				*Prim->GetName(), *Prim->GetClass()->GetName(), Prim->IsVisible(), Prim->bHiddenInGame,
				Mat ? *Mat->GetName() : TEXT("None"), *Prim->Bounds.BoxExtent.ToCompactString(),
				(int32)Prim->GetCollisionEnabled(), (int32)Prim->GetCollisionObjectType(), Prim->GetGenerateOverlapEvents(),
				(int32)Prim->GetCollisionResponseToChannel(ECC_Pawn));
		}
		if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(Fluid, 0))
		{
			TArray<UPrimitiveComponent*> PawnPrims;
			Pawn->GetComponents<UPrimitiveComponent>(PawnPrims);
			for (const UPrimitiveComponent* Prim : PawnPrims)
			{
				if (Prim->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
				{
					continue;
				}
				UE_LOG(LogSlimeFable, Log, TEXT("[FluidProbe]    pawn %s type=%d overlapEvents=%d vsWorldDynamic=%d root=%d"),
					*Prim->GetName(), (int32)Prim->GetCollisionObjectType(), Prim->GetGenerateOverlapEvents(),
					(int32)Prim->GetCollisionResponseToChannel(ECC_WorldDynamic), Prim == Pawn->GetRootComponent());
			}
			TArray<AActor*> Overlapping;
			Fluid->GetOverlappingActors(Overlapping);
			UE_LOG(LogSlimeFable, Log, TEXT("[FluidProbe]    fluid overlaps pawn=%d (overlapping %d actors)"),
				Overlapping.Contains(Pawn), Overlapping.Num());
		}
	}

	FAutoConsoleCommandWithWorldAndArgs Command(
		TEXT("Slime.FluidProbe"),
		TEXT("Slime.FluidProbe <0-4> [quit]: spawn a home fluid at the player and log its visibility over time."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const int32 Index = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
			const bool bQuit = Args.Contains(TEXT("quit"));
			const bool bShot = Args.Contains(TEXT("shot"));
			APawn* Pawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
			UClass* Class = LoadClass<AActor>(nullptr, Classes[FMath::Clamp(Index, 0, 4)]);
			if (!Pawn || !Class)
			{
				UE_LOG(LogSlimeFable, Warning, TEXT("[FluidProbe] pawn=%d class=%d"), Pawn != nullptr, Class != nullptr);
				return;
			}
			float Far = 0.f;
			for (const FString& Arg : Args)
			{
				if (Arg.StartsWith(TEXT("far=")))
				{
					Far = FCString::Atof(*Arg.Mid(4));
				}
			}
			FVector Location = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * Far;
			FHitResult Hit;
			FCollisionQueryParams Query(TEXT("FluidProbe"), false, Pawn);
			if (World->LineTraceSingleByChannel(Hit, Location, Location - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Query))
			{
				Location = Hit.ImpactPoint;
			}
			ASlimeHomeFluidPad* Pad = World->SpawnActorDeferred<ASlimeHomeFluidPad>(ASlimeHomeFluidPad::StaticClass(),
				FTransform(Location), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			Pad->Configure(Class, INDEX_NONE);
			Pad->FinishSpawning(FTransform(Location));
			UE_LOG(LogSlimeFable, Log, TEXT("[FluidProbe] spawned %s at %s, pawn %s at %s"), *Class->GetName(),
				*Location.ToCompactString(), *Pawn->GetName(), *Pawn->GetActorLocation().ToCompactString());
			const TWeakObjectPtr<ASlimeHomeFluidPad> WeakPad(Pad);
			for (const float Delay : {1.f, 4.f, 8.f, 11.f})
			{
				FTimerHandle Handle;
				World->GetTimerManager().SetTimer(Handle, [WeakPad, Delay]() { Dump(WeakPad, Delay); }, Delay, false);
			}
			if (Far > 0.f)
			{
				const TWeakObjectPtr<APawn> WeakPawn(Pawn);
				const FVector Onto = Location + FVector(0.f, 0.f, 150.f) - Pawn->GetActorForwardVector() * 300.f;
				FTimerHandle Handle;
				World->GetTimerManager().SetTimer(Handle, [WeakPawn, Onto]()
				{
					if (WeakPawn.IsValid())
					{
						WeakPawn->TeleportTo(Onto, WeakPawn->GetActorRotation());
						UE_LOG(LogSlimeFable, Log, TEXT("[FluidProbe] moved pawn to %s"), *Onto.ToCompactString());
					}
				}, 5.f, false);
			}
			if (bShot)
			{
				FTimerHandle Handle;
				World->GetTimerManager().SetTimer(Handle, []() { FScreenshotRequest::RequestScreenshot(TEXT("FluidProbe.png"), false, false); }, 12.f, false);
			}
			if (bQuit)
			{
				FTimerHandle Handle;
				World->GetTimerManager().SetTimer(Handle, []() { FPlatformMisc::RequestExit(false); }, 15.f, false);
			}
		}));
}
