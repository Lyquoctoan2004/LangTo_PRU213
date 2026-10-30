#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CharacterCustomizationSubsystem.generated.h"

class UCustomizableObjectInstance;
class SWidget;
class UWorld;

/** Keeps the player's Mutable character alive while travelling between maps. */
UCLASS()
class MUTABLESAMPLE_API UCharacterCustomizationSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category="Character|Customization")
	bool CaptureCustomization(UCustomizableObjectInstance* SourceInstance);

	UFUNCTION(BlueprintPure, Category="Character|Customization")
	bool HasCustomization() const { return SavedInstance != nullptr; }

	UCustomizableObjectInstance* CloneCustomization(UObject* Outer) const;

	void ShowLoadingScreen();

private:
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void HideLoadingScreen();

	UPROPERTY(Transient)
	TObjectPtr<UCustomizableObjectInstance> SavedInstance;

	TSharedPtr<SWidget> LoadingWidget;
	FDelegateHandle PostLoadMapHandle;
	FTimerHandle HideLoadingTimer;
};
