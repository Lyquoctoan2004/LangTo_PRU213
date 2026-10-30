// Copyright Epic Games, Inc. All Rights Reserved.

#include "LobbyAutoCustomizeSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Button.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogLobbyAutoCustomize, Log, All);

// Off by default: opening the customize screen makes Mutable regenerate the
// character, and the grooms lose their binding (strands fall back to the rest
// pose). Set lobby.AutoCustomize 1 to go straight into customization again.
static TAutoConsoleVariable<int32> CVarLobbyAutoCustomize(
	TEXT("lobby.AutoCustomize"),
	0,
	TEXT("1: skip the Lobby main menu and open character customization automatically."),
	ECVF_Default);

namespace LobbyAutoCustomize
{
	static const FString LobbyMapName = TEXT("Lobby");
	static const FString MainMenuClassPrefix = TEXT("BasicMenu");
	static const FName CustomizeButtonName = TEXT("EditCharacterButton");
	static constexpr float PollInterval = 0.1f;
	static constexpr float Timeout = 10.0f;
}

bool ULobbyAutoCustomizeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void ULobbyAutoCustomizeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (CVarLobbyAutoCustomize.GetValueOnGameThread() == 0)
	{
		UE_LOG(LogLobbyAutoCustomize, Log, TEXT("Auto-customize disabled (lobby.AutoCustomize 0)."));
		return;
	}

	if (InWorld.GetMapName().EndsWith(LobbyAutoCustomize::LobbyMapName))
	{
		UE_LOG(LogLobbyAutoCustomize, Log, TEXT("Lobby started, waiting for the main menu..."));
		ElapsedTime = 0.0f;
		InWorld.GetTimerManager().SetTimer(PollTimer, this, &ULobbyAutoCustomizeSubsystem::TryEnterCustomize, LobbyAutoCustomize::PollInterval, true);	}
}

UUserWidget* ULobbyAutoCustomizeSubsystem::FindMainMenu() const
{
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Widgets, UUserWidget::StaticClass(), false);
	for (UUserWidget* Widget : Widgets)
	{
		if (Widget && Widget->GetClass()->GetName().StartsWith(LobbyAutoCustomize::MainMenuClassPrefix) && Widget->IsInViewport())
		{
			return Widget;
		}
	}
	return nullptr;
}

void ULobbyAutoCustomizeSubsystem::TryEnterCustomize()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ElapsedTime += LobbyAutoCustomize::PollInterval;

	UUserWidget* MainMenu = FindMainMenu();
	if (!MainMenu)
	{
		if (ElapsedTime >= LobbyAutoCustomize::Timeout)
		{
			UE_LOG(LogLobbyAutoCustomize, Warning, TEXT("Main menu widget never appeared, giving up."));
			World->GetTimerManager().ClearTimer(PollTimer);
		}
		return;
	}

	World->GetTimerManager().ClearTimer(PollTimer);

	UButton* CustomizeButton = Cast<UButton>(MainMenu->GetWidgetFromName(LobbyAutoCustomize::CustomizeButtonName));
	if (!CustomizeButton)
	{
		UE_LOG(LogLobbyAutoCustomize, Warning, TEXT("Button '%s' not found in '%s'."), *LobbyAutoCustomize::CustomizeButtonName.ToString(), *MainMenu->GetClass()->GetName());
		return;
	}

	UE_LOG(LogLobbyAutoCustomize, Log, TEXT("Main menu '%s' found, clicking '%s'."), *MainMenu->GetClass()->GetName(), *LobbyAutoCustomize::CustomizeButtonName.ToString());
	CustomizeButton->OnClicked.Broadcast();
}
