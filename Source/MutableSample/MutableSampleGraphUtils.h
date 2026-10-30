// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "MutableSampleGraphUtils.generated.h"

/** Reflection helpers so editor Python scripts can inspect and edit Customizable Object graphs. */
UCLASS()
class MUTABLESAMPLE_API UMutableSampleGraphUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MutableSample|Graph")
	static int32 RestoreGraphConnections(UObject* SourceGraph, UObject* TargetGraph);

	UFUNCTION(BlueprintCallable, Category = "MutableSample|Graph")
	static TArray<UObject*> GetObjectsInPackage(UObject* Asset);

	UFUNCTION(BlueprintCallable, Category = "MutableSample|Graph")
	static FString DumpProperties(UObject* Object);

	UFUNCTION(BlueprintCallable, Category = "MutableSample|Graph")
	static FString GetPropertyText(UObject* Object, const FString& PropertyName);

	UFUNCTION(BlueprintCallable, Category = "MutableSample|Graph")
	static bool SetPropertyText(UObject* Object, const FString& PropertyName, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "MutableSample|Graph")
	static FString DumpPins(UObject* Node);

	UFUNCTION(BlueprintCallable, Category = "MutableSample|Graph")
	static bool LinkPins(UObject* FromNode, const FString& FromPin, UObject* ToNode, const FString& ToPin);

	UFUNCTION(BlueprintCallable, Category = "MutableSample|Graph")
	static bool BreakPinLinks(UObject* Node, const FString& PinName);

	UFUNCTION(BlueprintCallable, Category = "MutableSample|Graph")
	static bool RemoveNode(UObject* Node);

	UFUNCTION(BlueprintCallable, Category = "MutableSample|Graph")
	static void ReconstructNode(UObject* Node);

	UFUNCTION(BlueprintCallable, Category = "MutableSample|Graph")
	static UObject* CreateGraphNode(UObject* Graph, UClass* NodeClass, int32 PosX, int32 PosY);
};
