// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"

#include "VillageStoryTypes.generated.h"

/**
 * Story data for the Silk Village chapters. Writers edit three DataTables:
 * DT_VillageDialogue (what is said), DT_VillageTalkRules (which dialogue an NPC picks right now)
 * and DT_VillageQuests (quest titles and objectives). Actions are short strings run by
 * UVillageStorySubsystem::RunActions, e.g. "StartQuest:Q01", "Progress:Q01.MeetSilkMaster",
 * "GiveItem:MAT_COCOON_001:3", "SetFlag:Knowledge.Silkworm", "EndDay".
 */

USTRUCT(BlueprintType)
struct FVillageDialogueLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText Speaker;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (MultiLine = true))
	FText Text;
};

USTRUCT(BlueprintType)
struct FVillageDialogueChoice
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FString> Actions;

	/** Dialogue that follows this choice; None closes the conversation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FName NextDialogue;
};

USTRUCT(BlueprintType)
struct FVillageDialogueRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FVillageDialogueLine> Lines;

	/** Shown after the last line. Empty means the conversation simply ends. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FVillageDialogueChoice> Choices;

	/** Run after the last line when there are no choices. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FString> Actions;
};

/** Rows are checked top to bottom; the first rule whose flags match decides the NPC's dialogue. */
USTRUCT(BlueprintType)
struct FVillageTalkRuleRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FName NpcId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FName> RequireFlags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TArray<FName> BlockFlags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FName Dialogue;
};

USTRUCT(BlueprintType)
struct FVillageObjectiveDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	int32 Target = 1;

	/** When set, items already held when the quest starts count, then every one received adds progress. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FName TrackItem;
};

USTRUCT(BlueprintType)
struct FVillageQuestRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TArray<FVillageObjectiveDef> Objectives;

	/** Run once every objective is done. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest")
	TArray<FString> OnComplete;
};

/** One timing-bar round of a craft: where the good band sits and how fast the marker moves. */
USTRUCT(BlueprintType)
struct FVillageCraftRound
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Craft")
	FText Label;

	/** Centre of the good band (0..1); a negative value picks a random one each time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Craft")
	float Centre = -1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Craft")
	float GoodHalf = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Craft")
	float PerfectHalf = 0.04f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Craft")
	float Speed = 0.8f;

	/** Said when the press lands before / after the good band. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Craft")
	FText TooLow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Craft")
	FText TooHigh;
};

/**
 * A mini-game made of timing rounds (dyeing, weaving, cutting, sewing, practice, the festival challenge).
 * The row name is the key a station actor carries as its ItemID (tag Interact_Craft).
 */
USTRUCT(BlueprintType)
struct FVillageCraftRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Craft")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Craft", meta = (MultiLine = true))
	FText Hint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Craft")
	TArray<FName> RequireFlags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Craft")
	TArray<FName> BlockFlags;

	/** "ITEM_ID:count" strings: must be in the bag to start, used up when the craft succeeds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Craft")
	TArray<FString> TakeItems;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Craft")
	TArray<FVillageCraftRound> Rounds;

	/** Which mini-game runs: empty = the timing bar; Slider, Sequence, Lanes or Trace = the action mini-games (UVillageActionWidget). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Craft")
	FName Mode;

	/** Skill key (Care, Reel, Spin, Dye, Weave, Tailor): its level widens the bands and the craft earns its EXP. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Craft")
	FName Skill;

	/** Time slots used on the free preparation days (0 = free). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Craft")
	int32 SlotCost = 0;

	/** Average score (0..100) below which the craft fails and OnFail runs instead of OnDone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Craft")
	int32 FailBelow = 0;

	/** Toast on success; {0} = stars, {1} = score. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Craft")
	FText ResultText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Craft")
	TArray<FString> OnDone;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Craft")
	TArray<FString> OnFail;
};

/**
 * The end-of-day card for days after Chapter 3 ("EndDay:<row name>"). Texts may carry {Culture}, {Slots},
 * {Stars:<craft>}, {Score:<key>} and {Item:<item id>}.
 */
USTRUCT(BlueprintType)
struct FVillageDayRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day")
	TArray<FText> Lines;

	/** Actions run when the card opens (flags for the finished day). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day")
	TArray<FString> SetFlags;

	/** Actions run next morning, when the card closes (start the next quests). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day")
	TArray<FString> OnClose;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Day")
	FText Morning;
};

/** Silkworm tray simulation, advanced one step per player action. */
USTRUCT(BlueprintType)
struct FVillageSilkwormState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Silkworm") float Health = 80.f;
	UPROPERTY(BlueprintReadOnly, Category = "Silkworm") float Hunger = 30.f;
	UPROPERTY(BlueprintReadOnly, Category = "Silkworm") float Growth = 25.f;
	UPROPERTY(BlueprintReadOnly, Category = "Silkworm") float Temperature = 28.f;
	UPROPERTY(BlueprintReadOnly, Category = "Silkworm") float Humidity = 65.f;
	UPROPERTY(BlueprintReadOnly, Category = "Silkworm") float Cleanliness = 85.f;
	UPROPERTY(BlueprintReadOnly, Category = "Silkworm") int32 Steps = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Silkworm") bool bFinished = false;
};

/**
 * One round of a timing bar (Chapter 2: cocoon harvest, festival gate and decorations, finding the
 * silk thread). Positions are 0..1 along the bar; the perfect band sits inside the good band.
 */
USTRUCT(BlueprintType)
struct FVillageTimingRound
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Timing") FText Label;
	UPROPERTY(BlueprintReadOnly, Category = "Timing") float GoodMin = 0.4f;
	UPROPERTY(BlueprintReadOnly, Category = "Timing") float GoodMax = 0.6f;
	UPROPERTY(BlueprintReadOnly, Category = "Timing") float PerfectMin = 0.47f;
	UPROPERTY(BlueprintReadOnly, Category = "Timing") float PerfectMax = 0.53f;
	/** Bar lengths per second. */
	UPROPERTY(BlueprintReadOnly, Category = "Timing") float Speed = 0.6f;
	/** Bounces between the ends; otherwise it runs once and reaching the end counts as a press at 1. */
	UPROPERTY(BlueprintReadOnly, Category = "Timing") bool bPingPong = true;

	bool IsPerfect(float Position) const { return Position >= PerfectMin && Position <= PerfectMax; }
	bool IsGood(float Position) const { return Position >= GoodMin && Position <= GoodMax; }
};
