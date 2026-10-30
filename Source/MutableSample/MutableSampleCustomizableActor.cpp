// Copyright Epic Games, Inc. All Rights Reserved.

#include "MutableSampleCustomizableActor.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableSkeletalComponent.h"

#include "Components/SkeletalMeshComponent.h"


void AMutableSampleCustomizableActor::SetAlternateVisualizationMaterial(UMaterialInterface* InAlternateVisualizationMaterial)
{
	if (!InAlternateVisualizationMaterial)
	{
		return;
	}

	AlternateVisualizationMaterial = InAlternateVisualizationMaterial;
}


void AMutableSampleCustomizableActor::EnableAlternateVisualizationMaterial(bool bEnableAlternateVisualizationMaterial)
{
	bRemoveAlternateVisualizationMaterial = bAlternateVisualizationMaterialEnabled && !bEnableAlternateVisualizationMaterial;
	bAlternateVisualizationMaterialEnabled = bEnableAlternateVisualizationMaterial;

	if (UCustomizableObjectInstance* COInstance = GetInstance())
	{
		//Bind Instance Update delegate to Actor
		COInstance->UpdatedDelegate.AddUniqueDynamic(this, &AMutableSampleCustomizableActor::SwitchComponentMaterials);
		SwitchComponentMaterials(COInstance);
	}
}


TArray<USkeletalMeshComponent*> AMutableSampleCustomizableActor::GetActorSkeletalMeshComponents()
{
	TArray<USkeletalMeshComponent*> Components;
	GetComponents(USkeletalMeshComponent::StaticClass(), Components);

	return MoveTemp(Components);
}


TArray<UCustomizableSkeletalComponent*> AMutableSampleCustomizableActor::GetActorCustomizableMeshComponents()
{
	TArray<UCustomizableSkeletalComponent*> Components;
	GetComponents(UCustomizableSkeletalComponent::StaticClass(), Components);

	return MoveTemp(Components);
}


void AMutableSampleCustomizableActor::SwitchComponentMaterials(UCustomizableObjectInstance* Instance)
{
	if (!AlternateVisualizationMaterial)
	{
		return;
	}

	if (bAlternateVisualizationMaterialEnabled || bRemoveAlternateVisualizationMaterial)
	{
		UCustomizableObjectInstance* COInstance = GetInstance();

		if (!COInstance)
		{
			return;
		}

		//TArray<USkeletalMeshComponent*> ActorSkeletalMeshComponents = GetActorSkeletalMeshComponents();
		TArray<FName> ComponentNames = COInstance->GetComponentNames();
		TArray<TObjectPtr<USkeletalMeshComponent>> ActorSkeletalMeshComponents;

		for (const FName& ComponentName : ComponentNames)
		{
			ActorSkeletalMeshComponents.Add(GetSkeletalMeshComponent(ComponentName));
		}

		UCustomizableObjectSystem* CustomizableObjectSystem = UCustomizableObjectSystem::GetInstance();
		check(CustomizableObjectSystem);

		for (int32 ObjectComponentIndex = 0; ObjectComponentIndex < ActorSkeletalMeshComponents.Num(); ++ObjectComponentIndex)
		{
			int32 NumMaterials = ActorSkeletalMeshComponents[ObjectComponentIndex]->GetNumMaterials();

			if (bAlternateVisualizationMaterialEnabled)
			{
				for (int32 MatIndex = 0; MatIndex < NumMaterials; ++MatIndex)
				{
					ActorSkeletalMeshComponents[ObjectComponentIndex]->SetMaterial(MatIndex, AlternateVisualizationMaterial);
				}
			}
			else // Remove AlternateVisualizationMaterial
			{
				// check if original materials already overriden
				const TArray<UMaterialInterface*> OverrideMaterials = COInstance->GetSkeletalMeshComponentOverrideMaterials(ActorSkeletalMeshComponents[ObjectComponentIndex]->GetFName());
				const bool bUseOverrideMaterials = COInstance->GetCustomizableObject()->bEnableMeshCache && CustomizableObjectSystem->IsMeshCacheEnabled();

				if (bUseOverrideMaterials && OverrideMaterials.Num() > 0)
				{
					for (int32 MatIndex = 0; MatIndex < OverrideMaterials.Num(); ++MatIndex)
					{
						ActorSkeletalMeshComponents[ObjectComponentIndex]->SetMaterial(MatIndex, OverrideMaterials[MatIndex]);
					}
				}
				else
				{
					ActorSkeletalMeshComponents[ObjectComponentIndex]->EmptyOverrideMaterials();
				}

				bRemoveAlternateVisualizationMaterial = false;
			}
		}
	}
}


UCustomizableObjectInstance* AMutableSampleCustomizableActor::GetInstance()
{
	TArray<UCustomizableSkeletalComponent*> ActorCustomizableSkeletalComponents = GetActorCustomizableMeshComponents();

	for (const UCustomizableSkeletalComponent* Component : ActorCustomizableSkeletalComponents)
	{
		if (UCustomizableObjectInstance* COInstance = Component->CustomizableObjectInstance)
		{
			return COInstance;
		}
	}

	return nullptr;
}
