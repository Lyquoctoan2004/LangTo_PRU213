#include "CharacterCustomizationSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "MoviePlayer.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace CharacterLoadingScreen
{
	static TSharedRef<SWidget> BuildWidget()
	{
		return SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.BorderBackgroundColor(FLinearColor(0.008f, 0.012f, 0.018f, 1.0f))
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 0.0f, 0.0f, 24.0f)
				[
					SNew(SThrobber)
					.NumPieces(5)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("ĐANG TẢI THẾ GIỚI")))
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 30))
					.ColorAndOpacity(FLinearColor::White)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 12.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Đang chuẩn bị nhân vật của bạn...")))
					.Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 17))
					.ColorAndOpacity(FLinearColor(0.72f, 0.78f, 0.86f, 1.0f))
				]
			];
	}
}

void UCharacterCustomizationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UCharacterCustomizationSubsystem::HandlePostLoadMap);
}

void UCharacterCustomizationSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	HideLoadingScreen();
	Super::Deinitialize();
}

bool UCharacterCustomizationSubsystem::CaptureCustomization(UCustomizableObjectInstance* SourceInstance)
{
	SavedInstance = SourceInstance ? SourceInstance->CloneStatic(this) : nullptr;
	return SavedInstance != nullptr;
}

UCustomizableObjectInstance* UCharacterCustomizationSubsystem::CloneCustomization(UObject* Outer) const
{
	return SavedInstance && Outer ? SavedInstance->CloneStatic(Outer) : nullptr;
}

void UCharacterCustomizationSubsystem::ShowLoadingScreen()
{
	if (GEngine && GEngine->GameViewport && !LoadingWidget.IsValid())
	{
		LoadingWidget = CharacterLoadingScreen::BuildWidget();
		GEngine->GameViewport->AddViewportWidgetContent(LoadingWidget.ToSharedRef(), 10000);
	}

	if (IsMoviePlayerEnabled())
	{
		FLoadingScreenAttributes Attributes;
		Attributes.bAutoCompleteWhenLoadingCompletes = true;
		Attributes.bWaitForManualStop = false;
		Attributes.bMoviesAreSkippable = false;
		Attributes.MinimumLoadingScreenDisplayTime = 1.0f;
		Attributes.WidgetLoadingScreen = CharacterLoadingScreen::BuildWidget();
		GetMoviePlayer()->SetupLoadingScreen(Attributes);
		GetMoviePlayer()->PlayMovie();
	}
}

void UCharacterCustomizationSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadingWidget.IsValid())
	{
		return;
	}

	if (LoadedWorld)
	{
		LoadedWorld->GetTimerManager().SetTimer(HideLoadingTimer, this, &UCharacterCustomizationSubsystem::HideLoadingScreen, 0.75f, false);
	}
	else
	{
		HideLoadingScreen();
	}
}

void UCharacterCustomizationSubsystem::HideLoadingScreen()
{
	if (GEngine && GEngine->GameViewport && LoadingWidget.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(LoadingWidget.ToSharedRef());
	}
	LoadingWidget.Reset();

	if (IsMoviePlayerEnabled() && GetMoviePlayer()->IsMovieCurrentlyPlaying())
	{
		GetMoviePlayer()->StopMovie();
	}
}
