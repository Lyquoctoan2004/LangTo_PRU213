// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "VillageStoryLibrary.generated.h"

/** Blueprint entry points into UVillageStorySubsystem. */
UCLASS()
class MUTABLESAMPLE_API UVillageStoryLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Forward BPI_Interactable::Interact from BP_VillageNPC / BP_VillageInteractable. */
	UFUNCTION(BlueprintCallable, Category = "Village|Story", meta = (DisplayName = "Village Handle Interact"))
	static void HandleInteract(AActor* Target, AActor* Interactor);
};
