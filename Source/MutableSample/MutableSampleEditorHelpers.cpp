// Copyright Epic Games, Inc. All Rights Reserved.

#include "MutableSampleEditorHelpers.h"

#include "Engine/Blueprint.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"

namespace
{
#if WITH_EDITOR
	UBlueprint* ResolveBlueprint(UObject* Object)
	{
		if (UWorld* World = Cast<UWorld>(Object))
		{
			return World->PersistentLevel ? World->PersistentLevel->GetLevelScriptBlueprint(true) : nullptr;
		}
		return Cast<UBlueprint>(Object);
	}

	template <typename FuncType>
	void ForEachPin(UBlueprint* Blueprint, FuncType Func)
	{
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				for (UEdGraphPin* Pin : Node->Pins)
				{
					Func(Graph, Node, Pin);
				}
			}
		}
	}
#endif
}

FString UMutableSampleEditorHelpers::GetPropertyText(UObject* Object, FName PropertyName)
{
	FProperty* Property = Object ? Object->GetClass()->FindPropertyByName(PropertyName) : nullptr;
	if (!Property)
	{
		return FString();
	}

	FString Text;
	Property->ExportTextItem_InContainer(Text, Object, nullptr, Object, PPF_None);
	return Text;
}

bool UMutableSampleEditorHelpers::SetPropertyText(UObject* Object, FName PropertyName, const FString& Text)
{
	FProperty* Property = Object ? Object->GetClass()->FindPropertyByName(PropertyName) : nullptr;
	if (!Property)
	{
		return false;
	}

	Object->Modify();
	const TCHAR* Result = Property->ImportText_InContainer(*Text, Object, Object, PPF_None);
	Object->MarkPackageDirty();
	return Result != nullptr;
}

TArray<FString> UMutableSampleEditorHelpers::ListPinDefaults(UObject* Object, const FString& Filter)
{
	TArray<FString> Result;
#if WITH_EDITOR
	if (UBlueprint* Blueprint = ResolveBlueprint(Object))
	{
		ForEachPin(Blueprint, [&](UEdGraph* Graph, UEdGraphNode* Node, UEdGraphPin* Pin)
		{
			if (!Pin->DefaultValue.IsEmpty() && Pin->DefaultValue.Contains(Filter))
			{
				Result.Add(FString::Printf(TEXT("%s|%s|%s=%s"), *Graph->GetName(), *Node->GetName(), *Pin->PinName.ToString(), *Pin->DefaultValue));
			}
		});
	}
#endif
	return Result;
}

TArray<UObject*> UMutableSampleEditorHelpers::GetNestedObjects(UObject* Outer)
{
	TArray<UObject*> Result;
	if (Outer)
	{
		GetObjectsWithOuter(Outer, Result, true);
	}
	return Result;
}

UBlueprint* UMutableSampleEditorHelpers::GetLevelBlueprint(UWorld* World)
{
#if WITH_EDITOR
	return ResolveBlueprint(World);
#else
	return nullptr;
#endif
}

bool UMutableSampleEditorHelpers::SetPinDefault(UObject* Object, const FString& NodeName, const FString& PinName, const FString& Value)
{
	bool bFound = false;
#if WITH_EDITOR
	if (UBlueprint* Blueprint = ResolveBlueprint(Object))
	{
		ForEachPin(Blueprint, [&](UEdGraph* Graph, UEdGraphNode* Node, UEdGraphPin* Pin)
		{
			if (!bFound && Node->GetName() == NodeName && Pin->PinName.ToString() == PinName)
			{
				Node->Modify();
				Pin->DefaultValue = Value;
				Node->PinDefaultValueChanged(Pin);
				bFound = true;
			}
		});
		if (bFound)
		{
			Blueprint->MarkPackageDirty();
		}
	}
#endif
	return bFound;
}
