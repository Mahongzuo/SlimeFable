// Copyright Epic Games, Inc. All Rights Reserved.

#include "Hub/HomeBuild/SlimeHomeBuildEditorLibrary.h"

#include "GameFramework/Actor.h"

#if WITH_EDITOR
#include "AssetCompilingManager.h"
#include "ContentStreaming.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ShaderCompiler.h"
#include "SlimeFable.h"

namespace SlimeHomeBuildEditorPrivate
{
	bool PointsIntoLevel(const FProperty* Property)
	{
		if (const FObjectPropertyBase* ObjectProp = CastField<FObjectPropertyBase>(Property))
		{
			const UClass* Class = ObjectProp->PropertyClass;
			return Class && (Class->IsChildOf(AActor::StaticClass()) || Class->IsChildOf(UActorComponent::StaticClass()));
		}
		if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			return PointsIntoLevel(ArrayProp->Inner);
		}
		if (const FSetProperty* SetProp = CastField<FSetProperty>(Property))
		{
			return PointsIntoLevel(SetProp->ElementProp);
		}
		if (const FMapProperty* MapProp = CastField<FMapProperty>(Property))
		{
			return PointsIntoLevel(MapProp->KeyProp) || PointsIntoLevel(MapProp->ValueProp);
		}
		return false;
	}

	/**
	 * CreateBlueprintFromActor drops per-instance edits on components inherited from a parent
	 * blueprint (NinjaLive_C TraceMesh, volumes, NinjaLiveComponent). Copy them onto a temporary
	 * instance and push that instance back into the blueprint.
	 */
	int32 PushSourceOverrides(UBlueprint* Blueprint, AActor* Source)
	{
		UWorld* World = Source ? Source->GetWorld() : nullptr;
		UClass* Generated = Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
		if (!World || !Generated)
		{
			return 0;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.bTemporaryEditorActor = true;
		AActor* Temp = World->SpawnActor<AActor>(Generated, Source->GetActorTransform(), Params);
		if (!Temp)
		{
			return 0;
		}

		const EditorUtilities::FCopyOptions CopyOptions(
			EditorUtilities::ECopyOptions::OnlyCopyEditOrInterpProperties,
			[](FProperty& Property, UObject&) { return !PointsIntoLevel(&Property); });
		int32 Copied = EditorUtilities::CopyActorProperties(Source, Temp, CopyOptions);

		// Scripted edits on the level actor (e.g. shrinking a pool) are not seen by CopyActorProperties.
		TInlineComponentArray<USceneComponent*> SourceComponents(Source);
		TInlineComponentArray<USceneComponent*> TempComponents(Temp);
		for (USceneComponent* From : SourceComponents)
		{
			if (!From || From == Source->GetRootComponent())
			{
				continue;
			}
			for (USceneComponent* To : TempComponents)
			{
				if (To && To != Temp->GetRootComponent() && To->GetFName() == From->GetFName()
					&& !To->GetRelativeTransform().Equals(From->GetRelativeTransform()))
				{
					To->Modify();
					To->SetRelativeTransform(From->GetRelativeTransform());
					++Copied;
				}
			}
		}

		Temp->SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		const int32 Applied = FKismetEditorUtilities::ApplyInstanceChangesToBlueprint(Temp);
		World->EditorDestroyActor(Temp, false);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		UE_LOG(LogSlimeFable, Log, TEXT("[HomeFluid] %s copied %d properties, applied %d to blueprint"),
			*Blueprint->GetName(), Copied, Applied);
		return Applied;
	}
}
#endif

UObject* USlimeHomeBuildEditorLibrary::CreateBlueprintFromActor(const FString& AssetPath, AActor* Actor)
{
#if WITH_EDITOR
	if (!Actor || AssetPath.IsEmpty())
	{
		return nullptr;
	}
	FKismetEditorUtilities::FCreateBlueprintFromActorParams Params;
	Params.bReplaceActor = false;
	Params.bKeepMobility = true;
	Params.bOpenBlueprint = false;
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprintFromActor(AssetPath, Actor, Params);
	SlimeHomeBuildEditorPrivate::PushSourceOverrides(Blueprint, Actor);
	return Blueprint;
#else
	return nullptr;
#endif
}

void USlimeHomeBuildEditorLibrary::FinishRenderAssets()
{
#if WITH_EDITOR
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	IStreamingManager::Get().StreamAllResources(5.f);
#endif
}
