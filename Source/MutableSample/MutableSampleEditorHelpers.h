// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "MutableSampleEditorHelpers.generated.h"

class UBlueprint;
class UWorld;

/**
 * Small helpers for editor scripting (Python), e.g. to edit Blueprint struct properties
 * that Python cannot access directly.
 */
UCLASS()
class MUTABLESAMPLE_API UMutableSampleEditorHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Returns the value of a property as exported text (same format as copy/paste in the Details panel). */
	UFUNCTION(BlueprintCallable, Category = "MutableSample|Editor")
	static FString GetPropertyText(UObject* Object, FName PropertyName);

	/** Sets the value of a property from text (same format as copy/paste in the Details panel). */
	UFUNCTION(BlueprintCallable, Category = "MutableSample|Editor")
	static bool SetPropertyText(UObject* Object, FName PropertyName, const FString& Text);

	/**
	 * Lists the graph pins with a default value containing Filter, as "Graph|Node|Pin=Default".
	 * Object can be a Blueprint or a World (uses its level blueprint).
	 */
	UFUNCTION(BlueprintCallable, Category = "MutableSample|Editor")
	static TArray<FString> ListPinDefaults(UObject* Object, const FString& Filter);

	/** Returns all objects nested inside Outer (recursively), e.g. the widgets of a Widget Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "MutableSample|Editor")
	static TArray<UObject*> GetNestedObjects(UObject* Outer);

	/** Returns the level blueprint of a World (created if missing). */
	UFUNCTION(BlueprintCallable, Category = "MutableSample|Editor")
	static UBlueprint* GetLevelBlueprint(UWorld* World);

	/** Sets the default value of a graph pin, found by node and pin name (as returned by ListPinDefaults). */
	UFUNCTION(BlueprintCallable, Category = "MutableSample|Editor")
	static bool SetPinDefault(UObject* Object, const FString& NodeName, const FString& PinName, const FString& Value);
};
