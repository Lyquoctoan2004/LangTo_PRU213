#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "CustomizedCharacterStart.generated.h"

/**
 * Drop this PlayerStart into the gameplay map and move it to choose where the
 * customized player appears. The game-flow subsystem uses it automatically.
 */
UCLASS(Blueprintable)
class MUTABLESAMPLE_API ACustomizedCharacterStart : public APlayerStart
{
	GENERATED_BODY()

public:
	ACustomizedCharacterStart(const FObjectInitializer& ObjectInitializer);

	/** Actor used by the Mutable lobby to display the customized character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customized Character")
	TSoftClassPtr<AActor> VisualActorClass;

	/** Attach the Mutable visual to the existing gameplay pawn (movement/camera stay intact). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customized Character")
	bool bAttachVisualToPlayerPawn = true;

	/** Hide the mannequin mesh after the Mutable visual has spawned. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customized Character")
	bool bHideOriginalPawnMesh = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customized Character")
	FVector VisualRelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Customized Character")
	FRotator VisualRelativeRotation = FRotator::ZeroRotator;
};
