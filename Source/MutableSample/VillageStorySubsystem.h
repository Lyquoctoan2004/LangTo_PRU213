// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VillageStoryTypes.h"

#include "VillageStorySubsystem.generated.h"

class UDataTable;
class UUserWidget;
class UVillageActionWidget;
class UVillageDayEndWidget;
class UVillageDialogueWidget;
class UVillageHUDWidget;
class UVillageReelingWidget;
class UVillageSilkwormWidget;
class UVillageTimingWidget;

/** Which Chapter 2 timing bar is on screen. */
enum class EVillageMiniGame : uint8
{
	None,
	Cocoon,
	Gate,
	Hang,
	FindThread,
	Spin,
	Craft, // a data-driven timing craft (DT_VillageCrafts)
};

/**
 * Runs Chapters 1-3 on the Mountain map: story flags, quests, NPC dialogue, mulberry harvesting,
 * the silkworm tray mini-game and the end-of-day sequence (Ch1); cocoon harvest, festival
 * decoration and silk reeling, ending with the same end-of-day card (Ch2); the cocoon batch
 * reeling, the spinning wheel and the Day 3 end card (Ch3). Interactables (BP_VillageNPC,
 * BP_VillageInteractable) forward their BPI_Interactable::Interact to HandleInteract.
 */
UCLASS()
class MUTABLESAMPLE_API UVillageStorySubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* LeafItem = TEXT("MAT_MULBERRY_LEAF_001");
	static constexpr const TCHAR* CocoonItem = TEXT("MAT_COCOON_001");
	static constexpr const TCHAR* RawSilkItem = TEXT("MAT_RAW_SILK_001");
	static constexpr const TCHAR* ThreadItem = TEXT("MAT_SILK_THREAD_001");

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual UWorld* GetWorld() const override;

	UFUNCTION(BlueprintCallable, Category = "Village|Story")
	void HandleInteract(AActor* Target, AActor* Interactor);

	UFUNCTION(BlueprintCallable, Category = "Village|Story")
	void OpenShop();

	UFUNCTION(BlueprintCallable, Category = "Village|Story")
	void HandleEscapeKey();

	void SyncCoinHUD();

	UFUNCTION(BlueprintCallable, Category = "Village|Story")
	void StartDialogue(FName DialogueId);

	UFUNCTION(BlueprintCallable, Category = "Village|Story")
	void RunActions(const TArray<FString>& Actions);

	UFUNCTION(BlueprintPure, Category = "Village|Story")
	bool HasFlag(FName Flag) const { return Flags.Contains(Flag); }

	UFUNCTION(BlueprintCallable, Category = "Village|Story")
	void SetFlag(FName Flag, bool bValue = true);

	UFUNCTION(BlueprintCallable, Category = "Village|Story")
	void StartQuest(FName QuestId);

	UFUNCTION(BlueprintCallable, Category = "Village|Story")
	void AddProgress(FName QuestId, FName ObjectiveId, int32 Amount = 1);

	UFUNCTION(BlueprintCallable, Category = "Village|Story")
	void Notify(const FText& Message);

	// Test hooks so editor scripts can play the chapter without keyboard input.
	UFUNCTION(BlueprintCallable, Category = "Village|Debug")
	void DebugAdvanceDialogue();

	UFUNCTION(BlueprintCallable, Category = "Village|Debug")
	void DebugChoose(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Village|Debug")
	void DebugSilkwormAction(int32 Action) { SilkwormAction(Action); }

	UFUNCTION(BlueprintCallable, Category = "Village|Debug")
	void DebugCloseDayEnd() { CloseDayEnd(); }

	/** Presses the open timing bar at Position (0..1), as if E were pressed with the marker there. */
	UFUNCTION(BlueprintCallable, Category = "Village|Debug")
	void DebugTimingPress(float Position) { OnTimingPress(Position); }

	/** Finishes the open reeling panel with the given silk quality (0..100). */
	UFUNCTION(BlueprintCallable, Category = "Village|Debug")
	void DebugFinishReeling(float Quality);

	/** Skips ahead as if the earlier days were done: 3 = morning of Day 3, 4 = Day 4, 5 = Day 5, 6 or more = festival morning. */
	UFUNCTION(BlueprintCallable, Category = "Village|Debug")
	void DebugJumpToDay(int32 Day);

	/** Presses a craft's timing bar at the centre of every round until it ends (perfect score). */
	UFUNCTION(BlueprintCallable, Category = "Village|Debug")
	void DebugAutoCraft();

	UFUNCTION(BlueprintCallable, Category = "Village|Debug")
	void DebugSetCulture(int32 Value) { Culture = Value; }

	/** Flags, quests, progress and what is on screen, as one line of text. */
	UFUNCTION(BlueprintCallable, Category = "Village|Debug")
	FString DebugDescribe() const;

	// Inventory (BPC_Inventory on the player pawn, called by reflection).
	int32 GetItemCount(FName ItemId) const;
	bool GiveItem(FName ItemId, int32 Quantity);
	bool TakeItem(FName ItemId, int32 Quantity);

	// Called by widgets.
	void OnDialogueChoice(const FVillageDialogueChoice& Choice);
	void OnDialogueFinished(const TArray<FString>& Actions);
	void SilkwormAction(int32 Action);
	void CloseSilkworm();
	void CloseDayEnd();
	void OnTimingPress(float Position);
	void CloseTiming();
	/** The action mini-games (Slider, Sequence, Lanes, Trace) report a 0..100 score here. */
	void OnActionGameDone(float Score);
	void CloseActionGame();
	void OnReelingBreak();
	void OnReelingStage(int32 Stage, float Quality);
	void CloseReeling();

private:
	void HandleWorldReady(const FActorsInitializedParams& Params);
	void BeginChapter();
	void BeginMiniGameTest();
	bool IsStoryWorld(const UWorld* World) const;

	FName PickDialogue(FName NpcId) const;
	void SetTalking(AActor* Npc, bool bTalking);
	/** Takes one unit from a source holding Stock; false (with the wait) while it regrows. */
	bool DrawFromSource(AActor* Source, int32 Stock, double RegrowSeconds, int32& OutWaitSeconds);
	void HarvestMulberry(AActor* Bush);
	void OpenSilkworm();
	void RefreshSilkworm(const FText& Result);
	void StepSilkworm();
	TArray<FText> SilkwormWarnings() const;
	/** Sunset, then the "DAY n - COMPLETED" card; CloseDayEnd opens the next morning. */
	void EndDay(int32 Day = 1);
	/** Same sequence for Chapters 4-6: the card comes from a DT_VillageDays row. */
	void EndDayRow(FName Row);
	void StartSunset();
	void ShowRowCard();

	// Chapters 4-6: data-driven crafts, gathering, skills, culture and the final score.
	void OpenCraft(FName Key);
	void PressCraft(float Position);
	void FinishCraft(const FVillageCraftRow& Row);
	void Gather(AActor* Source);
	float CraftBonus(FName Skill) const;
	int32 SkillLevel(FName Skill) const;
	void AddSkillExp(FName Skill, int32 Amount);
	void AddCulture(int32 Amount);
	void AddRelationship(FName NpcId, int32 Amount);
	void SetSlots(int32 Count);
	void UseSlot(int32 Count);
	void ClearFlagsWithPrefix(const FString& Prefix);
	FText ExpandText(const FText& Text) const;
	bool FlagsMatch(const TArray<FName>& Require, const TArray<FName>& Block) const;
	void ComputeFinalScore();
	void FestivalReset();
	void ResetObjective(FName QuestId, FName ObjectiveId);

	/** Actors tagged ShowFlag_<Flag> stay hidden (and without collision) until that story flag is set, so props and stations
	 *  appear as the story reaches them instead of all being on show from the start. Scripts/apply_unlock_gates.py sets the tags. */
	void BuildFlagGates();
	void ApplyFlagGate(FName Flag, bool bOn);

	// Chapter 3
	void OpenSpinning();
	void PressSpin(float Position);
	void FinishSilkBatch(float Final, const FText& TierName);

	// Chapter 2
	void HarvestDecoration(AActor* Source);
	void OpenCocoon();
	void OpenGate();
	void OpenHang();
	void PlaceFestivalSpot(AActor* Spot);
	void OpenReelingPot();
	void OpenTiming(EVillageMiniGame Game, const FText& Title, const FText& Hint);
	void NextTimingRound(const FText& Result, const FLinearColor& ResultColor);
	void PressCocoon(float Position);
	void PressGate(float Position);
	void PressHang(float Position);
	void PressFindThread(float Position);
	void OpenReeling();
	void ShowTagged(FName Tag, bool bShow);
	float CocoonScore() const;
	bool IsObjectiveDone(FName QuestId, FName ObjectiveId) const;
	int32 ObjectiveTarget(FName QuestId, FName ObjectiveId) const;
	void TickSunset();
	void SpawnNameplates();
	void UpdateNameplates();
	/** True while the current quest step is to talk to this NPC (drives the HUD guide marker). */
	bool NpcNeeded(FName NpcId) const;
	/** Name of a usable item/station and, for mini-game areas, what the area is for. False if it has no nameplate. */
	bool DescribeInteractable(const AActor* Actor, FText& OutName, FText& OutPurpose) const;

	void CompleteQuest(FName QuestId);
	void SyncItemObjectives();
	void RefreshTracker();

	void EnterUI(UUserWidget* Focus);
	void ExitUI();
	APlayerController* GetPC() const;
	UActorComponent* FindInventory() const;
	void PlaySfx(const TCHAR* Path) const;
	template <typename T> T* MakeWidget();

	UPROPERTY(Transient) TObjectPtr<UDataTable> DialogueTable;
	UPROPERTY(Transient) TObjectPtr<UDataTable> TalkRuleTable;
	UPROPERTY(Transient) TObjectPtr<UDataTable> QuestTable;
	UPROPERTY(Transient) TObjectPtr<UDataTable> CraftTable;
	UPROPERTY(Transient) TObjectPtr<UDataTable> DayTable;

	UPROPERTY(Transient) TObjectPtr<UVillageHUDWidget> Hud;
	UPROPERTY(Transient) TObjectPtr<UVillageDialogueWidget> DialogueWidget;
	UPROPERTY(Transient) TObjectPtr<UVillageSilkwormWidget> SilkwormWidget;
	UPROPERTY(Transient) TObjectPtr<UVillageDayEndWidget> DayEndWidget;
	UPROPERTY(Transient) TObjectPtr<UVillageTimingWidget> TimingWidget;
	UPROPERTY(Transient) TObjectPtr<UVillageReelingWidget> ReelingWidget;
	UPROPERTY(Transient) TObjectPtr<UVillageActionWidget> ActionWidget;

	TSet<FName> Flags;
	TMultiMap<FName, TWeakObjectPtr<AActor>> FlagGated;
	TArray<FName> ActiveQuests;
	TMap<FName, int32> Progress; // "Quest.Objective" -> count
	// Stock left on a harvestable source: mulberry bushes and festival material sources.
	TMap<TWeakObjectPtr<AActor>, double> BushRegrowAt;
	TMap<TWeakObjectPtr<AActor>, int32> BushLeaves;
	TWeakObjectPtr<AActor> TalkingNpc;
	FVillageSilkwormState Silkworm;

	// Chapter 2 mini-games.
	EVillageMiniGame MiniGame = EVillageMiniGame::None;
	FVillageTimingRound Round;
	int32 RoundIndex = 0;
	int32 RoundScore = 0;
	int32 DecorationScore = 0; // gate + hanging, each 0..100
	TMap<FName, int32> CocoonQuality; // Poor / Normal / Good / Excellent -> cocoons harvested on Day 2
	TSet<FName> PlacedSpots;
	int32 TotalSpots = 0;
	float ReelStartQuality = 100.f;

	// Which day the running end-of-day sequence closes (1..3), or 0 with EndingRow set (Chapters 4-6).
	int32 EndingDay = 1;
	FName EndingRow;

	// Chapters 4-6 progress: skill EXP per craft, culture and relationship points, last craft results.
	TMap<FName, int32> SkillExp;
	TMap<FName, int32> Relationship;
	TMap<FName, int32> CraftStars;
	TMap<FName, int32> CraftScore;
	int32 Culture = 0;
	int32 Slots = 0; // time slots left today on the free preparation days
	FName ActiveCraft;
	// Chapter 3 spinning: round score 0..100 and how many threads came out.
	int32 SpinTangled = 0;
	int32 SpinWeak = 0;

	TWeakObjectPtr<AActor> Sun;
	FRotator SunStart;
	FLinearColor SunColorStart;
	float SunsetAlpha = 0.f;
	FTimerHandle SunsetTimer;
	FTimerHandle BeginTimer;
	FTimerHandle NameplateTimer;
	TArray<TWeakObjectPtr<class UWidgetComponent>> Nameplates;
	TArray<TWeakObjectPtr<AActor>> CachedInteractables;
	// Plates over materials and mini-game areas; shown only while the item is lit up for the player.
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<class UWidgetComponent>> InteractPlates;
	FDelegateHandle WorldReadyHandle;
};
