#include "CharacterTalkWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CharacterTalkChat.h"
#include "CharacterTalkVoice.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "CharacterTalkWidget"

namespace CharacterTalkUi
{
	const TCHAR* RegularFont = TEXT("/Game/Fonts/Montserrat/Montserrat-Regular_Font.Montserrat-Regular_Font");

	const FLinearColor Ink(0.97f, 0.93f, 0.84f);
	const FLinearColor Gold(0.96f, 0.76f, 0.36f);
	const FLinearColor Muted(0.75f, 0.70f, 0.62f);
	const FLinearColor Warn(1.0f, 0.55f, 0.40f);
	const FLinearColor PanelColor(0.07f, 0.04f, 0.03f, 0.82f);
	const FLinearColor PlayerBubble(0.35f, 0.18f, 0.10f, 0.92f);
	const FLinearColor CharacterBubble(0.14f, 0.09f, 0.06f, 0.92f);
	// NormalButton matches UVillageWidgetBase::MakeButton.
	const FLinearColor NormalButton(0.35f, 0.18f, 0.10f, 0.95f);
	const FLinearColor SelectedButton(0.85f, 0.55f, 0.18f, 1.f);

	/** The scenario and personality picked in the talk area, kept while the game runs so reopening the talk area keeps them. */
	int32 ChosenScenario = INDEX_NONE;
	int32 ChosenPersonality = INDEX_NONE;

	FSlateChildSize FillSize()
	{
		return FSlateChildSize(ESlateSizeRule::Fill);
	}

	/** A click shorter than this on the mic button toggles listening instead of push-to-talk. */
	static constexpr double MicClickSeconds = 0.35;

	/**
	 * Push-to-talk key, seen before any widget so it works while the chat box has focus. The key's own events are consumed,
	 * which is why it must be one that types nothing.
	 */
	class FPushToTalkProcessor : public IInputProcessor
	{
	public:
		FPushToTalkProcessor(UCharacterTalkWidget* InWidget, const FKey& InKey) : Widget(InWidget), Key(InKey) {}

		virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override
		{
			if (Event.GetKey() != Key)
			{
				return false;
			}
			UCharacterTalkWidget* Target = Widget.Get();
			if (Target && !Event.IsRepeat())
			{
				Target->StartListening();
			}
			return true;
		}

		virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& Event) override
		{
			if (Event.GetKey() != Key)
			{
				return false;
			}
			if (UCharacterTalkWidget* Target = Widget.Get())
			{
				Target->StopListening();
			}
			return true;
		}

		virtual const TCHAR* GetDebugName() const override { return TEXT("CharacterTalkPushToTalk"); }

	private:
		TWeakObjectPtr<UCharacterTalkWidget> Widget;
		FKey Key;
	};

	/** The open talk area, for the Talk.Face console command. */
	TWeakObjectPtr<UCharacterTalkWidget> ActiveWidget;

	static FAutoConsoleCommand PreviewFaceCommand(
		TEXT("Talk.Face"),
		TEXT("Talk.Face <personality index> [emotion]: shows a personality's face (and an emotion tag such as vui or ngạc nhiên) in the open talk area."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (UCharacterTalkWidget* Widget = ActiveWidget.Get())
			{
				const TArray<FString> EmotionWords(Args.GetData() + FMath::Min(1, Args.Num()), FMath::Max(0, Args.Num() - 1));
				Widget->PreviewFace(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0, FString::Join(EmotionWords, TEXT(" ")));
			}
		}));
}

void UCharacterTalkWidget::PreviewFace(int32 Personality, const FString& Emotion)
{
	const TArray<FCharacterTalkPersonality>& List = UCharacterTalkChat::Personalities();
	if (!List.IsValidIndex(Personality))
	{
		return;
	}
	Voice->SetMood(List[Personality].Mood, List[Personality].MoodIntensity);
	Voice->SetFaceStyle(List[Personality].Face);
	if (!Emotion.IsEmpty())
	{
		Voice->SetEmotion(Emotion);
	}
}

FKey UCharacterTalkWidget::PushToTalkKey()
{
	const FKey Key(FName(*GetDefault<UCharacterTalkSettings>()->PushToTalkKey));
	return Key.IsValid() ? Key : EKeys::Tab;
}

void UCharacterTalkWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (FSlateApplication::IsInitialized() && !PushToTalk.IsValid())
	{
		PushToTalk = MakeShared<CharacterTalkUi::FPushToTalkProcessor>(this, PushToTalkKey());
		FSlateApplication::Get().RegisterInputPreProcessor(PushToTalk);
	}
}

void UCharacterTalkWidget::BuildTree(UCanvasPanel* Root)
{
	Chat = NewObject<UCharacterTalkChat>(this);
	Voice = NewObject<UCharacterTalkVoice>(this);
	CharacterTalkUi::ActiveWidget = this;

	// Chat column on the right, full height, leaving the character visible on the left like RMHLipSync's W_Main.
	UBorder* Panel = MakePanel(CharacterTalkUi::PanelColor);
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Column);

	FEditableTextBoxStyle InputStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
	InputStyle.SetFont(FSlateFontInfo(LoadObject<UObject>(nullptr, CharacterTalkUi::RegularFont), 18));
	InputStyle.SetPadding(FMargin(10.f, 8.f));

	// Title with the API key button next to it.
	UHorizontalBox* TitleRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	UHorizontalBoxSlot* TitleSlot = TitleRow->AddChildToHorizontalBox(MakeText(LOCTEXT("Title", "Trò chuyện"), 24, CharacterTalkUi::Gold, true));
	TitleSlot->SetSize(CharacterTalkUi::FillSize());
	TitleSlot->SetVerticalAlignment(VAlign_Center);
	TitleRow->AddChildToHorizontalBox(MakeButton(LOCTEXT("KeyButton", "API key"), [this]() { ToggleKeyPanel(); }, 14))->SetVerticalAlignment(VAlign_Center);
	Column->AddChildToVerticalBox(TitleRow)->SetPadding(FMargin(0, 0, 0, 6));

	// Key panel, hidden until the API key button is pressed. The key is masked while typing.
	KeyPanel = WidgetTree->ConstructWidget<UVerticalBox>();
	KeyStatus = MakeText(FText::GetEmpty(), 14, CharacterTalkUi::Muted);
	KeyPanel->AddChildToVerticalBox(KeyStatus)->SetPadding(FMargin(0, 0, 0, 4));
	UHorizontalBox* KeyRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	KeyInput = WidgetTree->ConstructWidget<UEditableTextBox>();
	KeyInput->SetWidgetStyle(InputStyle);
	KeyInput->SetIsPassword(true);
	KeyInput->SetHintText(LOCTEXT("KeyHint", "Dán API key Gemini rồi nhấn Enter"));
	KeyInput->OnTextCommitted.AddDynamic(this, &UCharacterTalkWidget::HandleKeyCommitted);
	UHorizontalBoxSlot* KeyInputSlot = KeyRow->AddChildToHorizontalBox(KeyInput);
	KeyInputSlot->SetSize(CharacterTalkUi::FillSize());
	KeyInputSlot->SetVerticalAlignment(VAlign_Center);
	KeyRow->AddChildToHorizontalBox(MakeButton(LOCTEXT("KeySave", "Lưu"), [this]() { SaveKey(); }, 14))->SetPadding(FMargin(8, 0, 0, 0));
	KeyRow->AddChildToHorizontalBox(MakeButton(LOCTEXT("KeyClear", "Xóa"), [this]() { ClearKey(); }, 14))->SetPadding(FMargin(8, 0, 0, 0));
	KeyPanel->AddChildToVerticalBox(KeyRow);
	KeyPanel->SetVisibility(ESlateVisibility::Collapsed);
	Column->AddChildToVerticalBox(KeyPanel)->SetPadding(FMargin(0, 0, 0, 8));

	// Pickers: the scenario sets who the character is, the personality how it talks. The chosen button is highlighted.
	auto AddPicker = [this, Column](const FText& Label, const TArray<FString>& Names, TArray<TObjectPtr<UButton>>& OutButtons, TFunction<void(int32)> OnPick)
	{
		Column->AddChildToVerticalBox(MakeText(Label, 14, CharacterTalkUi::Muted))->SetPadding(FMargin(0, 0, 0, 4));
		UWrapBox* Row = WidgetTree->ConstructWidget<UWrapBox>();
		Row->SetInnerSlotPadding(FVector2D(6.f, 6.f));
		for (int32 Index = 0; Index < Names.Num(); ++Index)
		{
			UButton* Button = MakeButton(FText::FromString(Names[Index]), [OnPick, Index]() { OnPick(Index); }, 14);
			Row->AddChildToWrapBox(Button);
			OutButtons.Add(Button);
		}
		Column->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 0, 0, 8));
	};
	TArray<FString> ScenarioNames, PersonalityNames;
	for (const FCharacterTalkScenario& Scenario : UCharacterTalkChat::Scenarios())
	{
		ScenarioNames.Add(Scenario.Name);
	}
	for (const FCharacterTalkPersonality& Personality : UCharacterTalkChat::Personalities())
	{
		PersonalityNames.Add(Personality.Name);
	}
	AddPicker(LOCTEXT("ScenarioLabel", "Kịch bản"), ScenarioNames, ScenarioButtons, [this](int32 Index) { SelectScenario(Index); });
	AddPicker(LOCTEXT("PersonalityLabel", "Tính cách"), PersonalityNames, PersonalityButtons, [this](int32 Index) { SelectPersonality(Index); });
	// The picks above only take effect here, so trying out a combination does not restart the conversation each click.
	ConfirmChoicesButton = MakeButton(LOCTEXT("ConfirmChoices", "Xác nhận kịch bản & tính cách"), [this]() { ConfirmChoices(); }, 14);
	UVerticalBoxSlot* ConfirmSlot = Column->AddChildToVerticalBox(ConfirmChoicesButton);
	ConfirmSlot->SetHorizontalAlignment(HAlign_Left);
	ConfirmSlot->SetPadding(FMargin(0, 0, 0, 10));

	MessageList = WidgetTree->ConstructWidget<UScrollBox>();
	UVerticalBoxSlot* ListSlot = Column->AddChildToVerticalBox(MessageList);
	ListSlot->SetSize(CharacterTalkUi::FillSize());
	ListSlot->SetPadding(FMargin(0, 0, 0, 8));

	StatusText = MakeText(FText::GetEmpty(), 15, CharacterTalkUi::Muted);
	Column->AddChildToVerticalBox(StatusText)->SetPadding(FMargin(0, 0, 0, 6));

	UHorizontalBox* InputRow = WidgetTree->ConstructWidget<UHorizontalBox>();
	Input = WidgetTree->ConstructWidget<UEditableTextBox>();
	Input->SetWidgetStyle(InputStyle);
	Input->SetHintText(FText::Format(LOCTEXT("Hint", "Nhập rồi nhấn Enter, hoặc giữ {0} / giữ nút Nói để nói"), PushToTalkKey().GetDisplayName()));
	Input->OnTextCommitted.AddDynamic(this, &UCharacterTalkWidget::HandleCommitted);
	UHorizontalBoxSlot* InputSlot = InputRow->AddChildToHorizontalBox(Input);
	InputSlot->SetSize(CharacterTalkUi::FillSize());
	InputSlot->SetVerticalAlignment(VAlign_Center);
	UHorizontalBoxSlot* SendSlot = InputRow->AddChildToHorizontalBox(MakeButton(LOCTEXT("Send", "Gửi"), [this]() { SendInput(); }));
	SendSlot->SetPadding(FMargin(8, 0, 0, 0));
	SendSlot->SetVerticalAlignment(VAlign_Center);
	// Hold to talk and release to send; a quick click instead keeps listening until the next click.
	UButton* MicButton = MakeButton(LOCTEXT("Mic", "Nói"), []() {});
	MicButton->OnPressed.AddDynamic(this, &UCharacterTalkWidget::HandleMicPressed);
	MicButton->OnReleased.AddDynamic(this, &UCharacterTalkWidget::HandleMicReleased);
	MicLabel = Cast<UTextBlock>(MicButton->GetContent());
	UHorizontalBoxSlot* MicSlot = InputRow->AddChildToHorizontalBox(MicButton);
	MicSlot->SetPadding(FMargin(8, 0, 0, 0));
	MicSlot->SetVerticalAlignment(VAlign_Center);
	Column->AddChildToVerticalBox(InputRow);

	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
	PanelSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 1.f));
	PanelSlot->SetAlignment(FVector2D(1.f, 0.f));
	PanelSlot->SetOffsets(FMargin(-32.f, 32.f, 560.f, 32.f));

	UButton* Back = MakeButton(LOCTEXT("Back", "Quay lại tạo hình"), [this]() { GoBack(); });
	UCanvasPanelSlot* BackSlot = Root->AddChildToCanvas(Back);
	BackSlot->SetAnchors(FAnchors(0.f, 0.f));
	BackSlot->SetPosition(FVector2D(32.f, 32.f));
	BackSlot->SetAutoSize(true);

	// Start with what was picked last time in this session, or the configured defaults.
	const UCharacterTalkSettings& Settings = *GetDefault<UCharacterTalkSettings>();
	if (CharacterTalkUi::ChosenScenario == INDEX_NONE)
	{
		CharacterTalkUi::ChosenScenario = FMath::Max(0, ScenarioNames.IndexOfByKey(Settings.DefaultScenario));
	}
	if (CharacterTalkUi::ChosenPersonality == INDEX_NONE)
	{
		CharacterTalkUi::ChosenPersonality = FMath::Max(0, PersonalityNames.IndexOfByKey(Settings.DefaultPersonality));
	}
	ApplyChoices(CharacterTalkUi::ChosenScenario, CharacterTalkUi::ChosenPersonality);
	SetIsFocusable(true);
	// The face comes alive (blinks, glances, mood) as soon as the talk area opens, not only while the character speaks.
	Voice->SetFaceTarget(Speaker.Get());
	StartConversation();
}

void UCharacterTalkWidget::ApplyChoices(int32 Scenario, int32 Personality)
{
	Chat->SetScenario(Scenario);
	Chat->SetPersonality(Personality);
	// One voice for every personality unless bVoicePerPersonality (an empty name uses GeminiVoice).
	Voice->VoiceName = GetDefault<UCharacterTalkSettings>()->bVoicePerPersonality ? Chat->GetPersonality().Voice : FString();
	Voice->SetMood(Chat->GetPersonality().Mood, Chat->GetPersonality().MoodIntensity);
	Voice->SetFaceStyle(Chat->GetPersonality().Face);
	CharacterTalkUi::ChosenScenario = Scenario;
	CharacterTalkUi::ChosenPersonality = Personality;
	PendingScenario = Scenario;
	PendingPersonality = Personality;
	RefreshChoiceButtons();
}

void UCharacterTalkWidget::RefreshChoiceButtons()
{
	auto Highlight = [](const TArray<TObjectPtr<UButton>>& Buttons, int32 Picked)
	{
		for (int32 Index = 0; Index < Buttons.Num(); ++Index)
		{
			Buttons[Index]->SetBackgroundColor(Index == Picked ? CharacterTalkUi::SelectedButton : CharacterTalkUi::NormalButton);
		}
	};
	Highlight(ScenarioButtons, PendingScenario);
	Highlight(PersonalityButtons, PendingPersonality);
	if (ConfirmChoicesButton)
	{
		ConfirmChoicesButton->SetIsEnabled(PendingScenario != CharacterTalkUi::ChosenScenario || PendingPersonality != CharacterTalkUi::ChosenPersonality);
	}
}

void UCharacterTalkWidget::SelectScenario(int32 Index)
{
	// Only marks the choice; "Xác nhận" applies it.
	PendingScenario = Index;
	RefreshChoiceButtons();
}

void UCharacterTalkWidget::SelectPersonality(int32 Index)
{
	PendingPersonality = Index;
	RefreshChoiceButtons();
}

void UCharacterTalkWidget::ConfirmChoices()
{
	const bool bChanged = PendingScenario != CharacterTalkUi::ChosenScenario || PendingPersonality != CharacterTalkUi::ChosenPersonality;
	if (bChanged && CanChangeChoices())
	{
		ApplyChoices(PendingScenario, PendingPersonality);
		StartConversation();
	}
}

bool UCharacterTalkWidget::CanChangeChoices()
{
	if ((Chat && Chat->IsBusy()) || (Voice && Voice->IsRecording()))
	{
		return false;
	}
	Voice->StopAll();
	ShowPendingReply();
	return true;
}

void UCharacterTalkWidget::StartConversation()
{
	// A divider so the old conversation reads as finished, then the character opens the new one itself.
	const FCharacterTalkScenario& Scenario = Chat->GetScenario();
	AddNote(FText::Format(LOCTEXT("NewConversation", "Kịch bản: {0}  ·  Tính cách: {1}"),
		FText::FromString(Scenario.Name), FText::FromString(Chat->GetPersonality().Name)));
	SetBusy(true);
	bReplyEmotionChecked = false;
	const FString Fallback = Scenario.FallbackGreeting;
	TWeakObjectPtr<UCharacterTalkWidget> WeakThis(this);
	Chat->Greet([WeakThis, Fallback](bool bOk, const FString& Reply)
	{
		if (UCharacterTalkWidget* Widget = WeakThis.Get())
		{
			Widget->SetBusy(false);
			// Without an answer (no key, no network) the scenario's own line keeps the conversation going.
			Widget->HandleReply(true, bOk ? Reply : Fallback, bOk);
		}
	}, MakeSentenceHandler());
}

bool UCharacterTalkWidget::ShouldSpeak() const
{
	return Voice && GetDefault<UCharacterTalkSettings>()->bSpeakReplies;
}

UCharacterTalkChat::FOnSentence UCharacterTalkWidget::MakeSentenceHandler()
{
	TWeakObjectPtr<UCharacterTalkWidget> WeakThis(this);
	return [WeakThis](const FString& Sentence)
	{
		if (UCharacterTalkWidget* Widget = WeakThis.Get())
		{
			Widget->HandleReplySentence(Sentence);
		}
	};
}

void UCharacterTalkWidget::HandleReplySentence(const FString& InSentence)
{
	// The reply opens with an emotion tag ("[vui] ..."): it sets the face and is neither shown nor spoken.
	FString Sentence = InSentence;
	if (!bReplyEmotionChecked)
	{
		bReplyEmotionChecked = true;
		const FString Emotion = UCharacterTalkChat::ExtractEmotion(Sentence);
		if (!Emotion.IsEmpty() && Voice)
		{
			Voice->SetEmotion(Emotion);
		}
		if (Sentence.IsEmpty())
		{
			return;
		}
	}
	ReplyText += (ReplyText.IsEmpty() ? TEXT("") : TEXT(" ")) + Sentence;
	if (!ShouldSpeak())
	{
		// Shown whole when the reply is complete.
		return;
	}
	// The first sentence starts speaking while the model is still writing the rest.
	if (!bReplyVoice)
	{
		BeginReplyVoice();
	}
	Voice->QueueSentence(Sentence);
}

void UCharacterTalkWidget::BeginReplyVoice()
{
	bReplyVoice = true;
	TWeakObjectPtr<UCharacterTalkWidget> WeakThis(this);
	Voice->BeginUtterance(Speaker.Get(),
		[WeakThis](const FString& Text, float Duration)
		{
			if (UCharacterTalkWidget* Widget = WeakThis.Get())
			{
				Widget->RevealClip(Text, Duration);
			}
		},
		[WeakThis](bool bOk, const FString& Error)
		{
			UCharacterTalkWidget* Widget = WeakThis.Get();
			if (!Widget)
			{
				return;
			}
			// Whatever could not be voiced is still shown. Only report a voice problem once.
			Widget->CompleteReply();
			if (!bOk && !Widget->bVoiceErrorShown)
			{
				Widget->bVoiceErrorShown = true;
				Widget->AddMessage(FText::FromString(Error), false, true);
			}
		});
}

void UCharacterTalkWidget::HandleReply(bool bOk, const FString& Reply, bool bSpeak)
{
	FocusInput();
	if (!bOk)
	{
		// What was already voiced finishes playing; the error follows it.
		if (bReplyVoice)
		{
			Voice->EndUtterance();
		}
		else
		{
			CompleteReply();
		}
		AddMessage(FText::FromString(Reply), false, true);
		return;
	}

	// The complete text still carries the emotion tag; a reply that was not streamed sets the face here.
	FString Text = Reply;
	const FString Emotion = UCharacterTalkChat::ExtractEmotion(Text);
	if (!bReplyEmotionChecked && !Emotion.IsEmpty() && Voice)
	{
		Voice->SetEmotion(Emotion);
	}
	bReplyEmotionChecked = true;
	ReplyText = Text;
	if (bReplyVoice)
	{
		// Streamed: the remaining sentences are already queued; the text keeps appearing with the voice.
		Voice->EndUtterance();
	}
	else if (bSpeak && ShouldSpeak())
	{
		// Not streamed (e.g. Claude): speak it sentence by sentence so the first one starts sooner.
		BeginReplyVoice();
		for (const FString& Sentence : UCharacterTalkChat::SplitIntoSentences(Text))
		{
			Voice->QueueSentence(Sentence);
		}
		Voice->EndUtterance();
	}
	else
	{
		CompleteReply();
	}
}

void UCharacterTalkWidget::RevealClip(const FString& Text, float Duration)
{
	if (!ReplyBody)
	{
		ReplyBody = AddMessage(FText::GetEmpty(), false);
		RevealFull.Reset();
		RevealShown = 0.f;
	}
	RevealFull += (RevealFull.IsEmpty() ? TEXT("") : TEXT(" ")) + Text;
	// Type out what has not been shown yet over this clip, slightly ahead of the voice.
	RevealSpeed = (RevealFull.Len() - RevealShown) / FMath::Max(0.3f, Duration * 0.85f);
}

void UCharacterTalkWidget::CompleteReply()
{
	if (ReplyBody)
	{
		if (!ReplyText.IsEmpty())
		{
			RevealFull = ReplyText;
		}
		FinishReveal();
		ReplyBody = nullptr;
	}
	else if (!ReplyText.IsEmpty())
	{
		AddMessage(FText::FromString(ReplyText), false);
	}
	ReplyText.Reset();
	RevealFull.Reset();
	bReplyVoice = false;
}

void UCharacterTalkWidget::ToggleKeyPanel()
{
	const bool bShow = KeyPanel->GetVisibility() == ESlateVisibility::Collapsed;
	KeyPanel->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	KeyInput->SetText(FText::GetEmpty());
	KeyStatus->SetText(UCharacterTalkChat::DescribeKeySource());
	if (bShow)
	{
		KeyInput->SetKeyboardFocus();
	}
	else
	{
		FocusInput();
	}
}

void UCharacterTalkWidget::HandleKeyCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		SaveKey();
	}
}

void UCharacterTalkWidget::SaveKey()
{
	const FString Key = KeyInput->GetText().ToString().TrimStartAndEnd();
	if (Key.IsEmpty())
	{
		return;
	}
	UCharacterTalkChat::SaveApiKey(Key);
	KeyInput->SetText(FText::GetEmpty());
	KeyPanel->SetVisibility(ESlateVisibility::Collapsed);
	AddNote(LOCTEXT("KeySavedNote", "Đã lưu API key."));
	// Start over so the character can greet with the new key (a conversation that failed for lack of a key resumes).
	if (CanChangeChoices())
	{
		StartConversation();
	}
	FocusInput();
}

void UCharacterTalkWidget::ClearKey()
{
	UCharacterTalkChat::SaveApiKey(FString());
	KeyInput->SetText(FText::GetEmpty());
	KeyStatus->SetText(UCharacterTalkChat::DescribeKeySource());
	AddNote(LOCTEXT("KeyClearedNote", "Đã xóa API key đã lưu trong game."));
}

void UCharacterTalkWidget::AddNote(const FText& Text)
{
	UTextBlock* Note = MakeText(Text, 13, CharacterTalkUi::Muted);
	Note->SetJustification(ETextJustify::Center);
	if (UScrollBoxSlot* NoteSlot = Cast<UScrollBoxSlot>(MessageList->AddChild(Note)))
	{
		NoteSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 10.f));
	}
	MessageList->ScrollToEnd();
}

void UCharacterTalkWidget::FocusInput()
{
	if (Input)
	{
		Input->SetKeyboardFocus();
	}
}

void UCharacterTalkWidget::HandleCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	if (CommitMethod == ETextCommit::OnEnter)
	{
		SendInput();
	}
}

void UCharacterTalkWidget::SendInput()
{
	if (!Chat || Chat->IsBusy())
	{
		return;
	}
	const FString Text = Input->GetText().ToString().TrimStartAndEnd();
	if (Text.IsEmpty())
	{
		return;
	}

	Input->SetText(FText::GetEmpty());
	// Writing over the character cuts its reply short; its words still go in the chat so it stays in order.
	if ((bReplyVoice || ReplyBody) && Voice)
	{
		Voice->StopAll();
	}
	ShowPendingReply();
	AddMessage(FText::FromString(Text), true);
	SetBusy(true);
	bReplyEmotionChecked = false;

	TWeakObjectPtr<UCharacterTalkWidget> WeakThis(this);
	Chat->Send(Text, [WeakThis](bool bOk, const FString& Reply)
	{
		if (UCharacterTalkWidget* Widget = WeakThis.Get())
		{
			Widget->SetBusy(false);
			Widget->HandleReply(bOk, Reply, true);
		}
	}, MakeSentenceHandler());
}

void UCharacterTalkWidget::ShowPendingReply()
{
	CompleteReply();
}

void UCharacterTalkWidget::FinishReveal()
{
	if (ReplyBody)
	{
		ReplyBody->SetText(FText::FromString(RevealFull));
		RevealShown = RevealFull.Len();
	}
}

void UCharacterTalkWidget::StartListening()
{
	if (!Voice || Voice->IsRecording() || (Chat && Chat->IsBusy()))
	{
		return;
	}
	// Talking over the character cuts its line short, like interrupting someone; its words still go in the chat.
	Voice->StopAll();
	ShowPendingReply();
	bMicToggleMode = false;
	if (Voice->StartRecording())
	{
		MicLabel->SetText(LOCTEXT("MicListening", "Đang nghe"));
		StatusText->SetText(FText::Format(LOCTEXT("Listening", "Đang nghe... thả {0} hoặc nút Nói để gửi."), PushToTalkKey().GetDisplayName()));
	}
	else
	{
		AddMessage(LOCTEXT("NoMic", "Không mở được micro. Kiểm tra micro đã cắm và được Windows cho phép dùng."), false, true);
	}
}

void UCharacterTalkWidget::StopListening()
{
	if (!Voice || !Voice->IsRecording())
	{
		return;
	}
	bMicToggleMode = false;
	MicLabel->SetText(LOCTEXT("Mic", "Nói"));
	StatusText->SetText(LOCTEXT("Transcribing", "Đang nhận giọng nói..."));
	TWeakObjectPtr<UCharacterTalkWidget> WeakThis(this);
	Voice->StopRecording([WeakThis](bool bOk, const FString& Text)
	{
		UCharacterTalkWidget* Widget = WeakThis.Get();
		if (!Widget)
		{
			return;
		}
		Widget->StatusText->SetText(FText::GetEmpty());
		if (bOk)
		{
			Widget->Input->SetText(FText::FromString(Text));
			Widget->SendInput();
		}
		else
		{
			Widget->AddMessage(FText::FromString(Text), false, true);
		}
	});
}

void UCharacterTalkWidget::HandleMicPressed()
{
	if (Voice && Voice->IsRecording())
	{
		// Second click of a toggle.
		StopListening();
		return;
	}
	MicPressTime = FPlatformTime::Seconds();
	StartListening();
}

void UCharacterTalkWidget::HandleMicReleased()
{
	if (!Voice || !Voice->IsRecording() || bMicToggleMode)
	{
		return;
	}
	if (FPlatformTime::Seconds() - MicPressTime >= CharacterTalkUi::MicClickSeconds)
	{
		StopListening();
		return;
	}
	bMicToggleMode = true;
	MicLabel->SetText(LOCTEXT("MicStop", "Dừng"));
	StatusText->SetText(LOCTEXT("ListeningToggle", "Đang nghe... nói xong thì bấm Dừng."));
}

void UCharacterTalkWidget::GoBack()
{
	if (Voice)
	{
		Voice->StopAll();
	}
	ShowPendingReply();
	if (OnBack)
	{
		OnBack();
	}
}

void UCharacterTalkWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (!Voice)
	{
		return;
	}
	Voice->Tick(DeltaTime);

	if (ReplyBody && RevealShown < RevealFull.Len())
	{
		// The reply types itself out along with the voice (see RevealClip).
		RevealShown = FMath::Min<float>(RevealFull.Len(), RevealShown + RevealSpeed * DeltaTime);
		ReplyBody->SetText(FText::FromString(RevealFull.Left(FMath::CeilToInt(RevealShown))));
		MessageList->ScrollToEnd();
	}

	// Status line: speaking wins (a streamed reply speaks while still being written), then thinking, then waiting for audio.
	if (Voice->IsRecording())
	{
		return;
	}
	if (Voice->IsSpeaking())
	{
		StatusText->SetText(LOCTEXT("Speaking", "Nhân vật đang nói..."));
		bShowingVoiceStatus = true;
	}
	else if (Chat && Chat->IsBusy())
	{
		if (bShowingVoiceStatus)
		{
			StatusText->SetText(LOCTEXT("Thinking", "Nhân vật đang suy nghĩ..."));
			bShowingVoiceStatus = false;
		}
	}
	else if (Voice->IsPreparingSpeech())
	{
		StatusText->SetText(LOCTEXT("PreparingSpeech", "Nhân vật chuẩn bị nói..."));
		bShowingVoiceStatus = true;
	}
	else if (bShowingVoiceStatus)
	{
		StatusText->SetText(FText::GetEmpty());
		bShowingVoiceStatus = false;
	}
}

void UCharacterTalkWidget::NativeDestruct()
{
	if (PushToTalk.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(PushToTalk);
	}
	PushToTalk.Reset();
	if (Voice)
	{
		// Close the microphone now, while the engine is fully running (see UCharacterTalkVoice::BeginDestroy).
		Voice->Shutdown();
	}
	Super::NativeDestruct();
}

UTextBlock* UCharacterTalkWidget::AddMessage(const FText& Text, bool bPlayer, bool bError)
{
	UBorder* Bubble = MakePanel(bPlayer ? CharacterTalkUi::PlayerBubble : CharacterTalkUi::CharacterBubble);
	Bubble->SetPadding(FMargin(14.f, 10.f));
	UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>();
	const FText SpeakerName = bPlayer ? LOCTEXT("You", "Bạn")
		: FText::Format(LOCTEXT("Character", "{0} · {1}"), FText::FromString(Chat->GetScenario().Role), FText::FromString(Chat->GetPersonality().Name));
	Lines->AddChildToVerticalBox(MakeText(SpeakerName, 14, bPlayer ? CharacterTalkUi::Ink : CharacterTalkUi::Gold, true))->SetPadding(FMargin(0, 0, 0, 4));
	UTextBlock* Body = MakeText(Text, 18, bError ? CharacterTalkUi::Warn : CharacterTalkUi::Ink);
	Lines->AddChildToVerticalBox(Body);
	Bubble->SetContent(Lines);

	// Player lines are indented from the left, the character's from the right, so the two sides read apart.
	if (UScrollBoxSlot* BubbleSlot = Cast<UScrollBoxSlot>(MessageList->AddChild(Bubble)))
	{
		BubbleSlot->SetPadding(bPlayer ? FMargin(60.f, 0.f, 0.f, 10.f) : FMargin(0.f, 0.f, 60.f, 10.f));
	}
	MessageList->ScrollToEnd();
	return Body;
}

void UCharacterTalkWidget::SetBusy(bool bBusy)
{
	StatusText->SetText(bBusy ? LOCTEXT("Thinking", "Nhân vật đang suy nghĩ...") : FText::GetEmpty());
	Input->SetIsReadOnly(bBusy);
}

FReply UCharacterTalkWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	if (KeyEvent.GetKey() == EKeys::Escape)
	{
		GoBack();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, KeyEvent);
}

#undef LOCTEXT_NAMESPACE
