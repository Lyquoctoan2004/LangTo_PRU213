#pragma once

#include "AudioCaptureCore.h"
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CharacterTalkVoice.generated.h"

class UAudioComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class USoundWaveProcedural;

/**
 * Voice for the lobby talk area, with free local servers instead of paid plugins:
 * microphone (engine AudioCapture) -> whisper.cpp server -> text, and reply text -> Piper server -> WAV played back while the
 * speaker's jaw bone opens with the loudness of the audio. Endpoints are in UCharacterTalkSettings. The owner calls Tick.
 */
UCLASS()
class MUTABLESAMPLE_API UCharacterTalkVoice : public UObject
{
	GENERATED_BODY()

public:
	using FOnText = TFunction<void(bool bOk, const FString& Text)>;
	using FOnSpoken = TFunction<void(bool bOk, const FString& Error)>;

	virtual void BeginDestroy() override;

	/** Stops everything and closes the microphone. Call while the engine runs (when the talk area closes). */
	void Shutdown();

	/** Gemini voice for replies (from the chosen personality); empty uses UCharacterTalkSettings::GeminiVoice. */
	FString VoiceName;

	bool StartRecording();
	/** Stops the microphone and sends what was said to the speech-to-text server. */
	void StopRecording(FOnText OnText);
	bool IsRecording() const { return bRecording; }

	/** A clip of the reply started playing: its text and length, so the text can be shown in step with it. */
	using FOnClipStarted = TFunction<void(const FString& Text, float Duration)>;

	/**
	 * Starts a spoken reply that arrives in pieces, opening Speaker's jaw with it. QueueSentence fetches each sentence's audio
	 * (in order, one request at a time) and the clips play back to back as they become ready; EndUtterance says no more will
	 * come. OnDone runs after the last clip (false with the reason if nothing could be spoken). Anything playing is stopped.
	 * StopAll or a newer utterance cancels this one without calling OnDone.
	 */
	void BeginUtterance(AActor* Speaker, FOnClipStarted OnClipStarted, FOnSpoken OnDone);
	void QueueSentence(const FString& Text);
	void EndUtterance();
	bool IsSpeaking() const { return PlaybackDuration > 0.f; }
	/** True while a reply's speech is being fetched and nothing of it is playing yet (or between clips). */
	bool IsPreparingSpeech() const { return bPreparingSpeech; }

	/** Stops recording and playback (the face keeps blinking and showing its mood). */
	void StopAll();

	/** The character whose face blinks, glances, shows emotions and lip-syncs while the talk area is open. */
	void SetFaceTarget(AActor* Speaker);
	/** Shows a reply's emotion ("vui", "buồn", "ngạc nhiên", "tức giận", "suy tư", "bình thường") while it is spoken. */
	void SetEmotion(const FString& Emotion);
	/** The resting expression between replies (from the personality), at 0..1 strength. */
	void SetMood(const FString& Emotion, float Intensity);

	void Tick(float DeltaTime);

private:
	/** A mesh whose jaw bone is turned after its animation is evaluated. */
	struct FJawRig
	{
		TWeakObjectPtr<USkeletalMeshComponent> Mesh;
		/** The mesh asset the bone indices belong to; Mutable swaps it when the character is rebuilt. */
		TWeakObjectPtr<USkeletalMesh> Asset;
		int32 Jaw = INDEX_NONE;
		int32 LeftJawline = INDEX_NONE;
		int32 RightJawline = INDEX_NONE;
		int32 MouthLower = INDEX_NONE;
		/** Bones turned with the jaw and the share of the jaw angle each takes (1 = fully). */
		TArray<TPair<int32, float>> Bones;
		// Expression bones (each group includes the bones under it).
		int32 LeftEye = INDEX_NONE;
		int32 RightEye = INDEX_NONE;
		int32 Head = INDEX_NONE;
		int32 NoseTip = INDEX_NONE;
		TArray<int32> LeftEyeBones, RightEyeBones;
		TArray<int32> LeftUpperLid, RightUpperLid, LeftLowerLid, RightLowerLid;
		TArray<int32> LeftBrowInner, RightBrowInner, LeftBrowMid, RightBrowMid, LeftBrowOuter, RightBrowOuter;
		TArray<int32> LeftCorner, RightCorner, LeftCheek, RightCheek, UpperLip;
		TArray<int32> HeadBones;
		FDelegateHandle Handle;
	};

	/** A facial expression, eased between emotions. Smile/Brow* are -1..1 of the configured distances, angles in degrees. */
	struct FFaceShape
	{
		float Smile = 0.f;
		float BrowInner = 0.f;
		float BrowOuter = 0.f;
		/** Inner brow ends pulled together (frown). */
		float BrowSqueeze = 0.f;
		/** One brow higher than the other (thinking). */
		float BrowTilt = 0.f;
		/** 1 = eyes half closed (squint, sleepy), -1 = wide open. */
		float Lids = 0.f;
		float MouthOpen = 0.f;
		float HeadPitch = 0.f;
		float HeadRoll = 0.f;
		float LookUp = 0.f;

		static FFaceShape Lerp(const FFaceShape& A, const FFaceShape& B, float Alpha);
	};
	static FFaceShape ShapeFor(const FString& Emotion);
	/** Advances blinks, glances, head motion and the expression; ApplyJaw puts them on the bones. */
	void TickFace(float DeltaTime);

	void TranscribeWithGemini(const TArray<uint8>& Wav, FOnText OnText, int32 ModelIndex = 0);
	/** Speech audio (a WAV) for one sentence, or why there is none. */
	using FOnAudio = TFunction<void(bool bOk, TArray<uint8> Wav, const FString& Error)>;
	/** Gemini first, the local server when Gemini cannot (per the settings). Drops the result if Request is no longer current. */
	void FetchSpeech(const FString& Text, int32 Request, FOnAudio OnAudio);
	void FetchWithGemini(const FString& Text, int32 Request, FOnAudio OnAudio, int32 ModelIndex = 0);
	/** The local speech server (Piper): the main voice with VoiceProvider=Server, otherwise the fallback for Gemini. */
	void FetchWithServer(const FString& Text, int32 Request, FOnAudio OnAudio);

	/** Utterance queue: fetch the next sentence, play the next fetched clip, and report when everything has played. */
	void AddClip(const FString& Text);
	void FetchNextClip();
	void FetchClip(int32 Index);
	void PlayNextClip();
	bool PlayClip(const TArray<uint8>& Wav, const FString& Text, FString& OutError);
	void FinishUtteranceIfDone();
	void StopPlayback();
	void BindJaw(AActor* Speaker);
	void UnbindJaw();
	void ApplyJaw(int32 RigIndex);

	TUniquePtr<Audio::FAudioCaptureSynth> Capture;
	TArray<float> Recorded;
	int32 RecordedChannels = 1;
	int32 RecordedSampleRate = 48000;
	double RecordStartTime = 0.0;
	bool bRecording = false;

	UPROPERTY(Transient) TObjectPtr<USoundWaveProcedural> Wave;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> Playback;
	/** Loudness of the reply in 0..1, one value per EnvelopeStep seconds. */
	TArray<float> Envelope;
	float PlaybackTime = 0.f;
	float PlaybackDuration = 0.f;
	float JawOpen = 0.f;
	TArray<FJawRig> JawRigs;
	/** Bumped by every new utterance and StopAll so late audio for an older reply is dropped. */
	int32 SpeakRequest = 0;
	bool bPreparingSpeech = false;

	struct FClip
	{
		FString Text;
		bool bFetched = false;
		bool bOk = false;
		TArray<uint8> Wav;
		FString Error;
	};
	TArray<FClip> Clips;
	/** Sentences waiting to be sent to Gemini together (see QueueSentence). */
	FString HeldText;
	int32 NextClipToFetch = 0;
	int32 NextClipToPlay = 0;
	int32 ClipsInFlight = 0;
	bool bUtteranceOpen = false;
	bool bAnyClipPlayed = false;
	FString UtteranceError;
	TWeakObjectPtr<AActor> UtteranceSpeaker;
	FOnClipStarted OnClipStarted;
	FOnSpoken OnUtteranceDone;
	/** Gemini speech ran out of quota: until this time each sentence goes straight to the local server instead of waiting on a 429. */
	double GeminiSpeechPausedUntil = 0.0;

	// Face state (see TickFace).
	TWeakObjectPtr<AActor> FaceTarget;
	FFaceShape Face;
	FFaceShape MoodShape;
	FFaceShape EmotionShape;
	float EmotionTimeLeft = 0.f;
	float FaceTime = 0.f;
	float Blink = 0.f;
	float BlinkPhase = -1.f;
	float BlinkCountdown = 2.f;
	FVector2D EyeLook = FVector2D::ZeroVector;
	FVector2D EyeTarget = FVector2D::ZeroVector;
	float SaccadeCountdown = 1.f;
	float TalkWeight = 0.f;
	float SpeechBrow = 0.f;
	/** Head pitch (nod), yaw (turn) and roll (tilt) in degrees. */
	FVector HeadAngles = FVector::ZeroVector;
	float RebindCountdown = 1.f;
};
