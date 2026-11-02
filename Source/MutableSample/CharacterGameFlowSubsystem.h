#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "CharacterGameFlowSubsystem.generated.h"

class ACameraActor;
class UAudioComponent;
class ACustomizedCharacterStart;
class UButton;
class UCharacterTalkWidget;
class UCustomizableObjectInstance;
class UCustomizableSkeletalComponent;
class USkeletalMeshComponent;
class UUserWidget;

/** Connects the Mutable lobby confirmation button to the main gameplay map. */
UCLASS()
class MUTABLESAMPLE_API UCharacterGameFlowSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Opens the talk area as "Xác nhận nói chuyện" does (also the console command Talk.Open). */
	void OpenTalk() { HandleTalkConfirmed(); }

private:
	void TryBindLobbyConfirmButton();
	/** Puts the configured MetaHuman face on the lobby character once Mutable has built its head (polled until then). */
	void TryAttachMetaHumanFace();
	void TrySpawnCustomizedPlayer();
	ACustomizedCharacterStart* FindOrCreateCharacterStart();
	void TravelToGameplayMap();

	UFUNCTION()
	void HandleCustomizationConfirmed();

	UFUNCTION()
	void HandleCustomizationUpdated(UCustomizableObjectInstance* UpdatedInstance);

	void ApplyDefaultCharacterAnimations();

	/** The lobby's Mutable character component, or null. */
	UCustomizableSkeletalComponent* FindLobbyCustomizableComponent() const;

	/** Adds "Xác nhận nói chuyện" next to the lobby's "Xác nhận tạo hình", copying its look. */
	void AddTalkButton(UUserWidget* Menu, UButton* ConfirmButton);

	/** Keeps the current appearance, hides the customization UI, frames the character's face and opens the talk area. */
	UFUNCTION()
	void HandleTalkConfirmed();

	/** Closes the talk area and returns to the customization screen. */
	void EndTalk();

	FTimerHandle PollTimer;
	FTimerHandle TravelTimer;
	FTimerHandle FaceTimer;
	float ElapsedTime = 0.0f;
	float FaceElapsedTime = 0.0f;
	bool bTravelRequested = false;
	bool bCharacterSpawned = false;
	TWeakObjectPtr<AActor> SpawnedVisual;
	TWeakObjectPtr<USkeletalMeshComponent> AnimationSourceMesh;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterTalkWidget> TalkWidget;

	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> TalkCamera;

	/** Lobby widgets hidden while talking and their visibility before, restored by EndTalk. */
	TArray<TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>> HiddenLobbyWidgets;
	TWeakObjectPtr<AActor> ViewTargetBeforeTalk;

	/** Music faded out while talking, faded back in by EndTalk. */
	TArray<TWeakObjectPtr<UAudioComponent>> MutedMusic;
};
