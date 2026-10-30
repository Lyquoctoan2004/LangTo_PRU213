// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "UiTranslationSubsystem.generated.h"

/**
 * Shows the menu and the character customization screen in Vietnamese. Most of the customization text is not in any
 * widget asset: the Mutable GUI (DynamicallyGeneratedGUI_DGGUI) writes parameter, section and option names from
 * CO_Character into its text blocks at run time. A few times a second this looks at every text block in game worlds and
 * replaces an English label it knows with the Vietnamese one (see the table in UiTranslationSubsystem.cpp).
 */
UCLASS()
class MUTABLESAMPLE_API UUiTranslationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The Vietnamese label for an English one, or the input unchanged. Also usable from Blueprints. */
	UFUNCTION(BlueprintPure, Category = "Village|UI")
	static FText TranslateLabel(const FText& English);

private:
	bool Tick(float DeltaTime);

	FTSTicker::FDelegateHandle TickHandle;
};
