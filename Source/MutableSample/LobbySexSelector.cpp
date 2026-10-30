#include "LobbySexSelector.h"

#include "Blueprint/WidgetTree.h"
#include "Animation/AnimInstance.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "GroomComponent.h"
#include "GroomAsset.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableSkeletalComponent.h"
#include "UObject/UnrealType.h"

ULobbySexSelector* ULobbySexSelector::AddToCustomizationTree(UObject* TreeObject, UObject* PanelObject)
{
	UWidgetTree* Tree = Cast<UWidgetTree>(TreeObject);
	UPanelWidget* Panel = Cast<UPanelWidget>(PanelObject);
	if (!Tree || !Panel) return nullptr;
	if (ULobbySexSelector* Existing = Cast<ULobbySexSelector>(Tree->FindWidget(TEXT("SexSelector")))) return Existing;
	Tree->Modify();
	Panel->Modify();
	ULobbySexSelector* Widget = Tree->ConstructWidget<ULobbySexSelector>(StaticClass(),TEXT("SexSelector"));
	if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Panel->InsertChildAt(1,Widget)))
		Slot->SetPadding(FMargin(0.f,6.f));
	Tree->MarkPackageDirty();
	return Widget;
}

TSharedRef<SWidget> ULobbySexSelector::RebuildWidget()
{
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"));
	if (!WidgetTree->RootWidget)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
		Size->SetHeightOverride(36.f);
		WidgetTree->RootWidget = Size;
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
		Background->SetBrush(RowBrush);
		if (!RowBrush.GetResourceObject()) Background->SetBrushColor(FLinearColor(0.86f,0.77f,0.58f));
		Background->SetPadding(FMargin(14.f,2.f));
		Size->SetContent(Background);
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Background->SetContent(Row);
		UFont* Font = LoadObject<UFont>(nullptr,TEXT("/Game/Fonts/PatrickHand/PatrickHand_Font.PatrickHand_Font"));
		auto Text = [&](const FString& Value)
		{
			UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
			T->SetText(FText::FromString(Value));
			T->SetFont(FSlateFontInfo(Font,20));
			T->SetColorAndOpacity(FSlateColor(FLinearColor(0.043735f,0.017642f,0.008023f)));
			return T;
		};
		UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Text(TEXT("Giới tính")));
		LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		auto Arrow = [&](const FString& Value)
		{
			UButton* B = WidgetTree->ConstructWidget<UButton>();
			FButtonStyle Style = B->GetStyle();
			Style.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
			Style.Hovered.DrawAs = ESlateBrushDrawType::NoDrawType;
			Style.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
			B->SetStyle(Style);
			B->SetContent(Text(Value));
			B->OnClicked.AddUniqueDynamic(this,&ULobbySexSelector::ToggleSex);
			Row->AddChildToHorizontalBox(B)->SetVerticalAlignment(VAlign_Center);
			return B;
		};
		Arrow(TEXT("<"));
		SelectedText = Text(TEXT("Nam"));
		SelectedText->SetJustification(ETextJustify::Center);
		UHorizontalBoxSlot* ChoiceSlot = Row->AddChildToHorizontalBox(SelectedText);
		ChoiceSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ChoiceSlot->SetVerticalAlignment(VAlign_Center);
		Arrow(TEXT(">"));
	}
	return Super::RebuildWidget();
}

void ULobbySexSelector::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Visible);
}

UUserWidget* ULobbySexSelector::FindCustomizationWidget() const
{
	for (UObject* Outer = GetOuter(); Outer; Outer = Outer->GetOuter())
		if (UUserWidget* W = Cast<UUserWidget>(Outer))
			if (W->GetClass()->FindPropertyByName(TEXT("CustomizableComponent"))) return W;
	return nullptr;
}

UCustomizableSkeletalComponent* ULobbySexSelector::FindCustomizationComponent(UUserWidget* Editor) const
{
	if (!Editor) return nullptr;
	FObjectPropertyBase* P = FindFProperty<FObjectPropertyBase>(Editor->GetClass(),TEXT("CustomizableComponent"));
	return P ? Cast<UCustomizableSkeletalComponent>(P->GetObjectPropertyValue_InContainer(Editor)) : nullptr;
}

void ULobbySexSelector::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry,DeltaTime);
	if (bSwitching) return;
	if (UCustomizableSkeletalComponent* C = FindCustomizationComponent(FindCustomizationWidget()))
		if (UCustomizableObjectInstance* I = C->GetCustomizableObjectInstance())
		{
			bFemaleSelected = I->GetCustomizableObject() == FemaleObject;
			if (bFemaleSelected) FemaleInstance = I; else MaleInstance = I;
			if (SelectedText) SelectedText->SetText(FText::FromString(bFemaleSelected ? TEXT("Nữ") : TEXT("Nam")));
            TInlineComponentArray<UGroomComponent*> SexGrooms(C->GetOwner());
            for (UGroomComponent* Groom : SexGrooms)
            {
                const UGroomAsset* Asset = Groom->GroomAsset;
                const bool bFemaleHair = Asset && Asset->GetPathName().StartsWith(TEXT("/Game/MetaHumans/Female/Grooms/"));
                if (bFemaleSelected) Groom->SetVisibility(bFemaleHair);
                else if (bFemaleHair) Groom->SetVisibility(false);
                else if (OriginalGroomVisibility.Contains(Groom)) Groom->SetVisibility(OriginalGroomVisibility.FindChecked(Groom));
            }
		}
}

void ULobbySexSelector::ToggleSex() { SelectFemale(!bFemaleSelected); }

void ULobbySexSelector::SelectFemale(bool bFemale)
{
	if (bSwitching || bFemale == bFemaleSelected || IsDesignTime()) return;
	UUserWidget* Editor = FindCustomizationWidget();
	UCustomizableSkeletalComponent* Component = FindCustomizationComponent(Editor);
	UCustomizableObject* Target = bFemale ? FemaleObject : MaleObject;
	if (!Component || !Target) return;
#if WITH_EDITOR
	if (!Target->IsCompiled())
	{
		FCompileParams Params;
		Params.bAsync = false;
		Params.bSkipIfNotOutOfDate = false;
		Params.OptimizationLevel = ECustomizableObjectOptimizationLevel::None;
		Target->Compile(Params);
	}
#endif
	if (!Target->IsCompiled()) return;
	UCustomizableObjectInstance* Old = Component->GetCustomizableObjectInstance();
	if (!Old) return;
	if (bFemaleSelected) FemaleInstance = Old; else MaleInstance = Old;
	UCustomizableObjectInstance* Next = bFemale ? FemaleInstance.Get() : MaleInstance.Get();
	if (!Next)
	{
		Next = Target->CreateInstance();
		if (bFemale) FemaleInstance = Next; else MaleInstance = Next;
	}
	if (!Next) return;
	bSwitching = true;
	const FString State = Old->GetCurrentState();
	if (UFunction* DestructEvent = Editor->FindFunction(TEXT("Destruct"))) Editor->ProcessEvent(DestructEvent,nullptr);
	TInlineComponentArray<UCustomizableSkeletalComponent*> Components(Component->GetOwner());
	for (UCustomizableSkeletalComponent* C : Components)
		if (C->GetCustomizableObjectInstance() == Old) C->SetCustomizableObjectInstance(Next);
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(Component->GetOwner());
	USkeletalMeshComponent* FemaleBody = Cast<USkeletalMeshComponent>(Component->GetAttachParent());
	UClass* FemaleIdleClass = bFemale ? LoadClass<UAnimInstance>(nullptr,TEXT("/Game/Character/Female/Animation/ABP_FemaleIdle.ABP_FemaleIdle_C")) : nullptr;
	if (OriginalAnimations.IsEmpty())
		for (USkeletalMeshComponent* M : Meshes)
			OriginalAnimations.Add({M,M->GetAnimClass(),M->LeaderPoseComponent.Get(),static_cast<uint8>(M->GetAnimationMode())});
	for (const FOriginalAnimation& A : OriginalAnimations)
		if (USkeletalMeshComponent* M = A.Component.Get())
		{
			M->SetLeaderPoseComponent(bFemale ? nullptr : A.Leader.Get());
			if (bFemale)
			{
				if (M == FemaleBody && FemaleIdleClass)
				{
					M->SetAnimInstanceClass(FemaleIdleClass);
				}
				else
				{
					M->SetAnimationMode(EAnimationMode::AnimationSingleNode);
					M->SetAnimation(nullptr);
				}
			}
			else
			{
				M->SetAnimationMode(static_cast<EAnimationMode::Type>(A.Mode));
				M->SetAnimInstanceClass(A.Class.Get());
			}
		}
	if (bFemale && FemaleBody)
		for (USkeletalMeshComponent* M : Meshes)
			if (M != FemaleBody && M->GetFName() == TEXT("Head")) M->SetLeaderPoseComponent(FemaleBody);
	TInlineComponentArray<UGroomComponent*> Grooms(Component->GetOwner());
	for (UGroomComponent* Groom : Grooms)
	{
		if (!OriginalGroomVisibility.Contains(Groom)) OriginalGroomVisibility.Add(Groom,Groom->IsVisible());
		Groom->SetVisibility(bFemale ? false : OriginalGroomVisibility.FindChecked(Groom));
	}
	bFemaleSelected = bFemale;
	if (SelectedText) SelectedText->SetText(FText::FromString(bFemale ? TEXT("Nữ") : TEXT("Nam")));
	Next->SetCurrentState(State);
	for (const FName Name : {FName(TEXT("InstanceSelector")),FName(TEXT("ScrollBox")),FName(TEXT("HorizontalStateButtonsBox"))})
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Editor->GetWidgetFromName(Name))) Panel->ClearChildren();
	for (const FName Name : {FName(TEXT("GeneratedWidgets")),FName(TEXT("SectionsMap")),FName(TEXT("AllUIStateNames"))})
		if (FProperty* P = Editor->GetClass()->FindPropertyByName(Name))
			P->CopyCompleteValue_InContainer(Editor,Editor->GetClass()->GetDefaultObject());
	if (UFunction* PreConstructEvent = Editor->FindFunction(TEXT("PreConstruct")))
	{
		struct { bool IsDesignTime = false; } Params;
		Editor->ProcessEvent(PreConstructEvent,&Params);
	}
	if (UFunction* ConstructEvent = Editor->FindFunction(TEXT("Construct"))) Editor->ProcessEvent(ConstructEvent,nullptr);
	Next->SetBuildParameterRelevancy(true);
	Next->UpdateSkeletalMeshAsync(true,true);
	bSwitching = false;
}
