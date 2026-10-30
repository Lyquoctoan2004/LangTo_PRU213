#include "CustomizedCharacterStart.h"

ACustomizedCharacterStart::ACustomizedCharacterStart(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Tags.AddUnique(TEXT("CustomizedCharacterStart"));
	VisualActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Lobby/Instances/CharacterActor.CharacterActor_C")));
}
