// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/CustomizableSkeletalMeshActor.h"

#include "MutableSampleCustomizableActor.generated.h"


UCLASS(MinimalAPI, ClassGroup = CustomizableObject, Blueprintable, ComponentWrapperClass, ConversionRoot, meta = (ChildCanTick))
class AMutableSampleCustomizableActor : public ACustomizableSkeletalMeshActor
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, Category = MutableSampleCustomizableActor)
	void SetAlternateVisualizationMaterial(UMaterialInterface* InAlternateVisualizationMaterial);

	UFUNCTION(BlueprintCallable, Category = MutableSampleCustomizableActor)
	void EnableAlternateVisualizationMaterial(bool bEnableAlternateVisualizationMaterial);

	UFUNCTION()
	void SwitchComponentMaterials(UCustomizableObjectInstance* Instance);

	/** @return the instance represented in all the actor components. */
	UCustomizableObjectInstance* GetInstance();

	TArray<USkeletalMeshComponent*> GetActorSkeletalMeshComponents();
	TArray<UCustomizableSkeletalComponent*> GetActorCustomizableMeshComponents();

	UPROPERTY()
	TObjectPtr<UMaterialInterface> AlternateVisualizationMaterial;

	bool bAlternateVisualizationMaterialEnabled = false;
	bool bRemoveAlternateVisualizationMaterial = false;
};
