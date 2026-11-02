#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CharacterTalkChat.generated.h"

/**
 * Settings for the lobby talk area, read from [/Script/MutableSample.CharacterTalkSettings] in DefaultGame.ini.
 * Provider "Anthropic" calls the Claude Messages API; "OpenAI" calls any OpenAI-compatible /chat/completions endpoint
 * (OpenAI, Ollama, LM Studio...). An empty ApiKey falls back to the ANTHROPIC_API_KEY / OPENAI_API_KEY environment variable.
 */
UCLASS(Config = Game)
class MUTABLESAMPLE_API UCharacterTalkSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config) FString Provider = TEXT("Anthropic");
	UPROPERTY(Config) FString Endpoint = TEXT("https://api.anthropic.com/v1/messages");
	UPROPERTY(Config) FString Model = TEXT("claude-opus-5-5");
	UPROPERTY(Config) FString ApiKey;
	/** Environment variable holding the key when ApiKey is empty, e.g. GEMINI_API_KEY. Empty: the provider's default variable. */
	UPROPERTY(Config) FString ApiKeyEnvironmentVariable;
	UPROPERTY(Config) int32 MaxTokens = 16000;
	/**
	 * How long the model thinks before answering: output_config.effort for Claude, reasoning_effort for OpenAI-compatible
	 * endpoints, thinkingLevel for Gemini speech recognition. "low" keeps spoken replies quick (gemini-3.5-flash took ~45 s
	 * at its default, ~5 s at "low"). Empty sends nothing, for models that reject the field.
	 */
	UPROPERTY(Config) FString Effort = TEXT("low");
	/** Claude only: let the API re-serve a declined reply with a suitable fallback model (fallbacks: "default"). */
	UPROPERTY(Config) bool bServerSideFallback = true;
	UPROPERTY(Config) FString SystemPrompt;
	UPROPERTY(Config) float TimeoutSeconds = 120.f;
	/** Receive replies as they are written and speak them sentence by sentence (OpenAI-compatible endpoints, Gemini included). */
	UPROPERTY(Config) bool bStreamReplies = true;

	// Voice (see UCharacterTalkVoice). VoiceProvider "Gemini" uses the Gemini API for both directions with the GEMINI_API_KEY
	// environment variable (or ApiKey); "Server" uses free local servers: whisper.cpp for speech-to-text, Piper for text-to-speech.
	UPROPERTY(Config) FString VoiceProvider = TEXT("Gemini");
	UPROPERTY(Config) FString GeminiSpeechToTextModel = TEXT("gemini-3.5-flash-lite,gemini-3.5-flash,gemini-3-flash-preview");
	UPROPERTY(Config) FString GeminiTextToSpeechModel = TEXT("gemini-3.8-flash-tts,gemini-3.1-flash-tts-preview");
	/** Gemini voice for every reply, unless bVoicePerPersonality gives each personality its own. */
	UPROPERTY(Config) FString GeminiVoice = TEXT("Charon");
	UPROPERTY(Config) bool bVoicePerPersonality = false;
	/** Scenario and personality picked when the talk area first opens (names from UCharacterTalkChat::Scenarios / Personalities). */
	UPROPERTY(Config) FString DefaultScenario = TEXT("Làng Lụa");
	UPROPERTY(Config) FString DefaultPersonality = TEXT("Hiền lành");
	/** whisper.cpp server "/inference" endpoint; receives a 16 kHz mono WAV as multipart "file". */
	UPROPERTY(Config) FString SpeechToTextEndpoint = TEXT("http://127.0.0.1:8080/inference");
	UPROPERTY(Config) FString SpeechToTextLanguage = TEXT("vi");
	/** Piper http_server; POST {"text": ...} returns a WAV. */
	UPROPERTY(Config) FString TextToSpeechEndpoint = TEXT("http://127.0.0.1:5000/synthesize");
	/** With VoiceProvider=Gemini: read the line with the server above when Gemini cannot (quota used up, offline). */
	UPROPERTY(Config) bool bFallbackToServerSpeech = true;
	/**
	 * With VoiceProvider=Gemini: false reads every reply with the local server only (Gemini still listens), so the character
	 * keeps one voice instead of a different Gemini voice per personality and a switch to the server's voice when quota runs out.
	 */
	UPROPERTY(Config) bool bGeminiSpeaks = false;
	/**
	 * The local server's voice: a Piper voice name and, for a multi-speaker voice, the person reading. vi_VN-vivos-x_low has 65
	 * Vietnamese speakers; speaker 29 is a mid-pitched man (~120 Hz). vi_VN-vais1000-medium is clearer but a woman. -1: server default.
	 */
	UPROPERTY(Config) FString PiperVoice = TEXT("vi_VN-vivos-x_low");
	UPROPERTY(Config) int32 PiperSpeakerId = 29;
	/** Sentences of one reply fetched from the speech server at the same time (still played in order). */
	UPROPERTY(Config) int32 MaxParallelSpeechRequests = 3;
	UPROPERTY(Config) bool bSpeakReplies = true;
	/** Hold to talk, release to send (Unreal key name). A key that types nothing, so it never ends up in the chat box. */
	UPROPERTY(Config) FString PushToTalkKey = TEXT("Tab");
	/** Lip sync: the jaw bone opened with the loudness of the spoken reply, and how far it opens at full loudness. */
	UPROPERTY(Config) FName JawBone = TEXT("FACIAL_C_Jaw");
	UPROPERTY(Config) float JawOpenDegrees = 14.f;
	/** Blinks, glances, head motion and emotions on the face while talking. */
	UPROPERTY(Config) bool bLivelyFace = true;
	/** Strength of emotions and moods on the face, 0..1 (more can look exaggerated). */
	UPROPERTY(Config) float EmotionStrength = 1.f;
	/** How far brows move and mouth corners lift at full expression (cm), the eyelid turn for a full blink, head and eye motion. */
	UPROPERTY(Config) float BrowMoveCm = 0.5f;
	UPROPERTY(Config) float SmileCm = 0.4f;
	UPROPERTY(Config) float BlinkDegrees = 25.f;
	UPROPERTY(Config) float HeadMotionDegrees = 2.5f;
	UPROPERTY(Config) float EyeMotionDegrees = 4.f;
	/**
	 * A MetaHuman face mesh (built in MetaHuman Creator) put on the speaker in place of its own head while talking; its
	 * RigLogic shows the expressions and lip sync (see UTalkFaceAnimInstance). Empty keeps the speaker's head and moves its bones.
	 */
	UPROPERTY(Config) FSoftObjectPath MetaHumanFace;
	/** Grooms for that face (hair, brows, lashes...), each with its "<groom>_Binding" asset in the same folder. */
	UPROPERTY(Config) TArray<FSoftObjectPath> MetaHumanGrooms;
	/** How far the MetaHuman jaw opens at full loudness (CTRL_expressions_jawOpen, 0..1). */
	UPROPERTY(Config) float MetaHumanJawOpen = 0.45f;
	/** Mutable "Skin Tone" (0..1) set on the body when the MetaHuman face goes on, so neck and hands match it; negative leaves it. */
	UPROPERTY(Config) float MetaHumanBodySkinTone = -1.f;
	/**
	 * How thin the body's own upper neck is made under the MetaHuman face, so it stays inside the face's neck (1 = unchanged).
	 * These neck settings are editable while playing (Python: the class default object) to tune them by eye.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "MetaHuman") float MetaHumanBodyNeckScale = 1.f;
	/** The same for the body's lower neck (neck_01); too thin opens a hole above the shirt's neckline. */
	UPROPERTY(Config, EditAnywhere, Category = "MetaHuman") float MetaHumanBodyLowerNeckScale = 1.f;
	/**
	 * How thin the base of the MetaHuman neck is made (neck_01, and its upper chest shrunk every way), so it stays inside the
	 * body's neck and clothes for every body type instead of showing through them (1 = unchanged).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "MetaHuman") float MetaHumanNeckBaseScale = 1.f;
};

/** A role the character plays in the conversation (the game's story, an IT lecturer, a close friend...). */
struct FCharacterTalkScenario
{
	FString Name;
	/** Shown as the speaker of the character's lines, e.g. "Giảng viên". */
	FString Role;
	FString Prompt;
	/** Used when the opening line cannot be fetched. */
	FString FallbackGreeting;
};

/** How a personality carries its face, on top of its mood and each reply's emotion (see UCharacterTalkVoice). */
struct FCharacterTalkFaceStyle
{
	/** Scales every emotion and the mood (1 = as defined in UCharacterTalkVoice::ShapeFor). */
	float Expressiveness = 1.f;
	/** How often it blinks, relative to normal. */
	float BlinkRate = 1.f;
	/** Share of glances that leave the player (0..1) and how far they go (times EyeMotionDegrees). */
	float LookAway = 0.45f;
	float GlanceRange = 1.f;
	/** Resting gaze below the player's eyes, head nod down and head tilt (degrees). */
	float GazeDown = 0.f;
	float HeadPitch = 0.f;
	float HeadRoll = 0.f;
	/** Head movement while talking (times HeadMotionDegrees). */
	float HeadMotion = 1.f;
	/** A one-sided smile, at rest and more while talking (0..1; positive pulls the left corner, negative the right). */
	float Smirk = 0.f;
	/** How much the brows lift on loud syllables (1 = normal). */
	float TalkBrows = 1.f;
	/** Lips pressed together when not speaking (0..1). */
	float LipPress = 0.f;
	/** How wide the mouth opens when speaking (1 = normal). */
	float Jaw = 1.f;
};

/** A way the character talks within its role: tone (added to the system prompt) and the voice that reads it. */
struct FCharacterTalkPersonality
{
	FString Name;
	FString Prompt;
	FString Voice;
	/** Resting expression between replies (an emotion tag, see UCharacterTalkVoice::SetEmotion) and its strength. */
	FString Mood;
	float MoodIntensity = 0.f;
	FCharacterTalkFaceStyle Face;
};

/** One conversation with the player's character: keeps the history and sends it to the configured LLM. */
UCLASS()
class MUTABLESAMPLE_API UCharacterTalkChat : public UObject
{
	GENERATED_BODY()

public:
	using FOnReply = TFunction<void(bool bOk, const FString& Text)>;
	/** One finished sentence of a reply that is still being written (streaming), so it can be spoken right away. */
	using FOnSentence = TFunction<void(const FString& Sentence)>;

	/** Removes a leading emotion tag ("[vui] ...") from Text and returns it ("vui"), or an empty string if there is none. */
	static FString ExtractEmotion(FString& Text);

	/** Cuts a finished reply into sentences, the same way a streamed one is cut. */
	static TArray<FString> SplitIntoSentences(const FString& Text);

	/**
	 * API key typed in the talk area. Stored in the player's own GameUserSettings.ini under Saved/, never in the project's
	 * config, and used before ApiKey and the environment variables.
	 */
	static FString GetSavedApiKey();
	static void SaveApiKey(const FString& Key);
	/** Which key the chat would use right now, for the key panel. */
	static FText DescribeKeySource();

	/** The scenarios and personalities the player can pick from in the talk area. */
	static const TArray<FCharacterTalkScenario>& Scenarios();
	static const TArray<FCharacterTalkPersonality>& Personalities();

	/**
	 * Adds the player's line and asks for the character's answer. Ignored while a request is in flight.
	 * With OnSentence (and bStreamReplies), each sentence is handed out as soon as it is written; OnReply still gets the whole text.
	 */
	void Send(const FString& PlayerText, FOnReply OnReply, FOnSentence OnSentence = nullptr);
	bool IsBusy() const { return bBusy; }

	/** Asks the character to open the conversation in its current role and personality (a hidden line, not the player's). */
	void Greet(FOnReply OnReply, FOnSentence OnSentence = nullptr);

	/** Switching role or personality starts the conversation over, so earlier replies do not pull it back. */
	void SetScenario(int32 Index);
	void SetPersonality(int32 Index);
	const FCharacterTalkScenario& GetScenario() const { return Scenarios()[Scenario]; }
	const FCharacterTalkPersonality& GetPersonality() const { return Personalities()[Personality]; }

private:
	FString FullSystemPrompt(const UCharacterTalkSettings& Settings) const;
	int32 Scenario = 0;
	int32 Personality = 0;

	struct FLine
	{
		bool bPlayer = false;
		FString Text;
	};

	/** Sends the history with the Model list's entry at ModelIndex, moving on to the next entry when that one is busy. */
	void SendRequest(int32 ModelIndex, FOnReply OnReply, FOnSentence OnSentence);
	FString BuildAnthropicBody(const UCharacterTalkSettings& Settings, const FString& Model) const;
	FString BuildOpenAIBody(const UCharacterTalkSettings& Settings, const FString& Model, bool bStream) const;
	static bool ParseAnthropicReply(const FString& Json, FString& OutText);
	static bool ParseOpenAIReply(const FString& Json, FString& OutText);

	TArray<FLine> History;
	bool bBusy = false;
};
