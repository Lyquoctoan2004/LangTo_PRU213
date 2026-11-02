#include "CharacterTalkVoice.h"

#include "Audio.h"
#include "CharacterTalkChat.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Sound/SoundWaveProcedural.h"
#include "TalkFaceAnimInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogCharacterTalkVoice, Log, All);

namespace CharacterTalkVoice
{
	static constexpr int32 WhisperSampleRate = 16000;
	static constexpr float MaxRecordSeconds = 30.f;
	static constexpr float MinRecordSeconds = 0.3f;
	/** A recording whose loudest sample is below this has no speech in it (a muted or disconnected microphone). */
	static constexpr float SilencePeak = 0.002f;
	static constexpr float EnvelopeStep = 0.02f;
	static constexpr float JawFollowSpeed = 20.f;
	/** Facial expression timing: how long a reply's emotion stays after it was said, how fast faces change, blink length. */
	static constexpr float EmotionHoldSeconds = 2.5f;
	/** With the Gemini voice, sentences after the first are sent together once they reach this length (or the reply ends). */
	static constexpr int32 GeminiChunkChars = 400;
	static constexpr float ExpressionSpeed = 4.f;
	static constexpr float BlinkSeconds = 0.16f;
	/** Pause between two sentences of one reply. */
	static constexpr float ClipGapSeconds = 0.05f;
	/** How long Gemini speech is skipped after every Gemini speech model ran out of quota. */
	static constexpr double GeminiSpeechPauseSeconds = 120.0;
	static const FName LeftJawline = TEXT("FACIAL_L_Jawline");
	static const FName RightJawline = TEXT("FACIAL_R_Jawline");
	static const FName MouthLower = TEXT("FACIAL_C_MouthLower");

	/**
	 * Bones of the character's face rig that are not under the jaw bone but must open with it, and how much of the jaw angle
	 * each takes. The lower lip has its own bones; without them the chin drops while the lips stay shut. The corners and
	 * lower cheeks follow partly so the mouth stretches instead of tearing.
	 */
	static const TArray<TPair<FName, float>> MouthFollowers = {
		{ TEXT("FACIAL_C_MouthLower"), 1.f }, { TEXT("FACIAL_C_LowerLipRotation"), 1.f }, { TEXT("FACIAL_C_LipLower"), 1.f },
		{ TEXT("FACIAL_L_LipLower"), 1.f }, { TEXT("FACIAL_R_LipLower"), 1.f },
		{ TEXT("FACIAL_L_LipLowerOuter"), 0.9f }, { TEXT("FACIAL_R_LipLowerOuter"), 0.9f },
		{ TEXT("FACIAL_C_TeethLower"), 1.f }, { TEXT("FACIAL_C_Tongue1"), 1.f },
		{ TEXT("FACIAL_L_LipCorner"), 0.5f }, { TEXT("FACIAL_R_LipCorner"), 0.5f },
		{ TEXT("FACIAL_L_CheekLower"), 0.3f }, { TEXT("FACIAL_R_CheekLower"), 0.3f },
	};

	/**
	 * The sample rate the microphone really delivered, from how many frames arrived in how long. The device's "preferred"
	 * rate is not always what the stream runs at, and a wrong rate makes the speech too fast or slow to recognize.
	 */
	int32 MeasureSampleRate(int32 Frames, double Seconds, int32 Reported)
	{
		if (Seconds < 1.0)
		{
			return Reported;
		}
		const double Measured = Frames / Seconds;
		if (FMath::Abs(Measured - Reported) < Reported * 0.1)
		{
			return Reported;
		}
		static const int32 Standard[] = { 8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000, 96000 };
		int32 Best = Reported;
		for (const int32 Rate : Standard)
		{
			if (FMath::Abs(Measured - Rate) < FMath::Abs(Measured - Best))
			{
				Best = Rate;
			}
		}
		return Best;
	}

	/**
	 * Turns interleaved float microphone audio into 16 kHz mono 16-bit PCM for speech recognition: keeps the loudest channel
	 * (a headset may record on one side only, and averaging would halve it), raises quiet speech to a clear level and
	 * downsamples by averaging each output step, which also filters out what 16 kHz cannot hold.
	 */
	TArray<int16> ToWhisperPcm(const TArray<float>& Interleaved, int32 Channels, int32 SampleRate)
	{
		const int32 Frames = Interleaved.Num() / Channels;
		int32 Loudest = 0;
		double LoudestEnergy = -1.0;
		for (int32 Channel = 0; Channel < Channels; ++Channel)
		{
			double Energy = 0.0;
			for (int32 Frame = 0; Frame < Frames; ++Frame)
			{
				Energy += FMath::Square(Interleaved[Frame * Channels + Channel]);
			}
			if (Energy > LoudestEnergy)
			{
				LoudestEnergy = Energy;
				Loudest = Channel;
			}
		}

		const double Ratio = double(SampleRate) / WhisperSampleRate;
		const int32 OutFrames = int32(Frames / Ratio);
		TArray<float> Mono;
		Mono.SetNumUninitialized(OutFrames);
		float Peak = 0.f;
		for (int32 Index = 0; Index < OutFrames; ++Index)
		{
			const int32 Start = int32(Index * Ratio);
			const int32 End = FMath::Clamp(int32((Index + 1) * Ratio), Start + 1, Frames);
			float Sum = 0.f;
			for (int32 Frame = Start; Frame < End; ++Frame)
			{
				Sum += Interleaved[Frame * Channels + Loudest];
			}
			Mono[Index] = Sum / (End - Start);
			Peak = FMath::Max(Peak, FMath::Abs(Mono[Index]));
		}

		const float Gain = Peak > 0.f ? FMath::Clamp(0.9f / Peak, 1.f, 20.f) : 1.f;
		TArray<int16> Out;
		Out.SetNumUninitialized(OutFrames);
		for (int32 Index = 0; Index < OutFrames; ++Index)
		{
			Out[Index] = int16(FMath::Clamp(Mono[Index] * Gain, -1.f, 1.f) * 32767.f);
		}
		UE_LOG(LogCharacterTalkVoice, Log, TEXT("Recorded %.1fs at %d Hz, channel %d of %d, peak %.3f, gain x%.1f."),
			double(Frames) / SampleRate, SampleRate, Loudest + 1, Channels, Peak, Gain);
		return Out;
	}

	bool UseGemini(const UCharacterTalkSettings& Settings)
	{
		return Settings.VoiceProvider.Equals(TEXT("Gemini"), ESearchCase::IgnoreCase);
	}

	/** "a, b, c" -> {"a", "b", "c"}: models tried in order, since free quotas are counted per model. */
	TArray<FString> ModelList(const FString& Models)
	{
		TArray<FString> List;
		Models.ParseIntoArray(List, TEXT(","));
		for (FString& Model : List)
		{
			Model.TrimStartAndEndInline();
		}
		List.RemoveAll([](const FString& Model) { return Model.IsEmpty(); });
		return List;
	}

	/** POST Body to a Gemini model's generateContent; an empty key fails in the callback with a readable message. */
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> GeminiRequest(const UCharacterTalkSettings& Settings, const FString& Model, const TSharedRef<FJsonObject>& Body)
	{
		// Same order as the chat: the key typed in the game, then the environment, then the project config.
		FString Key = UCharacterTalkChat::GetSavedApiKey();
		if (Key.IsEmpty())
		{
			Key = FPlatformMisc::GetEnvironmentVariable(TEXT("GEMINI_API_KEY"));
		}
		if (Key.IsEmpty())
		{
			Key = Settings.ApiKey;
		}
		FString Json;
		FJsonSerializer::Serialize(Body, TJsonWriterFactory<>::Create(&Json));

		const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(FString::Printf(TEXT("https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent"), *Model));
		Request->SetVerb(TEXT("POST"));
		Request->SetTimeout(Settings.TimeoutSeconds);
		Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		Request->SetHeader(TEXT("x-goog-api-key"), Key);
		Request->SetContentAsString(Json);
		return Request;
	}

	/** The parts of the first candidate of a Gemini reply, or empty. */
	TArray<TSharedPtr<FJsonValue>> GeminiParts(const FString& Json)
	{
		TSharedPtr<FJsonObject> Root;
		const TArray<TSharedPtr<FJsonValue>>* Candidates = nullptr;
		if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) && Root.IsValid()
			&& Root->TryGetArrayField(TEXT("candidates"), Candidates) && Candidates->Num() > 0)
		{
			const TSharedPtr<FJsonObject>* Content = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Parts = nullptr;
			if ((*Candidates)[0]->AsObject()->TryGetObjectField(TEXT("content"), Content) && (*Content)->TryGetArrayField(TEXT("parts"), Parts))
			{
				return *Parts;
			}
		}
		return {};
	}

	FString GeminiError(const FHttpResponsePtr& Response)
	{
		if (Response->GetResponseCode() == 429)
		{
			return TEXT("Gemini đang quá giới hạn lượt dùng miễn phí, đợi khoảng một phút rồi thử lại nhé.");
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedPtr<FJsonObject>* Error = nullptr;
		FString Message;
		if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()), Root) && Root.IsValid()
			&& Root->TryGetObjectField(TEXT("error"), Error) && (*Error)->TryGetStringField(TEXT("message"), Message))
		{
			return FString::Printf(TEXT("Gemini báo lỗi %d: %s"), Response->GetResponseCode(), *Message);
		}
		return FString::Printf(TEXT("Gemini báo lỗi %d."), Response->GetResponseCode());
	}

	void AppendUtf8(TArray<uint8>& Body, const FString& Text)
	{
		const FTCHARToUTF8 Utf8(*Text);
		Body.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	}

	/** Loudness per EnvelopeStep, scaled so normal speech reaches about 1 and pauses fall to 0. */
	TArray<float> BuildEnvelope(const int16* Samples, int32 Frames, int32 Channels, int32 SampleRate)
	{
		const int32 FramesPerStep = FMath::Max(1, int32(SampleRate * EnvelopeStep));
		TArray<float> Rms;
		float Peak = 0.f;
		for (int32 Start = 0; Start < Frames; Start += FramesPerStep)
		{
			const int32 End = FMath::Min(Start + FramesPerStep, Frames);
			double Sum = 0.0;
			for (int32 Frame = Start; Frame < End; ++Frame)
			{
				const float Value = Samples[Frame * Channels] / 32768.f;
				Sum += Value * Value;
			}
			const float Value = FMath::Sqrt(float(Sum / FMath::Max(1, End - Start)));
			Rms.Add(Value);
			Peak = FMath::Max(Peak, Value);
		}
		for (float& Value : Rms)
		{
			Value = Peak > 0.f ? FMath::Clamp(Value / (Peak * 0.6f), 0.f, 1.f) : 0.f;
			if (Value < 0.1f)
			{
				Value = 0.f;
			}
		}
		return Rms;
	}
}

void UCharacterTalkVoice::Shutdown()
{
	StopAll();
	UnbindJaw();
	FaceTarget = nullptr;
	if (Capture)
	{
		Capture->AbortCapturing();
		Capture.Reset();
	}
}

void UCharacterTalkVoice::BeginDestroy()
{
	StopAll();
	// The microphone must be closed by Shutdown while the engine runs. Closing it here, during garbage collection at
	// editor exit, crashed inside the WASAPI capture module, so a stream still open now is left for the process to drop.
	if (Capture)
	{
		UE_LOG(LogCharacterTalkVoice, Warning, TEXT("Microphone still open when the voice was destroyed; it is released with the process."));
		Audio::FAudioCaptureSynth* Leaked = Capture.Release();
		(void)Leaked;
	}
	Super::BeginDestroy();
}

bool UCharacterTalkVoice::StartRecording()
{
	if (bRecording)
	{
		return true;
	}
	if (!Capture)
	{
		Capture = MakeUnique<Audio::FAudioCaptureSynth>();
	}

	Audio::FCaptureDeviceInfo Info;
	if (!Capture->GetDefaultCaptureDeviceInfo(Info) || !Capture->OpenDefaultStream())
	{
		UE_LOG(LogCharacterTalkVoice, Warning, TEXT("No microphone could be opened (is the AudioCapture plugin enabled?)."));
		return false;
	}
	RecordedChannels = FMath::Max(1, Info.InputChannels);
	RecordedSampleRate = Info.PreferredSampleRate > 0 ? Info.PreferredSampleRate : 48000;
	Recorded.Reset();
	Capture->StartCapturing();
	TArray<float> Stale;
	Capture->GetAudioData(Stale);
	bRecording = true;
	RecordStartTime = FPlatformTime::Seconds();
	UE_LOG(LogCharacterTalkVoice, Log, TEXT("Recording from '%s' (%d Hz, %d ch)."), *Info.DeviceName, RecordedSampleRate, RecordedChannels);
	return true;
}

void UCharacterTalkVoice::StopRecording(FOnText OnText)
{
	if (!bRecording)
	{
		return;
	}
	Tick(0.f);
	Capture->StopCapturing();
	bRecording = false;

	if (Recorded.Num() < RecordedSampleRate * RecordedChannels * CharacterTalkVoice::MinRecordSeconds)
	{
		OnText(false, TEXT("Không nghe thấy gì, bạn thử nói lại nhé."));
		return;
	}

	// Silence would come back as made-up words (speech recognition guesses when it hears nothing), so stop here instead.
	float RawPeak = 0.f;
	for (const float Sample : Recorded)
	{
		RawPeak = FMath::Max(RawPeak, FMath::Abs(Sample));
	}
	if (RawPeak < CharacterTalkVoice::SilencePeak)
	{
		UE_LOG(LogCharacterTalkVoice, Warning, TEXT("Recording is silent (peak %.5f); not sent for recognition."), RawPeak);
		Recorded.Reset();
		OnText(false, TEXT("Micro không thu được tiếng nào. Kiểm tra nút tắt tiếng trên tai nghe, micro đã bật, và trong Windows (Settings > System > Sound > Input) đã chọn đúng micro và thanh mức tiếng có nhảy khi bạn nói."));
		return;
	}

	const double Seconds = FPlatformTime::Seconds() - RecordStartTime;
	const int32 SampleRate = CharacterTalkVoice::MeasureSampleRate(Recorded.Num() / RecordedChannels, Seconds, RecordedSampleRate);
	if (SampleRate != RecordedSampleRate)
	{
		UE_LOG(LogCharacterTalkVoice, Warning, TEXT("Microphone reported %d Hz but delivered about %d Hz; using %d Hz."),
			RecordedSampleRate, int32(Recorded.Num() / RecordedChannels / Seconds), SampleRate);
	}
	const TArray<int16> Pcm = CharacterTalkVoice::ToWhisperPcm(Recorded, RecordedChannels, SampleRate);
	Recorded.Reset();
	TArray<uint8> Wav;
	SerializeWaveFile(Wav, reinterpret_cast<const uint8*>(Pcm.GetData()), Pcm.Num() * sizeof(int16), 1, CharacterTalkVoice::WhisperSampleRate);

	// Kept for checking what was actually sent when a transcript looks wrong.
	const FString DebugPath = FPaths::ProjectSavedDir() / TEXT("TalkDebug/LastRecording.wav");
	FFileHelper::SaveArrayToFile(Wav, *DebugPath);
	UE_LOG(LogCharacterTalkVoice, Log, TEXT("Sending recording for recognition (copy at %s)."), *DebugPath);

	const UCharacterTalkSettings& Settings = *GetDefault<UCharacterTalkSettings>();
	if (CharacterTalkVoice::UseGemini(Settings))
	{
		TranscribeWithGemini(Wav, MoveTemp(OnText));
		return;
	}

	const FString Boundary = TEXT("----LangToLuaVoice") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	TArray<uint8> Body;
	auto AddField = [&Body, &Boundary](const FString& Name, const FString& Value)
	{
		CharacterTalkVoice::AppendUtf8(Body, FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"%s\"\r\n\r\n%s\r\n"), *Boundary, *Name, *Value));
	};
	CharacterTalkVoice::AppendUtf8(Body, FString::Printf(TEXT("--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"speech.wav\"\r\nContent-Type: audio/wav\r\n\r\n"), *Boundary));
	Body.Append(Wav);
	CharacterTalkVoice::AppendUtf8(Body, TEXT("\r\n"));
	AddField(TEXT("response_format"), TEXT("json"));
	AddField(TEXT("temperature"), TEXT("0.0"));
	if (!Settings.SpeechToTextLanguage.IsEmpty())
	{
		AddField(TEXT("language"), Settings.SpeechToTextLanguage);
	}
	CharacterTalkVoice::AppendUtf8(Body, FString::Printf(TEXT("--%s--\r\n"), *Boundary));

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Settings.SpeechToTextEndpoint);
	Request->SetVerb(TEXT("POST"));
	Request->SetTimeout(Settings.TimeoutSeconds);
	Request->SetHeader(TEXT("Content-Type"), TEXT("multipart/form-data; boundary=") + Boundary);
	Request->SetContent(MoveTemp(Body));
	Request->OnProcessRequestComplete().BindLambda([OnText = MoveTemp(OnText)](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
	{
		if (!bConnected || !Response.IsValid())
		{
			OnText(false, TEXT("Không kết nối được máy chủ nhận giọng nói (whisper.cpp)."));
			return;
		}
		if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()))
		{
			OnText(false, FString::Printf(TEXT("Máy chủ nhận giọng nói báo lỗi %d."), Response->GetResponseCode()));
			return;
		}

		FString Text;
		TSharedPtr<FJsonObject> Root;
		if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Response->GetContentAsString()), Root) && Root.IsValid())
		{
			Root->TryGetStringField(TEXT("text"), Text);
		}
		Text.TrimStartAndEndInline();
		// whisper.cpp writes markers like "[BLANK_AUDIO]" or "(nhạc)" for silence and noise.
		if (Text.IsEmpty() || (Text.StartsWith(TEXT("[")) && Text.EndsWith(TEXT("]"))) || (Text.StartsWith(TEXT("(")) && Text.EndsWith(TEXT(")"))))
		{
			OnText(false, TEXT("Không nghe rõ, bạn thử nói lại nhé."));
			return;
		}
		OnText(true, Text);
	});
	Request->ProcessRequest();
}

void UCharacterTalkVoice::TranscribeWithGemini(const TArray<uint8>& Wav, FOnText OnText, int32 ModelIndex)
{
	const UCharacterTalkSettings& Settings = *GetDefault<UCharacterTalkSettings>();
	// A list of models, separate from the chat's so listening and answering do not use up the same per-model quota.
	const TArray<FString> Models = CharacterTalkVoice::ModelList(Settings.GeminiSpeechToTextModel);
	if (!Models.IsValidIndex(ModelIndex))
	{
		OnText(false, TEXT("Chưa cấu hình model nhận giọng Gemini (GeminiSpeechToTextModel)."));
		return;
	}
	const FString Model = Models[ModelIndex];
	const bool bHasNextModel = Models.IsValidIndex(ModelIndex + 1);
	// Transcription models (gemini-*-transcribe) only write down speech: they take a short instruction, do not think, and
	// answer with an audioTranscription part. On Vietnamese test lines they were exact where flash models misheard words.
	const bool bTranscribeModel = Model.Contains(TEXT("transcribe"));

	const TSharedRef<FJsonObject> Prompt = MakeShared<FJsonObject>();
	Prompt->SetStringField(TEXT("text"), bTranscribeModel ? TEXT("Transcribe this audio in Vietnamese.") : TEXT(
		"Đây là bản ghi âm một người chơi nói với nhân vật trong game, thường bằng tiếng Việt. "
		"Hãy chép lại NGUYÊN VĂN từng từ người đó nói, đúng chính tả tiếng Việt có dấu. "
		"Không trả lời câu nói, không dịch, không tóm tắt, không sửa ý, không thêm lời nào khác. "
		"Nếu đoạn nào nghe không rõ thì bỏ qua đoạn đó chứ đừng đoán. Nếu không có lời nói, trả về chuỗi rỗng."));
	const TSharedRef<FJsonObject> InlineData = MakeShared<FJsonObject>();
	InlineData->SetStringField(TEXT("mimeType"), TEXT("audio/wav"));
	InlineData->SetStringField(TEXT("data"), FBase64::Encode(Wav));
	const TSharedRef<FJsonObject> Audio = MakeShared<FJsonObject>();
	Audio->SetObjectField(TEXT("inlineData"), InlineData);
	const TSharedRef<FJsonObject> Content = MakeShared<FJsonObject>();
	Content->SetArrayField(TEXT("parts"), { MakeShared<FJsonValueObject>(Audio), MakeShared<FJsonValueObject>(Prompt) });
	const TSharedRef<FJsonObject> GenerationConfig = MakeShared<FJsonObject>();
	GenerationConfig->SetNumberField(TEXT("temperature"), 0.0);
	// Writing down what was said needs little thinking; at the default level it took about three times as long.
	if (!Settings.Effort.IsEmpty() && !bTranscribeModel)
	{
		const TSharedRef<FJsonObject> ThinkingConfig = MakeShared<FJsonObject>();
		ThinkingConfig->SetStringField(TEXT("thinkingLevel"), Settings.Effort);
		GenerationConfig->SetObjectField(TEXT("thinkingConfig"), ThinkingConfig);
	}
	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetArrayField(TEXT("contents"), { MakeShared<FJsonValueObject>(Content) });
	Body->SetObjectField(TEXT("generationConfig"), GenerationConfig);

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = CharacterTalkVoice::GeminiRequest(Settings, Model, Body);
	TWeakObjectPtr<UCharacterTalkVoice> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, Wav, Model, ModelIndex, bHasNextModel, OnText = MoveTemp(OnText)](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected) mutable
	{
		if (!bConnected || !Response.IsValid())
		{
			OnText(false, TEXT("Không kết nối được Gemini để nhận giọng nói."));
			return;
		}
		const int32 Code = Response->GetResponseCode();
		UCharacterTalkVoice* Voice = WeakThis.Get();
		if (Voice && bHasNextModel && (Code == 429 || Code == 404 || Code >= 500))
		{
			UE_LOG(LogCharacterTalkVoice, Log, TEXT("Recognition model %s returned %d; trying the next one."), *Model, Code);
			Voice->TranscribeWithGemini(Wav, MoveTemp(OnText), ModelIndex + 1);
			return;
		}
		if (!EHttpResponseCodes::IsOk(Code))
		{
			OnText(false, CharacterTalkVoice::GeminiError(Response));
			return;
		}

		// Skip "thought" parts; the transcript is the plain text, or a transcription model's audioTranscription.text.
		FString Text;
		for (const TSharedPtr<FJsonValue>& Value : CharacterTalkVoice::GeminiParts(Response->GetContentAsString()))
		{
			const TSharedPtr<FJsonObject> Part = Value->AsObject();
			bool bThought = false;
			FString PartText;
			const TSharedPtr<FJsonObject>* Transcription = nullptr;
			if (!Part.IsValid() || (Part->TryGetBoolField(TEXT("thought"), bThought) && bThought))
			{
				continue;
			}
			if (Part->TryGetStringField(TEXT("text"), PartText)
				|| (Part->TryGetObjectField(TEXT("audioTranscription"), Transcription) && (*Transcription)->TryGetStringField(TEXT("text"), PartText)))
			{
				Text += PartText;
			}
		}
		Text.TrimStartAndEndInline();
		if (Text.IsEmpty())
		{
			OnText(false, TEXT("Không nghe rõ, bạn thử nói lại nhé."));
			return;
		}
		UE_LOG(LogCharacterTalkVoice, Log, TEXT("Heard: %s"), *Text);
		OnText(true, Text);
	});
	Request->ProcessRequest();
}

void UCharacterTalkVoice::BeginUtterance(AActor* Speaker, FOnClipStarted InOnClipStarted, FOnSpoken OnDone)
{
	StopPlayback();
	++SpeakRequest;
	Clips.Reset();
	NextClipToFetch = 0;
	NextClipToPlay = 0;
	ClipsInFlight = 0;
	bUtteranceOpen = true;
	bAnyClipPlayed = false;
	UtteranceError.Reset();
	HeldText.Reset();
	UtteranceSpeaker = Speaker;
	OnClipStarted = MoveTemp(InOnClipStarted);
	OnUtteranceDone = MoveTemp(OnDone);
	bPreparingSpeech = true;
}

void UCharacterTalkVoice::QueueSentence(const FString& Text)
{
	if (!bUtteranceOpen || Text.TrimStartAndEnd().IsEmpty())
	{
		return;
	}
	const UCharacterTalkSettings& Settings = *GetDefault<UCharacterTalkSettings>();
	const bool bGeminiVoice = CharacterTalkVoice::UseGemini(Settings) && Settings.bGeminiSpeaks;
	if (!bGeminiVoice || Clips.IsEmpty())
	{
		AddClip(Text.TrimStartAndEnd());
		return;
	}
	// Gemini's free quota counts requests per minute: the first sentence goes alone so speech starts soon, the rest of the
	// reply is gathered into larger pieces.
	HeldText += (HeldText.IsEmpty() ? TEXT("") : TEXT(" ")) + Text.TrimStartAndEnd();
	if (HeldText.Len() >= CharacterTalkVoice::GeminiChunkChars)
	{
		AddClip(MoveTemp(HeldText));
		HeldText.Reset();
	}
}

void UCharacterTalkVoice::AddClip(const FString& Text)
{
	FClip& Clip = Clips.AddDefaulted_GetRef();
	Clip.Text = Text;
	FetchNextClip();
}

void UCharacterTalkVoice::EndUtterance()
{
	if (!HeldText.IsEmpty())
	{
		AddClip(MoveTemp(HeldText));
		HeldText.Reset();
	}
	bUtteranceOpen = false;
	FinishUtteranceIfDone();
}

void UCharacterTalkVoice::FetchNextClip()
{
	// A few sentences ahead at once: an online voice can take longer to fetch a sentence than to say it, and fetching the next
	// ones while the current one plays avoids silent gaps. Clips still play in order.
	const int32 MaxInFlight = FMath::Max(1, GetDefault<UCharacterTalkSettings>()->MaxParallelSpeechRequests);
	while (ClipsInFlight < MaxInFlight && Clips.IsValidIndex(NextClipToFetch))
	{
		FetchClip(NextClipToFetch++);
	}
}

void UCharacterTalkVoice::FetchClip(int32 Index)
{
	++ClipsInFlight;
	const int32 Request = SpeakRequest;
	UE_LOG(LogCharacterTalkVoice, Log, TEXT("Fetching speech for sentence %d (%d characters)."), Index + 1, Clips[Index].Text.Len());

	TWeakObjectPtr<UCharacterTalkVoice> WeakThis(this);
	FetchSpeech(Clips[Index].Text, Request, [WeakThis, Index, Request](bool bOk, TArray<uint8> Wav, const FString& Error)
	{
		UCharacterTalkVoice* Voice = WeakThis.Get();
		if (!Voice || Request != Voice->SpeakRequest || !Voice->Clips.IsValidIndex(Index))
		{
			return;
		}
		FClip& Clip = Voice->Clips[Index];
		Clip.bFetched = true;
		Clip.bOk = bOk;
		Clip.Wav = MoveTemp(Wav);
		Clip.Error = Error;
		--Voice->ClipsInFlight;
		Voice->FetchNextClip();
		Voice->PlayNextClip();
	});
}

void UCharacterTalkVoice::PlayNextClip()
{
	if (IsSpeaking())
	{
		return;
	}
	while (Clips.IsValidIndex(NextClipToPlay) && Clips[NextClipToPlay].bFetched)
	{
		FClip& Clip = Clips[NextClipToPlay++];
		FString Error = Clip.Error;
		if (Clip.bOk && PlayClip(Clip.Wav, Clip.Text, Error))
		{
			Clip.Wav.Empty();
			return;
		}
		// A sentence that cannot be spoken is skipped; the reply goes on with the next one.
		UE_LOG(LogCharacterTalkVoice, Warning, TEXT("Sentence %d not spoken: %s"), NextClipToPlay, *Error);
		if (UtteranceError.IsEmpty())
		{
			UtteranceError = Error;
		}
	}
	FinishUtteranceIfDone();
}

void UCharacterTalkVoice::FinishUtteranceIfDone()
{
	if (bUtteranceOpen || ClipsInFlight > 0 || IsSpeaking() || Clips.IsValidIndex(NextClipToPlay) || !OnUtteranceDone)
	{
		return;
	}
	bPreparingSpeech = false;
	const bool bOk = bAnyClipPlayed || Clips.IsEmpty();
	OnClipStarted = nullptr;
	FOnSpoken Done = MoveTemp(OnUtteranceDone);
	OnUtteranceDone = nullptr;
	Done(bOk, bOk ? FString() : UtteranceError);
}

void UCharacterTalkVoice::FetchSpeech(const FString& Text, int32 Request, FOnAudio OnAudio)
{
	const UCharacterTalkSettings& Settings = *GetDefault<UCharacterTalkSettings>();
	if (!CharacterTalkVoice::UseGemini(Settings) || !Settings.bGeminiSpeaks)
	{
		FetchWithServer(Text, Request, MoveTemp(OnAudio));
		return;
	}
	if (!Settings.bFallbackToServerSpeech)
	{
		FetchWithGemini(Text, Request, MoveTemp(OnAudio));
		return;
	}
	if (FPlatformTime::Seconds() < GeminiSpeechPausedUntil)
	{
		FetchWithServer(Text, Request, MoveTemp(OnAudio));
		return;
	}

	// Gemini first; when it cannot speak (quota used up, no key, offline) the local Piper server reads the line instead.
	TWeakObjectPtr<UCharacterTalkVoice> WeakThis(this);
	FetchWithGemini(Text, Request, [WeakThis, Text, Request, OnAudio](bool bOk, TArray<uint8> Wav, const FString& Error) mutable
	{
		UCharacterTalkVoice* Voice = WeakThis.Get();
		if (bOk || !Voice || Request != Voice->SpeakRequest)
		{
			OnAudio(bOk, MoveTemp(Wav), Error);
			return;
		}
		UE_LOG(LogCharacterTalkVoice, Log, TEXT("Gemini could not speak (%s); using the local speech server."), *Error);
		Voice->FetchWithServer(Text, Request, [OnAudio, Error](bool bServerOk, TArray<uint8> ServerWav, const FString& ServerError) mutable
		{
			OnAudio(bServerOk, MoveTemp(ServerWav), bServerOk ? FString() : Error + TEXT(" Máy đọc giọng dự phòng cũng lỗi: ") + ServerError
				+ TEXT(" (bật Scripts/StartVoiceServer.bat để có giọng miễn phí)."));
		});
	});
}

void UCharacterTalkVoice::FetchWithServer(const FString& Text, int32 Request, FOnAudio OnAudio)
{
	const UCharacterTalkSettings& Settings = *GetDefault<UCharacterTalkSettings>();
	const TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("text"), Text);
	// Piper loads the named voice from its voices folder; speaker_id picks one person in a multi-speaker voice.
	if (!Settings.PiperVoice.IsEmpty())
	{
		Json->SetStringField(TEXT("voice"), Settings.PiperVoice);
	}
	if (Settings.PiperSpeakerId >= 0)
	{
		Json->SetNumberField(TEXT("speaker_id"), Settings.PiperSpeakerId);
	}
	FString Body;
	FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Body));

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Settings.TextToSpeechEndpoint);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetTimeout(Settings.TimeoutSeconds);
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(Body);

	TWeakObjectPtr<UCharacterTalkVoice> WeakThis(this);
	HttpRequest->OnProcessRequestComplete().BindLambda(
		[WeakThis, Request, OnAudio = MoveTemp(OnAudio)](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected) mutable
		{
			UCharacterTalkVoice* Voice = WeakThis.Get();
			if (!Voice || Request != Voice->SpeakRequest)
			{
				return;
			}
			if (!bConnected || !Response.IsValid())
			{
				OnAudio(false, {}, TEXT("Không kết nối được máy chủ đọc giọng (Piper)."));
				return;
			}
			if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()))
			{
				OnAudio(false, {}, FString::Printf(TEXT("Máy chủ đọc giọng báo lỗi %d."), Response->GetResponseCode()));
				return;
			}
			OnAudio(true, Response->GetContent(), FString());
		});
	HttpRequest->ProcessRequest();
}

void UCharacterTalkVoice::FetchWithGemini(const FString& Text, int32 Request, FOnAudio OnAudio, int32 ModelIndex)
{
	const UCharacterTalkSettings& Settings = *GetDefault<UCharacterTalkSettings>();

	// GeminiTextToSpeechModel may list several models ("a,b"); quotas are per model, so when one is used up the next is tried.
	const TArray<FString> Models = CharacterTalkVoice::ModelList(Settings.GeminiTextToSpeechModel);
	if (!Models.IsValidIndex(ModelIndex))
	{
		OnAudio(false, {}, TEXT("Chưa cấu hình model đọc giọng Gemini (GeminiTextToSpeechModel)."));
		return;
	}
	const FString Model = Models[ModelIndex];
	const bool bHasNextModel = Models.IsValidIndex(ModelIndex + 1);

	const TSharedRef<FJsonObject> TextPart = MakeShared<FJsonObject>();
	TextPart->SetStringField(TEXT("text"), Text);
	const TSharedRef<FJsonObject> Content = MakeShared<FJsonObject>();
	Content->SetArrayField(TEXT("parts"), { MakeShared<FJsonValueObject>(TextPart) });

	const TSharedRef<FJsonObject> Prebuilt = MakeShared<FJsonObject>();
	Prebuilt->SetStringField(TEXT("voiceName"), VoiceName.IsEmpty() ? Settings.GeminiVoice : VoiceName);
	const TSharedRef<FJsonObject> VoiceConfig = MakeShared<FJsonObject>();
	VoiceConfig->SetObjectField(TEXT("prebuiltVoiceConfig"), Prebuilt);
	const TSharedRef<FJsonObject> SpeechConfig = MakeShared<FJsonObject>();
	SpeechConfig->SetObjectField(TEXT("voiceConfig"), VoiceConfig);
	const TSharedRef<FJsonObject> GenerationConfig = MakeShared<FJsonObject>();
	GenerationConfig->SetArrayField(TEXT("responseModalities"), { MakeShared<FJsonValueString>(TEXT("AUDIO")) });
	GenerationConfig->SetObjectField(TEXT("speechConfig"), SpeechConfig);

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetArrayField(TEXT("contents"), { MakeShared<FJsonValueObject>(Content) });
	Body->SetObjectField(TEXT("generationConfig"), GenerationConfig);

	TWeakObjectPtr<UCharacterTalkVoice> WeakThis(this);
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = CharacterTalkVoice::GeminiRequest(Settings, Model, Body);
	HttpRequest->OnProcessRequestComplete().BindLambda(
		[WeakThis, Request, Text, Model, ModelIndex, bHasNextModel, OnAudio = MoveTemp(OnAudio)](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected) mutable
		{
			UCharacterTalkVoice* Voice = WeakThis.Get();
			if (!Voice || Request != Voice->SpeakRequest)
			{
				return;
			}
			if (!bConnected || !Response.IsValid())
			{
				OnAudio(false, {}, TEXT("Không kết nối được Gemini để đọc giọng."));
				return;
			}
			const int32 Code = Response->GetResponseCode();
			// Out of quota (429) or model gone (404): try the next listed model before giving up.
			if ((Code == 429 || Code == 404) && bHasNextModel)
			{
				UE_LOG(LogCharacterTalkVoice, Log, TEXT("Speech model %s returned %d; trying the next one."), *Model, Code);
				Voice->FetchWithGemini(Text, Request, MoveTemp(OnAudio), ModelIndex + 1);
				return;
			}
			if (Code == 429)
			{
				// Every Gemini speech model is out of quota: skip Gemini for a while instead of waiting on it for each sentence.
				Voice->GeminiSpeechPausedUntil = FPlatformTime::Seconds() + CharacterTalkVoice::GeminiSpeechPauseSeconds;
			}
			if (!EHttpResponseCodes::IsOk(Code))
			{
				OnAudio(false, {}, CharacterTalkVoice::GeminiError(Response));
				return;
			}

			for (const TSharedPtr<FJsonValue>& Value : CharacterTalkVoice::GeminiParts(Response->GetContentAsString()))
			{
				const TSharedPtr<FJsonObject> Part = Value->AsObject();
				const TSharedPtr<FJsonObject>* InlineData = nullptr;
				FString MimeType, Data;
				if (!Part.IsValid() || !Part->TryGetObjectField(TEXT("inlineData"), InlineData) || !(*InlineData)->TryGetStringField(TEXT("data"), Data))
				{
					continue;
				}
				(*InlineData)->TryGetStringField(TEXT("mimeType"), MimeType);

				TArray<uint8> Audio;
				FBase64::Decode(Data, Audio);
				// Usually a WAV; raw "audio/L16;rate=24000" PCM gets a WAV header so it plays the same way.
				if (Audio.Num() < 4 || FMemory::Memcmp(Audio.GetData(), "RIFF", 4) != 0)
				{
					int32 Rate = 24000;
					FString RateText;
					if (MimeType.Split(TEXT("rate="), nullptr, &RateText))
					{
						Rate = FMath::Max(8000, FCString::Atoi(*RateText));
					}
					TArray<uint8> Wav;
					SerializeWaveFile(Wav, Audio.GetData(), Audio.Num() & ~1, 1, Rate);
					Audio = MoveTemp(Wav);
				}
				OnAudio(true, MoveTemp(Audio), FString());
				return;
			}
			OnAudio(false, {}, TEXT("Gemini không trả về âm thanh."));
		});
	HttpRequest->ProcessRequest();
}

bool UCharacterTalkVoice::PlayClip(const TArray<uint8>& Wav, const FString& Text, FString& OutError)
{
	FWaveModInfo Info;
	FString Error;
	if (!Info.ReadWaveInfo(Wav.GetData(), Wav.Num(), &Error) || *Info.pBitsPerSample != 16 || *Info.pChannels == 0)
	{
		OutError = TEXT("Máy đọc giọng trả về file WAV không hỗ trợ (cần PCM 16-bit). ") + Error;
		return false;
	}

	const int32 Channels = *Info.pChannels;
	const int32 SampleRate = *Info.pSamplesPerSec;
	// A streamed WAV may carry a placeholder data size; trust the bytes actually received.
	const int32 Available = Wav.Num() - int32(Info.SampleDataStart - Wav.GetData());
	const int32 DataBytes = FMath::Min<int32>(Info.SampleDataSize > 0 ? Info.SampleDataSize : Available, Available) & ~1;
	const int16* Samples = reinterpret_cast<const int16*>(Info.SampleDataStart);
	const int32 Frames = DataBytes / int32(sizeof(int16)) / Channels;
	if (Frames <= 0)
	{
		OutError = TEXT("Máy đọc giọng trả về âm thanh rỗng.");
		return false;
	}

	AActor* Speaker = UtteranceSpeaker.Get();
	UWorld* World = Speaker ? Speaker->GetWorld() : (GetOuter() ? GetOuter()->GetWorld() : nullptr);
	if (!World)
	{
		OutError = TEXT("Không có world để phát âm thanh.");
		return false;
	}

	Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(SampleRate);
	Wave->NumChannels = Channels;
	Wave->Duration = float(Frames) / SampleRate;
	Wave->SoundGroup = SOUNDGROUP_Voice;
	Wave->bLooping = false;
	Wave->QueueAudio(Info.SampleDataStart, Frames * Channels * int32(sizeof(int16)));

	Playback = UGameplayStatics::SpawnSound2D(World, Wave, 1.f, 1.f, 0.f, nullptr, false, false);
	Envelope = CharacterTalkVoice::BuildEnvelope(Samples, Frames, Channels, SampleRate);
	PlaybackTime = 0.f;
	PlaybackDuration = Wave->Duration;
	bPreparingSpeech = false;
	bAnyClipPlayed = true;
	UE_LOG(LogCharacterTalkVoice, Log, TEXT("Speaking %.1fs of audio (%d Hz, %d ch)%s."), PlaybackDuration, SampleRate, Channels,
		Playback ? TEXT("") : TEXT(" - but no audio component was created (audio device off?)"));
	if (JawRigs.IsEmpty())
	{
		BindJaw(FaceTarget.IsValid() ? FaceTarget.Get() : Speaker);
	}
	if (OnClipStarted)
	{
		OnClipStarted(Text, PlaybackDuration);
	}
	return true;
}

void UCharacterTalkVoice::StopPlayback()
{
	if (Playback)
	{
		Playback->Stop();
		Playback = nullptr;
	}
	Wave = nullptr;
	Envelope.Reset();
	PlaybackTime = 0.f;
	PlaybackDuration = 0.f;
}

void UCharacterTalkVoice::StopAll()
{
	++SpeakRequest;
	if (bRecording && Capture)
	{
		Capture->StopCapturing();
	}
	bRecording = false;
	Recorded.Reset();
	StopPlayback();
	Clips.Reset();
	HeldText.Reset();
	NextClipToFetch = 0;
	NextClipToPlay = 0;
	ClipsInFlight = 0;
	bUtteranceOpen = false;
	OnClipStarted = nullptr;
	OnUtteranceDone = nullptr;
	bPreparingSpeech = false;
	// The face stays bound (blinks, mood) until Shutdown; only the speech stops.
	JawOpen = 0.f;
	EmotionTimeLeft = 0.f;
}

void UCharacterTalkVoice::Tick(float DeltaTime)
{
	if (bRecording && Capture)
	{
		// The capture synth only buffers about two seconds, so drain it every frame.
		TArray<float> Chunk;
		if (Capture->GetAudioData(Chunk) && Recorded.Num() < RecordedSampleRate * RecordedChannels * CharacterTalkVoice::MaxRecordSeconds)
		{
			Recorded.Append(Chunk);
		}
	}

	float Target = 0.f;
	if (PlaybackDuration > 0.f)
	{
		PlaybackTime += DeltaTime;
		const int32 Step = int32(PlaybackTime / CharacterTalkVoice::EnvelopeStep);
		Target = Envelope.IsValidIndex(Step) ? Envelope[Step] : 0.f;
		// Procedural sounds keep playing silence after their data runs out, so end the clip ourselves and go on to the next.
		if (PlaybackTime >= PlaybackDuration + CharacterTalkVoice::ClipGapSeconds)
		{
			StopPlayback();
			bPreparingSpeech = bUtteranceOpen || Clips.IsValidIndex(NextClipToPlay);
			PlayNextClip();
		}
	}

	JawOpen = FMath::FInterpTo(JawOpen, Target, DeltaTime, CharacterTalkVoice::JawFollowSpeed);
	TickFace(DeltaTime);
}

void UCharacterTalkVoice::SetFaceTarget(AActor* Speaker)
{
	FaceTarget = Speaker;
	BindJaw(Speaker);
}

void UCharacterTalkVoice::SetEmotion(const FString& Emotion)
{
	EmotionShape = ShapeFor(Emotion);
	EmotionTimeLeft = CharacterTalkVoice::EmotionHoldSeconds;
	UE_LOG(LogCharacterTalkVoice, Log, TEXT("Emotion: %s"), *Emotion);
}

void UCharacterTalkVoice::SetMood(const FString& Emotion, float Intensity)
{
	const FFaceShape Shape = ShapeFor(Emotion);
	MoodShape = FFaceShape::Lerp(FFaceShape(), Shape, FMath::Clamp(Intensity, 0.f, 1.f));
}

UCharacterTalkVoice::FFaceShape UCharacterTalkVoice::FFaceShape::Lerp(const FFaceShape& A, const FFaceShape& B, float Alpha)
{
	FFaceShape Out;
	Out.Smile = FMath::Lerp(A.Smile, B.Smile, Alpha);
	Out.BrowInner = FMath::Lerp(A.BrowInner, B.BrowInner, Alpha);
	Out.BrowOuter = FMath::Lerp(A.BrowOuter, B.BrowOuter, Alpha);
	Out.BrowSqueeze = FMath::Lerp(A.BrowSqueeze, B.BrowSqueeze, Alpha);
	Out.BrowTilt = FMath::Lerp(A.BrowTilt, B.BrowTilt, Alpha);
	Out.Lids = FMath::Lerp(A.Lids, B.Lids, Alpha);
	Out.MouthOpen = FMath::Lerp(A.MouthOpen, B.MouthOpen, Alpha);
	Out.HeadPitch = FMath::Lerp(A.HeadPitch, B.HeadPitch, Alpha);
	Out.HeadRoll = FMath::Lerp(A.HeadRoll, B.HeadRoll, Alpha);
	Out.LookUp = FMath::Lerp(A.LookUp, B.LookUp, Alpha);
	return Out;
}

UCharacterTalkVoice::FFaceShape UCharacterTalkVoice::ShapeFor(const FString& Emotion)
{
	// Units: Smile/Brow* are -1..1 of the configured distances, Lids 1 = half closed and -1 = wide open, angles in degrees.
	FFaceShape Shape;
	const FString Key = Emotion.TrimStartAndEnd().ToLower();
	if (Key == TEXT("vui"))
	{
		Shape.Smile = 1.f; Shape.BrowInner = 0.25f; Shape.BrowOuter = 0.35f; Shape.Lids = 0.3f; Shape.HeadPitch = -1.5f;
	}
	else if (Key == TEXT("buồn"))
	{
		Shape.Smile = -0.7f; Shape.BrowInner = 0.8f; Shape.BrowOuter = -0.4f; Shape.BrowSqueeze = 0.3f; Shape.Lids = 0.45f; Shape.HeadPitch = 4.f;
	}
	else if (Key == TEXT("ngạc nhiên"))
	{
		Shape.Smile = 0.1f; Shape.BrowInner = 1.f; Shape.BrowOuter = 1.f; Shape.Lids = -0.8f; Shape.MouthOpen = 0.5f; Shape.HeadPitch = -3.f;
	}
	else if (Key == TEXT("tức giận"))
	{
		Shape.Smile = -0.5f; Shape.BrowInner = -0.9f; Shape.BrowOuter = 0.2f; Shape.BrowSqueeze = 1.f; Shape.Lids = 0.4f; Shape.HeadPitch = 2.f;
	}
	else if (Key == TEXT("suy tư"))
	{
		Shape.Smile = -0.1f; Shape.BrowInner = 0.2f; Shape.BrowTilt = 0.8f; Shape.BrowSqueeze = 0.3f; Shape.Lids = 0.15f; Shape.HeadRoll = 4.f; Shape.LookUp = 1.f;
	}
	else
	{
		// "bình thường" and anything unknown: a calm face with a hint of a smile.
		Shape.Smile = 0.15f;
	}
	return Shape;
}

void UCharacterTalkVoice::TickFace(float DeltaTime)
{
	const UCharacterTalkSettings& Settings = *GetDefault<UCharacterTalkSettings>();
	FaceTime += DeltaTime;

	// Expression: the reply's emotion while it is spoken and a little after, otherwise the personality's resting mood.
	if (IsSpeaking() || bPreparingSpeech)
	{
		EmotionTimeLeft = FMath::Max(EmotionTimeLeft, CharacterTalkVoice::EmotionHoldSeconds);
	}
	EmotionTimeLeft = FMath::Max(0.f, EmotionTimeLeft - DeltaTime);
	const FFaceShape Target = FFaceShape::Lerp(FFaceShape(), EmotionTimeLeft > 0.f ? EmotionShape : MoodShape, Settings.EmotionStrength * Style.Expressiveness);
	Face = FFaceShape::Lerp(Face, Target, FMath::Clamp(DeltaTime * CharacterTalkVoice::ExpressionSpeed, 0.f, 1.f));

	// Blinks every few seconds, sometimes twice.
	if (BlinkPhase < 0.f)
	{
		BlinkCountdown -= DeltaTime;
		if (BlinkCountdown <= 0.f)
		{
			BlinkPhase = 0.f;
			BlinkCountdown = FMath::FRand() < 0.2f ? 0.25f : FMath::FRandRange(2.f, 5.5f) / FMath::Max(Style.BlinkRate, 0.1f);
		}
	}
	if (BlinkPhase >= 0.f)
	{
		BlinkPhase += DeltaTime / CharacterTalkVoice::BlinkSeconds;
		Blink = FMath::Sin(FMath::Clamp(BlinkPhase, 0.f, 1.f) * PI);
		if (BlinkPhase >= 1.f)
		{
			BlinkPhase = -1.f;
			Blink = 0.f;
		}
	}

	// Quick glances: at the player or aside, as often and as far as the personality looks away (up when thinking, below
	// the player's eyes when shy).
	SaccadeCountdown -= DeltaTime;
	if (SaccadeCountdown <= 0.f)
	{
		const float Range = Settings.EyeMotionDegrees * Style.GlanceRange;
		EyeTarget = FMath::FRand() >= Style.LookAway ? FVector2D::ZeroVector : FVector2D(FMath::FRandRange(-Range, Range), FMath::FRandRange(-Range, Range) * 0.5f);
		SaccadeCountdown = FMath::FRandRange(0.7f, 2.5f);
	}
	const FVector2D Look = EyeTarget + FVector2D(0.f, Style.GazeDown - Face.LookUp * Settings.EyeMotionDegrees * 1.5f);
	EyeLook = FMath::Vector2DInterpTo(EyeLook, Look, DeltaTime, 25.f);

	// Talking: brows lift on loud syllables, the head moves with the rhythm.
	TalkWeight = FMath::FInterpTo(TalkWeight, IsSpeaking() ? 1.f : 0.f, DeltaTime, 3.f);
	SpeechBrow = FMath::FInterpTo(SpeechBrow, IsSpeaking() ? FMath::Clamp((JawOpen - 0.5f) * 2.f, 0.f, 1.f) : 0.f, DeltaTime, 6.f);
	const float Motion = Settings.HeadMotionDegrees * Style.HeadMotion * (0.35f + 0.65f * TalkWeight);
	HeadAngles.X = Style.HeadPitch + Face.HeadPitch + Motion * (0.6f * FMath::PerlinNoise1D(FaceTime * 0.6f) + 0.4f * JawOpen);
	HeadAngles.Y = Motion * 0.8f * FMath::PerlinNoise1D(FaceTime * 0.35f + 17.f);
	HeadAngles.Z = Style.HeadRoll + Face.HeadRoll + Motion * 0.5f * FMath::PerlinNoise1D(FaceTime * 0.27f + 41.f);

	// Mutable rebuilds the mesh when the character changes; bind again to the new one.
	RebindCountdown -= DeltaTime;
	if (RebindCountdown <= 0.f)
	{
		RebindCountdown = 1.f;
		const bool bStale = JawRigs.ContainsByPredicate([](const FJawRig& Rig) { return !Rig.Mesh.IsValid() || Rig.Mesh->GetSkeletalMeshAsset() != Rig.Asset.Get(); });
		if (FaceTarget.IsValid() && (bStale || JawRigs.IsEmpty()))
		{
			BindJaw(FaceTarget.Get());
		}
	}
	UpdateMetaHumanFace();
}

void UCharacterTalkVoice::UpdateMetaHumanFace()
{
	USkeletalMeshComponent* FaceMesh = MetaHumanFace.Get();
	UTalkFaceAnimInstance* Anim = FaceMesh ? Cast<UTalkFaceAnimInstance>(FaceMesh->GetAnimInstance()) : nullptr;
	if (!Anim)
	{
		return;
	}
	const UCharacterTalkSettings& Settings = *GetDefault<UCharacterTalkSettings>();

	// Values are 0..1 on MetaHuman expression controls; the face shape's signed values pick which control of a pair moves.
	// Contributions to one control add up (a smirk on top of a smile).
	TMap<FName, float> Values;
	auto Add = [&Values](const FString& Name, float Value)
	{
		if (Value > 0.f)
		{
			Values.FindOrAdd(FName(TEXT("CTRL_expressions_") + Name)) += Value;
		}
	};
	auto Both = [&Add](const TCHAR* Name, float Value)
	{
		Add(FString(Name) + TEXT("L"), Value);
		Add(FString(Name) + TEXT("R"), Value);
	};

	// Speech: the jaw follows the voice's loudness (as wide as the personality speaks), the upper lip lifts a little on
	// loud syllables. Between words the lips close, pressed together as firmly as the personality holds them.
	Add(TEXT("jawOpen"), (JawOpen * Settings.MetaHumanJawOpen + Face.MouthOpen * 0.45f) * Style.Jaw);
	Both(TEXT("mouthUpperLipRaise"), JawOpen * 0.2f);
	const float Press = Style.LipPress * FMath::Clamp(1.f - JawOpen * 3.f - Face.MouthOpen, 0.f, 1.f);
	for (const TCHAR* Lip : { TEXT("mouthLipsTogetherUL"), TEXT("mouthLipsTogetherUR"), TEXT("mouthLipsTogetherDL"), TEXT("mouthLipsTogetherDR") })
	{
		Add(Lip, Press);
	}

	if (Settings.bLivelyFace)
	{
		// MetaHuman controls need values near 1 to read clearly at talking distance, so the face shape is mapped generously.
		const float Smile = Face.Smile + TalkWeight * 0.1f;
		Both(TEXT("mouthCornerPull"), Smile * 1.1f);
		Both(TEXT("eyeCheekRaise"), Smile * 0.5f);
		Both(TEXT("mouthCornerDepress"), -Smile * 1.2f);
		Both(TEXT("mouthStretch"), -Smile * 0.3f);

		// A one-sided smile (the personality's), stronger while talking, with a dimple and a slight squint on that side.
		const float Smirk = FMath::Abs(Style.Smirk) * (0.6f + 0.4f * TalkWeight);
		const TCHAR* SmirkSide = Style.Smirk >= 0.f ? TEXT("L") : TEXT("R");
		Add(FString(TEXT("mouthCornerPull")) + SmirkSide, Smirk);
		Add(FString(TEXT("mouthDimple")) + SmirkSide, Smirk * 0.5f);
		Add(FString(TEXT("eyeCheekRaise")) + SmirkSide, Smirk * 0.3f);

		// Brows: sad raises the inner ends, anger pulls them down and together (with a wrinkled nose), thinking tilts one up.
		const float Lift = SpeechBrow * 0.3f * Style.TalkBrows;
		Both(TEXT("browRaiseIn"), (Face.BrowInner + Lift) * 1.2f);
		Add(TEXT("browRaiseOuterL"), (Face.BrowOuter + Lift + Face.BrowTilt * 0.8f) * 1.2f);
		Add(TEXT("browRaiseOuterR"), (Face.BrowOuter + Lift - Face.BrowTilt * 0.3f) * 1.2f);
		Both(TEXT("browDown"), (FMath::Max(-Face.BrowInner, 0.f) + FMath::Max(-Face.BrowOuter, 0.f) * 0.5f) * 1.3f);
		Both(TEXT("browLateral"), Face.BrowSqueeze * 1.2f);
		Both(TEXT("noseWrinkle"), -Face.BrowInner * Face.BrowSqueeze * 0.7f);
		Both(TEXT("jawClench"), -Face.BrowInner * Face.BrowSqueeze * 0.5f);

		// Lids: blinks, squints (Lids > 0) and wide eyes (Lids < 0). A smile already narrows the eyes through the cheeks,
		// so it squints less (a full smile with a full squint shuts them).
		const float Squint = FMath::Max(Face.Lids, 0.f) * (1.f - 0.6f * FMath::Clamp(Smile, 0.f, 1.f));
		Both(TEXT("eyeBlink"), Blink + Squint * 0.2f);
		Both(TEXT("eyeSquintInner"), Squint * 0.9f);
		Both(TEXT("eyeWiden"), FMath::Max(-Face.Lids, 0.f) * (1.f - Blink));

		// Glances (degrees to the controls' range; positive Y looks down).
		const float Side = EyeLook.X / 30.f;
		const float Down = EyeLook.Y / 25.f;
		Both(TEXT("eyeLookLeft"), Side);
		Both(TEXT("eyeLookRight"), -Side);
		Both(TEXT("eyeLookDown"), Down);
		Both(TEXT("eyeLookUp"), -Down);
	}

	TArray<TPair<FName, float>> Curves;
	for (const TPair<FName, float>& Value : Values)
	{
		Curves.Emplace(Value.Key, FMath::Min(Value.Value, 1.f));
	}
	Anim->SetCurves(MoveTemp(Curves));
}

void UCharacterTalkVoice::BindJaw(AActor* Speaker)
{
	UnbindJaw();
	if (!Speaker)
	{
		return;
	}

	// A configured MetaHuman face replaces the speaker's head; the rig below then only turns the head it follows.
	MetaHumanFace = UTalkFaceAnimInstance::AttachTo(Speaker);

	const FName JawBone = GetDefault<UCharacterTalkSettings>()->JawBone;
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(Speaker);
	for (USkeletalMeshComponent* Candidate : Meshes)
	{
		// The MetaHuman face, and body meshes that copy the head's pose through it, are not faces to bind.
		if (Candidate && (Candidate->ComponentHasTag(UTalkFaceAnimInstance::MetaHumanTag) || Cast<UTalkFaceAnimInstance>(Candidate->GetAnimInstance())))
		{
			continue;
		}
		// Followers copy their leader's pose, so the face is moved on the leader.
		USkeletalMeshComponent* Mesh = Candidate ? Cast<USkeletalMeshComponent>(Candidate->LeaderPoseComponent.Get()) : nullptr;
		if (!Mesh)
		{
			Mesh = Candidate;
		}
		USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
		const int32 Jaw = Mesh ? Mesh->GetBoneIndex(JawBone) : INDEX_NONE;
		if (!Asset || Jaw == INDEX_NONE || JawRigs.ContainsByPredicate([Mesh](const FJawRig& Rig) { return Rig.Mesh == Mesh; }))
		{
			continue;
		}

		FJawRig& Rig = JawRigs.AddDefaulted_GetRef();
		Rig.Mesh = Mesh;
		Rig.Asset = Asset;
		Rig.Jaw = Jaw;
		Rig.LeftJawline = Mesh->GetBoneIndex(CharacterTalkVoice::LeftJawline);
		Rig.RightJawline = Mesh->GetBoneIndex(CharacterTalkVoice::RightJawline);
		Rig.MouthLower = Mesh->GetBoneIndex(CharacterTalkVoice::MouthLower);

		const FReferenceSkeleton& Skeleton = Asset->GetRefSkeleton();
		auto Subtree = [&Skeleton](int32 Root)
		{
			TArray<int32> Bones;
			if (Root == INDEX_NONE)
			{
				return Bones;
			}
			for (int32 Bone = 0; Bone < Skeleton.GetNum(); ++Bone)
			{
				bool bUnderRoot = Bone == Root;
				for (int32 Parent = Skeleton.GetParentIndex(Bone); Parent != INDEX_NONE && !bUnderRoot; Parent = Skeleton.GetParentIndex(Parent))
				{
					bUnderRoot = Parent == Root;
				}
				if (bUnderRoot)
				{
					Bones.Add(Bone);
				}
			}
			return Bones;
		};
		auto Group = [&Subtree, Mesh](std::initializer_list<const TCHAR*> Names)
		{
			TArray<int32> Bones;
			for (const TCHAR* Name : Names)
			{
				for (const int32 Bone : Subtree(Mesh->GetBoneIndex(Name)))
				{
					Bones.AddUnique(Bone);
				}
			}
			return Bones;
		};

		// Each bone (with everything under it) turns by its share of the jaw angle; the first share given to a bone wins.
		TMap<int32, float> Shares;
		for (const int32 Bone : Subtree(Jaw))
		{
			Shares.Add(Bone, 1.f);
		}
		int32 MouthBones = 0;
		for (const TPair<FName, float>& Follower : CharacterTalkVoice::MouthFollowers)
		{
			const int32 Root = Mesh->GetBoneIndex(Follower.Key);
			MouthBones += Root != INDEX_NONE ? 1 : 0;
			for (const int32 Bone : Subtree(Root))
			{
				if (!Shares.Contains(Bone))
				{
					Shares.Add(Bone, Follower.Value);
				}
			}
		}
		for (const TPair<int32, float>& Share : Shares)
		{
			Rig.Bones.Emplace(Share.Key, Share.Value);
		}

		// Expression bones of the face rig.
		Rig.LeftEye = Mesh->GetBoneIndex(TEXT("FACIAL_L_Eye"));
		Rig.RightEye = Mesh->GetBoneIndex(TEXT("FACIAL_R_Eye"));
		Rig.Head = Mesh->GetBoneIndex(TEXT("head"));
		Rig.NoseTip = Mesh->GetBoneIndex(TEXT("FACIAL_C_NoseTip"));
		Rig.LeftEyeBones = Group({ TEXT("FACIAL_L_Eye") });
		Rig.RightEyeBones = Group({ TEXT("FACIAL_R_Eye") });
		Rig.LeftUpperLid = Group({ TEXT("FACIAL_L_EyelidUpperA"), TEXT("FACIAL_L_EyelidUpperB") });
		Rig.RightUpperLid = Group({ TEXT("FACIAL_R_EyelidUpperA"), TEXT("FACIAL_R_EyelidUpperB") });
		Rig.LeftLowerLid = Group({ TEXT("FACIAL_L_EyelidLowerA"), TEXT("FACIAL_L_EyelidLowerB") });
		Rig.RightLowerLid = Group({ TEXT("FACIAL_R_EyelidLowerA"), TEXT("FACIAL_R_EyelidLowerB") });
		Rig.LeftBrowInner = Group({ TEXT("FACIAL_L_ForeheadIn") });
		Rig.RightBrowInner = Group({ TEXT("FACIAL_R_ForeheadIn") });
		Rig.LeftBrowMid = Group({ TEXT("FACIAL_L_ForeheadMid") });
		Rig.RightBrowMid = Group({ TEXT("FACIAL_R_ForeheadMid") });
		Rig.LeftBrowOuter = Group({ TEXT("FACIAL_L_ForeheadOut") });
		Rig.RightBrowOuter = Group({ TEXT("FACIAL_R_ForeheadOut") });
		Rig.LeftCorner = Group({ TEXT("FACIAL_L_LipCorner") });
		Rig.RightCorner = Group({ TEXT("FACIAL_R_LipCorner") });
		Rig.LeftCheek = Group({ TEXT("FACIAL_L_CheekInner"), TEXT("FACIAL_L_CheekOuter") });
		Rig.RightCheek = Group({ TEXT("FACIAL_R_CheekInner"), TEXT("FACIAL_R_CheekOuter") });
		Rig.UpperLip = Group({ TEXT("FACIAL_C_LipUpper"), TEXT("FACIAL_L_LipUpper"), TEXT("FACIAL_R_LipUpper"),
			TEXT("FACIAL_L_LipUpperOuter"), TEXT("FACIAL_R_LipUpperOuter"), TEXT("FACIAL_C_MouthUpper") });
		Rig.HeadBones = Subtree(Rig.Head);

		Rig.Handle = Mesh->RegisterOnBoneTransformsFinalizedDelegate(
			FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateUObject(this, &UCharacterTalkVoice::ApplyJaw, JawRigs.Num() - 1));
		UE_LOG(LogCharacterTalkVoice, Log, TEXT("Face on '%s': jaw %d, %d of %d mouth bones, eyes %s, brows %s, head %s."),
			*Mesh->GetName(), Jaw, MouthBones, CharacterTalkVoice::MouthFollowers.Num(),
			Rig.LeftUpperLid.Num() > 0 ? TEXT("yes") : TEXT("no"), Rig.LeftBrowInner.Num() > 0 ? TEXT("yes") : TEXT("no"),
			Rig.HeadBones.Num() > 0 ? TEXT("yes") : TEXT("no"));
	}

	if (JawRigs.IsEmpty())
	{
		UE_LOG(LogCharacterTalkVoice, Warning, TEXT("'%s' has no bone '%s'; no lip sync or facial expressions."), *Speaker->GetName(), *JawBone.ToString());
	}
}

void UCharacterTalkVoice::UnbindJaw()
{
	for (const FJawRig& Rig : JawRigs)
	{
		if (USkeletalMeshComponent* Mesh = Rig.Mesh.Get())
		{
			Mesh->UnregisterOnBoneTransformsFinalizedDelegate(Rig.Handle);
		}
	}
	JawRigs.Reset();
}

void UCharacterTalkVoice::ApplyJaw(int32 RigIndex)
{
	if (!JawRigs.IsValidIndex(RigIndex))
	{
		return;
	}
	const FJawRig& Rig = JawRigs[RigIndex];
	USkeletalMeshComponent* Mesh = Rig.Mesh.Get();
	if (!Mesh || Mesh->GetSkeletalMeshAsset() != Rig.Asset.Get())
	{
		return;
	}

	// The animation has already written this frame's pose into the read buffer; moving bones there is what gets rendered.
	// The pose is rebuilt from the animation every frame, so the changes never accumulate.
	TArray<FTransform>& Pose = const_cast<TArray<FTransform>&>(Mesh->GetComponentSpaceTransforms());
	if (!Pose.IsValidIndex(Rig.Jaw))
	{
		return;
	}
	const UCharacterTalkSettings& Settings = *GetDefault<UCharacterTalkSettings>();

	// The face's left-right axis, from the jawline bones; the component's up is the face's up.
	FVector Lateral = FVector::ZeroVector;
	if (Pose.IsValidIndex(Rig.LeftJawline) && Pose.IsValidIndex(Rig.RightJawline))
	{
		Lateral = (Pose[Rig.LeftJawline].GetLocation() - Pose[Rig.RightJawline].GetLocation()).GetSafeNormal();
	}
	if (Lateral.IsNearlyZero())
	{
		Lateral = Pose[Rig.Jaw].GetUnitAxis(EAxis::Y);
	}
	const FVector Up = FVector::UpVector;
	const FVector Forward = FVector::CrossProduct(Lateral, Up).GetSafeNormal();

	auto Turn = [&Pose](const TArray<int32>& Bones, const FVector& Pivot, const FQuat& Rotation)
	{
		for (const int32 Bone : Bones)
		{
			if (Pose.IsValidIndex(Bone))
			{
				FTransform& Transform = Pose[Bone];
				Transform.SetLocation(Pivot + Rotation.RotateVector(Transform.GetLocation() - Pivot));
				Transform.SetRotation(Rotation * Transform.GetRotation());
			}
		}
	};
	auto Move = [&Pose](const TArray<int32>& Bones, const FVector& Offset)
	{
		for (const int32 Bone : Bones)
		{
			if (Pose.IsValidIndex(Bone))
			{
				Pose[Bone].AddToTranslation(Offset);
			}
		}
	};
	// A rotation about Axis by Degrees, flipped if needed so that it moves Probe (relative to Pivot) up (bUp) or down.
	auto Directed = [](const FVector& Axis, float Degrees, const FVector& Probe, bool bUp)
	{
		const FQuat Rotation(Axis, FMath::DegreesToRadians(FMath::Abs(Degrees)));
		const bool bRaises = Rotation.RotateVector(Probe).Z > Rotation.Inverse().RotateVector(Probe).Z;
		const FQuat Chosen = (bRaises == bUp) ? Rotation : Rotation.Inverse();
		return Degrees >= 0.f ? Chosen : Chosen.Inverse();
	};

	// The whole head, done last so everything moved before turns with it: positive pitch nods down (towards the nose),
	// plus turn and tilt.
	auto TurnHead = [&]()
	{
		if (Rig.HeadBones.Num() > 0 && Pose.IsValidIndex(Rig.Head))
		{
			const FVector Pivot = Pose[Rig.Head].GetLocation();
			const FVector Nose = Pose.IsValidIndex(Rig.NoseTip) ? Pose[Rig.NoseTip].GetLocation() - Pivot : Forward * 10.f;
			const FQuat Pitch = Directed(Lateral, HeadAngles.X, Nose, false);
			const FQuat Yaw(Up, FMath::DegreesToRadians(HeadAngles.Y));
			const FQuat Roll(Forward, FMath::DegreesToRadians(HeadAngles.Z));
			Turn(Rig.HeadBones, Pivot, Yaw * Roll * Pitch);
		}
	};

	// A MetaHuman face copies this pose and shows jaw, lids, brows and eyes itself (UpdateMetaHumanFace); only the head turns.
	if (MetaHumanFace.IsValid())
	{
		if (Settings.bLivelyFace)
		{
			TurnHead();
		}
		return;
	}

	// Jaw and lower lip: the voice's loudness plus the expression (a surprised mouth stays a little open).
	const float JawDegrees = (JawOpen + Face.MouthOpen * 0.35f) * Settings.JawOpenDegrees * Style.Jaw;
	if (JawDegrees > 0.05f)
	{
		const FVector Pivot = Pose[Rig.Jaw].GetLocation();
		const FVector Chin = Pose.IsValidIndex(Rig.MouthLower) ? Pose[Rig.MouthLower].GetLocation() - Pivot : -Up;
		const FQuat JawTurn = Directed(Lateral, JawDegrees, Chin, false);
		for (const TPair<int32, float>& Share : Rig.Bones)
		{
			if (Pose.IsValidIndex(Share.Key))
			{
				const FQuat BoneTurn = Share.Value >= 1.f ? JawTurn : FQuat::Slerp(FQuat::Identity, JawTurn, Share.Value);
				FTransform& Transform = Pose[Share.Key];
				Transform.SetLocation(Pivot + BoneTurn.RotateVector(Transform.GetLocation() - Pivot));
				Transform.SetRotation(BoneTurn * Transform.GetRotation());
			}
		}
	}

	if (!Settings.bLivelyFace)
	{
		return;
	}

	// Brows: inner, middle and outer parts move separately (sad raises the inner ends, anger pulls them down and together).
	const float Brow = Settings.BrowMoveCm;
	const float Lift = SpeechBrow * 0.6f * Style.TalkBrows;
	Move(Rig.LeftBrowInner, Up * (Face.BrowInner + Lift) * Brow - Lateral * Face.BrowSqueeze * Brow * 0.5f);
	Move(Rig.RightBrowInner, Up * (Face.BrowInner + Lift) * Brow + Lateral * Face.BrowSqueeze * Brow * 0.5f);
	Move(Rig.LeftBrowMid, Up * ((Face.BrowInner + Face.BrowOuter) * 0.5f + Lift + Face.BrowTilt * 0.5f) * Brow);
	Move(Rig.RightBrowMid, Up * ((Face.BrowInner + Face.BrowOuter) * 0.5f + Lift - Face.BrowTilt * 0.3f) * Brow);
	Move(Rig.LeftBrowOuter, Up * (Face.BrowOuter + Lift + Face.BrowTilt) * Brow);
	Move(Rig.RightBrowOuter, Up * (Face.BrowOuter + Lift - Face.BrowTilt * 0.6f) * Brow);

	// Mouth corners up and out for a smile, down for sad or angry; cheeks rise with a smile; the upper lip lifts on loud sounds.
	const float Smile = Face.Smile + TalkWeight * 0.1f;
	const float Corner = Settings.SmileCm;
	Move(Rig.LeftCorner, Up * Smile * Corner + Lateral * FMath::Max(Smile, 0.f) * Corner * 0.6f);
	Move(Rig.RightCorner, Up * Smile * Corner - Lateral * FMath::Max(Smile, 0.f) * Corner * 0.6f);
	Move(Rig.LeftCheek, Up * FMath::Max(Smile, 0.f) * Corner * 0.6f);
	Move(Rig.RightCheek, Up * FMath::Max(Smile, 0.f) * Corner * 0.6f);
	Move(Rig.UpperLip, Up * JawOpen * Corner * 0.35f);

	// Eyelids turn about each eye: blinks and squints close the upper lid, wide eyes open it; squints also lift the lower lid.
	auto Lids = [&](int32 Eye, const TArray<int32>& Upper, const TArray<int32>& Lower)
	{
		if (!Pose.IsValidIndex(Eye))
		{
			return;
		}
		const FVector Pivot = Pose[Eye].GetLocation();
		const float Close = FMath::Clamp(Blink + Face.Lids * 0.35f, -0.25f, 1.f);
		if (Upper.Num() > 0 && Pose.IsValidIndex(Upper[0]))
		{
			Turn(Upper, Pivot, Directed(Lateral, Close * Settings.BlinkDegrees, Pose[Upper[0]].GetLocation() - Pivot, false));
		}
		const float Raise = FMath::Max(Face.Lids, 0.f) * 0.3f + Blink * 0.15f;
		if (Lower.Num() > 0 && Pose.IsValidIndex(Lower[0]))
		{
			Turn(Lower, Pivot, Directed(Lateral, Raise * Settings.BlinkDegrees, Pose[Lower[0]].GetLocation() - Pivot, true));
		}
	};
	Lids(Rig.LeftEye, Rig.LeftUpperLid, Rig.LeftLowerLid);
	Lids(Rig.RightEye, Rig.RightUpperLid, Rig.RightLowerLid);

	// Eyes glance (yaw about up, pitch about the face's left-right axis; positive pitch looks down).
	auto Glance = [&](int32 Eye, const TArray<int32>& Bones)
	{
		if (Pose.IsValidIndex(Eye))
		{
			const FVector Pivot = Pose[Eye].GetLocation();
			const FQuat Pitch = Directed(Lateral, EyeLook.Y, Forward * 10.f, false);
			Turn(Bones, Pivot, FQuat(Up, FMath::DegreesToRadians(EyeLook.X)) * Pitch);
		}
	};
	Glance(Rig.LeftEye, Rig.LeftEyeBones);
	Glance(Rig.RightEye, Rig.RightEyeBones);

	TurnHead();
}
