// Copyright Epic Games, Inc. All Rights Reserved.

#include "MutableSampleGraphUtils.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

int32 UMutableSampleGraphUtils::RestoreGraphConnections(UObject* SourceGraph, UObject* TargetGraph)
{
#if WITH_EDITOR
	UEdGraph* Source = Cast<UEdGraph>(SourceGraph);
	UEdGraph* Target = Cast<UEdGraph>(TargetGraph);
	if (!Source || !Target || Source == Target) return 0;
	TMap<UEdGraphPin*, UEdGraphPin*> Pins;
	Target->Modify();
	for (UEdGraphNode* SourceNode : Source->Nodes)
	{
		if (!SourceNode) continue;
		UEdGraphNode* TargetNode = nullptr;
		for (UEdGraphNode* Candidate : Target->Nodes)
			if (Candidate && Candidate->GetFName() == SourceNode->GetFName()) { TargetNode = Candidate; break; }
		if (!TargetNode) continue;
		TargetNode->Modify();
		for (UEdGraphPin* SourcePin : SourceNode->Pins)
		{
			if (!SourcePin || SourcePin->bOrphanedPin) continue;
			UEdGraphPin* Match = nullptr;
			for (UEdGraphPin* Candidate : TargetNode->Pins)
				if (Candidate && Candidate->PinName == SourcePin->PinName && Candidate->Direction == SourcePin->Direction && !Candidate->bOrphanedPin) { Match = Candidate; break; }
			if (!Match)
			{
				Match = TargetNode->CreatePin(SourcePin->Direction, SourcePin->PinType, SourcePin->PinName);
				Match->PinFriendlyName = SourcePin->PinFriendlyName;
				Match->DefaultValue = SourcePin->DefaultValue;
				Match->DefaultObject = SourcePin->DefaultObject;
			}
			Pins.Add(SourcePin, Match);
		}
	}
	int32 Count = 0;
	for (const auto& Pair : Pins)
		if (Pair.Key->Direction == EGPD_Output)
			for (UEdGraphPin* Linked : Pair.Key->LinkedTo)
				if (UEdGraphPin** Other = Pins.Find(Linked))
				{
					Pair.Value->MakeLinkTo(*Other);
					++Count;
				}
	Target->MarkPackageDirty();
	return Count;
#else
	return 0;
#endif
}

namespace MutableSampleGraphUtils
{
	FProperty* FindProperty(const UObject* Object, const FString& PropertyName)
	{
		if (!Object)
		{
			return nullptr;
		}
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			if (It->GetName().Equals(PropertyName, ESearchCase::IgnoreCase))
			{
				return *It;
			}
		}
		return nullptr;
	}

#if WITH_EDITOR
	UEdGraphPin* FindPin(UEdGraphNode* Node, const FString& PinName)
	{
		if (!Node)
		{
			return nullptr;
		}
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->PinName.ToString().Equals(PinName, ESearchCase::IgnoreCase))
			{
				return Pin;
			}
		}
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->GetDisplayName().ToString().Equals(PinName, ESearchCase::IgnoreCase))
			{
				return Pin;
			}
		}
		return nullptr;
	}
#endif
}

TArray<UObject*> UMutableSampleGraphUtils::GetObjectsInPackage(UObject* Asset)
{
	TArray<UObject*> Result;
	if (Asset)
	{
		GetObjectsWithPackage(Asset->GetPackage(), Result, true);
	}
	return Result;
}

FString UMutableSampleGraphUtils::DumpProperties(UObject* Object)
{
	FString Out;
	if (!Object)
	{
		return Out;
	}
	for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
	{
		FString Value;
		It->ExportText_InContainer(0, Value, Object, nullptr, Object, PPF_None);
		Out += FString::Printf(TEXT("%s=%s\n"), *It->GetName(), *Value.Left(400));
	}
	return Out;
}

FString UMutableSampleGraphUtils::GetPropertyText(UObject* Object, const FString& PropertyName)
{
	FString Value;
	if (const FProperty* Property = MutableSampleGraphUtils::FindProperty(Object, PropertyName))
	{
		Property->ExportText_InContainer(0, Value, Object, nullptr, Object, PPF_None);
	}
	return Value;
}

bool UMutableSampleGraphUtils::SetPropertyText(UObject* Object, const FString& PropertyName, const FString& Value)
{
	FProperty* Property = MutableSampleGraphUtils::FindProperty(Object, PropertyName);
	if (!Property)
	{
		return false;
	}
#if WITH_EDITOR
	Object->Modify();
	Object->PreEditChange(Property);
#endif
	const TCHAR* Result = Property->ImportText_InContainer(*Value, Object, Object, PPF_None);
#if WITH_EDITOR
	FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
	Object->PostEditChangeProperty(Event);
#endif
	Object->MarkPackageDirty();
	return Result != nullptr;
}

FString UMutableSampleGraphUtils::DumpPins(UObject* Node)
{
	FString Out;
#if WITH_EDITOR
	UEdGraphNode* GraphNode = Cast<UEdGraphNode>(Node);
	if (!GraphNode)
	{
		return Out;
	}
	for (const UEdGraphPin* Pin : GraphNode->Pins)
	{
		if (!Pin)
		{
			continue;
		}
		FString Links;
		for (const UEdGraphPin* Linked : Pin->LinkedTo)
		{
			if (Linked && Linked->GetOwningNodeUnchecked())
			{
				Links += FString::Printf(TEXT("%s.%s; "), *Linked->GetOwningNodeUnchecked()->GetName(), *Linked->PinName.ToString());
			}
		}
		Out += FString::Printf(TEXT("%s [%s] %s%s%s -> %s\n"),
			*Pin->PinName.ToString(),
			Pin->Direction == EGPD_Input ? TEXT("in") : TEXT("out"),
			*Pin->PinType.PinCategory.ToString(),
			Pin->bHidden ? TEXT(" hidden") : TEXT(""),
			Pin->bOrphanedPin ? TEXT(" orphan") : TEXT(""),
			*Links);
	}
#endif
	return Out;
}

bool UMutableSampleGraphUtils::LinkPins(UObject* FromNode, const FString& FromPin, UObject* ToNode, const FString& ToPin)
{
#if WITH_EDITOR
	UEdGraphNode* A = Cast<UEdGraphNode>(FromNode);
	UEdGraphNode* B = Cast<UEdGraphNode>(ToNode);
	UEdGraphPin* PinA = MutableSampleGraphUtils::FindPin(A, FromPin);
	UEdGraphPin* PinB = MutableSampleGraphUtils::FindPin(B, ToPin);
	if (!PinA || !PinB || !A->GetGraph())
	{
		return false;
	}
	A->Modify();
	B->Modify();
	const bool bLinked = A->GetGraph()->GetSchema()->TryCreateConnection(PinA, PinB);
	A->MarkPackageDirty();
	return bLinked;
#else
	return false;
#endif
}

bool UMutableSampleGraphUtils::BreakPinLinks(UObject* Node, const FString& PinName)
{
#if WITH_EDITOR
	UEdGraphNode* GraphNode = Cast<UEdGraphNode>(Node);
	UEdGraphPin* Pin = MutableSampleGraphUtils::FindPin(GraphNode, PinName);
	if (!Pin || !GraphNode->GetGraph())
	{
		return false;
	}
	GraphNode->Modify();
	GraphNode->GetGraph()->GetSchema()->BreakPinLinks(*Pin, true);
	GraphNode->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

bool UMutableSampleGraphUtils::RemoveNode(UObject* Node)
{
#if WITH_EDITOR
	UEdGraphNode* GraphNode = Cast<UEdGraphNode>(Node);
	if (!GraphNode || !GraphNode->GetGraph())
	{
		return false;
	}
	UEdGraph* Graph = GraphNode->GetGraph();
	Graph->Modify();
	GraphNode->Modify();
	GraphNode->BreakAllNodeLinks();
	GraphNode->DestroyNode();
	Graph->NotifyGraphChanged();
	Graph->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

UObject* UMutableSampleGraphUtils::CreateGraphNode(UObject* Graph, UClass* NodeClass, int32 PosX, int32 PosY)
{
#if WITH_EDITOR
	UEdGraph* EdGraph = Cast<UEdGraph>(Graph);
	if (!EdGraph || !NodeClass || !NodeClass->IsChildOf(UEdGraphNode::StaticClass()))
	{
		return nullptr;
	}
	EdGraph->Modify();
	UEdGraphNode* Node = NewObject<UEdGraphNode>(EdGraph, NodeClass, NAME_None, RF_Transactional);
	EdGraph->AddNode(Node, false, false);
	Node->CreateNewGuid();
	Node->NodePosX = PosX;
	Node->NodePosY = PosY;
	Node->PostPlacedNewNode();
	Node->AllocateDefaultPins();
	EdGraph->NotifyGraphChanged();
	EdGraph->MarkPackageDirty();
	return Node;
#else
	return nullptr;
#endif
}

void UMutableSampleGraphUtils::ReconstructNode(UObject* Node)
{
#if WITH_EDITOR
	if (UEdGraphNode* GraphNode = Cast<UEdGraphNode>(Node))
	{
		GraphNode->Modify();
		GraphNode->ReconstructNode();
		GraphNode->MarkPackageDirty();
	}
#endif
}
