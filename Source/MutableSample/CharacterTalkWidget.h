#pragma once

#include "CharacterTalkChat.h"
#include "CoreMinimal.h"
#include "VillageWidgets.h"
#include "CharacterTalkWidget.generated.h"

class UCharacterTalkChat;
class UCharacterTalkVoice;
class UEditableTextBox;
class UScrollBox;

/**
 * Lobby talk area shown after "Xác nhận nói chuyện": chat history on the right, a text box with send and microphone
 * buttons under it, and a back button that returns to the customization screen. Replies come from UCharacterTalkChat and
 * are spoken with lip sync by UCharacterTalkVoice.
 */
UCLASS()
class MUTABLESAMPLE_API UCharacterTalkWidget : public UVillageWidgetBase
{
	GENERATED_BODY()

public:
	/** Called when the player presses "Quay lại tạo hình". */
	TFunction<void()> OnBack;

	/** The character who answers; its jaw moves with the spoken reply. */
	TWeakObjectPtr<AActor> Speaker;

	void FocusInput();

	/** Push-to-talk: start recording on press, send what was said on release. */
	void StartListening();
	void StopListening();

protected:
	virtual void BuildTree(UCanvasPanel* Root) override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

private:
	UFUNCTION()
	void HandleCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	void SendInput();
	/** Uses a scenario and personality for the chat and the voice and highlights their buttons. */
	void ApplyChoices(int32 Scenario, int32 Personality);
	/** The player marked a scenario or personality; nothing changes until ConfirmChoices. */
	void SelectScenario(int32 Index);
	void SelectPersonality(int32 Index);
	/** Applies the marked scenario and personality and starts the conversation over. */
	void ConfirmChoices();
	/** Highlights the marked buttons and enables the confirm button when they differ from what is in use. */
	void RefreshChoiceButtons();

	int32 PendingScenario = 0;
	int32 PendingPersonality = 0;
	UPROPERTY(Transient) TObjectPtr<UButton> ConfirmChoicesButton;
	/** False while waiting for a reply or listening; otherwise stops the current line so the choice can change. */
	bool CanChangeChoices();
	/** Marks a new conversation in the chat and lets the character open it. */
	void StartConversation();
	/** The complete reply from the character arrived: finish showing (and, when bSpeak, voicing) it. */
	void HandleReply(bool bOk, const FString& Reply, bool bSpeak);

	/**
	 * Streaming: each finished sentence of the reply is queued for speech at once, and its text appears when its audio
	 * starts (RevealClip). CompleteReply shows the whole reply when the voice is done, cut short or unavailable.
	 */
	UCharacterTalkChat::FOnSentence MakeSentenceHandler();
	void HandleReplySentence(const FString& Sentence);
	void BeginReplyVoice();
	void RevealClip(const FString& Text, float Duration);
	void CompleteReply();
	bool ShouldSpeak() const;
	/** A small centered line in the chat, e.g. which scenario a new conversation uses. */
	void AddNote(const FText& Text);

	/** API key panel: shows where the current key comes from and lets the player save or clear their own. */
	void ToggleKeyPanel();
	void SaveKey();
	void ClearKey();

	UFUNCTION()
	void HandleKeyCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UPROPERTY(Transient) TObjectPtr<UVerticalBox> KeyPanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> KeyStatus;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> KeyInput;
	void GoBack();

	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> ScenarioButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> PersonalityButtons;
	static FKey PushToTalkKey();

	UFUNCTION()
	void HandleMicPressed();

	UFUNCTION()
	void HandleMicReleased();

	TSharedPtr<class IInputProcessor> PushToTalk;
	double MicPressTime = 0.0;
	bool bMicToggleMode = false;
	/** Adds a chat bubble and returns its text block. */
	UTextBlock* AddMessage(const FText& Text, bool bPlayer, bool bError = false);
	/** Stops waiting on the current reply's voice and shows its whole text (callers stop the voice first). */
	void ShowPendingReply();
	/** Shows all of the reply text typed out so far at once. */
	void FinishReveal();

	/** The reply in progress: all its text so far, whether it is being voiced, and the bubble it types itself into. */
	FString ReplyText;
	bool bReplyVoice = false;
	/** Whether the reply's leading emotion tag has been looked for yet. */
	bool bReplyEmotionChecked = false;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ReplyBody;
	/** Text the bubble types out (what has been voiced so far), how much is shown, and how fast it goes. */
	FString RevealFull;
	float RevealShown = 0.f;
	float RevealSpeed = 0.f;
	void SetBusy(bool bBusy);

	bool bVoiceErrorShown = false;
	bool bShowingVoiceStatus = false;

	UPROPERTY(Transient) TObjectPtr<UCharacterTalkChat> Chat;
	UPROPERTY(Transient) TObjectPtr<UCharacterTalkVoice> Voice;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MicLabel;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> MessageList;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> Input;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
};
