// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "GroomRebindSubsystem.generated.h"

class AActor;
class UGroomComponent;
class UGroomBindingAsset;
class USkeletalMesh;

/**
 * Mutable regenerates the character mesh whenever the instance changes state
 * (entering the customize screen, switching tabs, changing a parameter).
 * The groom components keep pointing at a binding built for the previous mesh,
 * so the strands fall back to their rest pose and render as a speckled clump.
 *
 * Re-applying the binding is not an option either: projecting it onto a mesh
 * Mutable is regenerating trips an assert in HairStrandsMeshProjection.cpp.
 * So this subsystem strips the binding from every groom component and keeps the
 * strands attached to the head bone instead.
 */
UCLASS()
class MUTABLESAMPLE_API UGroomRebindSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	void PollGrooms();
	void OnActorSpawned(AActor* Actor);
	void UpdateActorGrooms(AActor* Actor);

	FTimerHandle PollTimer;
	FDelegateHandle ActorSpawnedHandle;
};
