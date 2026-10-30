// Copyright Epic Games, Inc. All Rights Reserved.

#include "VillageStorySubsystem.h"

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Animation/AnimationAsset.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/ComboBoxString.h"
#include "Components/InputComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"
#include "Engine/DataTable.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "VillageWidgets.h"

#define LOCTEXT_NAMESPACE "VillageStory"

DEFINE_LOG_CATEGORY_STATIC(LogVillageStory, Log, All);

namespace
{
	const TCHAR* DialogueTablePath = TEXT("/Game/SilkVillage/Story/DT_VillageDialogue.DT_VillageDialogue");
	const TCHAR* TalkRuleTablePath = TEXT("/Game/SilkVillage/Story/DT_VillageTalkRules.DT_VillageTalkRules");
	const TCHAR* QuestTablePath = TEXT("/Game/SilkVillage/Story/DT_VillageQuests.DT_VillageQuests");
	const TCHAR* CraftTablePath = TEXT("/Game/SilkVillage/Story/DT_VillageCrafts.DT_VillageCrafts");
	const TCHAR* DayTablePath = TEXT("/Game/SilkVillage/Story/DT_VillageDays.DT_VillageDays");
	const TCHAR* StoryMapName = TEXT("Mountain");
	// Small flat map with every Chapter 2 station (Scripts/build_minigame_test_level.py); starts on Day 2.
	const TCHAR* TestMapName = TEXT("Chapter2_MiniGameTest");
	// Same idea for Chapter 3 (Scripts/build_chapter3_test_level.py); starts on the morning of Day 3.
	const TCHAR* Test3MapName = TEXT("Chapter3_MiniGameTest");
	// Chapters 4-6 (Scripts/build_chapter4_6_test_level.py); starts on the morning of Day 4.
	const TCHAR* Test46MapName = TEXT("Chapter4to6_MiniGameTest");

	// Craft skills (DT_VillageCrafts "Skill" column); each has its own level on the free preparation days.
	const TCHAR* const SkillKeys[] = { TEXT("Care"), TEXT("Reel"), TEXT("Spin"), TEXT("Dye"), TEXT("Weave"), TEXT("Tailor") };

	constexpr int32 LeavesPerBush = 2;
	constexpr double BushRegrowSeconds = 45.0;
	constexpr int32 LeavesPerFeed = 3;

	FName ProgressKey(FName Quest, FName Objective)
	{
		return FName(*FString::Printf(TEXT("%s.%s"), *Quest.ToString(), *Objective.ToString()));
	}

	/** Reads a Blueprint variable (e.g. BP_ItemPickup's ItemID) by reflection. */
	template <typename TProperty, typename TValue>
	bool ReadProperty(const UObject* Object, const TCHAR* Name, TValue& Out)
	{
		if (!Object)
		{
			return false;
		}
		if (const TProperty* Property = FindFProperty<TProperty>(Object->GetClass(), Name))
		{
			Out = Property->GetPropertyValue_InContainer(Object);
			return true;
		}
		return false;
	}

	void ClearInventory(UActorComponent* Inventory);
}

void UVillageStorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	WorldReadyHandle = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UVillageStorySubsystem::HandleWorldReady);
}

UWorld* UVillageStorySubsystem::GetWorld() const
{
	return GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
}

template <typename T>
T* UVillageStorySubsystem::MakeWidget()
{
	T* Widget = CreateWidget<T>(GetPC(), T::StaticClass());
	Widget->Story = this;
	return Widget;
}

void UVillageStorySubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldReadyHandle);
	Super::Deinitialize();
}

bool UVillageStorySubsystem::IsStoryWorld(const UWorld* World) const
{
	return World && World->IsGameWorld() && (World->GetMapName().EndsWith(StoryMapName) || World->GetMapName().EndsWith(TestMapName) || World->GetMapName().EndsWith(Test3MapName) || World->GetMapName().EndsWith(Test46MapName));
}

void UVillageStorySubsystem::HandleWorldReady(const FActorsInitializedParams& Params)
{
	UWorld* World = Params.World;
	if (!IsStoryWorld(World) || World->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	// The player pawn and controller arrive a moment after actors initialize.
	World->GetTimerManager().SetTimer(BeginTimer, FTimerDelegate::CreateUObject(this, &UVillageStorySubsystem::BeginChapter), 0.5f, false);
}

void UVillageStorySubsystem::BeginChapter()
{
	if (!GetPC() || !GetPC()->GetPawn())
	{
		GetWorld()->GetTimerManager().SetTimer(BeginTimer, FTimerDelegate::CreateUObject(this, &UVillageStorySubsystem::BeginChapter), 0.5f, false);
		return;
	}
	DialogueTable = LoadObject<UDataTable>(nullptr, DialogueTablePath);
	TalkRuleTable = LoadObject<UDataTable>(nullptr, TalkRuleTablePath);
	QuestTable = LoadObject<UDataTable>(nullptr, QuestTablePath);
	// Chapters 4-6 tables are optional until Scripts/build_chapter4_6_story.py has run.
	CraftTable = LoadObject<UDataTable>(nullptr, CraftTablePath);
	DayTable = LoadObject<UDataTable>(nullptr, DayTablePath);
	if (!DialogueTable || !TalkRuleTable || !QuestTable)
	{
		UE_LOG(LogVillageStory, Error, TEXT("Story tables missing under /Game/SilkVillage/Story; run Scripts/build_chapter1_story.py"));
		return;
	}

	// A fresh chapter each time the map loads (no save game yet).
	Flags.Reset();
	ActiveQuests.Reset();
	Progress.Reset();
	BushRegrowAt.Reset();
	BushLeaves.Reset();
	Silkworm = FVillageSilkwormState();
	MiniGame = EVillageMiniGame::None;
	DecorationScore = 0;
	CocoonQuality.Reset();
	PlacedSpots.Reset();
	ReelStartQuality = 100.f;
	EndingDay = 1;
	EndingRow = NAME_None;
	SpinTangled = 0;
	SpinWeak = 0;
	SkillExp.Reset();
	Relationship.Reset();
	CraftStars.Reset();
	CraftScore.Reset();
	Culture = 0;
	Slots = 0;
	ActiveCraft = NAME_None;
	ClearInventory(FindInventory());
	// Ensure clean Virtual Shadow Map bias and Lumen GI so the terrain never displays shadow acne stripes
	auto SetCVarFloat = [](const TCHAR* Name, float Val)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(Val, ECVF_SetByScalability);
		}
	};
	SetCVarFloat(TEXT("r.Shadow.Virtual.ResolutionLodBiasDirectional"), 0.0f);
	SetCVarFloat(TEXT("r.DistanceFieldAO"), 0.0f);
	SetCVarFloat(TEXT("r.Lumen.DiffuseIndirect.Allow"), 1.0f);
	SetCVarFloat(TEXT("r.Lumen.Reflections.Allow"), 1.0f);
	SetCVarFloat(TEXT("r.VolumetricCloud.ShadowMap"), 0.0f);

	// Give a starter silk scarf so the player can immediately test selling in the shop, plus 500 coins to test buying.
	GiveItem(FName(TEXT("ACC_SILK_SCARF_001")), 1);
	if (FIntProperty* Coin = FindFProperty<FIntProperty>(GetGameInstance()->GetClass(), TEXT("VillageCoin")))
	{
		Coin->SetPropertyValue_InContainer(GetGameInstance(), 500);
		if (UFunction* SaveCoin = GetGameInstance()->FindFunction(TEXT("SaveCoin")); SaveCoin && SaveCoin->ParmsSize == 0)
		{
			GetGameInstance()->ProcessEvent(SaveCoin, nullptr);
		}
	}
	SyncCoinHUD();
	if (APlayerController* PC = GetPC(); PC && PC->InputComponent)
	{
		PC->InputComponent->BindKey(EKeys::B, IE_Pressed, this, &UVillageStorySubsystem::OpenShop).bConsumeInput = false;
		FInputKeyBinding& EscBind = PC->InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &UVillageStorySubsystem::HandleEscapeKey);
		EscBind.bConsumeInput = false;
		EscBind.bExecuteWhenPaused = true;
		FInputKeyBinding& PBind = PC->InputComponent->BindKey(EKeys::P, IE_Pressed, this, &UVillageStorySubsystem::HandleEscapeKey);
		PBind.bConsumeInput = false;
		PBind.bExecuteWhenPaused = true;
	}

	// Festival decorations appear as the player puts them up (Chapter2_Village sublevel).
	ShowTagged(TEXT("FestivalDecor_Gate"), false);
	ShowTagged(TEXT("FestivalDecor_Hang"), false);
	ShowTagged(TEXT("FestivalProp"), false);
	BuildFlagGates();
	TotalSpots = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TotalSpots += It->ActorHasTag(TEXT("Interact_FestivalSpot")) ? 1 : 0;
	}

	Hud = MakeWidget<UVillageHUDWidget>();
	Hud->AddToViewport(5);
	SpawnNameplates();
	if (GetWorld()->GetMapName().EndsWith(Test46MapName))
	{
		DebugJumpToDay(4);
		return;
	}
	if (GetWorld()->GetMapName().EndsWith(Test3MapName))
	{
		DebugJumpToDay(3);
		return;
	}
	if (GetWorld()->GetMapName().EndsWith(TestMapName))
	{
		BeginMiniGameTest();
		return;
	}
	StartQuest(TEXT("Q00"));
	Notify(LOCTEXT("Arrive", "Làng Tơ Lụa – Lễ hội Tơ Lụa còn 7 ngày nữa"));
}

void UVillageStorySubsystem::BeginMiniGameTest()
{
	// As if Chapter 1 had just ended: Day 1 flags set, cocoons from Day 1 in the bag, Day 2 quests running.
	for (const TCHAR* Flag : { TEXT("Quest.Q00.Done"), TEXT("Quest.Q01.Done"), TEXT("Quest.Q02.Done"), TEXT("Quest.Q03.Done"),
		TEXT("Day1.Complete"), TEXT("Unlock.ReelingWorkshop"), TEXT("Unlock.SpinningWorkshop"), TEXT("Knowledge.SilkProcess"),
		TEXT("Knowledge.SilkwormRaising"), TEXT("Day2.Started") })
	{
		SetFlag(Flag);
	}
	Silkworm.bFinished = true;
	const int32 Have = GetItemCount(CocoonItem);
	if (Have < 32)
	{
		GiveItem(CocoonItem, 32 - Have);
	}
	StartQuest(TEXT("Q10"));
	StartQuest(TEXT("Q11"));
	Notify(LOCTEXT("TestMap", "Bản đồ thử mini-game – Ngày 2 (đã có 32 kén từ Ngày 1)"));
}

APlayerController* UVillageStorySubsystem::GetPC() const
{
	UWorld* World = GetWorld();
	return World ? World->GetFirstPlayerController() : nullptr;
}

// ---------------------------------------------------------------- Interaction

void UVillageStorySubsystem::HandleInteract(AActor* Target, AActor* Interactor)
{
	// No talking while the day ends: the sunset runs a few seconds before the day-end panel opens.
	if (!Target || !Hud || DialogueWidget || SilkwormWidget || DayEndWidget || TimingWidget || ReelingWidget || ActionWidget
		|| GetWorld()->GetTimerManager().IsTimerActive(SunsetTimer))
	{
		return;
	}
	if (Target->ActorHasTag(TEXT("Interact_Shop")))
	{
		OpenShop();
		return;
	}
	if (Target->ActorHasTag(TEXT("Interact_MulberryBush")))
	{
		HarvestMulberry(Target);
		return;
	}
	if (Target->ActorHasTag(TEXT("Interact_SilkwormTray")))
	{
		// Day 2 reuses the trays: the batches have spun their cocoons and are ready to pick.
		HasFlag(TEXT("Day2.Started")) ? OpenCocoon() : OpenSilkworm();
		return;
	}
	if (Target->ActorHasTag(TEXT("Interact_DecoMaterial")))
	{
		HarvestDecoration(Target);
		return;
	}
	if (Target->ActorHasTag(TEXT("Interact_FestivalGate")))
	{
		OpenGate();
		return;
	}
	if (Target->ActorHasTag(TEXT("Interact_FestivalHang")))
	{
		OpenHang();
		return;
	}
	if (Target->ActorHasTag(TEXT("Interact_FestivalSpot")))
	{
		PlaceFestivalSpot(Target);
		return;
	}
	if (Target->ActorHasTag(TEXT("Interact_ReelingPot")))
	{
		OpenReelingPot();
		return;
	}
	if (Target->ActorHasTag(TEXT("Interact_SpinningWheel")))
	{
		OpenSpinning();
		return;
	}
	if (Target->ActorHasTag(TEXT("Interact_Craft")))
	{
		FName Key;
		ReadProperty<FNameProperty>(Target, TEXT("ItemID"), Key);
		OpenCraft(Key);
		return;
	}
	if (Target->ActorHasTag(TEXT("Interact_Gather")))
	{
		Gather(Target);
		return;
	}
	if (Target->ActorHasTag(TEXT("Interact_Dialogue")))
	{
		// A sign or stall that simply tells a story: its ItemID is the dialogue row.
		FName DialogueId;
		ReadProperty<FNameProperty>(Target, TEXT("ItemID"), DialogueId);
		TalkingNpc.Reset();
		StartDialogue(DialogueId);
		return;
	}
	FName NpcId;
	if (Target->ActorHasTag(TEXT("NPC")) && ReadProperty<FNameProperty>(Target, TEXT("ItemID"), NpcId))
	{
		SetFlag(FName(*(TEXT("Need.") + NpcId.ToString())), false); // the guide marker goes away once they are visited
		const FName Dialogue = PickDialogue(NpcId);
		if (Dialogue.IsNone())
		{
			UE_LOG(LogVillageStory, Warning, TEXT("No talk rule matches %s"), *NpcId.ToString());
			return;
		}
		TalkingNpc = Target;
		SetTalking(Target, true);
		StartDialogue(Dialogue);
	}
}

void UVillageStorySubsystem::OpenShop()
{
	APlayerController* PC = GetPC();
	if (!PC || !Hud || DialogueWidget || SilkwormWidget || DayEndWidget || TimingWidget || ReelingWidget || ActionWidget
		|| GetWorld()->GetTimerManager().IsTimerActive(SunsetTimer))
	{
		return;
	}
	UClass* ShopClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/SilkVillage/UI/WBP_Shop.WBP_Shop_C"));
	if (!ShopClass)
	{
		return;
	}
	TArray<UUserWidget*> Existing;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Existing, ShopClass, false);
	for (UUserWidget* W : Existing)
	{
		if (W && W->IsInViewport())
		{
			W->RemoveFromParent();
			ExitUI();
			return;
		}
	}
	if (UUserWidget* Shop = CreateWidget<UUserWidget>(PC, ShopClass))
	{
		Shop->AddToViewport(25);
		UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(PC, Shop, EMouseLockMode::DoNotLock, true);
		PC->bShowMouseCursor = true;
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Open.SFX_Open"));
	}
}

void UVillageStorySubsystem::SyncCoinHUD()
{
	if (!GetWorld())
	{
		return;
	}
	int32 CurrentCoins = 0;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (FIntProperty* CoinProp = FindFProperty<FIntProperty>(GI->GetClass(), TEXT("VillageCoin")))
		{
			CurrentCoins = CoinProp->GetPropertyValue_InContainer(GI);
		}
	}

	UUserWidget* PrimaryCoinHud = nullptr;
	if (APlayerController* PC = GetPC())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			if (FObjectPropertyBase* HudProp = FindFProperty<FObjectPropertyBase>(Pawn->GetClass(), TEXT("HUDRef")))
			{
				PrimaryCoinHud = Cast<UUserWidget>(HudProp->GetObjectPropertyValue_InContainer(Pawn));
			}
		}
	}

	TArray<UUserWidget*> AllWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), AllWidgets, UUserWidget::StaticClass(), false);
	for (UUserWidget* W : AllWidgets)
	{
		if (!W || !W->GetClass()->GetName().StartsWith(TEXT("WBP_CoinHUD")))
		{
			continue;
		}
		if (!PrimaryCoinHud && W->IsInViewport())
		{
			PrimaryCoinHud = W;
		}
		if (W != PrimaryCoinHud)
		{
			W->SetVisibility(ESlateVisibility::Collapsed);
			W->RemoveFromParent();
			continue;
		}
		if (UTextBlock* CoinTxt = Cast<UTextBlock>(W->GetWidgetFromName(TEXT("TXT_CoinAmount"))))
		{
			CoinTxt->SetText(FText::AsNumber(CurrentCoins));
		}
		if (W->WidgetTree)
		{
			W->WidgetTree->ForEachWidget([&](UWidget* SubW)
			{
				if (UTextBlock* TB = Cast<UTextBlock>(SubW))
				{
					if (TB->GetName() != TEXT("TXT_CoinAmount"))
					{
						const FString Txt = TB->GetText().ToString().TrimStartAndEnd();
						if (Txt == TEXT("500") || Txt.IsNumeric())
						{
							TB->SetText(FText::GetEmpty());
							TB->SetVisibility(ESlateVisibility::Collapsed);
						}
					}
				}
			});
		}
	}
}

void UVillageStorySubsystem::HandleEscapeKey()
{
	APlayerController* PC = GetPC();
	if (!PC || !GetWorld())
	{
		return;
	}

	// 1. If a story dialogue or minigame is active, handle or close it cleanly.
	if (DialogueWidget || DayEndWidget)
	{
		return;
	}
	if (SilkwormWidget)
	{
		CloseSilkworm();
		return;
	}
	if (TimingWidget)
	{
		CloseTiming();
		return;
	}
	if (ReelingWidget)
	{
		CloseReeling();
		return;
	}
	if (ActionWidget)
	{
		CloseActionGame();
		return;
	}

	// 2. If Shop, Inventory, or BigMap is open, close it first.
	TArray<UUserWidget*> AllWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), AllWidgets, UUserWidget::StaticClass(), false);
	UUserWidget* OpenSettings = nullptr;
	for (UUserWidget* W : AllWidgets)
	{
		if (!W || !W->IsInViewport())
		{
			continue;
		}
		const FString ClassName = W->GetClass()->GetName();
		if (ClassName.StartsWith(TEXT("WBP_SettingsMenu")))
		{
			OpenSettings = W;
		}
		else if ((ClassName.StartsWith(TEXT("WBP_Shop"))
			|| ClassName.StartsWith(TEXT("WBP_Inventory"))
			|| ClassName.StartsWith(TEXT("WBP_BigMap")))
			&& W->GetVisibility() != ESlateVisibility::Collapsed
			&& W->GetVisibility() != ESlateVisibility::Hidden)
		{
			W->RemoveFromParent();
			PC->SetPause(false);
			ExitUI();
			return;
		}
	}

	// 3. Toggle WBP_SettingsMenu.
	if (OpenSettings)
	{
		OpenSettings->RemoveFromParent();
		PC->SetPause(false);
		ExitUI();
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Close.SFX_Close"));
		return;
	}

	if (UClass* SettingsClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/SilkVillage/UI/WBP_SettingsMenu.WBP_SettingsMenu_C")))
	{
		if (UUserWidget* SettingsWidget = CreateWidget<UUserWidget>(PC, SettingsClass))
		{
			SettingsWidget->AddToViewport(60);
			if (UGameUserSettings* GUS = UGameUserSettings::GetGameUserSettings())
			{
				if (UComboBoxString* QualityCombo = Cast<UComboBoxString>(SettingsWidget->GetWidgetFromName(TEXT("COMBO_Quality"))))
				{
					const int32 QualityIdx = FMath::Clamp(GUS->GetShadowQuality(), 0, 3);
					if (QualityCombo->GetOptionCount() > QualityIdx)
					{
						QualityCombo->SetSelectedIndex(QualityIdx);
					}
				}
			}
			UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(PC, SettingsWidget, EMouseLockMode::DoNotLock, false, false);
			PC->bShowMouseCursor = true;
			PC->SetShowMouseCursor(true);
			PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Open.SFX_Open"));
		}
	}
}

FName UVillageStorySubsystem::PickDialogue(FName NpcId) const
{
	FName Result;
	TalkRuleTable->ForeachRow<FVillageTalkRuleRow>(TEXT("PickDialogue"), [&](const FName&, const FVillageTalkRuleRow& Rule)
	{
		if (!Result.IsNone() || Rule.NpcId != NpcId)
		{
			return;
		}
		for (const FName& Flag : Rule.RequireFlags)
		{
			if (!Flags.Contains(Flag)) return;
		}
		for (const FName& Flag : Rule.BlockFlags)
		{
			if (Flags.Contains(Flag)) return;
		}
		Result = Rule.Dialogue;
	});
	return Result;
}

void UVillageStorySubsystem::SetTalking(AActor* Npc, bool bTalking)
{
	if (!Npc)
	{
		return;
	}
	UAnimationAsset* Anim = nullptr;
	UObject* Value = nullptr;
	if (ReadProperty<FObjectProperty>(Npc, bTalking ? TEXT("TalkAnimation") : TEXT("IdleAnimation"), Value))
	{
		Anim = Cast<UAnimationAsset>(Value);
	}
	USkeletalMeshComponent* Mesh = Npc->FindComponentByClass<USkeletalMeshComponent>();
	if (Mesh && Anim)
	{
		Mesh->PlayAnimation(Anim, true);
	}
}

// ---------------------------------------------------------------- Dialogue

void UVillageStorySubsystem::StartDialogue(FName DialogueId)
{
	const FVillageDialogueRow* Row = DialogueTable ? DialogueTable->FindRow<FVillageDialogueRow>(DialogueId, TEXT("StartDialogue")) : nullptr;
	if (!Row)
	{
		UE_LOG(LogVillageStory, Error, TEXT("Dialogue %s not found"), *DialogueId.ToString());
		OnDialogueFinished({});
		return;
	}
	if (!DialogueWidget)
	{
		DialogueWidget = MakeWidget<UVillageDialogueWidget>();
		DialogueWidget->AddToViewport(20);
		EnterUI(DialogueWidget);
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Open.SFX_Open"));
	}
	FVillageDialogueRow Expanded = *Row; // lines may carry {Score:...}, {Culture}, {Stars:...} tokens
	for (FVillageDialogueLine& Line : Expanded.Lines)
	{
		Line.Text = ExpandText(Line.Text);
	}
	DialogueWidget->Play(Expanded);
}

void UVillageStorySubsystem::OnDialogueChoice(const FVillageDialogueChoice& Choice)
{
	PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_ItemClick.SFX_ItemClick"));
	if (!Choice.NextDialogue.IsNone())
	{
		RunActions(Choice.Actions);
		StartDialogue(Choice.NextDialogue);
		return;
	}
	OnDialogueFinished(Choice.Actions);
}

void UVillageStorySubsystem::OnDialogueFinished(const TArray<FString>& Actions)
{
	if (DialogueWidget)
	{
		DialogueWidget->RemoveFromParent();
		DialogueWidget = nullptr;
	}
	ExitUI();
	SetTalking(TalkingNpc.Get(), false);
	TalkingNpc.Reset();
	RunActions(Actions);
}

void UVillageStorySubsystem::RunActions(const TArray<FString>& Actions)
{
	for (const FString& Action : Actions)
	{
		FString Verb, Arg;
		if (!Action.Split(TEXT(":"), &Verb, &Arg))
		{
			Verb = Action;
		}
		Verb.TrimStartAndEndInline();
		Arg.TrimStartAndEndInline();
		if (Verb == TEXT("SetFlag"))
		{
			SetFlag(FName(*Arg));
		}
		else if (Verb == TEXT("ClearFlag"))
		{
			SetFlag(FName(*Arg), false);
		}
		else if (Verb == TEXT("StartQuest"))
		{
			StartQuest(FName(*Arg));
		}
		else if (Verb == TEXT("Progress"))
		{
			FString Quest, Rest, Objective, Amount;
			Arg.Split(TEXT("."), &Quest, &Rest);
			if (!Rest.Split(TEXT(":"), &Objective, &Amount))
			{
				Objective = Rest;
			}
			AddProgress(FName(*Quest), FName(*Objective), Amount.IsEmpty() ? 1 : FCString::Atoi(*Amount));
		}
		else if (Verb == TEXT("GiveItem") || Verb == TEXT("TakeItem"))
		{
			FString Item, Amount;
			if (!Arg.Split(TEXT(":"), &Item, &Amount))
			{
				Item = Arg;
			}
			const int32 Quantity = Amount.IsEmpty() ? 1 : FCString::Atoi(*Amount);
			Verb == TEXT("GiveItem") ? GiveItem(FName(*Item), Quantity) : TakeItem(FName(*Item), Quantity);
		}
		else if (Verb == TEXT("Notify"))
		{
			Notify(FText::FromString(Arg));
		}
		else if (Verb == TEXT("EndDay"))
		{
			// "EndDay" is Day 1, "EndDay:2" the end of Day 2, "EndDay:D4" a DT_VillageDays row (Chapters 4-6).
			if (Arg.IsEmpty() || Arg.IsNumeric())
			{
				EndDay(Arg.IsEmpty() ? 1 : FCString::Atoi(*Arg));
			}
			else
			{
				EndDayRow(FName(*Arg));
			}
		}
		else if (Verb == TEXT("AddCulture"))
		{
			AddCulture(FCString::Atoi(*Arg));
		}
		else if (Verb == TEXT("AddRel"))
		{
			FString Npc, Amount;
			if (Arg.Split(TEXT(":"), &Npc, &Amount))
			{
				AddRelationship(FName(*Npc), FCString::Atoi(*Amount));
			}
		}
		else if (Verb == TEXT("AddSkill"))
		{
			FString Skill, Amount;
			if (Arg.Split(TEXT(":"), &Skill, &Amount))
			{
				AddSkillExp(FName(*Skill), FCString::Atoi(*Amount));
			}
		}
		else if (Verb == TEXT("SetSlots"))
		{
			SetSlots(FCString::Atoi(*Arg));
		}
		else if (Verb == TEXT("UseSlot"))
		{
			UseSlot(Arg.IsEmpty() ? 1 : FCString::Atoi(*Arg));
		}
		else if (Verb == TEXT("ClearFlags"))
		{
			ClearFlagsWithPrefix(Arg);
		}
		else if (Verb == TEXT("ProgressIfStars"))
		{
			// "ProgressIfStars:Q33.PerfectThread:PracSpin:5": progress once the craft's last result has at least that many stars.
			TArray<FString> Parts;
			Arg.ParseIntoArray(Parts, TEXT(":"));
			FString Quest, Objective;
			if (Parts.Num() == 3 && Parts[0].Split(TEXT("."), &Quest, &Objective) && CraftStars.FindRef(FName(*Parts[1])) >= FCString::Atoi(*Parts[2]))
			{
				AddProgress(FName(*Quest), FName(*Objective));
			}
		}
		else if (Verb == TEXT("FinalScore"))
		{
			ComputeFinalScore();
		}
		else if (Verb == TEXT("FestivalReset"))
		{
			FestivalReset();
		}
		else if (Verb == TEXT("Dialogue"))
		{
			StartDialogue(FName(*Arg));
		}
		else if (Verb == TEXT("StartQuestIf"))
		{
			// "StartQuestIf:Q13:Quest.Q10.Done,Quest.Q11.Done": starts Q13 once every listed flag is set.
			FString Quest, Conditions;
			TArray<FString> Required;
			if (Arg.Split(TEXT(":"), &Quest, &Conditions))
			{
				Conditions.ParseIntoArray(Required, TEXT(","));
				if (Algo::AllOf(Required, [this](const FString& Flag) { return HasFlag(FName(*Flag.TrimStartAndEnd())); }))
				{
					StartQuest(FName(*Quest.TrimStartAndEnd()));
				}
			}
		}
		else if (Verb == TEXT("ResetObjective"))
		{
			FString Quest, Objective;
			if (Arg.Split(TEXT("."), &Quest, &Objective))
			{
				ResetObjective(FName(*Quest), FName(*Objective));
			}
		}
		else
		{
			UE_LOG(LogVillageStory, Warning, TEXT("Unknown story action '%s'"), *Action);
		}
	}
	RefreshTracker();
}

// ---------------------------------------------------------------- Flags and quests

void UVillageStorySubsystem::SetFlag(FName Flag, bool bValue)
{
	bValue ? (void)Flags.Add(Flag) : (void)Flags.Remove(Flag);
	if (FlagGated.Contains(Flag))
	{
		ApplyFlagGate(Flag, bValue);
	}
}

void UVillageStorySubsystem::BuildFlagGates()
{
	FlagGated.Reset();
	static const FString Prefix(TEXT("ShowFlag_"));
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		for (const FName& Tag : It->Tags)
		{
			const FString Name = Tag.ToString();
			if (Name.StartsWith(Prefix))
			{
				const FName Flag(*Name.RightChop(Prefix.Len()));
				FlagGated.Add(Flag, *It);
				const bool bOn = HasFlag(Flag);
				It->SetActorHiddenInGame(!bOn);
				It->SetActorEnableCollision(bOn);
			}
		}
	}
}

void UVillageStorySubsystem::ApplyFlagGate(FName Flag, bool bOn)
{
	TArray<TWeakObjectPtr<AActor>> Actors;
	FlagGated.MultiFind(Flag, Actors);
	for (const TWeakObjectPtr<AActor>& Actor : Actors)
	{
		if (Actor.IsValid())
		{
			Actor->SetActorHiddenInGame(!bOn);
			Actor->SetActorEnableCollision(bOn);
		}
	}
}

void UVillageStorySubsystem::StartQuest(FName QuestId)
{
	const FVillageQuestRow* Quest = QuestTable ? QuestTable->FindRow<FVillageQuestRow>(QuestId, TEXT("StartQuest")) : nullptr;
	if (!Quest || ActiveQuests.Contains(QuestId) || HasFlag(FName(*FString::Printf(TEXT("Quest.%s.Done"), *QuestId.ToString()))))
	{
		return;
	}
	ActiveQuests.Add(QuestId);
	SetFlag(FName(*FString::Printf(TEXT("Quest.%s.Active"), *QuestId.ToString())));
	if (QuestId != TEXT("Q00"))
	{
		Notify(FText::Format(LOCTEXT("NewQuest", "Nhiệm vụ mới: {0}"), Quest->Title));
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Open.SFX_Open"));
	}
	SyncItemObjectives();
	RefreshTracker();
}

void UVillageStorySubsystem::ResetObjective(FName QuestId, FName ObjectiveId)
{
	// Lets a quest step be played again (e.g. reel a new batch of silk) while its quest is still running.
	if (!ActiveQuests.Contains(QuestId))
	{
		return;
	}
	Progress.Remove(ProgressKey(QuestId, ObjectiveId));
	SetFlag(FName(*FString::Printf(TEXT("Obj.%s.%s"), *QuestId.ToString(), *ObjectiveId.ToString())), false);
	RefreshTracker();
}

void UVillageStorySubsystem::AddProgress(FName QuestId, FName ObjectiveId, int32 Amount)
{
	const FVillageQuestRow* Quest = QuestTable ? QuestTable->FindRow<FVillageQuestRow>(QuestId, TEXT("AddProgress")) : nullptr;
	if (!Quest || !ActiveQuests.Contains(QuestId))
	{
		return;
	}
	const FVillageObjectiveDef* Def = Quest->Objectives.FindByPredicate([&](const FVillageObjectiveDef& O) { return O.Id == ObjectiveId; });
	if (!Def)
	{
		return;
	}
	int32& Count = Progress.FindOrAdd(ProgressKey(QuestId, ObjectiveId));
	const bool bWasDone = Count >= Def->Target;
	Count = FMath::Min(Count + Amount, Def->Target);
	if (!bWasDone && Count >= Def->Target)
	{
		SetFlag(FName(*FString::Printf(TEXT("Obj.%s.%s"), *QuestId.ToString(), *ObjectiveId.ToString())));
		Notify(FText::Format(LOCTEXT("ObjDone", "✓ {0}"), Def->Text));
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_StageComplete.SFX_StageComplete"));
	}
	const bool bAllDone = Quest->Objectives.Num() > 0 && Algo::AllOf(Quest->Objectives, [&](const FVillageObjectiveDef& O)
	{
		return Progress.FindRef(ProgressKey(QuestId, O.Id)) >= O.Target;
	});
	if (bAllDone)
	{
		CompleteQuest(QuestId);
	}
	RefreshTracker();
}

void UVillageStorySubsystem::CompleteQuest(FName QuestId)
{
	const FVillageQuestRow* Quest = QuestTable->FindRow<FVillageQuestRow>(QuestId, TEXT("CompleteQuest"));
	ActiveQuests.Remove(QuestId);
	SetFlag(FName(*FString::Printf(TEXT("Quest.%s.Active"), *QuestId.ToString())), false);
	SetFlag(FName(*FString::Printf(TEXT("Quest.%s.Done"), *QuestId.ToString())));
	if (Quest)
	{
		RunActions(Quest->OnComplete);
	}
}

void UVillageStorySubsystem::SyncItemObjectives()
{
	for (const FName& QuestId : TArray<FName>(ActiveQuests))
	{
		const FVillageQuestRow* Quest = QuestTable->FindRow<FVillageQuestRow>(QuestId, TEXT("Sync"));
		if (!Quest)
		{
			continue;
		}
		for (const FVillageObjectiveDef& O : Quest->Objectives)
		{
			if (!O.TrackItem.IsNone())
			{
				const int32 Have = GetItemCount(O.TrackItem);
				const int32 Done = Progress.FindRef(ProgressKey(QuestId, O.Id));
				if (Have > Done)
				{
					AddProgress(QuestId, O.Id, Have - Done);
				}
			}
		}
	}
}

void UVillageStorySubsystem::RefreshTracker()
{
	if (!Hud || !QuestTable)
	{
		return;
	}
	// Newest quest is the panel title; older active quests follow as sub-headings.
	TArray<FText> Lines;
	FText Title;
	for (int32 i = ActiveQuests.Num() - 1; i >= 0; --i)
	{
		const FName QuestId = ActiveQuests[i];
		const FVillageQuestRow* Quest = QuestTable->FindRow<FVillageQuestRow>(QuestId, TEXT("Tracker"));
		if (!Quest)
		{
			continue;
		}
		if (Title.IsEmpty())
		{
			Title = Quest->Title;
		}
		else
		{
			Lines.Add(FText::Format(LOCTEXT("SubQuest", "— {0}"), Quest->Title));
		}
		for (const FVillageObjectiveDef& O : Quest->Objectives)
		{
			const int32 Count = Progress.FindRef(ProgressKey(QuestId, O.Id));
			const FText Mark = FText::FromString(Count >= O.Target ? TEXT("✓") : TEXT("•"));
			Lines.Add(O.Target > 1
				? FText::Format(LOCTEXT("ObjCount", "{0} {1} ({2}/{3})"), Mark, O.Text, FText::AsNumber(Count), FText::AsNumber(O.Target))
				: FText::Format(LOCTEXT("ObjLine", "{0} {1}"), Mark, O.Text));
		}
	}
	if (Title.IsEmpty())
	{
		Title = HasFlag(TEXT("Free.Days")) ? FText::Format(LOCTEXT("FreeDays", "Ngày chuẩn bị – còn {0} lượt hoạt động"), FText::AsNumber(Slots))
			: HasFlag(TEXT("Day5.Complete")) ? LOCTEXT("Chapter5Done", "Chương 5 hoàn thành")
			: HasFlag(TEXT("Day4.Complete")) ? LOCTEXT("Chapter4Done", "Chương 4 hoàn thành")
			: HasFlag(TEXT("Day3.Complete")) ? LOCTEXT("Chapter3Done", "Chương 3 hoàn thành")
			: HasFlag(TEXT("Day3.Started")) ? LOCTEXT("Day3Free", "Ngày 3 – Tự do khám phá làng")
			: HasFlag(TEXT("Day2.Complete")) ? LOCTEXT("Chapter2Done", "Chương 2 hoàn thành")
			: HasFlag(TEXT("Day2.Started")) ? LOCTEXT("Day2Free", "Ngày 2 – Tự do khám phá làng")
			: HasFlag(TEXT("Day1.Complete")) ? LOCTEXT("ChapterDone", "Chương 1 hoàn thành") : FText::GetEmpty();
	}
	Hud->SetTracker(Title, Lines);
}

void UVillageStorySubsystem::Notify(const FText& Message)
{
	if (Hud)
	{
		Hud->ShowToast(Message);
	}
}

// ---------------------------------------------------------------- Inventory bridge

UActorComponent* UVillageStorySubsystem::FindInventory() const
{
	APawn* Pawn = GetPC() ? GetPC()->GetPawn() : nullptr;
	if (!Pawn)
	{
		return nullptr;
	}
	for (UActorComponent* Component : Pawn->GetComponents())
	{
		if (Component && Component->GetClass()->GetName().StartsWith(TEXT("BPC_Inventory")))
		{
			return Component;
		}
	}
	return nullptr;
}

namespace
{
	/** Calls BPC_Inventory's AddItem / RemoveItem / GetItemQuantity through ProcessEvent. */
	int32 CallInventory(UActorComponent* Inventory, const TCHAR* FunctionName, FName ItemId, int32 Quantity)
	{
		UFunction* Function = Inventory ? Inventory->FindFunction(FunctionName) : nullptr;
		if (!Function)
		{
			return 0;
		}
		uint8* Params = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
		FMemory::Memzero(Params, Function->ParmsSize);
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Params);
		}
		if (FNameProperty* Item = FindFProperty<FNameProperty>(Function, TEXT("ItemID")))
		{
			Item->SetPropertyValue_InContainer(Params, ItemId);
		}
		if (FIntProperty* InQuantity = CastField<FIntProperty>(Function->FindPropertyByName(TEXT("Quantity"))); InQuantity && !InQuantity->HasAnyPropertyFlags(CPF_OutParm))
		{
			InQuantity->SetPropertyValue_InContainer(Params, Quantity);
		}
		Inventory->ProcessEvent(Function, Params);
		int32 Result = 0;
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (It->HasAnyPropertyFlags(CPF_OutParm))
			{
				if (const FBoolProperty* Bool = CastField<FBoolProperty>(*It)) Result = Bool->GetPropertyValue_InContainer(Params) ? 1 : 0;
				if (const FIntProperty* Int = CastField<FIntProperty>(*It)) Result = Int->GetPropertyValue_InContainer(Params);
			}
			It->DestroyValue_InContainer(Params);
		}
		return Result;
	}
}

int32 UVillageStorySubsystem::GetItemCount(FName ItemId) const
{
	return CallInventory(FindInventory(), TEXT("GetItemQuantity"), ItemId, 0);
}

bool UVillageStorySubsystem::GiveItem(FName ItemId, int32 Quantity)
{
	const bool bOk = CallInventory(FindInventory(), TEXT("AddItem"), ItemId, Quantity) != 0;
	if (!bOk)
	{
		return false;
	}
	// Every item received counts, even after some were used up (e.g. leaves fed to the silkworms).
	for (const FName& QuestId : TArray<FName>(ActiveQuests))
	{
		const FVillageQuestRow* Quest = QuestTable ? QuestTable->FindRow<FVillageQuestRow>(QuestId, TEXT("GiveItem")) : nullptr;
		if (!Quest)
		{
			continue;
		}
		for (const FVillageObjectiveDef& O : Quest->Objectives)
		{
			if (O.TrackItem == ItemId)
			{
				AddProgress(QuestId, O.Id, Quantity);
			}
		}
	}
	return true;
}

bool UVillageStorySubsystem::TakeItem(FName ItemId, int32 Quantity)
{
	return CallInventory(FindInventory(), TEXT("RemoveItem"), ItemId, Quantity) != 0;
}

namespace
{
void ClearInventory(UActorComponent* Inventory)
{
	// The story starts over on each load, so the bag does too: empty every array of item structs and save that.
	if (!Inventory)
	{
		return;
	}
	for (TFieldIterator<FArrayProperty> It(Inventory->GetClass()); It; ++It)
	{
		const FStructProperty* Inner = CastField<FStructProperty>(It->Inner);
		if (!Inner)
		{
			continue;
		}
		// User struct members carry a GUID suffix, so match by prefix.
		bool bHasItem = false;
		for (TFieldIterator<FProperty> Field(Inner->Struct); Field; ++Field)
		{
			bHasItem |= Field->GetName().StartsWith(TEXT("ItemID"));
		}
		if (bHasItem)
		{
			FScriptArrayHelper(*It, It->ContainerPtrToValuePtr<void>(Inventory)).EmptyValues();
		}
	}
	if (UFunction* Save = Inventory->FindFunction(TEXT("SaveInventory")); Save && Save->ParmsSize == 0)
	{
		Inventory->ProcessEvent(Save, nullptr);
	}
}
}

// ---------------------------------------------------------------- Mulberry

bool UVillageStorySubsystem::DrawFromSource(AActor* Source, int32 Stock, double RegrowSeconds, int32& OutWaitSeconds)
{
	const double Now = GetWorld()->GetTimeSeconds();
	int32& Left = BushLeaves.FindOrAdd(Source, Stock);
	if (Left <= 0)
	{
		const double ReadyAt = BushRegrowAt.FindRef(Source);
		if (Now < ReadyAt)
		{
			OutWaitSeconds = FMath::CeilToInt(ReadyAt - Now);
			return false;
		}
		Left = Stock;
	}
	if (--Left == 0)
	{
		BushRegrowAt.Add(Source, Now + RegrowSeconds);
	}
	return true;
}

void UVillageStorySubsystem::HarvestMulberry(AActor* Bush)
{
	int32 Wait = 0;
	if (!DrawFromSource(Bush, LeavesPerBush, BushRegrowSeconds, Wait))
	{
		Notify(FText::Format(LOCTEXT("Regrow", "Cây dâu này đã hết lá – mọc lại sau {0} giây"), FText::AsNumber(Wait)));
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Fail.SFX_Fail"));
		return;
	}
	GiveItem(LeafItem, 1);
	PlaySfx(TEXT("/Game/Audio/SFX/Gameplay/SFX_CollectItem.SFX_CollectItem"));
	Notify(FText::Format(LOCTEXT("Leaf", "+1 Lá dâu  (đang có {0})"), FText::AsNumber(GetItemCount(LeafItem))));
}

// ---------------------------------------------------------------- Silkworm tray

void UVillageStorySubsystem::OpenSilkworm()
{
	if (!HasFlag(TEXT("Quest.Q02.Active")) && !HasFlag(TEXT("Quest.Q02.Done")))
	{
		Notify(LOCTEXT("TrayLocked", "Hãy nói chuyện với nghệ nhân nuôi tằm trước."));
		return;
	}
	if (Silkworm.bFinished)
	{
		Notify(LOCTEXT("TrayDone", "Lứa tằm này đã kết kén xong."));
		return;
	}
	SilkwormWidget = MakeWidget<UVillageSilkwormWidget>();
	SilkwormWidget->AddToViewport(20);
	EnterUI(SilkwormWidget);
	PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Open.SFX_Open"));
	RefreshSilkworm(LOCTEXT("TrayHello", "Quan sát đàn tằm rồi chọn việc cần làm. Mỗi việc là một khoảng thời gian trôi qua."));
}

TArray<FText> UVillageStorySubsystem::SilkwormWarnings() const
{
	TArray<FText> Out;
	if (Silkworm.Hunger > 70.f) Out.Add(LOCTEXT("WHungry", "Tằm đang đói! Cho thêm lá dâu."));
	if (Silkworm.Temperature > 30.f) Out.Add(LOCTEXT("WHot", "Quá nóng! Mở cửa cho thông gió."));
	if (Silkworm.Temperature < 24.f) Out.Add(LOCTEXT("WCold", "Quá lạnh! Đóng cửa giữ ấm."));
	if (Silkworm.Humidity < 55.f) Out.Add(LOCTEXT("WDry", "Khay quá khô! Phun sương."));
	if (Silkworm.Humidity > 75.f) Out.Add(LOCTEXT("WWet", "Quá ẩm! Mở cửa cho thông gió."));
	if (Silkworm.Cleanliness < 40.f) Out.Add(LOCTEXT("WDirty", "Khay bẩn, nhiều lá cũ. Dọn khay."));
	if (Silkworm.Health < 40.f) Out.Add(LOCTEXT("WWeak", "Tằm ít hoạt động – cần chăm sóc ngay."));
	return Out;
}

void UVillageStorySubsystem::StepSilkworm()
{
	// Time passes: they get hungrier, the tray dirtier, the day warmer and drier.
	FVillageSilkwormState& S = Silkworm;
	S.Steps++;
	S.Hunger = FMath::Clamp(S.Hunger + 12.f, 0.f, 100.f);
	S.Cleanliness = FMath::Clamp(S.Cleanliness - 9.f, 0.f, 100.f);
	S.Temperature += FMath::FRandRange(-0.5f, 2.0f);
	S.Humidity += FMath::FRandRange(-6.f, 1.f);

	float HealthDelta = 2.f;
	if (S.Hunger > 70.f) HealthDelta -= 8.f;
	if (S.Temperature > 30.f || S.Temperature < 24.f) HealthDelta -= 5.f;
	if (S.Humidity < 55.f || S.Humidity > 75.f) HealthDelta -= 4.f;
	if (S.Cleanliness < 40.f) HealthDelta -= 6.f;
	S.Health = FMath::Clamp(S.Health + HealthDelta, 5.f, 100.f);
}

void UVillageStorySubsystem::SilkwormAction(int32 Action)
{
	FVillageSilkwormState& S = Silkworm;
	FText Result;
	switch (Action)
	{
	case 0:
		if (!TakeItem(LeafItem, LeavesPerFeed))
		{
			PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Fail.SFX_Fail"));
			RefreshSilkworm(LOCTEXT("NoLeaves", "Không đủ lá dâu. Ra vườn dâu hái thêm (cần 3 lá)."));
			return;
		}
		{
			const bool bGoodCare = SilkwormWarnings().Num() == 0;
			const float Gain = 15.f * FMath::Clamp(S.Health / 80.f, 0.3f, 1.2f) + (bGoodCare ? 5.f : 0.f);
			S.Hunger = FMath::Clamp(S.Hunger - 35.f, 0.f, 100.f);
			S.Growth = FMath::Clamp(S.Growth + Gain, 0.f, 100.f);
			S.Health = FMath::Clamp(S.Health + 4.f, 0.f, 100.f);
			S.Cleanliness = FMath::Clamp(S.Cleanliness - 6.f, 0.f, 100.f);
			PlaySfx(TEXT("/Game/Audio/SFX/Gameplay/SFX_FeedSilkworm.SFX_FeedSilkworm"));
			Result = FText::Format(LOCTEXT("Fed", "Tằm bắt đầu ăn lá dâu. Phát triển +{0}%{1}"), FText::AsNumber(FMath::RoundToInt(Gain)),
				bGoodCare ? LOCTEXT("Bonus", " (môi trường tốt, tằm lớn nhanh hơn)") : FText::GetEmpty());
		}
		break;
	case 1:
		S.Cleanliness = 100.f;
		S.Health = FMath::Clamp(S.Health + 3.f, 0.f, 100.f);
		Result = LOCTEXT("Cleaned", "Bỏ lá cũ, dọn khay, rải lá mới. Khay sạch sẽ.");
		break;
	case 2:
		S.Temperature -= 4.f;
		S.Humidity -= 6.f;
		Result = LOCTEXT("Vented", "Mở cửa: gió mát thổi vào, nhiệt độ và độ ẩm giảm.");
		break;
	case 3:
		S.Humidity += 12.f;
		Result = LOCTEXT("Misted", "Phun sương nhẹ quanh khay, độ ẩm tăng.");
		break;
	case 4:
		S.Temperature += 3.f;
		S.Humidity += 3.f;
		Result = LOCTEXT("Warmed", "Đóng cửa: trong nhà ấm hơn.");
		break;
	default:
		return;
	}
	StepSilkworm();

	if (S.Growth >= 100.f && !S.bFinished)
	{
		S.bFinished = true;
		const int32 Cocoons = 3 + FMath::RoundToInt(S.Health / 25.f);
		GiveItem(CocoonItem, Cocoons);
		SetFlag(TEXT("Knowledge.SilkwormRaising"));
		AddProgress(TEXT("Q02"), TEXT("RaiseSilkworms"));
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_StageComplete.SFX_StageComplete"));
		CloseSilkworm();
		Notify(FText::Format(LOCTEXT("Cocoons", "Tằm đã nhả tơ kết kén! Nhận {0} Kén tằm · Mở khóa kiến thức: Nuôi tằm"), FText::AsNumber(Cocoons)));
		return;
	}
	RefreshSilkworm(Result);
}

void UVillageStorySubsystem::RefreshSilkworm(const FText& Result)
{
	if (SilkwormWidget)
	{
		SilkwormWidget->Refresh(Silkworm, GetItemCount(LeafItem), SilkwormWarnings(), Result);
	}
}

void UVillageStorySubsystem::CloseSilkworm()
{
	if (SilkwormWidget)
	{
		SilkwormWidget->RemoveFromParent();
		SilkwormWidget = nullptr;
		ExitUI();
	}
}

// ---------------------------------------------------------------- End of day

void UVillageStorySubsystem::EndDay(int32 Day)
{
	EndingDay = FMath::Clamp(Day, 1, 3);
	EndingRow = NAME_None;
	StartSunset();
}

void UVillageStorySubsystem::EndDayRow(FName Row)
{
	EndingDay = 0;
	EndingRow = Row;
	StartSunset();
}

void UVillageStorySubsystem::StartSunset()
{
	UWorld* World = GetWorld();
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		Sun = *It;
		SunStart = It->GetActorRotation();
		SunColorStart = It->GetLightComponent()->GetLightColor();
		break;
	}
	if (APlayerController* PC = GetPC())
	{
		PC->SetIgnoreMoveInput(true);
	}
	PlaySfx(TEXT("/Game/Audio/Ambience/AMB_EveningDog.AMB_EveningDog"));
	SunsetAlpha = 0.f;
	World->GetTimerManager().SetTimer(SunsetTimer, FTimerDelegate::CreateUObject(this, &UVillageStorySubsystem::TickSunset), 0.033f, true);
}

void UVillageStorySubsystem::TickSunset()
{
	SunsetAlpha = FMath::Min(1.f, SunsetAlpha + 0.033f / 5.f);
	if (ADirectionalLight* Light = Cast<ADirectionalLight>(Sun.Get()))
	{
		const float Ease = FMath::InterpEaseInOut(0.f, 1.f, SunsetAlpha, 2.f);
		FRotator Rot = SunStart;
		Rot.Pitch = FMath::Lerp(SunStart.Pitch, -3.f, Ease);
		Light->SetActorRotation(Rot);
		Light->GetLightComponent()->SetLightColor(FMath::Lerp(SunColorStart, FLinearColor(1.f, 0.55f, 0.32f), Ease));
	}
	if (SunsetAlpha >= 0.6f && !DayEndWidget)
	{
		if (APlayerController* PC = GetPC())
		{
			PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, 2.f, FLinearColor::Black, false, true);
		}
		DayEndWidget = MakeWidget<UVillageDayEndWidget>();
		DayEndWidget->AddToViewport(30);
		// Same card every evening: what was done, the new goal, what opened up, what was learned.
		auto Mark = [this](const TCHAR* QuestFlag, const FText& Name)
		{
			return FText::Format(LOCTEXT("DayMark", "{0} {1}"), FText::FromString(HasFlag(QuestFlag) ? TEXT("✓") : TEXT("✗")), Name);
		};
		if (!EndingRow.IsNone())
		{
			ShowRowCard();
		}
		else switch (EndingDay)
		{
		case 2:
			DayEndWidget->Setup(LOCTEXT("Day2", "NGÀY 2 – HOÀN THÀNH"), {
				FText::Format(LOCTEXT("D2Progress", "Tiến độ: {0} · {1} · {2}"), Mark(TEXT("Quest.Q10.Done"), LOCTEXT("D2Cocoon", "Thu kén 50/50")),
					Mark(TEXT("Quest.Q11.Done"), LOCTEXT("D2Festival", "Trang trí lễ hội")),
					Mark(TEXT("Quest.Q12.Done"), HasFlag(TEXT("Quest.Q12.Done")) ? LOCTEXT("D2Reel", "Sợi tơ đầu tiên") : LOCTEXT("D2ReelLeft", "Sợi tơ đầu tiên (làm tiếp ngày mai)"))),
				HasFlag(TEXT("Quest.Q12.Done")) ? LOCTEXT("D2Goal", "Mục tiêu mới: Ươm tơ thành mẻ lớn và se thành sợi")
					: LOCTEXT("D2GoalLeft", "Mục tiêu mới: Hoàn thành sợi tơ đầu tiên (việc còn dở hôm nay)"),
				LOCTEXT("D2Areas", "Khu vực mới: Khu lễ hội đã hoàn thiện"),
				LOCTEXT("D2Knowledge", "Kiến thức mới: Thu hoạch kén · Văn hóa lễ hội"),
			});
			SetFlag(TEXT("Day2.Complete"));
			SetFlag(TEXT("Knowledge.FestivalCulture"));
			break;
		case 3:
			DayEndWidget->Setup(LOCTEXT("Day3", "NGÀY 3 – HOÀN THÀNH"), {
				FText::Format(LOCTEXT("D3Progress", "Tiến độ: {0} · {1} · {2} · {3}"), Mark(TEXT("Quest.Q02.Done"), LOCTEXT("D3Care", "Chăm tằm")),
					Mark(TEXT("Quest.Q10.Done"), LOCTEXT("D3Cocoon", "Chuẩn bị kén")), Mark(TEXT("Quest.Q15.Done"), LOCTEXT("D3Reel", "Ươm tơ")),
					Mark(TEXT("Quest.Q16.Done"), LOCTEXT("D3Spin", "Se tơ"))),
				FText::Format(LOCTEXT("D3Materials", "Nguyên liệu: Kén ×{0} · Tơ ×{1} · Sợi tơ ×{2}"), FText::AsNumber(GetItemCount(CocoonItem)),
					FText::AsNumber(GetItemCount(RawSilkItem)), FText::AsNumber(GetItemCount(ThreadItem))),
				LOCTEXT("D3Knowledge", "Kiến thức mới: Quy trình làm tơ lụa"),
				LOCTEXT("D3Areas", "Khu vực mới: Xưởng dệt"),
			});
			SetFlag(TEXT("Day3.Complete"));
			SetFlag(TEXT("Unlock.WeavingWorkshop"));
			SetFlag(TEXT("Knowledge.SilkMaking"));
			break;
		default:
			DayEndWidget->Setup(LOCTEXT("Day1", "NGÀY 1 – HOÀN THÀNH"), {
				LOCTEXT("NextGoal", "Mục tiêu mới: Học cách làm ra sợi tơ"),
				LOCTEXT("NewAreas", "Khu vực mới: Xưởng ươm tơ · Xưởng se tơ"),
				LOCTEXT("NewKnowledge", "Kiến thức mới: Quy trình làm tơ lụa"),
			});
			SetFlag(TEXT("Day1.Complete"));
			SetFlag(TEXT("Unlock.ReelingWorkshop"));
			SetFlag(TEXT("Unlock.SpinningWorkshop"));
			SetFlag(TEXT("Knowledge.SilkProcess"));
			break;
		}
		EnterUI(DayEndWidget);
	}
	if (SunsetAlpha >= 1.f)
	{
		GetWorld()->GetTimerManager().ClearTimer(SunsetTimer);
	}
}

void UVillageStorySubsystem::CloseDayEnd()
{
	if (!DayEndWidget)
	{
		return;
	}
	DayEndWidget->RemoveFromParent();
	DayEndWidget = nullptr;
	GetWorld()->GetTimerManager().ClearTimer(SunsetTimer);
	// Next morning: restore the sun and fade back in. Chapter 2 picks up from here.
	if (ADirectionalLight* Light = Cast<ADirectionalLight>(Sun.Get()))
	{
		Light->SetActorRotation(SunStart);
		Light->GetLightComponent()->SetLightColor(SunColorStart);
	}
	if (APlayerController* PC = GetPC())
	{
		PC->SetIgnoreMoveInput(false);
		PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 2.f, FLinearColor::Black, false, false);
	}
	ExitUI();
	// Each morning opens with a nudge toward the village chief, who hands out the day's quests.
	if (const FVillageDayRow* Row = (!EndingRow.IsNone() && DayTable) ? DayTable->FindRow<FVillageDayRow>(EndingRow, TEXT("CloseDay")) : nullptr)
	{
		const FVillageDayRow Copy = *Row; // OnClose may start another day card
		EndingRow = NAME_None;
		RunActions(Copy.OnClose);
		RefreshTracker();
		if (!Copy.Morning.IsEmpty())
		{
			Notify(ExpandText(Copy.Morning));
		}
		return;
	}
	switch (EndingDay)
	{
	case 2:
		StartQuest(TEXT("Q14"));
		RefreshTracker();
		Notify(HasFlag(TEXT("Quest.Q12.Done")) ? LOCTEXT("Morning3", "NGÀY 3 – Một sợi tơ mới bắt đầu")
			: LOCTEXT("Morning3Left", "NGÀY 3 – Một sợi tơ mới bắt đầu. Bạn còn việc dở từ hôm qua: Sợi tơ đầu tiên"));
		break;
	case 3:
		StartQuest(TEXT("Q18")); // Chapter 4 (no-op until its story tables are built)
		RefreshTracker();
		Notify(LOCTEXT("Morning4", "NGÀY 4 – Sắc màu của làng (Chương 4)"));
		break;
	default:
		StartQuest(TEXT("Q09"));
		RefreshTracker();
		Notify(LOCTEXT("Morning", "NGÀY 2 – Cả làng đang chuẩn bị cho Lễ hội Tơ Lụa"));
		break;
	}
}

// ---------------------------------------------------------------- Chapter 2

namespace
{
	const FLinearColor ResultPerfect(0.55f, 0.9f, 0.55f);
	const FLinearColor ResultGood(0.96f, 0.76f, 0.36f);
	const FLinearColor ResultBad(1.0f, 0.55f, 0.40f);
	const FLinearColor ResultNeutral(0.75f, 0.70f, 0.62f);

	constexpr int32 DecoPerSource = 3;
	constexpr double DecoRegrowSeconds = 30.0;
	constexpr int32 GateStrips = 5;
	constexpr int32 CocoonsPerReel = 5;
	// Chapter 3: one batch of cocoons reels into 3-4 silk; four silk are spun into three threads in four rounds.
	constexpr int32 SilkPerSpin = 4;
	constexpr int32 SpinRounds = 4;

	/** Festival materials: item, its Q11 objective, the name in toasts, and what decorating uses up. */
	struct FDecoItem
	{
		const TCHAR* Item;
		const TCHAR* Objective;
		const TCHAR* Name;
		int32 Used;
	};
	const FDecoItem DecoItems[] = {
		{ TEXT("DEC_BAMBOO_001"), TEXT("Bamboo"), TEXT("Tre"), 5 },
		{ TEXT("DEC_COLOR_CLOTH_001"), TEXT("Cloth"), TEXT("Vải màu"), 5 },
		{ TEXT("DEC_FLOWER_001"), TEXT("Flower"), TEXT("Hoa"), 10 },
		{ TEXT("DEC_ROPE_001"), TEXT("Rope"), TEXT("Dây thừng"), 5 },
		{ TEXT("DEC_WOOD_ORNAMENT_001"), TEXT("WoodDeco"), TEXT("Đồ gỗ trang trí"), 3 },
	};
	/** By item ID, or by objective ID (what source actors carry as ItemID, so they keep their FallbackPrompt). */
	const FDecoItem* FindDeco(FName Key)
	{
		for (const FDecoItem& Deco : DecoItems)
		{
			if (Key == Deco.Item || Key == Deco.Objective) return &Deco;
		}
		return nullptr;
	}

	const TCHAR* const HangNames[] = { TEXT("Đèn lồng"), TEXT("Dải vải"), TEXT("Hoa"), TEXT("Dây trang trí") };
	constexpr int32 HangCount = UE_ARRAY_COUNT(HangNames);

	FVillageTimingRound Centered(float Centre, float GoodHalf, float PerfectHalf, float Speed)
	{
		FVillageTimingRound R;
		R.GoodMin = Centre - GoodHalf;
		R.GoodMax = Centre + GoodHalf;
		R.PerfectMin = Centre - PerfectHalf;
		R.PerfectMax = Centre + PerfectHalf;
		R.Speed = Speed;
		return R;
	}

	const FName Q02(TEXT("Q02"));
	const FName Q10(TEXT("Q10"));
	const FName Q11(TEXT("Q11"));
	const FName Q12(TEXT("Q12"));
	const FName Q15(TEXT("Q15"));
	const FName Q16(TEXT("Q16"));
}

bool UVillageStorySubsystem::IsObjectiveDone(FName QuestId, FName ObjectiveId) const
{
	return HasFlag(FName(*FString::Printf(TEXT("Obj.%s.%s"), *QuestId.ToString(), *ObjectiveId.ToString())));
}

int32 UVillageStorySubsystem::ObjectiveTarget(FName QuestId, FName ObjectiveId) const
{
	const FVillageQuestRow* Quest = QuestTable ? QuestTable->FindRow<FVillageQuestRow>(QuestId, TEXT("Target")) : nullptr;
	const FVillageObjectiveDef* Def = Quest ? Quest->Objectives.FindByPredicate([&](const FVillageObjectiveDef& O) { return O.Id == ObjectiveId; }) : nullptr;
	return Def ? Def->Target : 1;
}

void UVillageStorySubsystem::ShowTagged(FName Tag, bool bShow)
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(Tag))
		{
			It->SetActorHiddenInGame(!bShow);
			It->SetActorEnableCollision(bShow);
		}
	}
}

float UVillageStorySubsystem::CocoonScore() const
{
	// 0 = all poor, 1 = all excellent. Day 1 cocoons were never graded, so they are left out.
	const TPair<const TCHAR*, float> Weights[] = { { TEXT("Poor"), 0.f }, { TEXT("Normal"), 1.f / 3.f }, { TEXT("Good"), 2.f / 3.f }, { TEXT("Excellent"), 1.f } };
	float Sum = 0.f;
	int32 Count = 0;
	for (const TPair<const TCHAR*, float>& W : Weights)
	{
		const int32 N = CocoonQuality.FindRef(W.Key);
		Sum += N * W.Value;
		Count += N;
	}
	return Count > 0 ? Sum / Count : 1.f / 3.f;
}

void UVillageStorySubsystem::HarvestDecoration(AActor* Source)
{
	FName Key;
	ReadProperty<FNameProperty>(Source, TEXT("ItemID"), Key);
	const FDecoItem* Deco = FindDeco(Key);
	if (!Deco)
	{
		UE_LOG(LogVillageStory, Warning, TEXT("%s is tagged Interact_DecoMaterial but ItemID '%s' is not a festival material"), *Source->GetName(), *Key.ToString());
		return;
	}
	const FName Item(Deco->Item);
	const FText Name = FText::FromString(Deco->Name);
	if (!HasFlag(TEXT("Quest.Q11.Active")))
	{
		Notify(LOCTEXT("DecoNotYet", "Bây giờ chưa cần thứ này."));
		return;
	}
	if (IsObjectiveDone(Q11, Deco->Objective))
	{
		Notify(FText::Format(LOCTEXT("DecoEnough", "Đã đủ {0} rồi."), Name));
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Fail.SFX_Fail"));
		return;
	}
	int32 Wait = 0;
	if (!DrawFromSource(Source, DecoPerSource, DecoRegrowSeconds, Wait))
	{
		Notify(FText::Format(LOCTEXT("DecoEmpty", "Chỗ này đã lấy hết {0} – quay lại sau {1} giây, hoặc tìm chỗ khác"), Name, FText::AsNumber(Wait)));
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Fail.SFX_Fail"));
		return;
	}
	GiveItem(Item, 1); // counts toward the Q11 objective through its TrackItem
	PlaySfx(TEXT("/Game/Audio/SFX/Gameplay/SFX_CollectItem.SFX_CollectItem"));
	const bool bAll = Algo::AllOf(DecoItems, [&](const FDecoItem& D) { return IsObjectiveDone(Q11, D.Objective); });
	Notify(bAll ? LOCTEXT("DecoAll", "Đủ nguyên liệu trang trí! Mang đến cổng lễ hội.")
		: FText::Format(LOCTEXT("DecoGot", "+1 {0}  ({1}/{2})"), Name,
			FText::AsNumber(Progress.FindRef(ProgressKey(Q11, Deco->Objective))), FText::AsNumber(ObjectiveTarget(Q11, Deco->Objective))));
}

void UVillageStorySubsystem::OpenTiming(EVillageMiniGame Game, const FText& Title, const FText& Hint)
{
	MiniGame = Game;
	RoundIndex = 0;
	RoundScore = 0;
	TimingWidget = MakeWidget<UVillageTimingWidget>();
	TimingWidget->AddToViewport(20);
	TimingWidget->Setup(Title, Hint);
	EnterUI(TimingWidget);
	PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Open.SFX_Open"));
}

void UVillageStorySubsystem::NextTimingRound(const FText& Result, const FLinearColor& ResultColor)
{
	switch (MiniGame)
	{
	case EVillageMiniGame::Cocoon:
	{
		// Each batch ripens left to right once: early on the left, too late past the band.
		const float Start = FMath::FRandRange(0.42f, 0.58f);
		Round = Centered(Start + 0.12f, 0.12f, 0.035f, FMath::FRandRange(0.38f, 0.5f));
		Round.bPingPong = false;
		Round.Label = FText::Format(LOCTEXT("CocoonRound", "Nong kén chín dần…   Đã thu: {0}/{1} kén"),
			FText::AsNumber(Progress.FindRef(ProgressKey(Q10, TEXT("CollectCocoons")))), FText::AsNumber(ObjectiveTarget(Q10, TEXT("CollectCocoons"))));
		break;
	}
	case EVillageMiniGame::Gate:
	{
		const int32 Slot = FMath::RandRange(0, 4);
		FString Dots;
		for (int32 i = 0; i < 5; ++i)
		{
			Dots += i == Slot ? TEXT("●  ") : TEXT("○  ");
		}
		Round = Centered(0.1f + 0.2f * Slot, 0.08f, 0.03f, 0.75f);
		Round.Label = FText::Format(LOCTEXT("GateRound", "Dải vải {0}/{1}      {2}"), FText::AsNumber(RoundIndex + 1), FText::AsNumber(GateStrips), FText::FromString(Dots.TrimEnd()));
		break;
	}
	case EVillageMiniGame::Hang:
		Round = Centered(FMath::FRandRange(0.3f, 0.7f), 0.12f, 0.04f, 1.0f);
		Round.Label = FText::Format(LOCTEXT("HangRound", "Treo {0}  ({1}/{2})"), FText::FromString(HangNames[RoundIndex]),
			FText::AsNumber(RoundIndex + 1), FText::AsNumber(HangCount));
		break;
	case EVillageMiniGame::FindThread:
		Round = Centered(FMath::FRandRange(0.35f, 0.65f), 0.09f, 0.03f, 0.8f);
		Round.Label = LOCTEXT("ThreadRound", "Tìm đầu sợi tơ trong nồi nước nóng");
		break;
	case EVillageMiniGame::Craft:
	{
		const FVillageCraftRow* Row = CraftTable ? CraftTable->FindRow<FVillageCraftRow>(ActiveCraft, TEXT("CraftRound")) : nullptr;
		if (!Row || !Row->Rounds.IsValidIndex(RoundIndex))
		{
			return;
		}
		const FVillageCraftRound& R = Row->Rounds[RoundIndex];
		const float Bonus = CraftBonus(Row->Skill);
		Round = Centered(R.Centre < 0.f ? FMath::FRandRange(0.35f, 0.65f) : R.Centre, FMath::Min(R.GoodHalf + Bonus, 0.3f),
			FMath::Min(R.PerfectHalf + Bonus * 0.35f, 0.12f), R.Speed);
		Round.Label = FText::Format(LOCTEXT("CraftRound", "{0}  ({1}/{2})"), R.Label, FText::AsNumber(RoundIndex + 1), FText::AsNumber(Row->Rounds.Num()));
		break;
	}
	case EVillageMiniGame::Spin:
		Round = Centered(FMath::FRandRange(0.35f, 0.65f), 0.14f, 0.05f, 0.9f);
		Round.Label = FText::Format(LOCTEXT("SpinRound", "Tốc độ se tơ  ({0}/{1})      Chậm ←  ĐÚNG  → Nhanh"), FText::AsNumber(RoundIndex + 1), FText::AsNumber(SpinRounds));
		break;
	default:
		return;
	}
	TimingWidget->StartRound(Round, Result, ResultColor);
}

void UVillageStorySubsystem::OnTimingPress(float Position)
{
	if (!TimingWidget)
	{
		return;
	}
	switch (MiniGame)
	{
	case EVillageMiniGame::Cocoon: PressCocoon(Position); break;
	case EVillageMiniGame::Gate: PressGate(Position); break;
	case EVillageMiniGame::Hang: PressHang(Position); break;
	case EVillageMiniGame::FindThread: PressFindThread(Position); break;
	case EVillageMiniGame::Spin: PressSpin(Position); break;
	case EVillageMiniGame::Craft: PressCraft(Position); break;
	default: break;
	}
}

void UVillageStorySubsystem::CloseTiming()
{
	if (TimingWidget)
	{
		TimingWidget->RemoveFromParent();
		TimingWidget = nullptr;
		MiniGame = EVillageMiniGame::None;
		ExitUI();
	}
}

// Cocoon harvest (Quest 02): the moment of picking decides the quality.

void UVillageStorySubsystem::OpenCocoon()
{
	if (!HasFlag(TEXT("Quest.Q10.Active")))
	{
		Notify(HasFlag(TEXT("Quest.Q10.Done")) ? LOCTEXT("CocoonDone", "Đã đủ 50 kén – sang hỏi ông Khánh chuyện ươm tơ.")
			: LOCTEXT("CocoonNotYet", "Hãy gặp bà Xuân ở sân lễ hội trước."));
		return;
	}
	OpenTiming(EVillageMiniGame::Cocoon, LOCTEXT("CocoonTitle", "Thu hoạch kén tằm"),
		LOCTEXT("CocoonHint", "Kén chín dần từ trái sang phải. Nhấn [E] khi kim trong vùng vàng – giữa vùng xanh là đúng lúc nhất.  ·  [Esc] rời đi"));
	NextTimingRound(LOCTEXT("CocoonHello", "Hái sớm quá kén chưa chắc, để muộn quá kén xấu đi."), ResultNeutral);
}

void UVillageStorySubsystem::PressCocoon(float Position)
{
	FName Quality;
	int32 Count;
	FText Verdict;
	FLinearColor Color;
	if (Position < Round.GoodMin)
	{
		Quality = TEXT("Poor"); Count = 2; Color = ResultBad;
		Verdict = LOCTEXT("TooEarly", "Quá sớm – kén chưa đủ chắc.  +2 kén Kém");
	}
	else if (Position > Round.GoodMax)
	{
		Quality = TEXT("Normal"); Count = 2; Color = ResultBad;
		Verdict = LOCTEXT("TooLate", "Quá muộn – kén đã giảm chất lượng.  +2 kén Thường");
	}
	else if (Round.IsPerfect(Position))
	{
		Quality = TEXT("Excellent"); Count = 4; Color = ResultPerfect;
		Verdict = LOCTEXT("PerfectTime", "Đúng lúc!  +4 kén Xuất sắc");
	}
	else
	{
		Quality = TEXT("Good"); Count = 3; Color = ResultGood;
		Verdict = LOCTEXT("GoodTime", "Tốt.  +3 kén Tốt");
	}
	CocoonQuality.FindOrAdd(Quality) += Count;
	GiveItem(CocoonItem, Count); // progresses Q10 through its TrackItem
	PlaySfx(TEXT("/Game/Audio/SFX/Gameplay/SFX_CollectItem.SFX_CollectItem"));
	if (!HasFlag(TEXT("Quest.Q10.Active")))
	{
		CloseTiming(); // 50/50: the quest's own completion toasts are showing
		return;
	}
	++RoundIndex;
	NextTimingRound(Verdict, Color);
}

// Festival decoration (Quest 03): materials -> gate -> hanging -> arranging -> the chief's final look.

void UVillageStorySubsystem::OpenGate()
{
	if (!HasFlag(TEXT("Quest.Q11.Active")))
	{
		Notify(HasFlag(TEXT("Quest.Q11.Done")) ? LOCTEXT("GateAllDone", "Khu lễ hội đã trang trí xong.")
			: LOCTEXT("GateNotYet", "Bà Xuân sẽ giao việc trang trí – hãy gặp bà trước."));
		return;
	}
	if (IsObjectiveDone(Q11, TEXT("DecorateGate")))
	{
		Notify(LOCTEXT("GateDone", "Cổng đã xong – giờ treo đèn, vải, hoa ở sân lễ hội."));
		return;
	}
	if (!Algo::AllOf(DecoItems, [&](const FDecoItem& D) { return IsObjectiveDone(Q11, D.Objective); }))
	{
		Notify(LOCTEXT("GateNeedMats", "Cần gom đủ nguyên liệu trước: tre, vải màu, hoa, dây thừng, đồ gỗ trang trí."));
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Fail.SFX_Fail"));
		return;
	}
	OpenTiming(EVillageMiniGame::Gate, LOCTEXT("GateTitle", "Trang trí cổng lễ hội"),
		LOCTEXT("GateHint", "Đặt từng dải vải vào vị trí sáng (●): nhấn [E] khi kim nằm trong vùng vàng.  ·  [Esc] rời đi"));
	NextTimingRound(LOCTEXT("GateHello", "Bà Xuân: “Hãy đặt những dải vải này lên cổng.”"), ResultNeutral);
}

void UVillageStorySubsystem::PressGate(float Position)
{
	FText Verdict;
	FLinearColor Color;
	if (Round.IsGood(Position))
	{
		const bool bPerfect = Round.IsPerfect(Position);
		RoundScore += bPerfect ? 20 : 12;
		++RoundIndex;
		Verdict = bPerfect ? LOCTEXT("GatePerfect", "Đặt hoàn hảo!") : LOCTEXT("GateGood", "Đặt tốt.");
		Color = bPerfect ? ResultPerfect : ResultGood;
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_ItemClick.SFX_ItemClick"));
	}
	else
	{
		// No penalty beyond a retry: the strip comes down and goes up again.
		Verdict = LOCTEXT("GateWrong", "Đặt chưa đúng chỗ – tháo ra đặt lại.");
		Color = ResultBad;
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Fail.SFX_Fail"));
	}
	if (RoundIndex < GateStrips)
	{
		NextTimingRound(Verdict, Color);
		return;
	}
	DecorationScore += RoundScore;
	TakeItem(TEXT("DEC_COLOR_CLOTH_001"), FindDeco(TEXT("DEC_COLOR_CLOTH_001"))->Used);
	CloseTiming();
	ShowTagged(TEXT("FestivalDecor_Gate"), true);
	AddProgress(Q11, TEXT("DecorateGate"));
	PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_StageComplete.SFX_StageComplete"));
	Notify(FText::Format(LOCTEXT("GateFinished", "Cổng lễ hội đã xong!  Điểm trang trí cổng: {0}/100"), FText::AsNumber(RoundScore)));
}

void UVillageStorySubsystem::OpenHang()
{
	if (!IsObjectiveDone(Q11, TEXT("DecorateGate")))
	{
		Notify(LOCTEXT("HangNotYet", "Trang trí cổng lễ hội trước đã."));
		return;
	}
	if (IsObjectiveDone(Q11, TEXT("HangDecorations")))
	{
		Notify(LOCTEXT("HangDone", "Đồ trang trí đã treo xong – giờ sắp xếp khu lễ hội."));
		return;
	}
	OpenTiming(EVillageMiniGame::Hang, LOCTEXT("HangTitle", "Treo đồ trang trí"),
		LOCTEXT("HangHint", "Nhấn [E] đúng lúc kim đi qua vùng xanh. Trượt cũng không sao – chỉ bớt điểm trang trí.  ·  [Esc] rời đi"));
	NextTimingRound(FText::GetEmpty(), ResultNeutral);
}

void UVillageStorySubsystem::PressHang(float Position)
{
	FText Verdict;
	FLinearColor Color;
	if (Round.IsPerfect(Position))
	{
		RoundScore += 25;
		Verdict = LOCTEXT("HangPerfect", "Perfect!");
		Color = ResultPerfect;
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_ItemClick.SFX_ItemClick"));
	}
	else if (Round.IsGood(Position))
	{
		RoundScore += 15;
		Verdict = LOCTEXT("HangGood", "Good!");
		Color = ResultGood;
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_ItemClick.SFX_ItemClick"));
	}
	else
	{
		Verdict = LOCTEXT("HangMiss", "Miss! – treo hơi lệch, điểm trang trí giảm.");
		Color = ResultBad;
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Fail.SFX_Fail"));
	}
	// No game over: every item goes up, a miss only costs points.
	if (++RoundIndex < HangCount)
	{
		NextTimingRound(Verdict, Color);
		return;
	}
	DecorationScore += RoundScore;
	for (const TCHAR* Item : { TEXT("DEC_BAMBOO_001"), TEXT("DEC_FLOWER_001"), TEXT("DEC_ROPE_001") })
	{
		TakeItem(Item, FindDeco(Item)->Used);
	}
	CloseTiming();
	ShowTagged(TEXT("FestivalDecor_Hang"), true);
	AddProgress(Q11, TEXT("HangDecorations"));
	PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_StageComplete.SFX_StageComplete"));
	if (TotalSpots == 0)
	{
		UE_LOG(LogVillageStory, Warning, TEXT("No Interact_FestivalSpot actors; run Scripts/build_chapter2_village.py. Skipping the arranging step."));
		AddProgress(Q11, TEXT("ArrangeArea"));
	}
	Notify(FText::Format(LOCTEXT("HangFinished", "Đã treo xong!  Điểm: {0}/100  ·  Giờ hãy đặt bàn ghế, khung dệt… vào các vị trí được đánh dấu."), FText::AsNumber(RoundScore)));
}

void UVillageStorySubsystem::PlaceFestivalSpot(AActor* Spot)
{
	if (!IsObjectiveDone(Q11, TEXT("HangDecorations")))
	{
		Notify(LOCTEXT("SpotNotYet", "Treo xong đồ trang trí rồi mới sắp xếp khu lễ hội."));
		return;
	}
	FName Key;
	ReadProperty<FNameProperty>(Spot, TEXT("ItemID"), Key);
	if (PlacedSpots.Contains(Key))
	{
		return;
	}
	PlacedSpots.Add(Key);
	ShowTagged(FName(*(TEXT("FestivalProp_") + Key.ToString())), true);
	Spot->SetActorHiddenInGame(true);
	Spot->SetActorEnableCollision(false);
	FText What;
	ReadProperty<FTextProperty>(Spot, TEXT("FallbackPrompt"), What);
	PlaySfx(TEXT("/Game/Audio/SFX/Gameplay/SFX_CollectItem.SFX_CollectItem"));
	Notify(FText::Format(LOCTEXT("SpotPlaced", "✓ {0}  ({1}/{2})"), What, FText::AsNumber(PlacedSpots.Num()), FText::AsNumber(TotalSpots)));
	if (PlacedSpots.Num() >= TotalSpots)
	{
		TakeItem(TEXT("DEC_WOOD_ORNAMENT_001"), FindDeco(TEXT("DEC_WOOD_ORNAMENT_001"))->Used);
		AddProgress(Q11, TEXT("ArrangeArea"));
		Notify(LOCTEXT("SpotsDone", "Khu lễ hội đã sắp xếp xong! Báo cho bà Xuân."));
	}
}

// First silk reeling (Quest 04): find the thread, then reel with steady tension and speed.

void UVillageStorySubsystem::OpenReelingPot()
{
	if (HasFlag(TEXT("Quest.Q15.Active")))
	{
		// Day 3: the whole batch. Pick the cocoons (a dialogue of choices), then find the thread and reel as on Day 2.
		if (IsObjectiveDone(Q15, TEXT("ReelSilk")))
		{
			Notify(LOCTEXT("BatchDone", "Mẻ tơ đã xong – hỏi ông Khánh nhận xét, hoặc chọn ươm lại."));
			return;
		}
		if (GetItemCount(CocoonItem) < CocoonsPerReel)
		{
			Notify(FText::Format(LOCTEXT("BatchNeedCocoons", "Cần ít nhất {0} kén để ươm một mẻ."), FText::AsNumber(CocoonsPerReel)));
			PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Fail.SFX_Fail"));
			return;
		}
		if (!IsObjectiveDone(Q15, TEXT("ChooseCocoons")))
		{
			StartDialogue(TEXT("REEL_SELECT"));
			return;
		}
		OpenTiming(EVillageMiniGame::FindThread, LOCTEXT("BatchThreadTitle", "Ươm tơ – Tìm đầu sợi"),
			LOCTEXT("ThreadHint", "Kén ngâm trong nồi nước nóng. Nhấn [E] khi kim vào vùng vàng để bắt lấy đầu sợi.  ·  [Esc] rời đi"));
		NextTimingRound(FText::GetEmpty(), ResultNeutral);
		return;
	}
	if (HasFlag(TEXT("Quest.Q12.Done")))
	{
		Notify(LOCTEXT("ReelDone", "Mẻ tơ đầu tiên đã xong."));
		return;
	}
	if (!HasFlag(TEXT("Quest.Q12.Active")))
	{
		Notify(HasFlag(TEXT("Unlock.SilkReeling")) ? LOCTEXT("ReelAsk", "Hỏi ông Khánh trước khi bắt đầu ươm tơ.")
			: LOCTEXT("ReelLocked", "Cần đủ 50 kén mới ươm tơ được."));
		return;
	}
	if (IsObjectiveDone(Q12, TEXT("FindThread")))
	{
		OpenReeling();
		return;
	}
	OpenTiming(EVillageMiniGame::FindThread, LOCTEXT("ThreadTitle", "Ươm tơ – Tìm đầu sợi"),
		LOCTEXT("ThreadHint", "Kén ngâm trong nồi nước nóng. Nhấn [E] khi kim vào vùng vàng để bắt lấy đầu sợi.  ·  [Esc] rời đi"));
	NextTimingRound(FText::GetEmpty(), ResultNeutral);
}

void UVillageStorySubsystem::PressFindThread(float Position)
{
	if (!Round.IsGood(Position))
	{
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Fail.SFX_Fail"));
		NextTimingRound(LOCTEXT("ThreadMiss", "Hụt rồi – đầu sợi tuột mất, thử lại."), ResultBad);
		return;
	}
	// A clean catch starts the reel with a little less loose fibre.
	ReelStartQuality = Round.IsPerfect(Position) ? 100.f : 95.f;
	CloseTiming();
	AddProgress(Q12, TEXT("FindThread"));
	OpenReeling();
}

void UVillageStorySubsystem::OpenReeling()
{
	ReelingWidget = MakeWidget<UVillageReelingWidget>();
	ReelingWidget->AddToViewport(20);
	ReelingWidget->Setup(ReelStartQuality);
	EnterUI(ReelingWidget);
	PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Open.SFX_Open"));
}

void UVillageStorySubsystem::OnReelingBreak()
{
	PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Fail.SFX_Fail"));
}

void UVillageStorySubsystem::OnReelingStage(int32 Stage, float Quality)
{
	const bool bBatch = HasFlag(TEXT("Quest.Q15.Active"));
	if (Stage == 1)
	{
		if (!bBatch)
		{
			AddProgress(Q12, TEXT("ControlTension"));
		}
		return;
	}
	// Good cocoons make good silk: the reel's own quality is scaled by the cocoons harvested on Day 2,
	// and on Day 3 by the cocoons picked for the batch.
	float Final = Quality * (0.8f + 0.2f * CocoonScore());
	if (bBatch)
	{
		Final *= HasFlag(TEXT("Pick.Good")) ? 1.f : HasFlag(TEXT("Pick.Normal")) ? 0.85f : 0.65f;
	}
	const TCHAR* Tier = Final >= 85.f ? TEXT("Excellent") : Final >= 65.f ? TEXT("Good") : Final >= 40.f ? TEXT("Normal") : TEXT("Poor");
	const FText TierName = Final >= 85.f ? LOCTEXT("TierExcellent", "Xuất sắc") : Final >= 65.f ? LOCTEXT("TierGood", "Tốt")
		: Final >= 40.f ? LOCTEXT("TierNormal", "Thường") : LOCTEXT("TierPoor", "Kém");
	if (bBatch)
	{
		FinishSilkBatch(Final, TierName);
		return;
	}
	SetFlag(FName(*FString::Printf(TEXT("Quality.RawSilk.%s"), Tier)));
	TakeItem(CocoonItem, CocoonsPerReel);
	AddProgress(Q12, TEXT("MaintainSpeed"));
	AddProgress(Q12, TEXT("FinishSilk")); // completes Q12: raw silk, knowledge flag
	CloseReeling();
	Notify(FText::Format(LOCTEXT("SilkDone", "Sợi tơ đầu tiên đã hoàn thành!  Tơ thô – chất lượng {0} ({1}%)"), TierName, FText::AsNumber(FMath::RoundToInt(Final))));
}

void UVillageStorySubsystem::CloseReeling()
{
	if (!ReelingWidget)
	{
		return;
	}
	ReelingWidget->RemoveFromParent();
	ReelingWidget = nullptr;
	ExitUI();
	if (HasFlag(TEXT("Quest.Q15.Active")) ? !IsObjectiveDone(Q15, TEXT("ReelSilk")) : !HasFlag(TEXT("Quest.Q12.Done")))
	{
		Notify(LOCTEXT("ReelStopped", "Đã dừng ươm tơ – quay lại nồi ươm để làm tiếp."));
	}
}

// Day 3 (Chapter 3): "The First Silk" reels a whole batch, then "Spin the Silk" twists it into thread.

void UVillageStorySubsystem::FinishSilkBatch(float Final, const FText& TierName)
{
	const int32 Silk = Final >= 40.f ? 4 : 3; // a poor batch loses a strand
	const int32 Stars = Final >= 85.f ? 5 : Final >= 65.f ? 4 : Final >= 40.f ? 3 : 2;
	const int32 Metres = FMath::RoundToInt(200.f + Final * 1.6f);
	TakeItem(CocoonItem, CocoonsPerReel);
	GiveItem(RawSilkItem, Silk);
	SetFlag(FName(*FString::Printf(TEXT("Quality.Silk.%d"), Stars)));
	AddProgress(Q15, TEXT("ReelSilk"));
	CloseReeling();
	FString StarText;
	for (int32 i = 0; i < 5; ++i)
	{
		StarText += i < Stars ? TEXT("★") : TEXT("☆");
	}
	Notify(FText::Format(LOCTEXT("BatchResult", "ƯƠM TƠ HOÀN TẤT · Dùng kén ×{0} · Nhận Tơ ×{1} · Chất lượng {2} {3} · Dài {4} m · EXP nghề +150"),
		FText::AsNumber(CocoonsPerReel), FText::AsNumber(Silk), TierName, FText::FromString(StarText), FText::AsNumber(Metres)));
}

void UVillageStorySubsystem::OpenSpinning()
{
	if (HasFlag(TEXT("Quest.Q16.Done")))
	{
		Notify(LOCTEXT("SpinAllDone", "Đã se xong sợi tơ – mang về cho bà Xuân."));
		return;
	}
	if (!HasFlag(TEXT("Quest.Q16.Active")) || !IsObjectiveDone(Q16, TEXT("BringSilk")))
	{
		Notify(LOCTEXT("SpinNotYet", "Hãy mang tơ đến gặp cô Mai trước."));
		return;
	}
	if (GetItemCount(RawSilkItem) < SilkPerSpin)
	{
		Notify(FText::Format(LOCTEXT("SpinNeedSilk", "Cần Tơ ×{0} – ươm thêm tơ ở nồi ươm."), FText::AsNumber(SilkPerSpin)));
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Fail.SFX_Fail"));
		return;
	}
	SpinTangled = 0;
	SpinWeak = 0;
	OpenTiming(EVillageMiniGame::Spin, LOCTEXT("SpinTitle", "Se tơ"),
		LOCTEXT("SpinHint", "Giữ guồng quay đúng nhịp: nhấn [E] khi kim trong vùng vàng. Quá nhanh sợi rối, quá chậm sợi yếu.  ·  [Esc] rời đi"));
	NextTimingRound(LOCTEXT("SpinHello", "Cô Mai: “Tơ tốt mới chỉ là bước đầu. Muốn dệt thành vải, cần một sợi đủ chắc.”"), ResultNeutral);
}

void UVillageStorySubsystem::PressSpin(float Position)
{
	FText Verdict;
	FLinearColor Color;
	if (Round.IsPerfect(Position))
	{
		RoundScore += 25;
		Verdict = LOCTEXT("SpinStrong", "Sợi chắc! (Strong Thread)");
		Color = ResultPerfect;
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_ItemClick.SFX_ItemClick"));
	}
	else if (Round.IsGood(Position))
	{
		RoundScore += 18;
		Verdict = LOCTEXT("SpinGood", "Sợi đều.");
		Color = ResultGood;
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_ItemClick.SFX_ItemClick"));
	}
	else if (Position < Round.GoodMin)
	{
		RoundScore += 8;
		++SpinWeak;
		Verdict = LOCTEXT("SpinWeak", "Quá chậm – sợi yếu (Weak Thread).");
		Color = ResultBad;
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Fail.SFX_Fail"));
	}
	else
	{
		RoundScore += 4;
		++SpinTangled;
		Verdict = LOCTEXT("SpinTangled", "Quá nhanh – sợi bị rối (Thread Tangled).");
		Color = ResultBad;
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Fail.SFX_Fail"));
	}
	// No game over: every round spins something, a bad one only costs quality.
	if (++RoundIndex < SpinRounds)
	{
		NextTimingRound(Verdict, Color);
		return;
	}
	const int32 Threads = SpinTangled + SpinWeak >= 3 ? 2 : 3;
	const int32 Stars = RoundScore >= 90 ? 5 : RoundScore >= 70 ? 4 : RoundScore >= 50 ? 3 : 2;
	TakeItem(RawSilkItem, SilkPerSpin);
	GiveItem(ThreadItem, Threads);
	SetFlag(FName(*FString::Printf(TEXT("Quality.Thread.%d"), Stars)));
	CloseTiming();
	AddProgress(Q16, TEXT("SpinThread"));
	PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_StageComplete.SFX_StageComplete"));
	FString StarText;
	for (int32 i = 0; i < 5; ++i)
	{
		StarText += i < Stars ? TEXT("★") : TEXT("☆");
	}
	Notify(FText::Format(LOCTEXT("SpinResult", "SE TƠ HOÀN TẤT · Tơ ×{0} → Sợi tơ ×{1} · Chất lượng {2} · EXP nghề +100 · Vật liệu mới: Sợi tơ"),
		FText::AsNumber(SilkPerSpin), FText::AsNumber(Threads), FText::FromString(StarText)));
}

void UVillageStorySubsystem::DebugJumpToDay(int32 Day)
{
	if (Day < 3 || !QuestTable)
	{
		return;
	}
	// As if Day 2 had just ended: every Day 1-2 quest done, festival up, 50 cocoons and the first strand of silk in the bag.
	for (const FName& QuestId : TArray<FName>(ActiveQuests))
	{
		SetFlag(FName(*FString::Printf(TEXT("Quest.%s.Active"), *QuestId.ToString())), false);
	}
	ActiveQuests.Reset();
	for (const TCHAR* Quest : { TEXT("Q00"), TEXT("Q01"), TEXT("Q02"), TEXT("Q03"), TEXT("Q09"), TEXT("Q10"), TEXT("Q11"), TEXT("Q12"), TEXT("Q13") })
	{
		SetFlag(FName(*FString::Printf(TEXT("Quest.%s.Done"), Quest)));
	}
	for (const TCHAR* Flag : { TEXT("Day1.Complete"), TEXT("Day2.Started"), TEXT("Day2.Complete"), TEXT("Unlock.SilkReeling"),
		TEXT("Unlock.ReelingWorkshop"), TEXT("Unlock.SpinningWorkshop"), TEXT("Knowledge.SilkProcess"), TEXT("Knowledge.SilkwormRaising"),
		TEXT("Knowledge.FestivalCulture"), TEXT("Festival.Decorated") })
	{
		SetFlag(Flag);
	}
	Silkworm.bFinished = true;
	ShowTagged(TEXT("FestivalDecor_Gate"), true);
	ShowTagged(TEXT("FestivalDecor_Hang"), true);
	ShowTagged(TEXT("FestivalProp"), true);
	if (const int32 Have = GetItemCount(CocoonItem); Have < 50)
	{
		GiveItem(CocoonItem, 50 - Have);
	}
	GiveItem(RawSilkItem, 1);
	EndingDay = 2;
	if (Day == 3)
	{
		StartQuest(TEXT("Q14"));
		RefreshTracker();
		Notify(LOCTEXT("JumpDay3", "Đã chuyển tới sáng Ngày 3 (thử nghiệm)"));
		return;
	}

	// Day 4 and later: Day 3 is done too, with its thread in the bag.
	for (const TCHAR* Quest : { TEXT("Q14"), TEXT("Q15"), TEXT("Q16"), TEXT("Q17") })
	{
		SetFlag(FName(*FString::Printf(TEXT("Quest.%s.Done"), Quest)));
	}
	for (const TCHAR* Flag : { TEXT("Day3.Started"), TEXT("Day3.Complete"), TEXT("Unlock.WeavingWorkshop"), TEXT("Knowledge.SilkMaking") })
	{
		SetFlag(Flag);
	}
	GiveItem(ThreadItem, 3);
	if (Day == 4)
	{
		StartQuest(TEXT("Q18"));
		RefreshTracker();
		Notify(LOCTEXT("JumpDay4", "Đã chuyển tới sáng Ngày 4 (thử nghiệm)"));
		return;
	}

	// Day 5 and later: the first cloth exists.
	for (const TCHAR* Quest : { TEXT("Q18"), TEXT("Q19"), TEXT("Q20"), TEXT("Q21"), TEXT("Q22") })
	{
		SetFlag(FName(*FString::Printf(TEXT("Quest.%s.Done"), Quest)));
	}
	for (const TCHAR* Flag : { TEXT("Day4.Started"), TEXT("Day4.Complete"), TEXT("Unlock.TailoringHouse"), TEXT("Color.Red") })
	{
		SetFlag(Flag);
	}
	CraftStars.Add(TEXT("Dye"), 4);
	CraftStars.Add(TEXT("Weave"), 4);
	GiveItem(FName(TEXT("FAB_SILK_001")), 1);
	if (Day == 5)
	{
		StartQuest(TEXT("Q23"));
		RefreshTracker();
		Notify(LOCTEXT("JumpDay5", "Đã chuyển tới sáng Ngày 5 (thử nghiệm)"));
		return;
	}

	// Day 6 and later: the first outfit is designed and sewn; the three free preparation days begin.
	for (const TCHAR* Quest : { TEXT("Q23"), TEXT("Q24"), TEXT("Q25"), TEXT("Q26"), TEXT("Q27") })
	{
		SetFlag(FName(*FString::Printf(TEXT("Quest.%s.Done"), Quest)));
	}
	for (const TCHAR* Flag : { TEXT("Day5.Started"), TEXT("Day5.Complete"), TEXT("Out.Type.AoDai"), TEXT("Out.Mat.Silk"), TEXT("Out.Color.Red"),
		TEXT("Out.Pat.Traditional"), TEXT("Out.Acc.Scarf"), TEXT("Out.Designed") })
	{
		SetFlag(Flag);
	}
	TakeItem(FName(TEXT("FAB_SILK_001")), 1);
	GiveItem(FName(TEXT("OUTFIT_FIRST_001")), 1);
	CraftStars.Add(TEXT("Sew"), 4);
	Culture = 25;
	Relationship.Add(TEXT("NPC_Chief"), 10);
	Relationship.Add(TEXT("NPC_SilkMaster"), 8);
	// Day 6 and later: festival morning (the preparation days were cut).
	SetFlag(TEXT("Festival.Day"));
	Culture = 55;
	Relationship.Add(TEXT("NPC_ThreadDyer"), 8);
	Relationship.Add(TEXT("NPC_WeaverTailor"), 8);
	for (const TCHAR* Key : SkillKeys)
	{
		SkillExp.Add(Key, 250);
	}
	StartQuest(TEXT("Q40"));
	RefreshTracker();
	Notify(LOCTEXT("JumpFestival", "Đã chuyển tới sáng Ngày hội Tơ Lụa (thử nghiệm)"));
}

void UVillageStorySubsystem::DebugAutoCraft()
{
	// Same as pressing E dead centre of the good band on every round of the open craft.
	if (ActionWidget)
	{
		ActionWidget->DebugFinish(100.f);
		return;
	}
	for (int32 Guard = 0; Guard < 40 && TimingWidget && MiniGame == EVillageMiniGame::Craft; ++Guard)
	{
		OnTimingPress((Round.GoodMin + Round.GoodMax) * 0.5f);
	}
}

void UVillageStorySubsystem::DebugFinishReeling(float Quality)
{
	if (ReelingWidget)
	{
		ReelingWidget->DebugFinish(Quality);
	}
}

// ---------------------------------------------------------------- Chapters 4-6: crafts, skills, festival

namespace
{
	constexpr int32 SkillExpPerLevel = 100;
	constexpr int32 MaxSkillLevel = 5;
	// Tuned with a simulation (see Scripts/balance_festival.py): a player who skipped the three preparation days
	// finishes the challenge around 60 points and fails the score; a player who prepared a little passes most
	// first tries; an expert passes every time. Culture and relationship are meant to be mostly full after
	// ~12 time slots spent mainly on lessons.
	// Finishing the five phases always ends the game; the total only ranks it (S >= 90, A >= 80, B >= 65, else C).
	// Without the preparation days the culture and relationship a player can earn tops out around 40 and 30.
	constexpr int32 CultureForFull = 40;    // culture points that count as 100% of the Cultural Knowledge share
	constexpr int32 RelationshipForFull = 30;

	bool ParseItemSpec(const FString& Spec, FName& OutItem, int32& OutCount)
	{
		FString Item, Count;
		if (!Spec.Split(TEXT(":"), &Item, &Count))
		{
			Item = Spec;
			Count = TEXT("1");
		}
		OutItem = FName(*Item.TrimStartAndEnd());
		OutCount = FMath::Max(1, FCString::Atoi(*Count));
		return !OutItem.IsNone();
	}

	FText SkillName(FName Skill)
	{
		if (Skill == TEXT("Care")) return LOCTEXT("SkillCare", "Nuôi tằm");
		if (Skill == TEXT("Reel")) return LOCTEXT("SkillReel", "Ươm tơ");
		if (Skill == TEXT("Spin")) return LOCTEXT("SkillSpin", "Se tơ");
		if (Skill == TEXT("Dye")) return LOCTEXT("SkillDye", "Nhuộm");
		if (Skill == TEXT("Weave")) return LOCTEXT("SkillWeave", "Dệt");
		if (Skill == TEXT("Tailor")) return LOCTEXT("SkillTailor", "May");
		return FText::FromName(Skill);
	}

	FText NpcName(FName Npc)
	{
		if (Npc == TEXT("NPC_Chief")) return LOCTEXT("NpcChief", "bà Xuân");
		if (Npc == TEXT("NPC_SilkMaster")) return LOCTEXT("NpcSilk", "ông Khánh");
		if (Npc == TEXT("NPC_ThreadDyer")) return LOCTEXT("NpcDyer", "cô Mai");
		if (Npc == TEXT("NPC_WeaverTailor")) return LOCTEXT("NpcWeaver", "cô Lụa");
		return FText::FromName(Npc);
	}

	FString StarString(int32 Stars)
	{
		FString Out;
		for (int32 i = 0; i < 5; ++i)
		{
			Out += i < Stars ? TEXT("★") : TEXT("☆");
		}
		return Out;
	}
}

bool UVillageStorySubsystem::FlagsMatch(const TArray<FName>& Require, const TArray<FName>& Block) const
{
	return Algo::AllOf(Require, [this](const FName& F) { return HasFlag(F); })
		&& !Algo::AnyOf(Block, [this](const FName& F) { return HasFlag(F); });
}

int32 UVillageStorySubsystem::SkillLevel(FName Skill) const
{
	return 1 + FMath::Min(MaxSkillLevel - 1, SkillExp.FindRef(Skill) / SkillExpPerLevel);
}

float UVillageStorySubsystem::CraftBonus(FName Skill) const
{
	// Skill level, a master's technique and the materials chosen for the festival all widen the timing bands a little.
	float Bonus = Skill.IsNone() ? 0.f : 0.012f * (SkillLevel(Skill) - 1);
	if (HasFlag(TEXT("Bonus.Technique")))
	{
		Bonus += 0.02f;
	}
	if (HasFlag(TEXT("Fest.Mat.High")))
	{
		int32 Total = 0;
		for (const TCHAR* Key : SkillKeys)
		{
			Total += SkillLevel(Key);
		}
		Bonus += Total >= 12 ? 0.03f : 0.015f; // premium materials only pay off with a well-trained hand
	}
	else if (HasFlag(TEXT("Fest.Mat.Mid")))
	{
		Bonus += 0.01f;
	}
	return Bonus;
}

void UVillageStorySubsystem::AddSkillExp(FName Skill, int32 Amount)
{
	if (Skill.IsNone() || Amount <= 0)
	{
		return;
	}
	const int32 Before = SkillLevel(Skill);
	SkillExp.FindOrAdd(Skill) += Amount;
	if (SkillLevel(Skill) > Before)
	{
		Notify(FText::Format(LOCTEXT("SkillUp", "Kỹ năng {0} lên cấp {1}!"), SkillName(Skill), FText::AsNumber(SkillLevel(Skill))));
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_StageComplete.SFX_StageComplete"));
	}
}

void UVillageStorySubsystem::AddCulture(int32 Amount)
{
	Culture += Amount;
	Notify(FText::Format(LOCTEXT("CultureUp", "Hiểu biết văn hóa +{0}  (tổng {1})"), FText::AsNumber(Amount), FText::AsNumber(Culture)));
}

void UVillageStorySubsystem::AddRelationship(FName NpcId, int32 Amount)
{
	Relationship.FindOrAdd(NpcId) += Amount;
	Notify(FText::Format(LOCTEXT("RelUp", "Thiện cảm với {0} +{1}"), NpcName(NpcId), FText::AsNumber(Amount)));
}

void UVillageStorySubsystem::SetSlots(int32 Count)
{
	Slots = FMath::Max(0, Count);
	SetFlag(TEXT("Slots.Out"), Slots == 0);
	RefreshTracker();
}

void UVillageStorySubsystem::UseSlot(int32 Count)
{
	Slots = FMath::Max(0, Slots - Count);
	SetFlag(TEXT("Slots.Out"), Slots == 0);
	Notify(Slots == 0 ? LOCTEXT("SlotsOut", "Hôm nay đã hết thời gian hoạt động – về gặp bà Xuân để nghỉ.")
		: FText::Format(LOCTEXT("SlotsLeft", "Còn {0} lượt hoạt động hôm nay"), FText::AsNumber(Slots)));
	RefreshTracker();
}

void UVillageStorySubsystem::ClearFlagsWithPrefix(const FString& Prefix)
{
	TArray<FName> Removed;
	for (auto It = Flags.CreateIterator(); It; ++It)
	{
		if (It->ToString().StartsWith(Prefix))
		{
			Removed.Add(*It);
			It.RemoveCurrent();
		}
	}
	for (const FName& Flag : Removed)
	{
		if (FlagGated.Contains(Flag))
		{
			ApplyFlagGate(Flag, false);
		}
	}
}

FText UVillageStorySubsystem::ExpandText(const FText& Text) const
{
	FString S = Text.ToString();
	if (!S.Contains(TEXT("{")))
	{
		return Text;
	}
	S.ReplaceInline(TEXT("{Culture}"), *FString::FromInt(Culture));
	if (const int32* Total = CraftScore.Find(TEXT("Final.Total")))
	{
		S.ReplaceInline(TEXT("{Rank}"), *Total >= 90 ? TEXT("S") : *Total >= 80 ? TEXT("A") : *Total >= 65 ? TEXT("B") : TEXT("C"));
	}
	S.ReplaceInline(TEXT("{Slots}"), *FString::FromInt(Slots));
	if (CraftTable)
	{
		for (const FName& Key : CraftTable->GetRowNames())
		{
			S.ReplaceInline(*FString::Printf(TEXT("{Stars:%s}"), *Key.ToString()), *StarString(CraftStars.FindRef(Key)));
		}
	}
	for (const TPair<FName, int32>& Pair : CraftScore)
	{
		S.ReplaceInline(*FString::Printf(TEXT("{Score:%s}"), *Pair.Key.ToString()), *FString::FromInt(Pair.Value));
	}
	for (const TCHAR* Key : SkillKeys)
	{
		S.ReplaceInline(*FString::Printf(TEXT("{Skill:%s}"), Key), *FString::FromInt(SkillLevel(Key)));
	}
	int32 Start = INDEX_NONE;
	while ((Start = S.Find(TEXT("{Item:"))) != INDEX_NONE)
	{
		const int32 End = S.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
		if (End == INDEX_NONE)
		{
			break;
		}
		const FString Id = S.Mid(Start + 6, End - Start - 6);
		S = S.Left(Start) + FString::FromInt(GetItemCount(FName(*Id))) + S.Mid(End + 1);
	}
	return FText::FromString(S);
}

void UVillageStorySubsystem::ShowRowCard()
{
	const FVillageDayRow* Row = DayTable ? DayTable->FindRow<FVillageDayRow>(EndingRow, TEXT("DayCard")) : nullptr;
	if (!Row)
	{
		UE_LOG(LogVillageStory, Warning, TEXT("Day row %s not found; run Scripts/build_chapter4_6_story.py"), *EndingRow.ToString());
		DayEndWidget->Setup(LOCTEXT("DayGeneric", "KẾT THÚC NGÀY"), {});
		return;
	}
	RunActions(Row->SetFlags); // flags first, so the card's texts can read them
	TArray<FText> Lines;
	for (const FText& Line : Row->Lines)
	{
		Lines.Add(ExpandText(Line));
	}
	DayEndWidget->Setup(ExpandText(Row->Title), Lines);
}

// Crafts: one timing-bar mini-game described by a DT_VillageCrafts row.

void UVillageStorySubsystem::OpenCraft(FName Key)
{
	const FVillageCraftRow* Row = CraftTable ? CraftTable->FindRow<FVillageCraftRow>(Key, TEXT("OpenCraft")) : nullptr;
	if (!Row || Row->Rounds.Num() == 0)
	{
		UE_LOG(LogVillageStory, Warning, TEXT("No craft '%s'; run Scripts/build_chapter4_6_story.py"), *Key.ToString());
		return;
	}
	if (!FlagsMatch(Row->RequireFlags, Row->BlockFlags))
	{
		Notify(LOCTEXT("CraftNotYet", "Bây giờ chưa phải lúc làm việc này."));
		return;
	}
	if (Row->SlotCost > 0 && HasFlag(TEXT("Free.Days")) && HasFlag(TEXT("Slots.Out")))
	{
		Notify(LOCTEXT("CraftNoSlots", "Hôm nay đã hết thời gian hoạt động – về gặp bà Xuân để nghỉ."));
		return;
	}
	for (const FString& Spec : Row->TakeItems)
	{
		FName Item;
		int32 Count;
		if (ParseItemSpec(Spec, Item, Count) && GetItemCount(Item) < Count)
		{
			Notify(FText::Format(LOCTEXT("CraftNeeds", "Thiếu nguyên liệu: cần {0} ×{1} (đang có {2})"), FText::FromName(Item), FText::AsNumber(Count), FText::AsNumber(GetItemCount(Item))));
			PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Fail.SFX_Fail"));
			return;
		}
	}
	ActiveCraft = Key;
	if (!Row->Mode.IsNone())
	{
		// An action mini-game (dyeing sliders, weaving arrows, the show's lanes, cutting): one widget, one score.
		MiniGame = EVillageMiniGame::Craft;
		RoundIndex = 0;
		RoundScore = 0;
		ActionWidget = MakeWidget<UVillageActionWidget>();
		ActionWidget->AddToViewport(20);
		const FVillageCraftRound& First = Row->Rounds[0];
		ActionWidget->Setup(Row->Mode, Row->Title, Row->Hint, Row->Rounds.Num(), First.Speed, First.GoodHalf + CraftBonus(Row->Skill));
		EnterUI(ActionWidget);
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Open.SFX_Open"));
		return;
	}
	OpenTiming(EVillageMiniGame::Craft, Row->Title, Row->Hint);
	NextTimingRound(FText::GetEmpty(), ResultNeutral);
}

void UVillageStorySubsystem::OnActionGameDone(float Score)
{
	const FVillageCraftRow* Row = CraftTable ? CraftTable->FindRow<FVillageCraftRow>(ActiveCraft, TEXT("ActionDone")) : nullptr;
	if (ActionWidget)
	{
		ActionWidget->RemoveFromParent();
		ActionWidget = nullptr;
		MiniGame = EVillageMiniGame::None;
		ExitUI();
	}
	if (Row)
	{
		RoundScore = FMath::RoundToInt(Score) * FMath::Max(1, Row->Rounds.Num()); // FinishCraft averages over the rounds
		FinishCraft(*Row);
	}
}

void UVillageStorySubsystem::CloseActionGame()
{
	if (ActionWidget)
	{
		ActionWidget->RemoveFromParent();
		ActionWidget = nullptr;
		MiniGame = EVillageMiniGame::None;
		ExitUI();
	}
}

void UVillageStorySubsystem::PressCraft(float Position)
{
	const FVillageCraftRow* Row = CraftTable ? CraftTable->FindRow<FVillageCraftRow>(ActiveCraft, TEXT("PressCraft")) : nullptr;
	if (!Row || !Row->Rounds.IsValidIndex(RoundIndex))
	{
		CloseTiming();
		return;
	}
	const FVillageCraftRound& R = Row->Rounds[RoundIndex];
	FText Verdict;
	FLinearColor Color;
	if (Round.IsPerfect(Position))
	{
		RoundScore += 100;
		Verdict = LOCTEXT("CraftPerfect", "Perfect!");
		Color = ResultPerfect;
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_ItemClick.SFX_ItemClick"));
	}
	else if (Round.IsGood(Position))
	{
		RoundScore += 70;
		Verdict = LOCTEXT("CraftGood", "Good!");
		Color = ResultGood;
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_ItemClick.SFX_ItemClick"));
	}
	else
	{
		RoundScore += 30;
		const FText& Why = Position < Round.GoodMin ? R.TooLow : R.TooHigh;
		Verdict = Why.IsEmpty() ? LOCTEXT("CraftMiss", "Miss!") : Why;
		Color = ResultBad;
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Fail.SFX_Fail"));
	}
	// No game over: every round counts, a miss only lowers the average.
	if (++RoundIndex < Row->Rounds.Num())
	{
		NextTimingRound(Verdict, Color);
		return;
	}
	FinishCraft(*Row);
}

void UVillageStorySubsystem::FinishCraft(const FVillageCraftRow& Row)
{
	const FName Key = ActiveCraft;
	const int32 Score = RoundScore / FMath::Max(1, Row.Rounds.Num());
	const int32 Stars = Score >= 90 ? 5 : Score >= 75 ? 4 : Score >= 60 ? 3 : Score >= 45 ? 2 : 1;
	CraftScore.Add(Key, Score);
	CraftStars.Add(Key, Stars);
	CloseTiming();
	if (Row.SlotCost > 0 && HasFlag(TEXT("Free.Days")))
	{
		UseSlot(Row.SlotCost);
	}
	AddSkillExp(Row.Skill, 8 + 8 * Stars);
	if (Score < Row.FailBelow)
	{
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Fail.SFX_Fail"));
		Notify(FText::Format(LOCTEXT("CraftFailed", "Thất bại – điểm {0}/100 chưa đủ."), FText::AsNumber(Score)));
		RunActions(Row.OnFail);
		return;
	}
	for (const FString& Spec : Row.TakeItems)
	{
		FName Item;
		int32 Count;
		if (ParseItemSpec(Spec, Item, Count))
		{
			TakeItem(Item, Count);
		}
	}
	PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_StageComplete.SFX_StageComplete"));
	if (!Row.ResultText.IsEmpty())
	{
		Notify(FText::Format(Row.ResultText, FText::FromString(StarString(Stars)), FText::AsNumber(Score)));
	}
	RunActions(Row.OnDone);
}

void UVillageStorySubsystem::Gather(AActor* Source)
{
	FName Item;
	FText Prompt;
	ReadProperty<FNameProperty>(Source, TEXT("ItemID"), Item);
	ReadProperty<FTextProperty>(Source, TEXT("FallbackPrompt"), Prompt);
	if (!HasFlag(TEXT("Gather.On")))
	{
		Notify(LOCTEXT("GatherNotYet", "Bây giờ chưa cần thứ này."));
		return;
	}
	int32 Wait = 0;
	if (!DrawFromSource(Source, 3, 30.0, Wait))
	{
		Notify(FText::Format(LOCTEXT("GatherEmpty", "Chỗ này đã lấy hết – quay lại sau {0} giây, hoặc tìm chỗ khác"), FText::AsNumber(Wait)));
		PlaySfx(TEXT("/Game/Audio/SFX/UI/SFX_Fail.SFX_Fail"));
		return;
	}
	GiveItem(Item, 1); // counts toward the quest objective through its TrackItem
	PlaySfx(TEXT("/Game/Audio/SFX/Gameplay/SFX_CollectItem.SFX_CollectItem"));
	Notify(FText::Format(LOCTEXT("GatherGot", "{0}  (đang có {1})"), Prompt, FText::AsNumber(GetItemCount(Item))));
}

// The festival: five weighted scores decide whether the Master's Challenge succeeded.

void UVillageStorySubsystem::ComputeFinalScore()
{
	auto Avg = [this](std::initializer_list<const TCHAR*> Keys)
	{
		float Sum = 0.f;
		int32 N = 0;
		for (const TCHAR* Key : Keys)
		{
			if (const int32* Score = CraftScore.Find(Key))
			{
				Sum += *Score;
				++N;
			}
		}
		return N > 0 ? Sum / N : 40.f;
	};
	const float Crafting = Avg({ TEXT("ChDye"), TEXT("ChWeave"), TEXT("ChTailor") });

	float Fashion = 50.f;
	Fashion += HasFlag(TEXT("Out.Mat.High")) ? 20.f : HasFlag(TEXT("Out.Mat.Silk")) ? 10.f : 0.f;
	Fashion += HasFlag(TEXT("Out.Pat.Ancient")) ? 25.f : HasFlag(TEXT("Out.Pat.Traditional")) ? 15.f
		: (HasFlag(TEXT("Out.Pat.Mountain")) || HasFlag(TEXT("Out.Pat.Flower"))) ? 10.f : 3.f;
	for (const TCHAR* Acc : { TEXT("Out.Acc.Scarf"), TEXT("Out.Acc.Belt"), TEXT("Out.Acc.Bracelet"), TEXT("Out.Acc.Decor") })
	{
		Fashion += HasFlag(Acc) ? 6.f : 0.f;
	}
	Fashion += HasFlag(TEXT("Out.Type.AoDai")) ? 5.f : 0.f;
	Fashion += (HasFlag(TEXT("Out.Color.Red")) || HasFlag(TEXT("Out.Color.Yellow")) || HasFlag(TEXT("Out.Color.Blue"))) ? 5.f : 0.f;
	Fashion = FMath::Min(Fashion, 100.f);

	const float CultureScore = FMath::Min(100.f, 100.f * Culture / CultureForFull);
	int32 RelTotal = 0;
	for (const TPair<FName, int32>& Pair : Relationship)
	{
		RelTotal += Pair.Value;
	}
	const float RelScore = FMath::Min(100.f, 100.f * RelTotal / RelationshipForFull);
	const float Performance = static_cast<float>(CraftScore.FindRef(TEXT("ChStage")));
	const float Total = Crafting * 0.30f + Fashion * 0.25f + CultureScore * 0.20f + RelScore * 0.10f + Performance * 0.15f;

	CraftScore.Add(TEXT("Final.Craft"), FMath::RoundToInt(Crafting));
	CraftScore.Add(TEXT("Final.Fashion"), FMath::RoundToInt(Fashion));
	CraftScore.Add(TEXT("Final.Culture"), FMath::RoundToInt(CultureScore));
	CraftScore.Add(TEXT("Final.Rel"), FMath::RoundToInt(RelScore));
	CraftScore.Add(TEXT("Final.Perf"), FMath::RoundToInt(Performance));
	CraftScore.Add(TEXT("Final.Total"), FMath::RoundToInt(Total));
	SetFlag(TEXT("Fest.Success"));
	StartDialogue(TEXT("FEST_SUCCESS"));
}

void UVillageStorySubsystem::FestivalReset()
{
	// Only the festival day starts over: skills, knowledge, items and the outfit stay (flags outside Fest. / Q4x are kept).
	ClearFlagsWithPrefix(TEXT("Fest."));
	ClearFlagsWithPrefix(TEXT("Quest.Q4"));
	ClearFlagsWithPrefix(TEXT("Obj.Q4"));
	TArray<FName> ProgressKeys;
	Progress.GetKeys(ProgressKeys);
	for (const FName& Key : ProgressKeys)
	{
		if (Key.ToString().StartsWith(TEXT("Q4")))
		{
			Progress.Remove(Key);
		}
	}
	ActiveQuests.RemoveAll([](const FName& Quest) { return Quest.ToString().StartsWith(TEXT("Q4")); });
	for (const TCHAR* Key : { TEXT("ChDye"), TEXT("ChWeave"), TEXT("ChTailor"), TEXT("ChStage") })
	{
		CraftStars.Remove(Key);
		CraftScore.Remove(Key);
	}
	for (const TCHAR* Key : { TEXT("Final.Craft"), TEXT("Final.Fashion"), TEXT("Final.Culture"), TEXT("Final.Rel"), TEXT("Final.Perf"), TEXT("Final.Total") })
	{
		CraftScore.Remove(Key);
	}
	// Back home at 06:00 on the morning of the festival.
	if (APlayerController* PC = GetPC())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
			{
				Pawn->SetActorLocation(It->GetActorLocation() + FVector(0.f, 0.f, 100.f));
				break;
			}
		}
		PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, 2.f, FLinearColor::Black, false, false);
	}
	StartQuest(TEXT("Q40"));
	RefreshTracker();
	Notify(LOCTEXT("FestivalMorning", "06:00 – NGÀY HỘI TƠ LỤA: lễ hội bắt đầu lại. Kỹ năng, vật phẩm và trang phục của bạn vẫn còn."));
}

// ---------------------------------------------------------------- Nameplates

void UVillageStorySubsystem::SpawnNameplates()
{
	// Important NPCs carry DisplayName / DisplayRole (BP_VillageNPC variables).
	Nameplates.Reset();
	CachedInteractables.Reset();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		const bool bHasInteractTag = Actor->ActorHasTag(TEXT("NPC")) || Actor->ActorHasTag(TEXT("Interact_Shop"))
			|| Actor->ActorHasTag(TEXT("Interact_MulberryBush")) || Actor->ActorHasTag(TEXT("Interact_SilkwormTray"))
			|| Actor->ActorHasTag(TEXT("Interact_DecoMaterial")) || Actor->ActorHasTag(TEXT("Interact_FestivalGate"))
			|| Actor->ActorHasTag(TEXT("Interact_FestivalHang")) || Actor->ActorHasTag(TEXT("Interact_FestivalSpot"))
			|| Actor->ActorHasTag(TEXT("Interact_ReelingPot")) || Actor->ActorHasTag(TEXT("Interact_SpinningWheel"))
			|| Actor->ActorHasTag(TEXT("Interact_Craft")) || Actor->ActorHasTag(TEXT("Interact_Gather"))
			|| Actor->ActorHasTag(TEXT("Interact_Dialogue")) || Actor->GetClass()->GetName().StartsWith(TEXT("BP_ItemPickup"));
		if (bHasInteractTag)
		{
			CachedInteractables.Add(Actor);
		}
		FText Name, Role;
		if (!It->ActorHasTag(TEXT("NPC")) || !ReadProperty<FTextProperty>(*It, TEXT("DisplayName"), Name) || Name.IsEmpty())
		{
			continue;
		}
		ReadProperty<FTextProperty>(*It, TEXT("DisplayRole"), Role);
		UWidgetComponent* Plate = NewObject<UWidgetComponent>(*It, TEXT("Nameplate"));
		Plate->SetupAttachment(It->GetRootComponent());
		Plate->SetRelativeLocation(FVector(0.f, 0.f, 215.f));
		Plate->SetWidgetSpace(EWidgetSpace::Screen);
		Plate->SetDrawAtDesiredSize(true);
		Plate->SetPivot(FVector2D(0.5f, 1.f));
		Plate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Plate->RegisterComponent();
		UVillageNameplateWidget* Widget = MakeWidget<UVillageNameplateWidget>();
		Widget->Setup(Name, Role);
		Plate->SetWidget(Widget);
		Nameplates.Add(Plate);
	}

	// Materials and mini-game areas: name (plus what the area is for) shown when the player gets close.
	InteractPlates.Reset();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		FText Name, Purpose;
		if (!DescribeInteractable(*It, Name, Purpose))
		{
			continue;
		}
		FVector Origin, Extent;
		It->GetActorBounds(false, Origin, Extent);
		UWidgetComponent* Plate = NewObject<UWidgetComponent>(*It, TEXT("InteractNameplate"));
		Plate->SetupAttachment(It->GetRootComponent());
		Plate->SetWidgetSpace(EWidgetSpace::Screen);
		Plate->SetDrawAtDesiredSize(true);
		Plate->SetPivot(FVector2D(0.5f, 1.f));
		Plate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Plate->RegisterComponent();
		// Tall models (bamboo) would put the plate far above the screen: keep it within reach of the base.
		const double Height = FMath::Min(Extent.Z * 2.0, 220.0);
		Plate->SetWorldLocation(FVector(Origin.X, Origin.Y, Origin.Z - Extent.Z + Height + 30.f));
		UVillageNameplateWidget* Widget = MakeWidget<UVillageNameplateWidget>();
		Widget->Setup(Name, Purpose);
		Plate->SetWidget(Widget);
		Plate->SetVisibility(false);
		InteractPlates.Add(*It, Plate);
	}
	GetWorld()->GetTimerManager().SetTimer(NameplateTimer, FTimerDelegate::CreateUObject(this, &UVillageStorySubsystem::UpdateNameplates), 0.25f, true);
	UpdateNameplates();
}

bool UVillageStorySubsystem::DescribeInteractable(const AActor* Actor, FText& OutName, FText& OutPurpose) const
{
	OutPurpose = FText::GetEmpty();
	if (Actor->ActorHasTag(TEXT("Interact_MulberryBush")))
	{
		OutName = LOCTEXT("PlateBush", "Bụi dâu tằm");
	}
	else if (Actor->ActorHasTag(TEXT("Interact_DecoMaterial")))
	{
		FName Key;
		ReadProperty<FNameProperty>(Actor, TEXT("ItemID"), Key);
		const FDecoItem* Deco = FindDeco(Key);
		if (!Deco)
		{
			return false;
		}
		OutName = FText::FromString(Deco->Name);
	}
	else if (Actor->ActorHasTag(TEXT("Interact_SilkwormTray")))
	{
		OutName = LOCTEXT("PlateTray", "Khay nuôi tằm");
		OutPurpose = HasFlag(TEXT("Day2.Started"))
			? LOCTEXT("PlateTrayDay2", "Khu vực mini game: Thu hoạch kén tằm")
			: LOCTEXT("PlateTrayDay1", "Khu vực chăm sóc tằm");
	}
	else if (Actor->ActorHasTag(TEXT("Interact_FestivalGate")))
	{
		OutName = LOCTEXT("PlateGate", "Cổng lễ hội");
		OutPurpose = LOCTEXT("PlateGatePurpose", "Khu vực mini game: Trang trí cổng lễ hội");
	}
	else if (Actor->ActorHasTag(TEXT("Interact_FestivalHang")))
	{
		OutName = LOCTEXT("PlateHang", "Điểm treo trang trí");
		OutPurpose = LOCTEXT("PlateHangPurpose", "Khu vực mini game: Treo đồ trang trí");
	}
	else if (Actor->ActorHasTag(TEXT("Interact_ReelingPot")))
	{
		OutName = LOCTEXT("PlatePot", "Nồi ươm tơ");
		OutPurpose = HasFlag(TEXT("Quest.Q15.Active")) ? LOCTEXT("PlatePotBatch", "Khu vực mini game: Ươm tơ – Chọn kén & kéo tơ")
			: LOCTEXT("PlatePotPurpose", "Khu vực mini game: Ươm tơ – Tìm đầu sợi");
	}
	else if (Actor->ActorHasTag(TEXT("Interact_SpinningWheel")))
	{
		OutName = LOCTEXT("PlateWheel", "Guồng se tơ");
		OutPurpose = LOCTEXT("PlateWheelPurpose", "Khu vực mini game: Se tơ");
	}
	else if (Actor->ActorHasTag(TEXT("Interact_Craft")))
	{
		FName Key;
		ReadProperty<FNameProperty>(Actor, TEXT("ItemID"), Key);
		const FVillageCraftRow* Row = CraftTable ? CraftTable->FindRow<FVillageCraftRow>(Key, TEXT("Describe")) : nullptr;
		if (!Row)
		{
			return false;
		}
		OutName = Row->Title;
		OutPurpose = LOCTEXT("PlateCraftPurpose", "Khu vực mini game");
	}
	else if (Actor->ActorHasTag(TEXT("Interact_Gather")) || Actor->ActorHasTag(TEXT("Interact_Dialogue")))
	{
		ReadProperty<FTextProperty>(Actor, TEXT("FallbackPrompt"), OutName);
		return !OutName.IsEmpty();
	}
	else if (Actor->ActorHasTag(TEXT("Interact_FestivalSpot")) || Actor->GetClass()->GetName().StartsWith(TEXT("BP_ItemPickup")))
	{
		ReadProperty<FTextProperty>(Actor, TEXT("FallbackPrompt"), OutName);
		return !OutName.IsEmpty();
	}
	else if (Actor->ActorHasTag(TEXT("Interact_Shop")) && !Actor->ActorHasTag(TEXT("NPC")))
	{
		OutName = LOCTEXT("PlateShop", "Cửa Hàng Tơ Lụa");
		OutPurpose = LOCTEXT("PlateShopPurpose", "Khu vực mua bán vật phẩm · Nhấn [E] hoặc [B]");
	}
	else
	{
		return false;
	}
	return true;
}

bool UVillageStorySubsystem::NpcNeeded(FName NpcId) const
{
	auto Active = [this](const TCHAR* Quest) { return HasFlag(FName(*FString::Printf(TEXT("Quest.%s.Active"), Quest))); };
	auto Done = [this](const TCHAR* Quest) { return HasFlag(FName(*FString::Printf(TEXT("Quest.%s.Done"), Quest))); };
	auto Obj = [this](const TCHAR* Quest, const TCHAR* Objective) { return IsObjectiveDone(FName(Quest), FName(Objective)); };

	// Chapters 4-6 drive their guide markers with data: a dialogue action SetFlag:Need.<NpcId> (cleared when visited).
	if (HasFlag(FName(*(TEXT("Need.") + NpcId.ToString()))))
	{
		return true;
	}
	if (NpcId == TEXT("NPC_Chief"))
	{
		return (Active(TEXT("Q00")) && !Obj(TEXT("Q00"), TEXT("TalkChief")))
			|| Active(TEXT("Q03"))
			|| (HasFlag(TEXT("Day1.Complete")) && !HasFlag(TEXT("Day2.Started")))
			|| (Active(TEXT("Q09")) && !Obj(TEXT("Q09"), TEXT("TalkChief")))
			|| (Done(TEXT("Q10")) && !Active(TEXT("Q11")) && !Done(TEXT("Q11")))
			|| (Active(TEXT("Q11")) && Obj(TEXT("Q11"), TEXT("ArrangeArea")) && !Obj(TEXT("Q11"), TEXT("FinalDecoration")))
			// End of Day 2 / Day 3: back to the festival square once the day's work is done.
			|| (Active(TEXT("Q13")) && Done(TEXT("Q12")))
			|| (Active(TEXT("Q14")) && !Obj(TEXT("Q14"), TEXT("TalkChief")))
			|| (Active(TEXT("Q17")) && Done(TEXT("Q16")));
	}
	if (NpcId == TEXT("NPC_SilkMaster"))
	{
		return (Active(TEXT("Q01")) && !Obj(TEXT("Q01"), TEXT("MeetSilkMaster")))
			|| (Active(TEXT("Q02")) && Obj(TEXT("Q02"), TEXT("RaiseSilkworms")) && !Obj(TEXT("Q02"), TEXT("ShowCocoon")))
			|| (HasFlag(TEXT("Unlock.SilkReeling")) && !Active(TEXT("Q12")) && !Done(TEXT("Q12")))
			|| (Active(TEXT("Q15")) && Obj(TEXT("Q15"), TEXT("ReelSilk")) && !Obj(TEXT("Q15"), TEXT("Feedback")));
	}
	if (NpcId == TEXT("NPC_ThreadDyer"))
	{
		return (Active(TEXT("Q01")) && !Obj(TEXT("Q01"), TEXT("MeetThreadDyer")))
			|| (Active(TEXT("Q16")) && !Obj(TEXT("Q16"), TEXT("BringSilk")));
	}
	if (NpcId == TEXT("NPC_WeaverTailor"))
	{
		return Active(TEXT("Q01")) && !Obj(TEXT("Q01"), TEXT("MeetWeaverTailor"));
	}
	return false;
}

void UVillageStorySubsystem::UpdateNameplates()
{
	SyncCoinHUD();
	if (ActiveQuests.Num() > 0)
	{
		SyncItemObjectives();
	}
	const APawn* Pawn = GetPC() ? GetPC()->GetPawn() : nullptr;
	for (const TWeakObjectPtr<UWidgetComponent>& Plate : Nameplates)
	{
		if (Plate.IsValid())
		{
			const bool bNear = Pawn && FVector::Dist(Pawn->GetActorLocation(), Plate->GetComponentLocation()) < 1500.f;
			Plate->SetVisibility(bNear && !DayEndWidget);
		}
	}

	// Outline usable items near the player: custom depth stencil 252 is drawn by
	// M_InteractOutline (PP_InteractOutline volume). Spent bushes and a finished tray stay dark.
	constexpr int32 OutlineStencil = 252;
	constexpr float OutlineRange = 800.f;
	const double Now = GetWorld()->GetTimeSeconds();
	// The nearest usable station of each kind becomes a guide marker on the HUD.
	TMap<FName, TPair<double, FVillageGuideMarker>> Guide;
	for (const TWeakObjectPtr<AActor>& WeakActor : CachedInteractables)
	{
		AActor* Actor = WeakActor.Get();
		if (!Actor)
		{
			continue;
		}
		const bool bBush = Actor->ActorHasTag(TEXT("Interact_MulberryBush"));
		const bool bTray = Actor->ActorHasTag(TEXT("Interact_SilkwormTray"));
		const bool bPickup = Actor->GetClass()->GetName().StartsWith(TEXT("BP_ItemPickup"));
		const bool bDeco = Actor->ActorHasTag(TEXT("Interact_DecoMaterial"));
		const bool bGate = Actor->ActorHasTag(TEXT("Interact_FestivalGate"));
		const bool bHang = Actor->ActorHasTag(TEXT("Interact_FestivalHang"));
		const bool bSpot = Actor->ActorHasTag(TEXT("Interact_FestivalSpot"));
		const bool bPot = Actor->ActorHasTag(TEXT("Interact_ReelingPot"));
		const bool bWheel = Actor->ActorHasTag(TEXT("Interact_SpinningWheel"));
		const bool bCraft = Actor->ActorHasTag(TEXT("Interact_Craft"));
		const bool bGather = Actor->ActorHasTag(TEXT("Interact_Gather"));
		const bool bSign = Actor->ActorHasTag(TEXT("Interact_Dialogue"));
		const bool bShop = Actor->ActorHasTag(TEXT("Interact_Shop"));
		if (!bBush && !bTray && !bPickup && !bDeco && !bGate && !bHang && !bSpot && !bPot && !bWheel && !bCraft && !bGather && !bSign && !bShop)
		{
			// Story NPCs are guide targets while a quest step needs them (their nameplate is handled elsewhere).
			FName NpcId;
			if (Pawn && !DayEndWidget && !Actor->IsHidden() && Actor->ActorHasTag(TEXT("NPC")) && ReadProperty<FNameProperty>(Actor, TEXT("ItemID"), NpcId) && NpcNeeded(NpcId))
			{
				FText NpcName;
				ReadProperty<FTextProperty>(Actor, TEXT("DisplayName"), NpcName);
				if (!NpcName.IsEmpty())
				{
					const double Dist = FVector::Dist(Pawn->GetActorLocation(), Actor->GetActorLocation());
					TPair<double, FVillageGuideMarker>* Best = Guide.Find(NpcId);
					if (!Best || Dist < Best->Key)
					{
						FVillageGuideMarker Marker;
						Marker.Location = Actor->GetActorLocation() + FVector(0.f, 0.f, 230.f);
						Marker.Name = NpcName;
						Guide.Add(NpcId, TPair<double, FVillageGuideMarker>(Dist, Marker));
					}
				}
			}
			continue;
		}
		// bActive: this item is something the player can use right now; bOn adds being close enough to light up.
		bool bActive = !DayEndWidget;
		FName CraftKey;
		if (bCraft)
		{
			// A craft station lights up only while its row's flags are met (and, on free days, a time slot is left).
			ReadProperty<FNameProperty>(Actor, TEXT("ItemID"), CraftKey);
			const FVillageCraftRow* Row = CraftTable ? CraftTable->FindRow<FVillageCraftRow>(CraftKey, TEXT("Plate")) : nullptr;
			if (!Row || !FlagsMatch(Row->RequireFlags, Row->BlockFlags) || (Row->SlotCost > 0 && HasFlag(TEXT("Free.Days")) && HasFlag(TEXT("Slots.Out"))))
			{
				bActive = false;
			}
		}
		if (bGather && !HasFlag(TEXT("Gather.On")))
		{
			bActive = false;
		}
		if ((bBush || bDeco || bGather) && BushLeaves.FindRef(Actor) <= 0 && BushLeaves.Contains(Actor) && Now < BushRegrowAt.FindRef(Actor))
		{
			bActive = false;
		}
		// Chapter 2 stations only light up while they are the next thing to do.
		if (bTray && (HasFlag(TEXT("Day2.Started")) ? !HasFlag(TEXT("Quest.Q10.Active")) : Silkworm.bFinished))
		{
			bActive = false;
		}
		if ((bDeco && !HasFlag(TEXT("Quest.Q11.Active")))
			|| (bGate && (!HasFlag(TEXT("Quest.Q11.Active")) || HasFlag(TEXT("Obj.Q11.DecorateGate"))))
			|| (bHang && (!HasFlag(TEXT("Obj.Q11.DecorateGate")) || HasFlag(TEXT("Obj.Q11.HangDecorations"))))
			|| (bSpot && !HasFlag(TEXT("Obj.Q11.HangDecorations")))
			|| (bPot && !(HasFlag(TEXT("Quest.Q12.Active")) || (HasFlag(TEXT("Quest.Q15.Active")) && !HasFlag(TEXT("Obj.Q15.ReelSilk")))))
			|| (bWheel && !(HasFlag(TEXT("Quest.Q16.Active")) && HasFlag(TEXT("Obj.Q16.BringSilk")))))
		{
			bActive = false;
		}
		const bool bOn = bActive && Pawn && FVector::Dist(Pawn->GetActorLocation(), Actor->GetActorLocation()) < OutlineRange;

		// Guide markers: mulberry bushes and trays while Quest 02 needs them, every festival material still
		// needed, then the gate, hanging point, arranging area, trays (cocoons) and reeling pot as each becomes the next step.
		const bool bNeedLeaves = bBush && HasFlag(TEXT("Quest.Q02.Active")) && !IsObjectiveDone(Q02, TEXT("CollectLeaves"));
		const bool bNeedTray = bTray && (HasFlag(TEXT("Day2.Started")) ? true : HasFlag(TEXT("Quest.Q02.Active")) && !IsObjectiveDone(Q02, TEXT("RaiseSilkworms")));
		if (bActive && Pawn && !Actor->IsHidden() && (bDeco || bGate || bHang || bSpot || bPot || bWheel || bCraft || bGather || bNeedLeaves || bNeedTray))
		{
			FName Kind;
			FText Name, Purpose;
			if (bDeco)
			{
				FName Key;
				ReadProperty<FNameProperty>(Actor, TEXT("ItemID"), Key);
				const FDecoItem* Deco = FindDeco(Key);
				if (Deco && !IsObjectiveDone(Q11, Deco->Objective))
				{
					Kind = Deco->Objective;
				}
			}
			else if (bCraft || bGather)
			{
				ReadProperty<FNameProperty>(Actor, TEXT("ItemID"), Kind); // one marker per station / material kind
			}
			else
			{
				Kind = bGate ? FName(TEXT("Gate")) : bHang ? FName(TEXT("Hang")) : bSpot ? FName(TEXT("Spot")) : bPot ? FName(TEXT("Pot")) : bWheel ? FName(TEXT("Wheel")) : bBush ? FName(TEXT("Bush")) : FName(TEXT("Tray"));
			}
			if (!Kind.IsNone() && DescribeInteractable(Actor, Name, Purpose))
			{
				if (bSpot)
				{
					Name = LOCTEXT("GuideSpots", "Sắp xếp khu lễ hội");
				}
				const double Dist = FVector::Dist(Pawn->GetActorLocation(), Actor->GetActorLocation());
				TPair<double, FVillageGuideMarker>* Best = Guide.Find(Kind);
				if (!Best || Dist < Best->Key)
				{
					FVillageGuideMarker Marker;
					Marker.Location = Actor->GetActorLocation() + FVector(0.f, 0.f, 220.f);
					Marker.Name = Name;
					Guide.Add(Kind, TPair<double, FVillageGuideMarker>(Dist, Marker));
				}
			}
		}
		if (const TWeakObjectPtr<UWidgetComponent>* Found = InteractPlates.Find(Actor))
		{
			if (UWidgetComponent* Plate = Found->Get())
			{
				Plate->SetVisibility(bOn && !Actor->IsHidden());
				FText Name, Purpose;
				UVillageNameplateWidget* Widget = Cast<UVillageNameplateWidget>(Plate->GetWidget());
				if (bOn && Widget && DescribeInteractable(Actor, Name, Purpose))
				{
					Widget->Setup(Name, Purpose); // the purpose can change with the day (trays)
				}
			}
		}
		TInlineComponentArray<UMeshComponent*> Meshes(Actor);
		for (UMeshComponent* Mesh : Meshes)
		{
			if (Mesh->IsVisible() && Mesh->bRenderCustomDepth != bOn)
			{
				Mesh->SetCustomDepthStencilValue(OutlineStencil);
				Mesh->SetRenderCustomDepth(bOn);
			}
		}
	}

	if (Hud)
	{
		// Nothing floats over a panel the player is working in.
		const bool bBusy = DialogueWidget || SilkwormWidget || DayEndWidget || TimingWidget || ReelingWidget || ActionWidget;
		TArray<FVillageGuideMarker> Markers;
		if (!bBusy)
		{
			Guide.ValueSort([](const TPair<double, FVillageGuideMarker>& A, const TPair<double, FVillageGuideMarker>& B) { return A.Key < B.Key; });
			for (const TPair<FName, TPair<double, FVillageGuideMarker>>& Entry : Guide)
			{
				Markers.Add(Entry.Value.Value);
			}
		}
		Hud->SetGuideMarkers(Markers);
	}
}

// ---------------------------------------------------------------- Debug

void UVillageStorySubsystem::DebugAdvanceDialogue()
{
	if (DialogueWidget)
	{
		DialogueWidget->DebugAdvance();
	}
}

void UVillageStorySubsystem::DebugChoose(int32 Index)
{
	if (DialogueWidget)
	{
		DialogueWidget->DebugChoose(Index);
	}
}

FString UVillageStorySubsystem::DebugDescribe() const
{
	TArray<FString> FlagNames;
	for (const FName& Flag : Flags)
	{
		FlagNames.Add(Flag.ToString());
	}
	FlagNames.Sort();
	TArray<FString> Quests;
	for (const FName& Quest : ActiveQuests)
	{
		Quests.Add(Quest.ToString());
	}
	TArray<FString> Counts;
	for (const TPair<FName, int32>& Pair : Progress)
	{
		Counts.Add(FString::Printf(TEXT("%s=%d"), *Pair.Key.ToString(), Pair.Value));
	}
	FString Screen = TEXT("none");
	if (DialogueWidget) Screen = TEXT("dialogue: ") + DialogueWidget->DebugDescribe();
	else if (SilkwormWidget) Screen = FString::Printf(TEXT("silkworm H%.0f Hu%.0f G%.0f T%.0f Hm%.0f C%.0f"), Silkworm.Health, Silkworm.Hunger, Silkworm.Growth, Silkworm.Temperature, Silkworm.Humidity, Silkworm.Cleanliness);
	else if (DayEndWidget) Screen = TEXT("day end");
	else if (TimingWidget) Screen = FString::Printf(TEXT("timing %d round %d score %d"), static_cast<int32>(MiniGame), RoundIndex, RoundScore);
	else if (ReelingWidget) Screen = TEXT("reeling");
	else if (ActionWidget) Screen = TEXT("action mini-game");
	return FString::Printf(TEXT("hud=%d quests=[%s] progress=[%s] leaves=%d cocoons=%d screen=%s flags=[%s]"),
		Hud ? 1 : 0, *FString::Join(Quests, TEXT(",")), *FString::Join(Counts, TEXT(",")), GetItemCount(LeafItem), GetItemCount(CocoonItem), *Screen, *FString::Join(FlagNames, TEXT(",")));
}

// ---------------------------------------------------------------- Helpers

void UVillageStorySubsystem::EnterUI(UUserWidget* Focus)
{
	if (APlayerController* PC = GetPC())
	{
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(Focus->TakeWidget());
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
		PC->SetIgnoreLookInput(true);
	}
}

void UVillageStorySubsystem::ExitUI()
{
	// Another panel is still open; keep focus on it.
	UUserWidget* const Panels[] = { DialogueWidget, SilkwormWidget, TimingWidget, ReelingWidget, ActionWidget, DayEndWidget };
	for (UUserWidget* Open : Panels)
	{
		if (Open)
		{
			EnterUI(Open);
			return;
		}
	}
	if (APlayerController* PC = GetPC())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
		PC->ResetIgnoreLookInput();
	}
}

void UVillageStorySubsystem::PlaySfx(const TCHAR* Path) const
{
	if (USoundBase* Sound = LoadObject<USoundBase>(nullptr, Path))
	{
		UGameplayStatics::PlaySound2D(GetWorld(), Sound);
	}
}

#undef LOCTEXT_NAMESPACE
