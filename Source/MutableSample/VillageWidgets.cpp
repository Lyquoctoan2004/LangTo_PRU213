// Copyright Epic Games, Inc. All Rights Reserved.

#include "VillageWidgets.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "VillageStorySubsystem.h"

#define LOCTEXT_NAMESPACE "VillageWidgets"

namespace
{
	// Montserrat ships with the project and covers Vietnamese diacritics.
	const TCHAR* BoldFont = TEXT("/Game/Fonts/Montserrat/Montserrat-Bold_Font.Montserrat-Bold_Font");
	const TCHAR* RegularFont = TEXT("/Game/Fonts/Montserrat/Montserrat-Regular_Font.Montserrat-Regular_Font");

	const FLinearColor Ink(0.97f, 0.93f, 0.84f);
	const FLinearColor Gold(0.96f, 0.76f, 0.36f);
	const FLinearColor Muted(0.75f, 0.70f, 0.62f);
	const FLinearColor Warn(1.0f, 0.55f, 0.40f);
	const FLinearColor Healthy(0.55f, 0.9f, 0.55f);
	const FLinearColor PanelColor(0.07f, 0.04f, 0.03f, 0.82f);

	UCanvasPanelSlot* Place(UCanvasPanel* Root, UWidget* Widget, FAnchors Anchors, FVector2D Alignment, FVector2D Position, FVector2D Size)
	{
		UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Widget);
		Slot->SetAnchors(Anchors);
		Slot->SetAlignment(Alignment);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		return Slot;
	}

	FSlateChildSize FillSize(float Value)
	{
		FSlateChildSize Size(ESlateSizeRule::Fill);
		Size.Value = Value;
		return Size;
	}

	bool IsAdvanceKey(const FKey& Key)
	{
		return Key == EKeys::E || Key == EKeys::SpaceBar || Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom;
	}

	bool IsCloseKey(const FKey& Key)
	{
		return Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right;
	}

	/** Track pieces stretch to the track's height; X and width are set in pixels. */
	void SetSpan(UBorder* Piece, float X, float Width, float Bleed = 0.f)
	{
		if (UCanvasPanelSlot* Slot = Piece ? Cast<UCanvasPanelSlot>(Piece->Slot) : nullptr)
		{
			Slot->SetOffsets(FMargin(X, -Bleed, FMath::Max(Width, 0.f), -Bleed));
		}
	}
}

void FVillageTrack::SetZones(float GoodMin, float GoodMax, float PerfectMin, float PerfectMax) const
{
	SetSpan(Good, GoodMin * Width, (GoodMax - GoodMin) * Width);
	SetSpan(Perfect, PerfectMin * Width, (PerfectMax - PerfectMin) * Width);
}

void FVillageTrack::SetMarker(float Position) const
{
	SetSpan(Marker, FMath::Clamp(Position, 0.f, 1.f) * Width - 3.f, 6.f, 6.f);
}

void UVillageButtonProxy::HandleClicked()
{
	if (Callback)
	{
		Callback();
	}
}

TSharedRef<SWidget> UVillageWidgetBase::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;
		BuildTree(Root);
	}
	return Super::RebuildWidget();
}

UTextBlock* UVillageWidgetBase::MakeText(const FText& Text, int32 Size, const FLinearColor& Color, bool bBold)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>();
	FSlateFontInfo Font(LoadObject<UObject>(nullptr, bBold ? BoldFont : RegularFont), Size);
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Color));
	Block->SetText(Text);
	Block->SetAutoWrapText(true);
	return Block;
}

UButton* UVillageWidgetBase::MakeButton(const FText& Label, TFunction<void()> OnClicked, int32 Size)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>();
	Button->SetBackgroundColor(FLinearColor(0.35f, 0.18f, 0.10f, 0.95f));
	UTextBlock* Text = MakeText(Label, Size, Ink, true);
	Text->SetAutoWrapText(false);
	Button->AddChild(Text);
	if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(Text->Slot))
	{
		LabelSlot->SetPadding(FMargin(14.f, 6.f));
	}
	UVillageButtonProxy* Proxy = NewObject<UVillageButtonProxy>(this);
	Proxy->Callback = MoveTemp(OnClicked);
	Button->OnClicked.AddDynamic(Proxy, &UVillageButtonProxy::HandleClicked);
	Proxies.Add(Proxy);
	return Button;
}

UBorder* UVillageWidgetBase::MakePanel(const FLinearColor& Color)
{
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(Color);
	Panel->SetPadding(FMargin(22.f, 16.f));
	return Panel;
}

USizeBox* UVillageWidgetBase::MakeTrack(FVillageTrack& Out, float Width, float Height)
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
	Box->SetWidthOverride(Width);
	Box->SetHeightOverride(Height);
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	Box->AddChild(Canvas);
	auto Piece = [&](const FLinearColor& Color)
	{
		UBorder* Border = WidgetTree->ConstructWidget<UBorder>();
		Border->SetBrushColor(Color);
		Border->SetPadding(FMargin(0.f));
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Border);
		Slot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 1.f));
		return Border;
	};
	UBorder* Back = Piece(FLinearColor(0.20f, 0.13f, 0.09f, 1.f));
	Out.Good = Piece(FLinearColor(0.62f, 0.47f, 0.22f, 1.f));
	Out.Perfect = Piece(FLinearColor(0.35f, 0.75f, 0.38f, 1.f));
	Out.Marker = Piece(Ink);
	Out.Width = Width;
	SetSpan(Back, 0.f, Width);
	Out.SetZones(0.f, 0.f, 0.f, 0.f);
	Out.SetMarker(0.f);
	return Box;
}

// ---------------------------------------------------------------- HUD

void UVillageHUDWidget::BuildTree(UCanvasPanel* Root)
{
	TrackerPanel = MakePanel(FLinearColor(0.05f, 0.03f, 0.02f, 0.62f));
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	TrackerTitle = MakeText(FText::GetEmpty(), 19, Gold, true);
	TrackerLines = WidgetTree->ConstructWidget<UVerticalBox>();
	Box->AddChildToVerticalBox(TrackerTitle)->SetPadding(FMargin(0, 0, 0, 6));
	Box->AddChildToVerticalBox(TrackerLines);
	TrackerPanel->SetContent(Box);
	UCanvasPanelSlot* TrackerSlot = Place(Root, TrackerPanel, FAnchors(0.f, 0.f), FVector2D(0, 0), FVector2D(28, 230), FVector2D(390, 0));
	TrackerSlot->SetAutoSize(true);
	TrackerPanel->SetVisibility(ESlateVisibility::Collapsed);

	ToastPanel = MakePanel(PanelColor);
	ToastText = MakeText(FText::GetEmpty(), 20, Ink, true);
	ToastText->SetJustification(ETextJustify::Center);
	ToastPanel->SetContent(ToastText);
	UCanvasPanelSlot* ToastSlot = Place(Root, ToastPanel, FAnchors(0.5f, 0.f), FVector2D(0.5f, 0), FVector2D(0, 120), FVector2D(0, 0));
	ToastSlot->SetAutoSize(true);
	ToastPanel->SetVisibility(ESlateVisibility::Collapsed);

	UBorder* HintPanel = MakePanel(FLinearColor(0.05f, 0.03f, 0.02f, 0.65f));
	HintPanel->SetPadding(FMargin(14.f, 8.f));
	UTextBlock* HintLabel = MakeText(LOCTEXT("HudKeyHints", "[E] Tương tác   ·   [B] Cửa hàng Tơ Lụa   ·   [I] Túi đồ   ·   [M] Bản đồ   ·   [Esc] Cài đặt"), 14, Muted, false);
	HintLabel->SetAutoWrapText(false);
	HintPanel->SetContent(HintLabel);
	Place(Root, HintPanel, FAnchors(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(28.f, -24.f), FVector2D::ZeroVector)->SetAutoSize(true);

	// Guide markers: a pool of small labels moved over the screen each frame.
	for (int32 i = 0; i < MaxGuideMarkers; ++i)
	{
		UBorder* Panel = MakePanel(FLinearColor(0.05f, 0.03f, 0.02f, 0.72f));
		Panel->SetPadding(FMargin(10.f, 4.f));
		UTextBlock* Text = MakeText(FText::GetEmpty(), 16, Gold, true);
		Text->SetAutoWrapText(false);
		Panel->SetContent(Text);
		Place(Root, Panel, FAnchors(0.f, 0.f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector, FVector2D::ZeroVector)->SetAutoSize(true);
		Panel->SetVisibility(ESlateVisibility::Collapsed);
		GuidePanels.Add(Panel);
		GuideTexts.Add(Text);
	}
	GuideShown.SetNum(MaxGuideMarkers);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UVillageHUDWidget::SetGuideMarkers(const TArray<FVillageGuideMarker>& Markers)
{
	GuideMarkers = Markers;
}

void UVillageHUDWidget::UpdateGuide()
{
	const APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
	const FVector2D Size = Scale > 0.f ? UWidgetLayoutLibrary::GetViewportSize(this) / Scale : FVector2D::ZeroVector;
	const bool bReady = Pawn && PC->PlayerCameraManager && Size.X > 1.f && Size.Y > 1.f;

	constexpr float HideWithinCm = 600.f; // the item's own nameplate takes over up close
	const FVector2D Margin(110.f, 70.f);
	for (int32 i = 0; i < GuidePanels.Num(); ++i)
	{
		const bool bUsed = bReady && GuideMarkers.IsValidIndex(i);
		const double DistCm = bUsed ? FVector::Dist(Pawn->GetActorLocation(), GuideMarkers[i].Location) : 0.0;
		if (!bUsed || DistCm < HideWithinCm)
		{
			GuidePanels[i]->SetVisibility(ESlateVisibility::Collapsed);
			continue;
		}
		const FVillageGuideMarker& Marker = GuideMarkers[i];

		FVector2D Screen;
		FVector2D Pos = FVector2D::ZeroVector;
		const bool bProjected = PC->ProjectWorldLocationToScreen(Marker.Location, Screen, true);
		if (bProjected)
		{
			Pos = Screen / Scale;
		}
		const bool bInside = bProjected && Pos.X > Margin.X && Pos.X < Size.X - Margin.X && Pos.Y > Margin.Y && Pos.Y < Size.Y - Margin.Y;
		FString Arrow;
		if (!bInside)
		{
			// Off screen: sit on the screen edge in the direction to turn (camera space: X forward, Y right, Z up).
			const FVector Rel = PC->PlayerCameraManager->GetCameraRotation().UnrotateVector(Marker.Location - PC->PlayerCameraManager->GetCameraLocation());
			FVector2D Dir(Rel.Y, -Rel.Z);
			if (Dir.SizeSquared() < 1.f)
			{
				Dir = FVector2D(0.f, 1.f);
			}
			Dir.Normalize();
			const FVector2D Half = Size * 0.5f - Margin;
			const double T = FMath::Min(Half.X / FMath::Max(FMath::Abs(Dir.X), 1e-4), Half.Y / FMath::Max(FMath::Abs(Dir.Y), 1e-4));
			Pos = Size * 0.5f + Dir * T;
			Arrow = FMath::Abs(Dir.X) > FMath::Abs(Dir.Y) ? (Dir.X < 0.f ? TEXT("<<  ") : TEXT("  >>")) : TEXT("");
			if (Arrow.IsEmpty() && Rel.X < 0.f)
			{
				Arrow = TEXT("(phía sau)  ");
			}
		}

		const int32 Metres = FMath::RoundToInt(DistCm / 100.0);
		const FString Label = Arrow.StartsWith(TEXT("  "))
			? FString::Printf(TEXT("%s · %d m%s"), *Marker.Name.ToString(), Metres, *Arrow)
			: FString::Printf(TEXT("%s%s · %d m"), *Arrow, *Marker.Name.ToString(), Metres);
		if (GuideShown[i] != Label)
		{
			GuideShown[i] = Label;
			GuideTexts[i]->SetText(FText::FromString(Label));
		}
		if (UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(GuidePanels[i]->Slot))
		{
			MarkerSlot->SetPosition(Pos);
		}
		GuidePanels[i]->SetRenderOpacity(bInside ? 0.85f : 1.f);
		GuidePanels[i]->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UVillageHUDWidget::SetTracker(const FText& Title, const TArray<FText>& Lines)
{
	if (!TrackerPanel)
	{
		return;
	}
	TrackerTitle->SetText(Title);
	TrackerLines->ClearChildren();
	for (const FText& Line : Lines)
	{
		UTextBlock* Text = MakeText(Line, 16, Ink);
		TrackerLines->AddChildToVerticalBox(Text)->SetPadding(FMargin(0, 2));
	}
	TrackerPanel->SetVisibility(Title.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UVillageHUDWidget::ShowToast(const FText& Message, float Seconds)
{
	if (!ToastPanel)
	{
		return;
	}
	ToastText->SetText(Message);
	ToastPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	ToastPanel->SetRenderOpacity(1.f);
	ToastTimeLeft = Seconds;
}

void UVillageHUDWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	UpdateGuide();
	if (ToastTimeLeft > 0.f)
	{
		ToastTimeLeft -= DeltaTime;
		ToastPanel->SetRenderOpacity(FMath::Clamp(ToastTimeLeft / 0.5f, 0.f, 1.f));
		if (ToastTimeLeft <= 0.f)
		{
			ToastPanel->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

// ---------------------------------------------------------------- Dialogue

void UVillageDialogueWidget::BuildTree(UCanvasPanel* Root)
{
	UBorder* Panel = MakePanel(PanelColor);
	Panel->SetPadding(FMargin(34.f, 22.f));
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	SpeakerText = MakeText(FText::GetEmpty(), 22, Gold, true);
	BodyText = MakeText(FText::GetEmpty(), 21, Ink);
	ChoiceBox = WidgetTree->ConstructWidget<UVerticalBox>();
	HintText = MakeText(LOCTEXT("Continue", "[E]  Tiếp tục"), 15, Muted);
	HintText->SetJustification(ETextJustify::Right);
	Box->AddChildToVerticalBox(SpeakerText)->SetPadding(FMargin(0, 0, 0, 8));
	Box->AddChildToVerticalBox(BodyText)->SetPadding(FMargin(0, 0, 0, 10));
	Box->AddChildToVerticalBox(ChoiceBox);
	Box->AddChildToVerticalBox(HintText);
	Panel->SetContent(Box);
	UCanvasPanelSlot* PanelSlot = Place(Root, Panel, FAnchors(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0, -48), FVector2D(1180, 0));
	PanelSlot->SetAutoSize(false);
	PanelSlot->SetSize(FVector2D(1180, 250));
	SetIsFocusable(true);
}

void UVillageDialogueWidget::Play(const FVillageDialogueRow& Dialogue)
{
	Current = Dialogue;
	bChoosing = false;
	ChoiceBox->ClearChildren();
	ShowLine(0);
}

FString UVillageDialogueWidget::DebugDescribe() const
{
	FString Out = FString::Printf(TEXT("%s: %s"), *SpeakerText->GetText().ToString(), *FullText);
	if (bChoosing)
	{
		for (int32 i = 0; i < Current.Choices.Num(); ++i)
		{
			Out += FString::Printf(TEXT(" [%d] %s"), i + 1, *Current.Choices[i].Text.ToString());
		}
	}
	return Out;
}

void UVillageDialogueWidget::ShowLine(int32 Index)
{
	LineIndex = Index;
	if (!Current.Lines.IsValidIndex(Index))
	{
		ShowChoices();
		return;
	}
	const FVillageDialogueLine& Line = Current.Lines[Index];
	SpeakerText->SetText(Line.Speaker);
	FullText = Line.Text.ToString();
	Revealed = 0.f;
	BodyText->SetText(FText::GetEmpty());
	HintText->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UVillageDialogueWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (!bChoosing && Revealed < FullText.Len())
	{
		Revealed = FMath::Min<float>(Revealed + DeltaTime * 55.f, FullText.Len());
		BodyText->SetText(FText::FromString(FullText.Left(FMath::FloorToInt(Revealed))));
	}
}

void UVillageDialogueWidget::Advance()
{
	if (bChoosing)
	{
		return;
	}
	if (Revealed < FullText.Len())
	{
		Revealed = FullText.Len();
		BodyText->SetText(FText::FromString(FullText));
		return;
	}
	ShowLine(LineIndex + 1);
}

void UVillageDialogueWidget::ShowChoices()
{
	if (Current.Choices.Num() == 0)
	{
		if (UVillageStorySubsystem* S = Story.Get())
		{
			S->OnDialogueFinished(Current.Actions);
		}
		return;
	}
	bChoosing = true;
	HintText->SetVisibility(ESlateVisibility::Collapsed);
	ChoiceBox->ClearChildren();
	for (int32 i = 0; i < Current.Choices.Num(); ++i)
	{
		const FText Label = FText::Format(LOCTEXT("ChoiceFmt", "{0}.  {1}"), FText::AsNumber(i + 1), Current.Choices[i].Text);
		UButton* Button = MakeButton(Label, [this, i]() { Choose(i); }, 18);
		ChoiceBox->AddChildToVerticalBox(Button)->SetPadding(FMargin(0, 3));
	}
}

void UVillageDialogueWidget::Choose(int32 Index)
{
	if (!bChoosing || !Current.Choices.IsValidIndex(Index))
	{
		return;
	}
	bChoosing = false;
	const FVillageDialogueChoice Choice = Current.Choices[Index];
	if (UVillageStorySubsystem* S = Story.Get())
	{
		S->OnDialogueChoice(Choice);
	}
}

FReply UVillageDialogueWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	const FKey Key = KeyEvent.GetKey();
	if (bChoosing)
	{
		const FKey Numbers[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four };
		for (int32 i = 0; i < 4; ++i)
		{
			if (Key == Numbers[i])
			{
				Choose(i);
				return FReply::Handled();
			}
		}
		return FReply::Handled();
	}
	if (IsAdvanceKey(Key))
	{
		Advance();
	}
	return FReply::Handled();
}

FReply UVillageDialogueWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if (!bChoosing && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Advance();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(Geometry, MouseEvent);
}

// ---------------------------------------------------------------- Silkworm tray

void UVillageSilkwormWidget::AddStat(UVerticalBox* Box, const FText& Label, TObjectPtr<UProgressBar>& Bar, TObjectPtr<UTextBlock>& Value)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
	UTextBlock* Name = MakeText(Label, 17, Ink, true);
	Name->SetAutoWrapText(false);
	Row->AddChildToHorizontalBox(Name)->SetSize(FillSize(0.32f));
	Bar = WidgetTree->ConstructWidget<UProgressBar>();
	Bar->SetFillColorAndOpacity(Gold);
	UHorizontalBoxSlot* BarSlot = Row->AddChildToHorizontalBox(Bar);
	BarSlot->SetSize(FillSize(0.5f));
	BarSlot->SetVerticalAlignment(VAlign_Center);
	BarSlot->SetPadding(FMargin(8, 4));
	Value = MakeText(FText::GetEmpty(), 16, Ink);
	Value->SetAutoWrapText(false);
	Row->AddChildToHorizontalBox(Value)->SetSize(FillSize(0.18f));
	Box->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 3));
}

void UVillageSilkwormWidget::BuildTree(UCanvasPanel* Root)
{
	UBorder* Panel = MakePanel(PanelColor);
	Panel->SetPadding(FMargin(30.f, 24.f));
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	Box->AddChildToVerticalBox(MakeText(LOCTEXT("TrayTitle", "Nuôi tằm – Nong tằm"), 26, Gold, true))->SetPadding(FMargin(0, 0, 0, 12));
	AddStat(Box, LOCTEXT("Health", "Sức khỏe"), HealthBar, HealthValue);
	AddStat(Box, LOCTEXT("Hunger", "Độ đói"), HungerBar, HungerValue);
	AddStat(Box, LOCTEXT("Growth", "Phát triển"), GrowthBar, GrowthValue);
	AddStat(Box, LOCTEXT("Clean", "Vệ sinh khay"), CleanBar, CleanValue);
	ClimateText = MakeText(FText::GetEmpty(), 17, Ink);
	LeavesText = MakeText(FText::GetEmpty(), 17, Ink);
	WarningText = MakeText(FText::GetEmpty(), 17, Warn, true);
	ResultText = MakeText(FText::GetEmpty(), 16, Muted);
	Box->AddChildToVerticalBox(ClimateText)->SetPadding(FMargin(0, 8, 0, 2));
	Box->AddChildToVerticalBox(LeavesText)->SetPadding(FMargin(0, 2));
	Box->AddChildToVerticalBox(WarningText)->SetPadding(FMargin(0, 8, 0, 4));
	Box->AddChildToVerticalBox(ResultText)->SetPadding(FMargin(0, 0, 0, 12));

	struct FAction { FText Label; int32 Id; };
	const FAction Actions[] = {
		{ LOCTEXT("Feed", "Cho ăn (3 lá dâu)"), 0 },
		{ LOCTEXT("CleanTray", "Dọn khay"), 1 },
		{ LOCTEXT("Vent", "Mở cửa thông gió"), 2 },
		{ LOCTEXT("Mist", "Phun sương"), 3 },
		{ LOCTEXT("Warm", "Đóng cửa giữ ấm"), 4 },
		{ LOCTEXT("Leave", "Rời đi [Esc]"), 5 },
	};
	UHorizontalBox* RowA = WidgetTree->ConstructWidget<UHorizontalBox>();
	UHorizontalBox* RowB = WidgetTree->ConstructWidget<UHorizontalBox>();
	for (int32 i = 0; i < UE_ARRAY_COUNT(Actions); ++i)
	{
		const int32 Id = Actions[i].Id;
		UButton* Button = MakeButton(Actions[i].Label, [this, Id]()
		{
			if (UVillageStorySubsystem* S = Story.Get())
			{
				Id == 5 ? S->CloseSilkworm() : S->SilkwormAction(Id);
			}
		}, 16);
		(i < 3 ? RowA : RowB)->AddChildToHorizontalBox(Button)->SetPadding(FMargin(0, 0, 8, 0));
	}
	Box->AddChildToVerticalBox(RowA)->SetPadding(FMargin(0, 0, 0, 8));
	Box->AddChildToVerticalBox(RowB);
	Panel->SetContent(Box);
	UCanvasPanelSlot* PanelSlot = Place(Root, Panel, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D(0, 0), FVector2D(760, 0));
	PanelSlot->SetAutoSize(true);
	SetIsFocusable(true);
}

void UVillageSilkwormWidget::Refresh(const FVillageSilkwormState& State, int32 Leaves, const TArray<FText>& Warnings, const FText& LastResult)
{
	auto Set = [](UProgressBar* Bar, UTextBlock* Value, float V, bool bInverse)
	{
		Bar->SetPercent(V / 100.f);
		const bool bBad = bInverse ? V > 70.f : V < 40.f;
		Bar->SetFillColorAndOpacity(bBad ? Warn : Gold);
		Value->SetText(FText::Format(LOCTEXT("Pct", "{0}%"), FText::AsNumber(FMath::RoundToInt(V))));
	};
	Set(HealthBar, HealthValue, State.Health, false);
	Set(HungerBar, HungerValue, State.Hunger, true);
	Set(GrowthBar, GrowthValue, State.Growth, false);
	Set(CleanBar, CleanValue, State.Cleanliness, false);
	ClimateText->SetText(FText::Format(LOCTEXT("Climate", "Nhiệt độ: {0}°C   ·   Độ ẩm: {1}%   (tốt: 24–30°C, 55–75%)"),
		FText::AsNumber(FMath::RoundToInt(State.Temperature)), FText::AsNumber(FMath::RoundToInt(State.Humidity))));
	LeavesText->SetText(FText::Format(LOCTEXT("Leaves", "Lá dâu trong túi: {0}"), FText::AsNumber(Leaves)));
	FString WarningLines;
	for (const FText& W : Warnings)
	{
		WarningLines += TEXT("⚠ ") + W.ToString() + TEXT("\n");
	}
	WarningText->SetText(WarningLines.IsEmpty() ? LOCTEXT("AllGood", "Tằm khỏe mạnh – môi trường tốt.") : FText::FromString(WarningLines.TrimEnd()));
	WarningText->SetColorAndOpacity(FSlateColor(WarningLines.IsEmpty() ? FLinearColor(0.55f, 0.9f, 0.55f) : Warn));
	ResultText->SetText(LastResult);
}

FReply UVillageSilkwormWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (KeyEvent.GetKey() == EKeys::Escape || KeyEvent.GetKey() == EKeys::E)
	{
		if (UVillageStorySubsystem* S = Story.Get())
		{
			S->CloseSilkworm();
		}
	}
	return FReply::Handled();
}

// ---------------------------------------------------------------- Timing bar

void UVillageTimingWidget::BuildTree(UCanvasPanel* Root)
{
	UBorder* Panel = MakePanel(PanelColor);
	Panel->SetPadding(FMargin(30.f, 24.f));
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	TitleText = MakeText(FText::GetEmpty(), 26, Gold, true);
	RoundText = MakeText(FText::GetEmpty(), 19, Ink, true);
	ResultText = MakeText(FText::GetEmpty(), 18, Muted, true);
	HintText = MakeText(FText::GetEmpty(), 15, Muted);
	Box->AddChildToVerticalBox(TitleText)->SetPadding(FMargin(0, 0, 0, 10));
	Box->AddChildToVerticalBox(RoundText)->SetPadding(FMargin(0, 0, 0, 14));
	UVerticalBoxSlot* TrackSlot = Box->AddChildToVerticalBox(MakeTrack(Track, 700.f, 34.f));
	TrackSlot->SetHorizontalAlignment(HAlign_Center);
	TrackSlot->SetPadding(FMargin(0, 6, 0, 16));
	Box->AddChildToVerticalBox(ResultText)->SetPadding(FMargin(0, 0, 0, 10));
	Box->AddChildToVerticalBox(HintText);
	Panel->SetContent(Box);
	UCanvasPanelSlot* PanelSlot = Place(Root, Panel, FAnchors(0.5f, 0.6f), FVector2D(0.5f, 0.5f), FVector2D(0, 0), FVector2D(780, 0));
	PanelSlot->SetAutoSize(true);
	SetIsFocusable(true);
}

void UVillageTimingWidget::Setup(const FText& Title, const FText& Hint)
{
	TitleText->SetText(Title);
	HintText->SetText(Hint);
}

void UVillageTimingWidget::StartRound(const FVillageTimingRound& InRound, const FText& Result, const FLinearColor& ResultColor)
{
	Round = InRound;
	Position = 0.f;
	Direction = 1.f;
	bWaiting = false;
	Track.SetZones(Round.GoodMin, Round.GoodMax, Round.PerfectMin, Round.PerfectMax);
	Track.SetMarker(Position);
	RoundText->SetText(Round.Label);
	ResultText->SetText(Result);
	ResultText->SetColorAndOpacity(FSlateColor(ResultColor));
}

void UVillageTimingWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (bWaiting)
	{
		return;
	}
	Position += Direction * Round.Speed * DeltaTime;
	if (Round.bPingPong)
	{
		if (Position >= 1.f || Position <= 0.f)
		{
			Position = FMath::Clamp(Position, 0.f, 1.f);
			Direction = -Direction;
		}
	}
	else if (Position >= 1.f)
	{
		// Ran out the bar without a press (e.g. the cocoon went past its moment).
		Position = 1.f;
		Track.SetMarker(Position);
		Press();
		return;
	}
	Track.SetMarker(Position);
}

void UVillageTimingWidget::Press()
{
	if (bWaiting)
	{
		return;
	}
	bWaiting = true;
	if (UVillageStorySubsystem* S = Story.Get())
	{
		S->OnTimingPress(Position);
	}
}

FReply UVillageTimingWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (IsCloseKey(KeyEvent.GetKey()))
	{
		if (UVillageStorySubsystem* S = Story.Get())
		{
			S->CloseTiming();
		}
	}
	else if (IsAdvanceKey(KeyEvent.GetKey()) && !KeyEvent.IsRepeat())
	{
		Press();
	}
	return FReply::Handled();
}

FReply UVillageTimingWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Press();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(Geometry, MouseEvent);
}

// ---------------------------------------------------------------- Silk reeling

void UVillageReelingWidget::BuildTree(UCanvasPanel* Root)
{
	UBorder* Panel = MakePanel(PanelColor);
	Panel->SetPadding(FMargin(30.f, 24.f));
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	Box->AddChildToVerticalBox(MakeText(LOCTEXT("ReelTitle", "Ươm tơ – Sợi tơ đầu tiên"), 26, Gold, true))->SetPadding(FMargin(0, 0, 0, 12));

	auto AddTrack = [&](const FText& Label, FVillageTrack& Track, float SafeMin, float SafeMax)
	{
		Box->AddChildToVerticalBox(MakeText(Label, 17, Ink, true))->SetPadding(FMargin(0, 6, 0, 4));
		Box->AddChildToVerticalBox(MakeTrack(Track, 700.f, 26.f))->SetPadding(FMargin(0, 4, 0, 8));
		// The whole safe band counts; the narrow centre is only a visual aim point.
		const float Mid = (SafeMin + SafeMax) * 0.5f;
		Track.SetZones(SafeMin, SafeMax, Mid - 0.05f, Mid + 0.05f);
	};
	AddTrack(LOCTEXT("Tension", "Lực căng sợi   (CHÙNG ←  an toàn  → ĐỨT)"), TensionTrack, SafeTensionMin, SafeTensionMax);
	AddTrack(LOCTEXT("Speed", "Tốc độ kéo"), SpeedTrack, SafeSpeedMin, SafeSpeedMax);

	Box->AddChildToVerticalBox(MakeText(LOCTEXT("Reel", "Guồng tơ"), 17, Ink, true))->SetPadding(FMargin(0, 6, 0, 4));
	ReelBar = WidgetTree->ConstructWidget<UProgressBar>();
	ReelBar->SetFillColorAndOpacity(Gold);
	Box->AddChildToVerticalBox(ReelBar)->SetPadding(FMargin(0, 0, 0, 12));
	StatusText = MakeText(FText::GetEmpty(), 19, Ink, true);
	QualityText = MakeText(FText::GetEmpty(), 16, Muted);
	Box->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0, 0, 0, 6));
	Box->AddChildToVerticalBox(QualityText)->SetPadding(FMargin(0, 0, 0, 10));
	Box->AddChildToVerticalBox(MakeText(LOCTEXT("ReelHint", "Giữ [E] hoặc chuột trái để kéo nhanh hơn, thả ra để chậm lại  ·  [Esc] dừng"), 15, Muted));
	Panel->SetContent(Box);
	UCanvasPanelSlot* PanelSlot = Place(Root, Panel, FAnchors(0.5f, 0.55f), FVector2D(0.5f, 0.5f), FVector2D(0, 0), FVector2D(780, 0));
	PanelSlot->SetAutoSize(true);
	SetIsFocusable(true);
}

void UVillageReelingWidget::Setup(float StartQuality)
{
	Quality = StartQuality;
	Speed = 0.3f;
	Tension = 0.35f;
	Progress = 0.f;
	Overload = 0.f;
	Stage = 0;
	bBroken = false;
	bHolding = false;
	Redraw();
}

void UVillageReelingWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	Step(FMath::Min(DeltaTime, 0.1f));
}

void UVillageReelingWidget::Step(float DeltaTime)
{
	if (bBroken || Stage >= 2)
	{
		return;
	}
	Time += DeltaTime;
	Speed = FMath::Clamp(Speed + (bHolding ? 0.45f : -0.35f) * DeltaTime, 0.f, 1.f);

	// The cocoon in the pot tugs back unevenly: a slow wobble plus the odd sudden snag.
	NextGust -= DeltaTime;
	if (NextGust <= 0.f)
	{
		Gust = FMath::FRandRange(-0.18f, 0.18f);
		NextGust = FMath::FRandRange(1.8f, 3.5f);
	}
	Gust = FMath::FInterpTo(Gust, 0.f, DeltaTime, 0.8f);
	const float Drift = 0.10f * FMath::Sin(Time * 1.3f) + 0.06f * FMath::Sin(Time * 3.1f + 1.f) + Gust;
	Tension = FMath::FInterpTo(Tension, FMath::Clamp(0.08f + Speed * 0.85f + Drift, 0.f, 1.f), DeltaTime, 3.f);

	const bool bSafeTension = Tension >= SafeTensionMin && Tension <= SafeTensionMax;
	const bool bSafeSpeed = Speed >= SafeSpeedMin && Speed <= SafeSpeedMax;
	if (bSafeTension)
	{
		Progress += DeltaTime * (bSafeSpeed ? 0.085f : 0.03f);
	}
	else
	{
		Quality -= DeltaTime * 1.5f;
	}
	if (!bSafeSpeed)
	{
		Quality -= DeltaTime * 0.5f;
	}
	Overload = Tension > 0.85f ? Overload + DeltaTime : FMath::Max(0.f, Overload - DeltaTime);
	if (Overload > 0.5f)
	{
		bBroken = true;
		bHolding = false;
		Quality -= 10.f;
		if (UVillageStorySubsystem* S = Story.Get())
		{
			S->OnReelingBreak();
		}
	}
	Quality = FMath::Clamp(Quality, 0.f, 100.f);
	Redraw();

	UVillageStorySubsystem* S = Story.Get();
	if (Stage == 0 && Progress >= 0.35f)
	{
		Stage = 1;
		if (S) S->OnReelingStage(1, Quality);
	}
	if (Progress >= 1.f)
	{
		Progress = 1.f;
		Stage = 2;
		if (S) S->OnReelingStage(2, Quality); // closes this widget
	}
}

void UVillageReelingWidget::Redraw()
{
	TensionTrack.SetMarker(Tension);
	SpeedTrack.SetMarker(Speed);
	ReelBar->SetPercent(Progress);
	FText Status;
	FLinearColor Color = Ink;
	if (bBroken)
	{
		Status = LOCTEXT("Broken", "SỢI TƠ BỊ ĐỨT!  Nhấn [E] để nối lại.");
		Color = Warn;
	}
	else if (Tension > 0.75f)
	{
		Status = LOCTEXT("TooTight", "Căng quá! Thả tay ra kẻo đứt sợi.");
		Color = Warn;
	}
	else if (Tension < 0.15f)
	{
		Status = LOCTEXT("Loose", "Sợi đang chùng – kéo nhanh hơn.");
		Color = Warn;
	}
	else if (Tension < SafeTensionMin || Tension > SafeTensionMax)
	{
		Status = LOCTEXT("OutOfBand", "Đưa lực căng về vùng an toàn.");
	}
	else if (Speed < SafeSpeedMin || Speed > SafeSpeedMax)
	{
		Status = LOCTEXT("SpeedHint", "Lực căng ổn – giữ tốc độ trong vùng xanh để tơ ra đều.");
	}
	else
	{
		Status = LOCTEXT("Steady", "Đều tay lắm, cứ giữ như vậy.");
		Color = Healthy;
	}
	StatusText->SetText(Status);
	StatusText->SetColorAndOpacity(FSlateColor(Color));
	QualityText->SetText(FText::Format(LOCTEXT("ReelQuality", "{0}  ·  Chất lượng tơ: {1}%"),
		Stage == 0 ? LOCTEXT("Phase2", "Giai đoạn: Giữ lực căng") : LOCTEXT("Phase3", "Giai đoạn: Giữ tốc độ ổn định"),
		FText::AsNumber(FMath::RoundToInt(Quality))));
}

void UVillageReelingWidget::Rejoin()
{
	bBroken = false;
	Speed = 0.3f;
	Tension = 0.35f;
	Overload = 0.f;
	Redraw();
}

void UVillageReelingWidget::DebugFinish(float FinalQuality)
{
	Quality = FinalQuality;
	Progress = 1.f;
	UVillageStorySubsystem* S = Story.Get();
	if (S && Stage < 1)
	{
		Stage = 1;
		S->OnReelingStage(1, Quality);
	}
	Stage = 2;
	if (S) S->OnReelingStage(2, Quality);
}

FReply UVillageReelingWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	const FKey Key = KeyEvent.GetKey();
	if (IsCloseKey(Key))
	{
		if (UVillageStorySubsystem* S = Story.Get())
		{
			S->CloseReeling();
		}
	}
	else if (IsAdvanceKey(Key))
	{
		if (bBroken)
		{
			if (!KeyEvent.IsRepeat()) Rejoin();
		}
		else
		{
			bHolding = true;
		}
	}
	return FReply::Handled();
}

FReply UVillageReelingWidget::NativeOnKeyUp(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (IsAdvanceKey(KeyEvent.GetKey()))
	{
		bHolding = false;
	}
	return FReply::Handled();
}

FReply UVillageReelingWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(Geometry, MouseEvent);
	}
	bBroken ? Rejoin() : (void)(bHolding = true);
	return FReply::Handled();
}

FReply UVillageReelingWidget::NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bHolding = false;
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonUp(Geometry, MouseEvent);
}

// ---------------------------------------------------------------- Nameplate

TSharedRef<SWidget> UVillageNameplateWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Box;
		NameText = MakeText(FText::GetEmpty(), 20, Gold, true);
		RoleText = MakeText(FText::GetEmpty(), 14, Ink);
		for (UTextBlock* Text : { NameText.Get(), RoleText.Get() })
		{
			Text->SetAutoWrapText(false);
			Text->SetJustification(ETextJustify::Center);
			Text->SetShadowOffset(FVector2D(1.5f, 1.5f));
			Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.85f));
			Box->AddChildToVerticalBox(Text)->SetHorizontalAlignment(HAlign_Center);
		}
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	return Super::RebuildWidget(); // base skips its canvas because the root is already set
}

void UVillageNameplateWidget::Setup(const FText& Name, const FText& Role)
{
	if (!NameText)
	{
		TakeWidget();
	}
	NameText->SetText(Name);
	RoleText->SetText(Role);
	RoleText->SetVisibility(Role.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

// ---------------------------------------------------------------- Day end

void UVillageDayEndWidget::BuildTree(UCanvasPanel* Root)
{
	Backdrop = MakePanel(FLinearColor(0.f, 0.f, 0.f, 1.f));
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	TitleText = MakeText(FText::GetEmpty(), 46, Gold, true);
	TitleText->SetJustification(ETextJustify::Center);
	LineBox = WidgetTree->ConstructWidget<UVerticalBox>();
	Box->AddChildToVerticalBox(TitleText)->SetPadding(FMargin(0, 0, 0, 30));
	Box->AddChildToVerticalBox(LineBox)->SetPadding(FMargin(0, 0, 0, 30));
	UButton* Continue = MakeButton(LOCTEXT("DayContinue", "Tiếp tục"), [this]()
	{
		if (UVillageStorySubsystem* S = Story.Get())
		{
			S->CloseDayEnd();
		}
	}, 20);
	UVerticalBoxSlot* ButtonSlot = Box->AddChildToVerticalBox(Continue);
	ButtonSlot->SetHorizontalAlignment(HAlign_Center);
	Backdrop->SetContent(Box);
	Backdrop->SetHorizontalAlignment(HAlign_Center);
	Backdrop->SetVerticalAlignment(VAlign_Center);
	Place(Root, Backdrop, FAnchors(0.f, 0.f, 1.f, 1.f), FVector2D(0, 0), FVector2D(0, 0), FVector2D(0, 0));
	SetIsFocusable(true);
}

void UVillageDayEndWidget::Setup(const FText& Title, const TArray<FText>& Lines)
{
	TitleText->SetText(Title);
	LineBox->ClearChildren();
	for (const FText& Line : Lines)
	{
		UTextBlock* Text = MakeText(Line, 22, Ink);
		Text->SetJustification(ETextJustify::Center);
		LineBox->AddChildToVerticalBox(Text)->SetPadding(FMargin(0, 5));
	}
	FadeIn = 0.f;
	SetRenderOpacity(0.f);
}

void UVillageDayEndWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (FadeIn < 1.f)
	{
		FadeIn = FMath::Min(1.f, FadeIn + DeltaTime / 1.5f);
		SetRenderOpacity(FadeIn);
	}
}

FReply UVillageDayEndWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (FadeIn >= 1.f && IsAdvanceKey(KeyEvent.GetKey()))
	{
		if (UVillageStorySubsystem* S = Story.Get())
		{
			S->CloseDayEnd();
		}
	}
	return FReply::Handled();
}

// ---------------------------------------------------------------- Action mini-games (Chapters 4-6)

namespace
{
	constexpr float StageW = 760.f;
	constexpr float StageH = 300.f;
	constexpr float TrackX = 30.f;
	constexpr float TrackW = 700.f;
	// The project font has no arrow glyphs, so the prompts show the WASD key (the arrow keys work the same).
	const TCHAR* const ArrowGlyph[4] = { TEXT("A"), TEXT("W"), TEXT("S"), TEXT("D") };
	const TCHAR* const LaneKeys[4] = { TEXT("A"), TEXT("S"), TEXT("W"), TEXT("D") };

	/** 0 left, 1 up, 2 down, 3 right (arrows or WASD); -1 for anything else. */
	int32 DirectionOf(const FKey& Key)
	{
		if (Key == EKeys::Left || Key == EKeys::A) return 0;
		if (Key == EKeys::Up || Key == EKeys::W) return 1;
		if (Key == EKeys::Down || Key == EKeys::S) return 2;
		if (Key == EKeys::Right || Key == EKeys::D) return 3;
		return -1;
	}

	float TracePath(float X, float Frequency, float Phase)
	{
		return 0.5f + 0.28f * FMath::Sin(UE_TWO_PI * (Frequency * X * 0.5f + Phase));
	}
}

void UVillageActionWidget::BuildTree(UCanvasPanel* Root)
{
	UBorder* Panel = MakePanel(PanelColor);
	Panel->SetPadding(FMargin(30.f, 22.f));
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
	TitleText = MakeText(FText::GetEmpty(), 26, Gold, true);
	StatusText = MakeText(FText::GetEmpty(), 18, Ink, true);
	HintText = MakeText(FText::GetEmpty(), 15, Muted);
	Box->AddChildToVerticalBox(TitleText)->SetPadding(FMargin(0, 0, 0, 8));
	Box->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0, 0, 0, 10));
	USizeBox* StageBox = WidgetTree->ConstructWidget<USizeBox>();
	StageBox->SetWidthOverride(StageW);
	StageBox->SetHeightOverride(StageH);
	Stage = WidgetTree->ConstructWidget<UCanvasPanel>();
	Stage->SetClipping(EWidgetClipping::ClipToBounds); // falling notes must not draw outside the panel
	StageBox->AddChild(Stage);
	UVerticalBoxSlot* StageSlot = Box->AddChildToVerticalBox(StageBox);
	StageSlot->SetHorizontalAlignment(HAlign_Center);
	StageSlot->SetPadding(FMargin(0, 0, 0, 12));
	Box->AddChildToVerticalBox(HintText);
	Panel->SetContent(Box);
	UCanvasPanelSlot* PanelSlot = Place(Root, Panel, FAnchors(0.5f, 0.55f), FVector2D(0.5f, 0.5f), FVector2D(0, 0), FVector2D(840, 0));
	PanelSlot->SetAutoSize(true);
	SetIsFocusable(true);
}

UBorder* UVillageActionWidget::AddPiece(const FLinearColor& Color, FVector2D Pos, FVector2D Size)
{
	UBorder* Border = WidgetTree->ConstructWidget<UBorder>();
	Border->SetBrushColor(Color);
	Border->SetPadding(FMargin(0.f));
	UCanvasPanelSlot* CanvasSlot = Stage->AddChildToCanvas(Border);
	CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
	CanvasSlot->SetPosition(Pos);
	CanvasSlot->SetSize(Size);
	return Border;
}

UTextBlock* UVillageActionWidget::AddLabel(const FText& Text, int32 FontSize, FVector2D Pos, FVector2D Size, const FLinearColor& Color)
{
	UTextBlock* Block = MakeText(Text, FontSize, Color, true);
	Block->SetAutoWrapText(false);
	UCanvasPanelSlot* CanvasSlot = Stage->AddChildToCanvas(Block);
	CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
	CanvasSlot->SetPosition(Pos);
	CanvasSlot->SetSize(Size);
	return Block;
}

void UVillageActionWidget::Move(UBorder* Piece, FVector2D Pos, FVector2D Size)
{
	if (UCanvasPanelSlot* Slot = Piece ? Cast<UCanvasPanelSlot>(Piece->Slot) : nullptr)
	{
		Slot->SetPosition(Pos);
		Slot->SetSize(Size);
	}
}

void UVillageActionWidget::SetStatus(const FText& Text)
{
	if (StatusText)
	{
		StatusText->SetText(Text);
	}
}

void UVillageActionWidget::Setup(FName InMode, const FText& Title, const FText& Hint, int32 InSteps, float InSpeed, float InTolerance)
{
	TitleText->SetText(Title);
	HintText->SetText(Hint);
	Steps = FMath::Max(1, InSteps);
	Speed = FMath::Max(0.3f, InSpeed);
	Tolerance = FMath::Clamp(InTolerance, 0.04f, 0.3f);
	Mode = InMode == TEXT("Sequence") ? EMode::Sequence : InMode == TEXT("Lanes") ? EMode::Lanes : InMode == TEXT("Trace") ? EMode::Trace : EMode::Slider;
	AddPiece(FLinearColor(0.12f, 0.08f, 0.06f, 1.f), FVector2D(0, 0), FVector2D(StageW, StageH));
	switch (Mode)
	{
	case EMode::Slider: SetupSlider(); break;
	case EMode::Sequence: SetupSequence(); break;
	case EMode::Lanes: SetupLanes(); break;
	case EMode::Trace: SetupTrace(); break;
	}
	bStarted = true;
}

void UVillageActionWidget::Finish(float Score)
{
	if (bDone)
	{
		return;
	}
	bDone = true;
	if (UVillageStorySubsystem* S = Story.Get())
	{
		S->OnActionGameDone(FMath::Clamp(Score, 0.f, 100.f));
	}
}

// Slider (dyeing): three drifting values to keep in their bands.

void UVillageActionWidget::SetupSlider()
{
	const TCHAR* const Names[3] = { TEXT("Nhiệt độ"), TEXT("Lượng thuốc nhuộm"), TEXT("Thời gian ngâm") };
	Duration = 4.f + Steps * 2.f;
	for (int32 i = 0; i < 3; ++i)
	{
		const float Y = 40.f + i * 90.f;
		AddLabel(FText::FromString(Names[i]), 17, FVector2D(TrackX, Y - 30.f), FVector2D(400.f, 26.f), Ink);
		AddPiece(FLinearColor(0.22f, 0.15f, 0.10f, 1.f), FVector2D(TrackX, Y), FVector2D(TrackW, 26.f));
		Center[i] = FMath::FRandRange(0.28f, 0.72f);
		Value[i] = FMath::Clamp(Center[i] + FMath::FRandRange(-0.3f, 0.3f), 0.05f, 0.95f);
		Pieces.Add(AddPiece(FLinearColor(0.35f, 0.75f, 0.38f, 0.85f), FVector2D(TrackX + (Center[i] - Tolerance) * TrackW, Y), FVector2D(2.f * Tolerance * TrackW, 26.f)));
		Pieces2.Add(AddPiece(Ink, FVector2D(TrackX + Value[i] * TrackW - 4.f, Y - 8.f), FVector2D(8.f, 42.f)));
	}
	SetStatus(FText::GetEmpty());
}

void UVillageActionWidget::TickSlider(float Dt)
{
	Elapsed += Dt;
	DriftTimer -= Dt;
	if (DriftTimer <= 0.f)
	{
		DriftTimer = 1.1f;
		for (float& D : Drift)
		{
			D = FMath::FRandRange(-0.14f, 0.14f) * Speed;
		}
	}
	int32 Inside = 0;
	for (int32 i = 0; i < 3; ++i)
	{
		Value[i] += Drift[i] * Dt + (i == Selected ? Nudge * 0.45f * Dt : 0.f);
		Value[i] = FMath::Clamp(Value[i], 0.f, 1.f);
		const bool bIn = FMath::Abs(Value[i] - Center[i]) <= Tolerance;
		Inside += bIn ? 1 : 0;
		const float Y = 40.f + i * 90.f;
		const bool bSel = i == Selected;
		if (Pieces2.IsValidIndex(i))
		{
			Pieces2[i]->SetBrushColor(bIn ? FLinearColor(0.55f, 0.95f, 0.55f) : bSel ? FLinearColor(1.f, 0.80f, 0.30f) : FLinearColor(1.f, 0.55f, 0.40f));
			Move(Pieces2[i], FVector2D(TrackX + Value[i] * TrackW - (bSel ? 6.f : 4.f), Y - (bSel ? 12.f : 8.f)), FVector2D(bSel ? 12.f : 8.f, bSel ? 50.f : 42.f));
		}
	}
	Accum += Dt * Inside / 3.f;
	AccumAll += Inside == 3 ? Dt : 0.f;
	SetStatus(FText::Format(LOCTEXT("SliderStatus", "Còn {0} giây  ·  Trong vùng xanh: {1}/3"), FText::AsNumber(FMath::CeilToInt(FMath::Max(0.f, Duration - Elapsed))), FText::AsNumber(Inside)));
	if (Elapsed >= Duration)
	{
		Finish(100.f * (0.35f * Accum / Duration + 0.65f * AccumAll / Duration));
	}
}

// Sequence (weaving): press the arrow shown.

void UVillageActionWidget::SetupSequence()
{
	const int32 Count = FMath::Max(4, Steps);
	Window = FMath::Max(0.55f, 1.55f / Speed);
	Directions.Reset();
	const float Left = (StageW - Count * 80.f) * 0.5f;
	Pieces.Add(AddPiece(FLinearColor(0.35f, 0.75f, 0.38f, 1.f), FVector2D(TrackX, 230.f), FVector2D(TrackW, 14.f))); // time left for this arrow
	for (int32 i = 0; i < Count; ++i)
	{
		Directions.Add(FMath::RandRange(0, 3));
		Pieces2.Add(AddPiece(FLinearColor(0.28f, 0.19f, 0.13f, 1.f), FVector2D(Left + i * 80.f, 100.f), FVector2D(70.f, 70.f)));
		Labels.Add(AddLabel(FText::FromString(ArrowGlyph[Directions[i]]), 36, FVector2D(Left + i * 80.f + 18.f, 106.f), FVector2D(50.f, 60.f), Ink));
	}
	Step = 0;
	StepTime = 0.f;
	PointsSum = 0.f;
	Pieces2[0]->SetBrushColor(FLinearColor(0.62f, 0.47f, 0.22f, 1.f));
	SetStatus(LOCTEXT("SeqStart", "Bấm đúng phím đang sáng trước khi hết giờ"));
}

void UVillageActionWidget::TickSequence(float Dt)
{
	StepTime += Dt;
	Move(Pieces[0], FVector2D(TrackX, 230.f), FVector2D(TrackW * FMath::Clamp(1.f - StepTime / Window, 0.f, 1.f), 14.f));
	if (StepTime >= Window)
	{
		Pieces2[Step]->SetBrushColor(FLinearColor(0.75f, 0.28f, 0.22f, 1.f));
		AdvanceSequence(30.f);
	}
}

void UVillageActionWidget::KeySequence(int32 Direction)
{
	if (bDone || !Directions.IsValidIndex(Step))
	{
		return;
	}
	if (Direction == Directions[Step])
	{
		const bool bFast = StepTime < Window * 0.45f;
		Pieces2[Step]->SetBrushColor(bFast ? FLinearColor(0.35f, 0.75f, 0.38f, 1.f) : FLinearColor(0.62f, 0.60f, 0.25f, 1.f));
		AdvanceSequence(bFast ? 100.f : 70.f);
	}
	else
	{
		Pieces2[Step]->SetBrushColor(FLinearColor(0.75f, 0.28f, 0.22f, 1.f));
		AdvanceSequence(30.f);
	}
}

void UVillageActionWidget::AdvanceSequence(float Points)
{
	PointsSum += Points;
	++Step;
	StepTime = 0.f;
	if (Step >= Directions.Num())
	{
		Finish(PointsSum / Directions.Num());
		return;
	}
	Pieces2[Step]->SetBrushColor(FLinearColor(0.62f, 0.47f, 0.22f, 1.f));
	SetStatus(FText::Format(LOCTEXT("SeqStatus", "Phím {0}/{1}"), FText::AsNumber(Step + 1), FText::AsNumber(Directions.Num())));
}

// Lanes (the show): notes fall to the line.

void UVillageActionWidget::SetupLanes()
{
	Fall = FMath::Clamp(1.9f / Speed, 1.1f, 2.6f);
	const float Interval = FMath::Max(0.42f, 0.62f / Speed);
	const int32 Count = FMath::Max(8, Steps * 4);
	for (int32 lane = 0; lane < 4; ++lane)
	{
		AddPiece(FLinearColor(0.20f, 0.14f, 0.10f, 1.f), FVector2D(130.f + lane * 130.f, 0.f), FVector2D(100.f, StageH));
		AddLabel(FText::FromString(LaneKeys[lane]), 20, FVector2D(172.f + lane * 130.f, 264.f), FVector2D(40.f, 30.f), Gold);
	}
	AddPiece(FLinearColor(0.96f, 0.76f, 0.36f, 0.9f), FVector2D(110.f, 244.f), FVector2D(540.f, 6.f)); // the line
	int32 PreviousLane = -1, Repeats = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		int32 Lane = FMath::RandRange(0, 3);
		Repeats = Lane == PreviousLane ? Repeats + 1 : 0;
		if (Repeats >= 2)
		{
			Lane = (Lane + 1 + FMath::RandRange(0, 2)) % 4;
			Repeats = 0;
		}
		PreviousLane = Lane;
		FNote Note;
		Note.Lane = Lane;
		Note.HitTime = 1.2f + Fall + i * Interval;
		Notes.Add(Note);
		Pieces2.Add(AddPiece(FLinearColor(0.96f, 0.60f, 0.30f, 1.f), FVector2D(-200.f, -200.f), FVector2D(84.f, 22.f)));
	}
	PointsSum = 0.f;
	Step = 0; // notes judged
	SetStatus(LOCTEXT("LanesStart", "Bấm phím của làn khi nốt chạm vạch vàng"));
}

void UVillageActionWidget::TickLanes(float Dt)
{
	Elapsed += Dt;
	float LastHit = 0.f;
	for (int32 i = 0; i < Notes.Num(); ++i)
	{
		FNote& Note = Notes[i];
		LastHit = FMath::Max(LastHit, Note.HitTime);
		if (Note.bDone)
		{
			continue;
		}
		const float Y = (Elapsed - (Note.HitTime - Fall)) / Fall * 244.f;
		if (Elapsed > Note.HitTime + 0.30f)
		{
			Note.bDone = true;
			++Step;
			PointsSum += 30.f;
			Move(Pieces2[i], FVector2D(-200.f, -200.f), FVector2D(84.f, 22.f));
			SetStatus(LOCTEXT("LaneMiss", "Trượt!"));
			continue;
		}
		// Off stage until it enters from the top.
		Move(Pieces2[i], Y < 0.f ? FVector2D(-200.f, -200.f) : FVector2D(138.f + Note.Lane * 130.f, Y - 11.f), FVector2D(84.f, 22.f));
	}
	if (Step >= Notes.Num() && Elapsed > LastHit + 0.5f)
	{
		Finish(PointsSum / FMath::Max(1, Notes.Num()));
	}
}

void UVillageActionWidget::KeyLane(int32 Lane)
{
	if (bDone)
	{
		return;
	}
	int32 Best = INDEX_NONE;
	float BestDelta = 0.30f;
	for (int32 i = 0; i < Notes.Num(); ++i)
	{
		if (!Notes[i].bDone && Notes[i].Lane == Lane && FMath::Abs(Elapsed - Notes[i].HitTime) <= BestDelta)
		{
			Best = i;
			BestDelta = FMath::Abs(Elapsed - Notes[i].HitTime);
		}
	}
	if (Best == INDEX_NONE)
	{
		return;
	}
	Notes[Best].bDone = true;
	++Step;
	const bool bPerfect = BestDelta < 0.12f;
	PointsSum += bPerfect ? 100.f : BestDelta < 0.20f ? 75.f : 55.f;
	Move(Pieces2[Best], FVector2D(-200.f, -200.f), FVector2D(84.f, 22.f));
	SetStatus(bPerfect ? LOCTEXT("LanePerfect", "Perfect!") : LOCTEXT("LaneGood", "Good!"));
}

// Trace (cutting): steer the scissors along the curve.

void UVillageActionWidget::SetupTrace()
{
	Duration = 9.f / Speed;
	Frequency = 0.8f + 0.28f * Steps;
	Phase = FMath::FRand();
	const int32 Dots = 70;
	for (int32 i = 0; i < Dots; ++i)
	{
		const float X = i / static_cast<float>(Dots - 1);
		AddPiece(FLinearColor(0.62f, 0.47f, 0.22f, 1.f), FVector2D(TrackX + X * TrackW - 4.f, 20.f + TracePath(X, Frequency, Phase) * 260.f - 4.f), FVector2D(8.f, 8.f));
	}
	Pieces2.Add(AddPiece(Ink, FVector2D(TrackX - 8.f, 20.f + SteerY * 260.f - 8.f), FVector2D(16.f, 16.f)));
	SteerY = TracePath(0.f, Frequency, Phase);
	SetStatus(LOCTEXT("TraceStart", "Điều khiển kéo bám theo đường cắt (di chuột hoặc giữ W/S)"));
}

void UVillageActionWidget::TickTrace(float Dt)
{
	Elapsed += Dt;
	SteerY = FMath::Clamp(SteerY + NudgeV * 0.7f * Dt, 0.f, 1.f);
	const float X = FMath::Clamp(Elapsed / Duration, 0.f, 1.f);
	const float Err = FMath::Abs(SteerY - TracePath(X, Frequency, Phase));
	ErrorSum += Err;
	++ErrorSamples;
	if (Pieces2.IsValidIndex(0))
	{
		Pieces2[0]->SetBrushColor(Err <= Tolerance ? FLinearColor(0.55f, 0.95f, 0.55f) : FLinearColor(1.f, 0.55f, 0.40f));
		Move(Pieces2[0], FVector2D(TrackX + X * TrackW - 8.f, 20.f + SteerY * 260.f - 8.f), FVector2D(16.f, 16.f));
	}
	SetStatus(FText::Format(LOCTEXT("TraceStatus", "Đường cắt: {0}%"), FText::AsNumber(FMath::RoundToInt(X * 100.f))));
	if (X >= 1.f)
	{
		const float Average = ErrorSum / FMath::Max(1, ErrorSamples);
		Finish(100.f * (1.f - FMath::Clamp(Average / (0.10f + Tolerance), 0.f, 1.f)));
	}
}

void UVillageActionWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (bDone || !bStarted)
	{
		return;
	}
	switch (Mode)
	{
	case EMode::Slider: TickSlider(DeltaTime); break;
	case EMode::Sequence: TickSequence(DeltaTime); break;
	case EMode::Lanes: TickLanes(DeltaTime); break;
	case EMode::Trace: TickTrace(DeltaTime); break;
	}
}

FReply UVillageActionWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	const FKey Key = KeyEvent.GetKey();
	if (IsCloseKey(Key))
	{
		if (UVillageStorySubsystem* S = Story.Get())
		{
			S->CloseActionGame();
		}
		return FReply::Handled();
	}
	const int32 Direction = DirectionOf(Key);
	switch (Mode)
	{
	case EMode::Slider:
		if (Key == EKeys::Up || Key == EKeys::W) { if (!KeyEvent.IsRepeat()) Selected = (Selected + 2) % 3; }
		else if (Key == EKeys::Down || Key == EKeys::S || Key == EKeys::Tab) { if (!KeyEvent.IsRepeat()) Selected = (Selected + 1) % 3; }
		else if (Direction == 0) Nudge = -1;
		else if (Direction == 3) Nudge = 1;
		break;
	case EMode::Sequence:
		if (Direction >= 0 && !KeyEvent.IsRepeat()) KeySequence(Direction);
		break;
	case EMode::Lanes:
		if (Direction >= 0 && !KeyEvent.IsRepeat())
		{
			// A S W D = lanes 0..3 left to right: left, down, up, right.
			KeyLane(Direction == 0 ? 0 : Direction == 2 ? 1 : Direction == 1 ? 2 : 3);
		}
		break;
	case EMode::Trace:
		if (Direction == 1) NudgeV = -1;
		else if (Direction == 2) NudgeV = 1;
		break;
	}
	return FReply::Handled();
}

FReply UVillageActionWidget::NativeOnKeyUp(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	const int32 Direction = DirectionOf(KeyEvent.GetKey());
	if (Direction == 0 || Direction == 3)
	{
		Nudge = 0;
	}
	if (Direction == 1 || Direction == 2)
	{
		NudgeV = 0;
	}
	return FReply::Handled();
}

FReply UVillageActionWidget::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if (Mode == EMode::Trace && Stage)
	{
		const FVector2D Local = Stage->GetCachedGeometry().AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		SteerY = FMath::Clamp((Local.Y - 20.f) / 260.f, 0.f, 1.f);
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(Geometry, MouseEvent);
}

#undef LOCTEXT_NAMESPACE
