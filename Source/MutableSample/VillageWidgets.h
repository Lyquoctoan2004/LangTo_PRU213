// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VillageStoryTypes.h"

#include "VillageWidgets.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UProgressBar;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UVillageStorySubsystem;

/** Horizontal bar with a good band, a perfect band inside it and a moving marker (0..1). */
USTRUCT()
struct FVillageTrack
{
	GENERATED_BODY()

	UPROPERTY(Transient) TObjectPtr<UBorder> Good;
	UPROPERTY(Transient) TObjectPtr<UBorder> Perfect;
	UPROPERTY(Transient) TObjectPtr<UBorder> Marker;
	float Width = 0.f;

	void SetZones(float GoodMin, float GoodMax, float PerfectMin, float PerfectMax) const;
	void SetMarker(float Position) const;
};

/** Lets a UButton's dynamic OnClicked call a C++ lambda. */
UCLASS()
class MUTABLESAMPLE_API UVillageButtonProxy : public UObject
{
	GENERATED_BODY()

public:
	TFunction<void()> Callback;

	UFUNCTION()
	void HandleClicked();
};

/** Widgets in this file build their tree in code, so no Widget Blueprint is needed. */
UCLASS(Abstract)
class MUTABLESAMPLE_API UVillageWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	TWeakObjectPtr<UVillageStorySubsystem> Story;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void BuildTree(UCanvasPanel* Root) {}

	UTextBlock* MakeText(const FText& Text, int32 Size, const FLinearColor& Color, bool bBold = false);
	UButton* MakeButton(const FText& Label, TFunction<void()> OnClicked, int32 Size = 18);
	UBorder* MakePanel(const FLinearColor& Color);
	USizeBox* MakeTrack(FVillageTrack& Out, float Width, float Height);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UVillageButtonProxy>> Proxies;
};

/** A place the player should head for next; drawn on the HUD with its distance, clamped to the screen edge. */
struct FVillageGuideMarker
{
	FVector Location = FVector::ZeroVector;
	FText Name;
};

/** Always-on overlay: quest tracker on the left, short toasts at the bottom, guide markers over the world. */
UCLASS()
class MUTABLESAMPLE_API UVillageHUDWidget : public UVillageWidgetBase
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxGuideMarkers = 8;

	void SetTracker(const FText& Title, const TArray<FText>& Lines);
	void ShowToast(const FText& Message, float Seconds = 3.f);
	void SetGuideMarkers(const TArray<FVillageGuideMarker>& Markers);

protected:
	virtual void BuildTree(UCanvasPanel* Root) override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

private:
	void UpdateGuide();

	TArray<FVillageGuideMarker> GuideMarkers;
	TArray<FString> GuideShown; // text last written to each slot, so unchanged labels are not re-laid out
	UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> GuidePanels;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> GuideTexts;

	UPROPERTY(Transient) TObjectPtr<UBorder> TrackerPanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TrackerTitle;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> TrackerLines;
	UPROPERTY(Transient) TObjectPtr<UBorder> ToastPanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ToastText;
	float ToastTimeLeft = 0.f;
};

/** Bottom-of-screen conversation box with a typewriter reveal and up to four choices. */
UCLASS()
class MUTABLESAMPLE_API UVillageDialogueWidget : public UVillageWidgetBase
{
	GENERATED_BODY()

public:
	void Play(const FVillageDialogueRow& Dialogue);

	// Same as pressing E / a number key; used by UVillageStorySubsystem's debug hooks.
	void DebugAdvance() { Advance(); }
	void DebugChoose(int32 Index) { Choose(Index); }
	FString DebugDescribe() const;

protected:
	virtual void BuildTree(UCanvasPanel* Root) override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;

private:
	void ShowLine(int32 Index);
	void Advance();
	void ShowChoices();
	void Choose(int32 Index);

	FVillageDialogueRow Current;
	int32 LineIndex = 0;
	FString FullText;
	float Revealed = 0.f;
	bool bChoosing = false;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> SpeakerText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> BodyText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HintText;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> ChoiceBox;
};

/** Silkworm tray panel: condition readout plus care actions. */
UCLASS()
class MUTABLESAMPLE_API UVillageSilkwormWidget : public UVillageWidgetBase
{
	GENERATED_BODY()

public:
	void Refresh(const FVillageSilkwormState& State, int32 Leaves, const TArray<FText>& Warnings, const FText& LastResult);

protected:
	virtual void BuildTree(UCanvasPanel* Root) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

private:
	void AddStat(UVerticalBox* Box, const FText& Label, TObjectPtr<UProgressBar>& Bar, TObjectPtr<UTextBlock>& Value);

	UPROPERTY(Transient) TObjectPtr<UProgressBar> HealthBar;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> HungerBar;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> GrowthBar;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> CleanBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HealthValue;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HungerValue;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> GrowthValue;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CleanValue;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ClimateText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LeavesText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WarningText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultText;
};

/**
 * Chapter 2 timing bar: press E while the marker is in the band. The subsystem decides what a press
 * means (cocoon quality, a cloth strip on the gate, a hung lantern, the end of the silk thread).
 */
UCLASS()
class MUTABLESAMPLE_API UVillageTimingWidget : public UVillageWidgetBase
{
	GENERATED_BODY()

public:
	void Setup(const FText& Title, const FText& Hint);
	void StartRound(const FVillageTimingRound& Round, const FText& Result, const FLinearColor& ResultColor);
	float GetPosition() const { return Position; }

protected:
	virtual void BuildTree(UCanvasPanel* Root) override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;

private:
	void Press();

	FVillageTimingRound Round;
	float Position = 0.f;
	float Direction = 1.f;
	bool bWaiting = true; // between a press and the next StartRound

	UPROPERTY(Transient) FVillageTrack Track;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RoundText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResultText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HintText;
};

/**
 * Silk reeling: hold E to pull faster, let go to ease off. Tension follows the pull with a wandering
 * drift; keeping both in the safe band fills the reel, too much tension snaps the thread.
 * The simulation lives here (it is per-frame); milestones are reported to the subsystem.
 */
UCLASS()
class MUTABLESAMPLE_API UVillageReelingWidget : public UVillageWidgetBase
{
	GENERATED_BODY()

public:
	static constexpr float SafeTensionMin = 0.35f;
	static constexpr float SafeTensionMax = 0.65f;
	static constexpr float SafeSpeedMin = 0.40f;
	static constexpr float SafeSpeedMax = 0.70f;

	void Setup(float StartQuality);
	// Used by UVillageStorySubsystem's debug hook to finish without holding a key.
	void DebugFinish(float FinalQuality);

protected:
	virtual void BuildTree(UCanvasPanel* Root) override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;

private:
	void Step(float DeltaTime);
	void Rejoin();
	void Redraw();

	float Speed = 0.3f;
	float Tension = 0.4f;
	float Progress = 0.f;
	float Quality = 100.f;
	float Overload = 0.f;
	float Time = 0.f;
	float Gust = 0.f;
	float NextGust = 2.f;
	int32 Stage = 0;
	bool bHolding = false;
	bool bBroken = false;

	UPROPERTY(Transient) FVillageTrack SpeedTrack;
	UPROPERTY(Transient) FVillageTrack TensionTrack;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> ReelBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> QualityText;
};

/**
 * The action mini-games of Chapters 4-6, one widget with four mechanics (chosen by a craft row's Mode):
 *  Slider   - dyeing: three drifting sliders, keep each in its green band (W/S pick, A/D nudge).
 *  Sequence - weaving: press the arrow shown (arrows or WASD) before its window runs out.
 *  Lanes    - the show: notes fall down four lanes, press A S W D / arrows as they reach the line.
 *  Trace    - cutting: scissors cross the cloth on their own, steer them along the curve (mouse or W/S).
 * It reports a 0..100 score to UVillageStorySubsystem::OnActionGameDone.
 */
UCLASS()
class MUTABLESAMPLE_API UVillageActionWidget : public UVillageWidgetBase
{
	GENERATED_BODY()

public:
	/** Steps = how many rounds the craft row lists; Speed and Tolerance come from its first round (plus skill bonus). */
	void Setup(FName InMode, const FText& Title, const FText& Hint, int32 InSteps, float InSpeed, float InTolerance);
	void DebugFinish(float Score) { Finish(Score); }

protected:
	virtual void BuildTree(UCanvasPanel* Root) override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;

private:
	enum class EMode : uint8 { Slider, Sequence, Lanes, Trace };

	struct FNote
	{
		int32 Lane = 0;
		float HitTime = 0.f;
		bool bDone = false;
	};

	UBorder* AddPiece(const FLinearColor& Color, FVector2D Pos, FVector2D Size);
	UTextBlock* AddLabel(const FText& Text, int32 FontSize, FVector2D Pos, FVector2D Size, const FLinearColor& Color);
	static void Move(UBorder* Piece, FVector2D Pos, FVector2D Size);
	void Finish(float Score);
	void SetStatus(const FText& Text);

	void SetupSlider();
	void TickSlider(float Dt);
	void SetupSequence();
	void TickSequence(float Dt);
	void KeySequence(int32 Direction);
	void AdvanceSequence(float Points);
	void SetupLanes();
	void TickLanes(float Dt);
	void KeyLane(int32 Lane);
	void SetupTrace();
	void TickTrace(float Dt);

	EMode Mode = EMode::Slider;
	int32 Steps = 3;
	float Speed = 1.f;
	float Tolerance = 0.1f;
	float Elapsed = 0.f;
	bool bDone = false;
	bool bStarted = false;
	float Duration = 10.f;
	float Accum = 0.f;
	float AccumAll = 0.f;

	// Slider
	float Value[3] = { 0.5f, 0.5f, 0.5f };
	float Center[3] = { 0.5f, 0.5f, 0.5f };
	float Drift[3] = { 0.f, 0.f, 0.f };
	float DriftTimer = 0.f;
	int32 Selected = 0;
	int32 Nudge = 0; // -1, 0, +1 held (A/D)
	int32 NudgeV = 0; // -1, 0, +1 held (W/S, Trace mode)

	// Sequence
	TArray<int32> Directions;
	int32 Step = 0;
	float StepTime = 0.f;
	float Window = 1.4f;
	float PointsSum = 0.f;

	// Lanes
	TArray<FNote> Notes;
	float Fall = 1.8f;

	// Trace
	float Frequency = 1.f;
	float Phase = 0.f;
	float SteerY = 0.5f;
	float ErrorSum = 0.f;
	int32 ErrorSamples = 0;

	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Stage;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HintText;
	UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> Pieces;
	UPROPERTY(Transient) TArray<TObjectPtr<UBorder>> Pieces2;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> Labels;
};

/** Name and role floating above an important NPC (drawn by a screen-space UWidgetComponent). */
UCLASS()
class MUTABLESAMPLE_API UVillageNameplateWidget : public UVillageWidgetBase
{
	GENERATED_BODY()

public:
	void Setup(const FText& Name, const FText& Role);

protected:
	// Sized to its text (no canvas root) so DrawAtDesiredSize works.
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient) TObjectPtr<UTextBlock> NameText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> RoleText;
};

/** Full-screen "DAY 1 - COMPLETED" card. */
UCLASS()
class MUTABLESAMPLE_API UVillageDayEndWidget : public UVillageWidgetBase
{
	GENERATED_BODY()

public:
	void Setup(const FText& Title, const TArray<FText>& Lines);

protected:
	virtual void BuildTree(UCanvasPanel* Root) override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

private:
	UPROPERTY(Transient) TObjectPtr<UBorder> Backdrop;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> LineBox;
	float FadeIn = 0.f;
};
