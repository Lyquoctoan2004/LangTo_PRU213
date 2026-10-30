// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "LobbyAutoCustomizeSubsystem.generated.h"

/**
 * Skips the Lobby main menu and opens the character customization directly,
 * by clicking the main menu's "Customize" button (EditCharacterButton)
 * once the main menu widget has been added to the viewport.
 */
UCLASS()
class MUTABLESAMPLE_API ULobbyAutoCustomizeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	void TryEnterCustomize();
	class UUserWidget* FindMainMenu() const;

	FTimerHandle PollTimer;
	float ElapsedTime = 0.0f;
};
