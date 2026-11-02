#include "CharacterGameFlowSubsystem.h"

#include "CharacterCustomizationSubsystem.h"
#include "CharacterTalkChat.h"
#include "CharacterTalkWidget.h"
#include "CustomizedCharacterStart.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableSkeletalComponent.h"
#include "GroomComponent.h"
#include "TalkFaceAnimInstance.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCharacterGameFlow, Log, All);

namespace CharacterGameFlow
{
	static const FString LobbyMapName = TEXT("Lobby");
	static const FString GameplayMapName = TEXT("Mountain");
	static const FName ConfirmButtonName = TEXT("ConfirmButton");
	static const FName TalkButtonName = TEXT("TalkButton");
	static const FName StartTag = TEXT("CustomizedCharacterStart");
	/** CO_Character's skin colour slider ("Màu da"): 0 light to 1 dark and warm. */
	static const FString SkinToneParameter = TEXT("Skin Tone");
	static constexpr float PollInterval = 0.1f;
	static constexpr float Timeout = 30.0f;
	static constexpr float ButtonGap = 12.0f;

	// Talk camera: in front of the face, shifted so the face sits left of the chat column.
	static constexpr float TalkCameraDistance = 120.0f;
	static constexpr float TalkCameraSideShift = 18.0f;
	static constexpr float TalkCameraFOV = 40.0f;
	static constexpr float TalkCameraBlendTime = 0.6f;
	static constexpr float MusicFadeTime = 0.8f;

	/** Music lives under /Game/Audio/Music (e.g. the lobby's MUS_MainMenu); other sounds keep playing while talking. */
	bool IsMusic(const UAudioComponent* Audio)
	{
		const USoundBase* Sound = Audio ? Audio->Sound.Get() : nullptr;
		return Sound && (Sound->GetPathName().Contains(TEXT("/Music/")) || Sound->GetName().StartsWith(TEXT("MUS_")));
	}

	/** The first text block inside a button (its label), or null. */
	UTextBlock* FindLabel(UButton* Button)
	{
		UTextBlock* Found = nullptr;
		UWidgetTree::ForWidgetAndChildren(Button, [&Found](UWidget* Child)
		{
			if (!Found)
			{
				Found = Cast<UTextBlock>(Child);
			}
		});
		return Found;
	}

	static FAutoConsoleCommandWithWorld OpenTalkCommand(
		TEXT("Talk.Open"),
		TEXT("Opens the lobby talk area, as the \"Xác nhận nói chuyện\" button does."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UCharacterGameFlowSubsystem* Flow = World ? World->GetSubsystem<UCharacterGameFlowSubsystem>() : nullptr)
			{
				Flow->OpenTalk();
			}
		}));
}

bool UCharacterGameFlowSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UCharacterGameFlowSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	ElapsedTime = 0.0f;

	if (InWorld.GetMapName().EndsWith(CharacterGameFlow::LobbyMapName))
	{
		InWorld.GetTimerManager().SetTimer(PollTimer, this, &UCharacterGameFlowSubsystem::TryBindLobbyConfirmButton, CharacterGameFlow::PollInterval, true);
		FaceElapsedTime = 0.0f;
		InWorld.GetTimerManager().SetTimer(FaceTimer, this, &UCharacterGameFlowSubsystem::TryAttachMetaHumanFace, CharacterGameFlow::PollInterval, true);
	}
	else if (InWorld.GetMapName().EndsWith(CharacterGameFlow::GameplayMapName))
	{
		// The lobby leaves the viewport in GameAndUI capture mode, which only rotates the camera while a mouse button is held.
		if (APlayerController* Controller = InWorld.GetFirstPlayerController())
		{
			Controller->SetInputMode(FInputModeGameOnly());
			Controller->SetShowMouseCursor(false);
		}
		InWorld.GetTimerManager().SetTimer(PollTimer, this, &UCharacterGameFlowSubsystem::TrySpawnCustomizedPlayer, CharacterGameFlow::PollInterval, true);
	}
}

void UCharacterGameFlowSubsystem::TryBindLobbyConfirmButton()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ElapsedTime += CharacterGameFlow::PollInterval;
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UUserWidget::StaticClass(), false);
	for (UUserWidget* Widget : Widgets)
	{
		if (!Widget || !Widget->GetClass()->GetName().StartsWith(TEXT("BasicMenu")))
		{
			continue;
		}

		if (UButton* ConfirmButton = Cast<UButton>(Widget->GetWidgetFromName(CharacterGameFlow::ConfirmButtonName)))
		{
			ConfirmButton->OnClicked.AddUniqueDynamic(this, &UCharacterGameFlowSubsystem::HandleCustomizationConfirmed);
			AddTalkButton(Widget, ConfirmButton);
			World->GetTimerManager().ClearTimer(PollTimer);
			UE_LOG(LogCharacterGameFlow, Log, TEXT("Mutable ConfirmButton connected to the LangToLua game flow."));
			return;
		}
	}

	if (ElapsedTime >= CharacterGameFlow::Timeout)
	{
		World->GetTimerManager().ClearTimer(PollTimer);
		UE_LOG(LogCharacterGameFlow, Warning, TEXT("Could not find the Mutable lobby ConfirmButton."));
	}
}

void UCharacterGameFlowSubsystem::TryAttachMetaHumanFace()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// The face is worn while customizing too, so the player shapes the body and clothes around the face they will talk to.
	FaceElapsedTime += CharacterGameFlow::PollInterval;
	const UCharacterTalkSettings& Settings = *GetDefault<UCharacterTalkSettings>();
	UCustomizableSkeletalComponent* LobbyComponent = FindLobbyCustomizableComponent();
	const bool bAttached = LobbyComponent && UTalkFaceAnimInstance::AttachTo(LobbyComponent->GetOwner());
	if (bAttached)
	{
		// The neck and hands are the Mutable body's; give them the face's skin colour (the player can still change it).
		if (UCustomizableObjectInstance* Instance = LobbyComponent->GetCustomizableObjectInstance(); Instance && Settings.MetaHumanBodySkinTone >= 0.f)
		{
			Instance->SetFloatParameterSelectedOption(CharacterGameFlow::SkinToneParameter, Settings.MetaHumanBodySkinTone);
			Instance->UpdateSkeletalMeshAsync();
		}
	}
	if (bAttached || FaceElapsedTime >= CharacterGameFlow::Timeout || Settings.MetaHumanFace.IsNull())
	{
		World->GetTimerManager().ClearTimer(FaceTimer);
	}
}

void UCharacterGameFlowSubsystem::HandleCustomizationConfirmed()
{
	if (bTravelRequested)
	{
		return;
	}

	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UCharacterCustomizationSubsystem* Store = GameInstance ? GameInstance->GetSubsystem<UCharacterCustomizationSubsystem>() : nullptr;
	UCustomizableSkeletalComponent* LobbyComponent = FindLobbyCustomizableComponent();
	UCustomizableObjectInstance* ChosenInstance = LobbyComponent ? LobbyComponent->GetCustomizableObjectInstance() : nullptr;

	if (Store && Store->CaptureCustomization(ChosenInstance))
	{
		UE_LOG(LogCharacterGameFlow, Log, TEXT("Character customization captured; opening /Game/Mountain."));
	}
	else
	{
		UE_LOG(LogCharacterGameFlow, Warning, TEXT("No Mutable character instance was found; opening gameplay with the default pawn."));
	}

	bTravelRequested = true;
	if (Store)
	{
		Store->ShowLoadingScreen();
	}
	World->GetTimerManager().SetTimer(TravelTimer, this, &UCharacterGameFlowSubsystem::TravelToGameplayMap, 0.1f, false);
}

void UCharacterGameFlowSubsystem::TravelToGameplayMap()
{
	if (APlayerController* Controller = UGameplayStatics::GetPlayerController(this, 0))
	{
		Controller->SetInputMode(FInputModeGameOnly());
		Controller->SetShowMouseCursor(false);
	}
	UGameplayStatics::OpenLevel(this, FName(*CharacterGameFlow::GameplayMapName));
}

UCustomizableSkeletalComponent* UCharacterGameFlowSubsystem::FindLobbyCustomizableComponent() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		TInlineComponentArray<UCustomizableSkeletalComponent*> Components(*ActorIt);
		for (UCustomizableSkeletalComponent* Component : Components)
		{
			if (Component && Component->GetCustomizableObjectInstance())
			{
				return Component;
			}
		}
	}
	return nullptr;
}

void UCharacterGameFlowSubsystem::AddTalkButton(UUserWidget* Menu, UButton* ConfirmButton)
{
	if (!Menu->WidgetTree || Menu->GetWidgetFromName(CharacterGameFlow::TalkButtonName))
	{
		return;
	}

	// The confirm button may be wrapped in a SizeBox or ScaleBox; the new button goes beside whatever holds its place in the panel.
	UWidget* Anchor = ConfirmButton;
	while (Anchor->GetParent() && (Anchor->GetParent()->IsA<USizeBox>() || Anchor->GetParent()->IsA<UScaleBox>()))
	{
		Anchor = Anchor->GetParent();
	}
	UPanelWidget* Panel = Anchor->GetParent();
	if (!Panel)
	{
		UE_LOG(LogCharacterGameFlow, Warning, TEXT("ConfirmButton has no parent panel; the talk button was not added."));
		return;
	}

	UWidgetTree* Tree = Menu->WidgetTree;
	UButton* TalkButton = Tree->ConstructWidget<UButton>(UButton::StaticClass(), CharacterGameFlow::TalkButtonName);
	TalkButton->SetStyle(ConfirmButton->GetStyle());
	TalkButton->SetColorAndOpacity(ConfirmButton->GetColorAndOpacity());
	TalkButton->SetBackgroundColor(ConfirmButton->GetBackgroundColor());

	UTextBlock* Label = Tree->ConstructWidget<UTextBlock>();
	Label->SetText(FText::FromString(TEXT("Xác nhận nói chuyện")));
	Label->SetJustification(ETextJustify::Center);
	TalkButton->AddChild(Label);
	if (UTextBlock* SourceLabel = CharacterGameFlow::FindLabel(ConfirmButton))
	{
		Label->SetFont(SourceLabel->GetFont());
		Label->SetColorAndOpacity(SourceLabel->GetColorAndOpacity());
	}
	UButtonSlot* SourceButtonSlot = ConfirmButton->GetContent() ? Cast<UButtonSlot>(ConfirmButton->GetContent()->Slot) : nullptr;
	if (UButtonSlot* LabelSlot = Cast<UButtonSlot>(Label->Slot); LabelSlot && SourceButtonSlot)
	{
		LabelSlot->SetPadding(SourceButtonSlot->GetPadding());
		LabelSlot->SetHorizontalAlignment(SourceButtonSlot->GetHorizontalAlignment());
		LabelSlot->SetVerticalAlignment(SourceButtonSlot->GetVerticalAlignment());
	}

	UWidget* NewAnchor = TalkButton;
	if (const USizeBox* SourceBox = Cast<USizeBox>(Anchor))
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>();
		if (SourceBox->GetWidthOverride() > 0.0f)
		{
			Box->SetWidthOverride(SourceBox->GetWidthOverride());
		}
		if (SourceBox->GetHeightOverride() > 0.0f)
		{
			Box->SetHeightOverride(SourceBox->GetHeightOverride());
		}
		Box->AddChild(TalkButton);
		NewAnchor = Box;
	}

	// Canvas: same layout, one button width to the left (the lobby's control hints sit just above the confirm button).
	if (const UCanvasPanelSlot* SourceSlot = Cast<UCanvasPanelSlot>(Anchor->Slot))
	{
		UCanvasPanelSlot* NewSlot = CastChecked<UCanvasPanel>(Panel)->AddChildToCanvas(NewAnchor);
		NewSlot->SetLayout(SourceSlot->GetLayout());
		NewSlot->SetAutoSize(SourceSlot->GetAutoSize());
		NewSlot->SetZOrder(SourceSlot->GetZOrder());
		float Width = SourceSlot->GetAutoSize() ? Anchor->GetDesiredSize().X : SourceSlot->GetSize().X;
		if (Width <= 0.0f)
		{
			Width = 220.0f;
		}
		FVector2D Position = SourceSlot->GetPosition();
		Position.X -= Width + CharacterGameFlow::ButtonGap;
		NewSlot->SetPosition(Position);
	}
	else if (const UVerticalBoxSlot* SourceVSlot = Cast<UVerticalBoxSlot>(Anchor->Slot))
	{
		UVerticalBoxSlot* NewSlot = CastChecked<UVerticalBox>(Panel)->AddChildToVerticalBox(NewAnchor);
		NewSlot->SetPadding(SourceVSlot->GetPadding());
		NewSlot->SetSize(SourceVSlot->GetSize());
		NewSlot->SetHorizontalAlignment(SourceVSlot->GetHorizontalAlignment());
		NewSlot->SetVerticalAlignment(SourceVSlot->GetVerticalAlignment());
	}
	else if (const UHorizontalBoxSlot* SourceHSlot = Cast<UHorizontalBoxSlot>(Anchor->Slot))
	{
		UHorizontalBoxSlot* NewSlot = CastChecked<UHorizontalBox>(Panel)->AddChildToHorizontalBox(NewAnchor);
		NewSlot->SetPadding(SourceHSlot->GetPadding());
		NewSlot->SetSize(SourceHSlot->GetSize());
		NewSlot->SetHorizontalAlignment(SourceHSlot->GetHorizontalAlignment());
		NewSlot->SetVerticalAlignment(SourceHSlot->GetVerticalAlignment());
	}
	else
	{
		Panel->AddChild(NewAnchor);
	}

	TalkButton->OnClicked.AddUniqueDynamic(this, &UCharacterGameFlowSubsystem::HandleTalkConfirmed);
	UE_LOG(LogCharacterGameFlow, Log, TEXT("Talk button added next to ConfirmButton (parent %s)."), *Panel->GetClass()->GetName());
}

void UCharacterGameFlowSubsystem::HandleTalkConfirmed()
{
	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (!Controller || TalkWidget || bTravelRequested)
	{
		return;
	}

	// Keep the appearance as it is now, so it survives a later "Xác nhận tạo hình" or map change too.
	UCustomizableSkeletalComponent* LobbyComponent = FindLobbyCustomizableComponent();
	UCharacterCustomizationSubsystem* Store = World->GetGameInstance()->GetSubsystem<UCharacterCustomizationSubsystem>();
	if (Store && LobbyComponent)
	{
		Store->CaptureCustomization(LobbyComponent->GetCustomizableObjectInstance());
	}

	HiddenLobbyWidgets.Reset();
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Widgets, UUserWidget::StaticClass(), true);
	for (UUserWidget* Widget : Widgets)
	{
		if (Widget && Widget->IsInViewport() && Widget->GetVisibility() != ESlateVisibility::Collapsed && Widget->GetVisibility() != ESlateVisibility::Hidden)
		{
			HiddenLobbyWidgets.Emplace(Widget, Widget->GetVisibility());
			Widget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// The menu music would talk over the character, so it fades out until the player goes back.
	MutedMusic.Reset();
	for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
	{
		TInlineComponentArray<UAudioComponent*> AudioComponents(*ActorIt);
		for (UAudioComponent* Audio : AudioComponents)
		{
			if (Audio && Audio->IsPlaying() && CharacterGameFlow::IsMusic(Audio))
			{
				Audio->FadeOut(CharacterGameFlow::MusicFadeTime, 0.0f);
				MutedMusic.Add(Audio);
			}
		}
	}

	// Frame the face from the side the lobby camera is looking from.
	AActor* Character = LobbyComponent ? LobbyComponent->GetOwner() : nullptr;
	if (Character)
	{
		FVector Origin, Extent;
		Character->GetActorBounds(true, Origin, Extent);
		FVector Head(Origin.X, Origin.Y, Origin.Z + Extent.Z - 15.0f);
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(Character);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (Mesh && Mesh->GetBoneIndex(TEXT("head")) != INDEX_NONE)
			{
				Head = Mesh->GetBoneLocation(TEXT("head"));
				break;
			}
		}

		FVector ToViewer = Controller->PlayerCameraManager ? (Controller->PlayerCameraManager->GetCameraLocation() - Head).GetSafeNormal2D() : FVector::ZeroVector;
		if (ToViewer.IsNearlyZero())
		{
			ToViewer = Character->GetActorForwardVector().GetSafeNormal2D();
		}
		const FRotator LookRotation = (-ToViewer).Rotation();
		const FVector Right = FRotationMatrix(LookRotation).GetUnitAxis(EAxis::Y);
		const FVector CameraLocation = Head + ToViewer * CharacterGameFlow::TalkCameraDistance + Right * CharacterGameFlow::TalkCameraSideShift;

		if (!TalkCamera)
		{
			TalkCamera = World->SpawnActor<ACameraActor>(CameraLocation, LookRotation);
		}
		if (TalkCamera)
		{
			TalkCamera->SetActorLocationAndRotation(CameraLocation, LookRotation);
			TalkCamera->GetCameraComponent()->SetFieldOfView(CharacterGameFlow::TalkCameraFOV);
			TalkCamera->GetCameraComponent()->SetConstraintAspectRatio(false);
			ViewTargetBeforeTalk = Controller->GetViewTarget();
			Controller->SetViewTargetWithBlend(TalkCamera, CharacterGameFlow::TalkCameraBlendTime, VTBlend_Cubic);
		}
	}
	else
	{
		UE_LOG(LogCharacterGameFlow, Warning, TEXT("No Mutable character found in the lobby; opening the talk area without a close-up."));
	}

	TalkWidget = CreateWidget<UCharacterTalkWidget>(Controller, UCharacterTalkWidget::StaticClass());
	TalkWidget->OnBack = [this]() { EndTalk(); };
	TalkWidget->Speaker = Character;
	TalkWidget->AddToViewport(10);

	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(TalkWidget->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Controller->SetInputMode(InputMode);
	Controller->SetShowMouseCursor(true);
	TalkWidget->FocusInput();
	UE_LOG(LogCharacterGameFlow, Log, TEXT("Talk area opened with the customized character."));
}

void UCharacterGameFlowSubsystem::EndTalk()
{
	if (TalkWidget)
	{
		TalkWidget->RemoveFromParent();
		TalkWidget = nullptr;
	}

	for (const TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>& Hidden : HiddenLobbyWidgets)
	{
		if (UUserWidget* Widget = Hidden.Key.Get())
		{
			Widget->SetVisibility(Hidden.Value);
		}
	}
	HiddenLobbyWidgets.Reset();

	for (const TWeakObjectPtr<UAudioComponent>& Music : MutedMusic)
	{
		if (UAudioComponent* Audio = Music.Get())
		{
			Audio->FadeIn(CharacterGameFlow::MusicFadeTime);
		}
	}
	MutedMusic.Reset();

	UWorld* World = GetWorld();
	if (APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr)
	{
		AActor* PreviousTarget = ViewTargetBeforeTalk.Get();
		if (!PreviousTarget)
		{
			PreviousTarget = Controller->GetPawn();
		}
		if (PreviousTarget)
		{
			Controller->SetViewTargetWithBlend(PreviousTarget, CharacterGameFlow::TalkCameraBlendTime, VTBlend_Cubic);
		}
		Controller->SetInputMode(FInputModeGameAndUI());
		Controller->SetShowMouseCursor(true);
	}
}

ACustomizedCharacterStart* UCharacterGameFlowSubsystem::FindOrCreateCharacterStart()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ACustomizedCharacterStart> It(World); It; ++It)
	{
		return *It;
	}

	FTransform SpawnTransform(FRotator::ZeroRotator, FVector(0.0, 0.0, 200.0));
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		SpawnTransform = It->GetActorTransform();
		break;
	}

	ACustomizedCharacterStart* NewStart = World->SpawnActor<ACustomizedCharacterStart>(ACustomizedCharacterStart::StaticClass(), SpawnTransform);
	if (NewStart)
	{
		UE_LOG(LogCharacterGameFlow, Warning, TEXT("No CustomizedCharacterStart was placed; using a temporary marker at %s."), *SpawnTransform.GetLocation().ToString());
	}
	return NewStart;
}

void UCharacterGameFlowSubsystem::TrySpawnCustomizedPlayer()
{
	UWorld* World = GetWorld();
	if (!World || bCharacterSpawned)
	{
		return;
	}

	ElapsedTime += CharacterGameFlow::PollInterval;
	APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0);
	APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	ACustomizedCharacterStart* Start = FindOrCreateCharacterStart();

	if (!Pawn || !Start)
	{
		if (ElapsedTime >= CharacterGameFlow::Timeout)
		{
			World->GetTimerManager().ClearTimer(PollTimer);
			UE_LOG(LogCharacterGameFlow, Warning, TEXT("Customized player spawn timed out (Pawn=%s, Start=%s)."), Pawn ? TEXT("yes") : TEXT("no"), Start ? TEXT("yes") : TEXT("no"));
		}
		return;
	}

	UCharacterCustomizationSubsystem* Store = World->GetGameInstance()->GetSubsystem<UCharacterCustomizationSubsystem>();
	if (!Store || !Store->HasCustomization())
	{
		World->GetTimerManager().ClearTimer(PollTimer);
		return;
	}

	UClass* VisualClass = Start->VisualActorClass.LoadSynchronous();
	if (!VisualClass)
	{
		World->GetTimerManager().ClearTimer(PollTimer);
		UE_LOG(LogCharacterGameFlow, Error, TEXT("Could not load the Mutable CharacterActor class."));
		return;
	}

	Pawn->SetActorLocationAndRotation(Start->GetActorLocation(), Start->GetActorRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	AActor* Visual = World->SpawnActor<AActor>(VisualClass, Start->GetActorTransform());
	if (!Visual)
	{
		return;
	}

	UCustomizableObjectInstance* Instance = Store->CloneCustomization(Visual);
	TInlineComponentArray<UCustomizableSkeletalComponent*> MutableComponents(Visual);
	for (UCustomizableSkeletalComponent* Component : MutableComponents)
	{
		if (Component)
		{
			Component->SetCustomizableObjectInstance(Instance);
		}
	}
	if (Instance)
	{
		Instance->SetBuildParameterRelevancy(true);
		Instance->UpdateSkeletalMeshAsync(true, true);
	}

	Visual->SetActorEnableCollision(false);
	if (Start->bAttachVisualToPlayerPawn)
	{
		USceneComponent* AttachParent = Pawn->GetRootComponent();
		if (ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			if (USkeletalMeshComponent* PawnMesh = Character->GetMesh())
			{
				AttachParent = PawnMesh;
				AnimationSourceMesh = PawnMesh;
				PawnMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			}
		}
		Visual->AttachToComponent(AttachParent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Visual->SetActorRelativeLocation(Start->VisualRelativeLocation);
		Visual->SetActorRelativeRotation(Start->VisualRelativeRotation);
	}

	SpawnedVisual = Visual;
	if (Instance)
	{
		Instance->UpdatedDelegate.AddUniqueDynamic(this, &UCharacterGameFlowSubsystem::HandleCustomizationUpdated);
	}
	ApplyDefaultCharacterAnimations();

	if (Start->bHideOriginalPawnMesh)
	{
		if (ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			if (USkeletalMeshComponent* Mesh = Character->GetMesh())
			{
				Mesh->SetVisibility(false, true);
			}
		}
	}

	bCharacterSpawned = true;
	World->GetTimerManager().ClearTimer(PollTimer);
	UE_LOG(LogCharacterGameFlow, Log, TEXT("Customized character spawned at '%s'."), *Start->GetName());
}

void UCharacterGameFlowSubsystem::HandleCustomizationUpdated(UCustomizableObjectInstance* UpdatedInstance)
{
	ApplyDefaultCharacterAnimations();
}

void UCharacterGameFlowSubsystem::ApplyDefaultCharacterAnimations()
{
	AActor* Visual = SpawnedVisual.Get();
	USkeletalMeshComponent* SourceMesh = AnimationSourceMesh.Get();
	if (!Visual || !SourceMesh)
	{
		return;
	}

	TInlineComponentArray<USkeletalMeshComponent*> VisualMeshes(Visual);
	for (USkeletalMeshComponent* VisualMesh : VisualMeshes)
	{
		if (VisualMesh && VisualMesh->GetSkeletalMeshAsset())
		{
			VisualMesh->SetLeaderPoseComponent(SourceMesh, true, false);
			VisualMesh->SetVisibility(true, true);
		}
	}

	TInlineComponentArray<UGroomComponent*> Grooms(Visual);
	for (UGroomComponent* Groom : Grooms)
	{
		if (Groom)
		{
			Groom->SetEnableSimulation(false);
			Groom->ResetSimulation();
		}
	}

	UE_LOG(LogCharacterGameFlow, Log, TEXT("Applied the default player animation pose to %d Mutable mesh components."), VisualMeshes.Num());
}
