// Copyright Epic Games, Inc. All Rights Reserved.

#include "VillageStoryLibrary.h"

#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "VillageStorySubsystem.h"

void UVillageStoryLibrary::HandleInteract(AActor* Target, AActor* Interactor)
{
	UGameInstance* GameInstance = Target ? Target->GetGameInstance() : nullptr;
	if (UVillageStorySubsystem* Story = GameInstance ? GameInstance->GetSubsystem<UVillageStorySubsystem>() : nullptr)
	{
		Story->HandleInteract(Target, Interactor);
	}
}
