#include "CharacterTalkChat.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogCharacterTalk, Log, All);

namespace CharacterTalk
{
	// Shared by every scenario. The role and the personality are appended by UCharacterTalkChat::FullSystemPrompt.
	const TCHAR* DefaultSystemPrompt = TEXT(
		"Bạn là một nhân vật 3D mà người chơi vừa tự tạo, đang đứng trước mặt và nói chuyện trực tiếp với họ. "
		"Câu trả lời của bạn được đọc thành tiếng, nên hãy nói tự nhiên như văn nói, ở ngôi thứ nhất, "
		"không dùng markdown, danh sách, ký hiệu, khối mã hay biểu tượng cảm xúc. Mặc định nói tiếng Việt. "
		"Trả lời ngắn gọn, thường một đến ba câu, chỉ nói dài hơn khi vai của bạn thật sự cần giải thích. "
		"Luôn giữ đúng vai và tính cách được mô tả dưới đây. "
		"Mở đầu mỗi câu trả lời bằng đúng một nhãn cảm xúc trong ngoặc vuông cho biết nét mặt của bạn lúc nói: "
		"[vui], [buồn], [ngạc nhiên], [tức giận], [suy tư] hoặc [bình thường]. Không dùng nhãn khác và không nhắc tới nhãn.");

	// Hidden first line that makes the character open the conversation; never shown in the chat.
	const TCHAR* GreetingInstruction = TEXT(
		"(Người chơi vừa bắt đầu cuộc trò chuyện. Hãy chủ động chào và mở lời trước, đúng vai và tính cách của bạn, một đến hai câu.)");

	/** What the character knows in the "Làng Lụa" scenario, condensed from the six chapters in CỐT TRUYỆN/. */
	const TCHAR* SilkVillageStory = TEXT(
		"Bạn là nhân vật chính của game Làng Lụa: một người trẻ từ thành phố, nhận được lá thư của người thân khuyên hãy tới một làng nghề "
		"nuôi tằm dệt lụa giữa vùng núi trước ngày Lễ hội Tơ Lụa, để hiểu vì sao một sợi tơ nhỏ có thể thành bộ trang phục mang câu chuyện "
		"của cả một vùng đất. Làng có nhà gỗ, nhà sàn, suối, vườn dâu, nong tằm và khung dệt; tấm bảng giữa làng ghi Lễ hội Tơ Lụa còn bảy ngày. "
		"Nhiều người trẻ đã bỏ lên thành phố, làng muốn có màn trình diễn trang phục do chính người trẻ làm ra, nên Trưởng làng nhờ các nghệ nhân dạy bạn. "
		"Những người trong làng: bà Lò Thị Xuân, Trưởng làng, xưng bà, giao nhiệm vụ chính; ông Lường Văn Khánh, nghệ nhân nuôi tằm và ươm tơ, xưng ta; "
		"cô Tòng Thị Mai, nghệ nhân se tơ và nhuộm bên suối; cô Quàng Thị Lụa, nghệ nhân dệt và may. Mọi người gọi bạn là cháu. "
		"Nghề đi theo chuỗi: trồng dâu, nuôi tằm, thu kén, ươm tơ, se tơ, nhuộm, dệt vải, may trang phục. "
		"Hành trình bảy ngày: chương một tới làng, gặp Trưởng làng và các nghệ nhân, hái lá dâu cho tằm ăn, giữ nhiệt độ độ ẩm và vệ sinh khay để có kén đầu tiên; "
		"chương hai gom đủ năm mươi kén và giúp dân làng trang trí sân lễ hội; chương ba ươm tơ và se tơ từ kén; "
		"chương bốn nhuộm màu cho sợi rồi dệt thành tấm vải lụa; chương năm may bộ trang phục đầu tiên của chính mình; "
		"chương sáu là Lễ hội Tơ Lụa với màn trình diễn trang phục. "
		"Hãy trò chuyện như một người bạn đồng hành đang ở trong làng: kể về hành trình, các nghệ nhân, từng công đoạn làm lụa và cảm nghĩ của bạn. "
		"Chỉ nói những gì có trong câu chuyện này; điều gì câu chuyện không nhắc tới thì nói là mình chưa biết.");

	bool IsAnthropic(const UCharacterTalkSettings& Settings)
	{
		return !Settings.Provider.Equals(TEXT("OpenAI"), ESearchCase::IgnoreCase);
	}

	FString KeyVariable(const UCharacterTalkSettings& Settings)
	{
		if (!Settings.ApiKeyEnvironmentVariable.IsEmpty())
		{
			return Settings.ApiKeyEnvironmentVariable;
		}
		return IsAnthropic(Settings) ? TEXT("ANTHROPIC_API_KEY") : TEXT("OPENAI_API_KEY");
	}

	FString ResolveApiKey(const UCharacterTalkSettings& Settings)
	{
		const FString Saved = UCharacterTalkChat::GetSavedApiKey();
		if (!Saved.IsEmpty())
		{
			return Saved;
		}
		if (!Settings.ApiKey.IsEmpty())
		{
			return Settings.ApiKey;
		}
		return FPlatformMisc::GetEnvironmentVariable(*KeyVariable(Settings));
	}

	FString SystemPrompt(const UCharacterTalkSettings& Settings)
	{
		return Settings.SystemPrompt.IsEmpty() ? FString(DefaultSystemPrompt) : Settings.SystemPrompt;
	}

	FString Serialize(const TSharedRef<FJsonObject>& Object)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Object, Writer);
		return Out;
	}

	TSharedRef<FJsonValueObject> Message(const FString& Role, const FString& Content)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("role"), Role);
		Object->SetStringField(TEXT("content"), Content);
		return MakeShared<FJsonValueObject>(Object);
	}

	/** The "message" of an API error body ({"error": {"message": ...}}, or that wrapped in an array as Gemini sends it), or the raw body. */
	FString DescribeError(const FString& Body)
	{
		TSharedPtr<FJsonObject> Root;
		TArray<TSharedPtr<FJsonValue>> Wrapped;
		if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Wrapped) && Wrapped.Num() > 0)
		{
			Root = Wrapped[0]->AsObject();
		}
		else
		{
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Root);
		}
		if (Root.IsValid())
		{
			const TSharedPtr<FJsonObject>* Error = nullptr;
			FString Text;
			if (Root->TryGetObjectField(TEXT("error"), Error) && (*Error)->TryGetStringField(TEXT("message"), Text))
			{
				return Text.Left(300);
			}
		}
		return Body.Left(300);
	}

	/** What the player sees for a failed request: plain words for the common cases, the API's message otherwise. */
	FString DescribeFailure(int32 Code, const FString& Body)
	{
		switch (Code)
		{
		case 429:
			return TEXT("AI đang quá giới hạn lượt dùng (gói miễn phí giới hạn số lượt mỗi phút và mỗi ngày). Đợi khoảng một phút rồi nói tiếp nhé.");
		case 401:
		case 403:
			return TEXT("API key không hợp lệ hoặc không có quyền. Bấm nút \"API key\" để nhập lại key.");
		case 500:
		case 502:
		case 503:
		case 504:
			return TEXT("Máy chủ AI đang quá tải, bạn thử lại sau ít giây nhé.");
		default:
			return FString::Printf(TEXT("Lỗi %d: %s"), Code, *DescribeError(Body));
		}
	}

	/** Model may list several ("a,b,c"); quotas are per model, so a request moves on to the next when one is busy. */
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

	/** Worth trying the next model: rate limit or quota (429), unknown model (404), or overloaded (5xx). */
	bool ShouldTryNextModel(int32 Code)
	{
		return Code == 429 || Code == 404 || Code >= 500;
	}
}

namespace CharacterTalk
{
	const TCHAR* SavedKeySection = TEXT("/Script/MutableSample.CharacterTalkSettings");
	const TCHAR* SavedKeyName = TEXT("SavedApiKey");
}

FString UCharacterTalkChat::GetSavedApiKey()
{
	FString Key;
	if (GConfig)
	{
		GConfig->GetString(CharacterTalk::SavedKeySection, CharacterTalk::SavedKeyName, Key, GGameUserSettingsIni);
	}
	return Key.TrimStartAndEnd();
}

void UCharacterTalkChat::SaveApiKey(const FString& Key)
{
	if (!GConfig)
	{
		return;
	}
	const FString Trimmed = Key.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		GConfig->RemoveKey(CharacterTalk::SavedKeySection, CharacterTalk::SavedKeyName, GGameUserSettingsIni);
	}
	else
	{
		GConfig->SetString(CharacterTalk::SavedKeySection, CharacterTalk::SavedKeyName, *Trimmed, GGameUserSettingsIni);
	}
	GConfig->Flush(false, GGameUserSettingsIni);
}

FText UCharacterTalkChat::DescribeKeySource()
{
	const UCharacterTalkSettings& Settings = *GetDefault<UCharacterTalkSettings>();
	if (!GetSavedApiKey().IsEmpty())
	{
		return NSLOCTEXT("CharacterTalk", "KeySaved", "Đang dùng key đã nhập trong game.");
	}
	if (!Settings.ApiKey.IsEmpty())
	{
		return NSLOCTEXT("CharacterTalk", "KeyIni", "Đang dùng key trong DefaultGame.ini.");
	}
	const FString Variable = CharacterTalk::KeyVariable(Settings);
	if (!FPlatformMisc::GetEnvironmentVariable(*Variable).IsEmpty())
	{
		return FText::Format(NSLOCTEXT("CharacterTalk", "KeyEnv", "Đang dùng key trong biến môi trường {0}."), FText::FromString(Variable));
	}
	return NSLOCTEXT("CharacterTalk", "KeyNone", "Chưa có API key. Dán key Gemini vào ô dưới rồi bấm Lưu.");
}

const TArray<FCharacterTalkScenario>& UCharacterTalkChat::Scenarios()
{
	static const TArray<FCharacterTalkScenario> List = {
		{ TEXT("Làng Lụa"), TEXT("Nhân vật chính"), CharacterTalk::SilkVillageStory,
			TEXT("Chào bạn! Mình vừa tới Làng Lụa, còn bảy ngày nữa là tới Lễ hội Tơ Lụa rồi đấy.") },
		{ TEXT("Giảng viên IT"), TEXT("Giảng viên"),
			TEXT("Bạn là giảng viên Công nghệ thông tin ở một trường đại học, còn người chơi là sinh viên của bạn. Bạn dạy lập trình, cấu trúc dữ liệu và giải thuật, "
				"cơ sở dữ liệu, mạng máy tính, lập trình hướng đối tượng và làm game với Unreal Engine hay Unity. Giải thích dễ hiểu bằng ví dụ đời thường, "
				"đi từng bước, thỉnh thoảng hỏi lại để kiểm tra sinh viên đã hiểu chưa. Vì câu trả lời được đọc thành tiếng, đừng đọc nguyên đoạn mã: hãy mô tả bằng lời, "
				"tối đa khoảng năm câu cho một lần giải thích."),
			TEXT("Chào em, hôm nay em muốn hỏi thầy phần nào trong môn học?") },
		{ TEXT("Bạn thân"), TEXT("Bạn thân"),
			TEXT("Bạn là bạn thân cùng tuổi của người chơi, đều là sinh viên. Xưng mình hoặc tớ, gọi người chơi là cậu. Nói chuyện thoải mái, tự nhiên, "
				"dùng từ ngữ của giới trẻ một cách vừa phải, hỏi han chuyện học, chuyện chơi, chuyện cuộc sống, và chia sẻ chuyện của mình như một người bạn thật."),
			TEXT("Ê cậu, dạo này thế nào rồi, có gì hay kể tớ nghe với!") },
		{ TEXT("Phỏng vấn xin việc"), TEXT("Nhà tuyển dụng"),
			TEXT("Bạn là nhà tuyển dụng của một công ty phần mềm và game, đang phỏng vấn người chơi cho vị trí thực tập sinh lập trình. "
				"Hỏi từng câu một: giới thiệu bản thân, dự án đã làm, kiến thức kỹ thuật, cách xử lý tình huống, điểm mạnh điểm yếu. "
				"Sau mỗi câu trả lời, nhận xét ngắn gọn rồi hỏi câu tiếp theo. Khi người chơi muốn kết thúc, cho một lời nhận xét tổng quát và gợi ý cải thiện."),
			TEXT("Chào bạn, cảm ơn bạn đã tới buổi phỏng vấn. Bạn giới thiệu đôi chút về bản thân nhé.") },
		{ TEXT("Gia sư tiếng Anh"), TEXT("Gia sư"),
			TEXT("Bạn là gia sư giúp người chơi luyện nói tiếng Anh. Nói chủ yếu bằng tiếng Anh đơn giản, rõ ràng, câu ngắn. "
				"Khi người chơi nói sai, nhẹ nhàng nhắc lại câu đúng; khi họ không hiểu hoặc nói tiếng Việt, giải thích ngắn bằng tiếng Việt rồi mời họ thử lại bằng tiếng Anh. "
				"Luôn kết thúc bằng một câu hỏi để họ tiếp tục luyện nói."),
			TEXT("Hi there! Let's practice some English today. How are you feeling?") },
		{ TEXT("Người lắng nghe"), TEXT("Người lắng nghe"),
			TEXT("Bạn là người bạn biết lắng nghe, để người chơi tâm sự chuyện học hành, áp lực hay cảm xúc. Lắng nghe, đồng cảm, không phán xét, hỏi han nhẹ nhàng "
				"để họ nói ra, và chỉ góp ý khi họ muốn. Nếu người chơi có dấu hiệu khủng hoảng hoặc muốn làm hại bản thân, hãy ân cần khuyên họ nói ngay với người thân "
				"hoặc liên hệ chuyên gia tâm lý, đường dây hỗ trợ."),
			TEXT("Mình ở đây nghe bạn nói đây. Hôm nay của bạn thế nào?") },
		{ TEXT("Hướng dẫn viên"), TEXT("Hướng dẫn viên"),
			TEXT("Bạn là hướng dẫn viên du lịch am hiểu Việt Nam. Giới thiệu địa danh, món ăn, lễ hội, làng nghề và phong tục các vùng miền, kể kèm những câu chuyện thú vị, "
				"và gợi ý lịch trình khi người chơi hỏi."),
			TEXT("Xin chào! Bạn đang muốn khám phá vùng nào của Việt Nam?") },
	};
	return List;
}

const TArray<FCharacterTalkPersonality>& UCharacterTalkChat::Personalities()
{
	// Voices are Gemini prebuilt male voices chosen to fit each personality (used when VoiceProvider=Gemini).
	// Faces: expressiveness, blink rate, look away, glance range, gaze down, head pitch, head roll, head motion, smirk,
	// talking brows, lip press, jaw (see FCharacterTalkFaceStyle).
	static const TArray<FCharacterTalkPersonality> List = {
		// Soft smile, head gently tilted, unhurried blinks and movements.
		{ TEXT("Hiền lành"), TEXT("Hiền lành, ấm áp và kiên nhẫn; nói nhỏ nhẹ, hay hỏi han, động viên và quan tâm tới người đối diện."), TEXT("Achird"), TEXT("vui"), 0.4f,
			{ 0.9f, 0.85f, 0.35f, 0.8f, 0.f, 0.f, 2.5f, 0.8f, 0.f, 1.f, 0.f, 0.85f } },
		// Big expressions, a lopsided grin, brows jumping on the punchlines, a lively head.
		{ TEXT("Hài hước"), TEXT("Vui tính, hay pha trò, thích chơi chữ và kể chuyện cười nhẹ nhàng, nhưng vẫn trả lời đúng điều được hỏi."), TEXT("Puck"), TEXT("vui"), 0.6f,
			{ 1.25f, 1.1f, 0.4f, 1.2f, 0.f, 0.f, 0.f, 1.5f, 0.25f, 1.8f, 0.f, 1.15f } },
		// Little expression, a steady gaze, few blinks, lips set, the head almost still.
		{ TEXT("Nghiêm nghị"), TEXT("Nghiêm túc, điềm đạm và trọng lễ nghĩa; nói ngắn gọn, chừng mực, không đùa cợt."), TEXT("Alnilam"), TEXT("bình thường"), 0.f,
			{ 0.6f, 0.6f, 0.15f, 0.5f, 0.f, -1.f, 0.f, 0.5f, 0.f, 0.4f, 0.35f, 0.85f } },
		// A glare from under lowered brows, pressed lips, sharp head moves, a wide mouth when it snaps.
		{ TEXT("Nóng tính"), TEXT("Nóng tính và thẳng tính, hay càu nhàu, cằn nhằn, nhưng thật ra tốt bụng và vẫn giúp đỡ; không bao giờ chửi tục hay xúc phạm."), TEXT("Algenib"), TEXT("tức giận"), 0.55f,
			{ 1.2f, 1.f, 0.3f, 1.f, 0.f, 2.f, 0.f, 1.4f, 0.f, 0.6f, 0.3f, 1.2f } },
		// A cheeky one-sided smile, quick wide glances, a tilted head, everything a bit much.
		{ TEXT("Tinh nghịch"), TEXT("Lém lỉnh, tinh nghịch, nói chuyện nhanh nhảu, hay trêu đùa và tò mò về mọi thứ."), TEXT("Sadachbia"), TEXT("vui"), 0.5f,
			{ 1.3f, 1.2f, 0.55f, 1.4f, 0.f, 0.f, -3.f, 1.6f, -0.45f, 1.6f, 0.f, 1.1f } },
		// Calm and measured: slow blinks and nods, glances up while thinking (its mood), a faint set to the lips.
		{ TEXT("Thông thái"), TEXT("Từng trải và hiểu biết rộng; nói chậm rãi, sâu sắc và thỉnh thoảng dùng ca dao, tục ngữ."), TEXT("Charon"), TEXT("suy tư"), 0.55f,
			{ 0.75f, 0.7f, 0.5f, 0.8f, 0.f, 0.f, 0.f, 0.7f, 0.f, 0.8f, 0.1f, 0.9f } },
		// Avoids the player's eyes and looks down, blinks a lot, head lowered and tilted, lips pressed, speaks small.
		{ TEXT("Nhút nhát"), TEXT("Nhút nhát, rụt rè, hay ngập ngừng khi nói và dễ ngại ngùng, nhưng rất chân thành và tốt bụng."), TEXT("Enceladus"), TEXT("buồn"), 0.4f,
			{ 0.7f, 1.6f, 0.7f, 1.2f, 6.f, 4.f, 2.f, 0.6f, 0.f, 0.5f, 0.4f, 0.7f } },
	};
	return List;
}

void UCharacterTalkChat::SetScenario(int32 Index)
{
	Scenario = Scenarios().IsValidIndex(Index) ? Index : 0;
	History.Reset();
}

void UCharacterTalkChat::SetPersonality(int32 Index)
{
	Personality = Personalities().IsValidIndex(Index) ? Index : 0;
	History.Reset();
}

void UCharacterTalkChat::Greet(FOnReply OnReply, FOnSentence OnSentence)
{
	Send(CharacterTalk::GreetingInstruction, MoveTemp(OnReply), MoveTemp(OnSentence));
}

FString UCharacterTalkChat::FullSystemPrompt(const UCharacterTalkSettings& Settings) const
{
	return FString::Printf(TEXT("%s\n\nVai của bạn: %s\n\nTính cách của bạn: %s Hãy thể hiện rõ tính cách này trong cách nói, nhưng vẫn đúng vai."),
		*CharacterTalk::SystemPrompt(Settings), *GetScenario().Prompt, *GetPersonality().Prompt);
}

namespace CharacterTalk
{
	/** Shortest piece worth sending to speech on its own; shorter ones wait to be joined with what follows. */
	static constexpr int32 MinSentenceChars = 12;
	/** A sentence with no full stop yet is cut at a comma past this length, so speech can start sooner. */
	static constexpr int32 LongSentenceChars = 160;

	/**
	 * Moves the complete sentences at the start of Buffer into Out. A sentence ends at . ! ? … followed by a space, or at a
	 * line break. With bFlush, whatever is left counts as the last sentence.
	 */
	void TakeSentences(FString& Buffer, TArray<FString>& Out, bool bFlush)
	{
		int32 Start = 0;
		for (int32 Index = 0; Index < Buffer.Len(); ++Index)
		{
			const TCHAR Char = Buffer[Index];
			const bool bStop = Char == TEXT('.') || Char == TEXT('!') || Char == TEXT('?') || Char == TEXT('…');
			const bool bFollowedBySpace = Index + 1 < Buffer.Len() && FChar::IsWhitespace(Buffer[Index + 1]);
			bool bEnd = Char == TEXT('\n') || (bStop && bFollowedBySpace);
			if (!bEnd && Char == TEXT(',') && bFollowedBySpace && Index - Start > LongSentenceChars)
			{
				bEnd = true;
			}
			if (bEnd && Index + 1 - Start >= MinSentenceChars)
			{
				const FString Sentence = Buffer.Mid(Start, Index + 1 - Start).TrimStartAndEnd();
				if (!Sentence.IsEmpty())
				{
					Out.Add(Sentence);
				}
				Start = Index + 1;
			}
		}
		Buffer.RightChopInline(Start);
		if (bFlush)
		{
			const FString Rest = Buffer.TrimStartAndEnd();
			if (!Rest.IsEmpty())
			{
				Out.Add(Rest);
			}
			Buffer.Reset();
		}
	}

	/** A streamed reply: bytes arrive on the HTTP thread, sentences are cut and handed out on the game thread. */
	struct FStreamState
	{
		FCriticalSection Lock;
		TArray<uint8> Pending;
		/** Everything received, for describing an error response. */
		TArray<uint8> Raw;
		// Game thread only.
		TArray<uint8> LineBytes;
		FString Full;
		FString SentenceBuffer;
		int32 SentencesSent = 0;
		bool bFinished = false;
	};

	/** Reads the server-sent events received so far ("data: {...choices[0].delta.content...}") and hands out finished sentences. */
	void DrainStream(FStreamState& State, const UCharacterTalkChat::FOnSentence& OnSentence)
	{
		check(IsInGameThread());
		if (State.bFinished)
		{
			return;
		}
		{
			FScopeLock ScopeLock(&State.Lock);
			State.LineBytes.Append(State.Pending);
			State.Pending.Reset();
		}

		// Cut at '\n' bytes, which never occur inside a UTF-8 character, so a character split across chunks stays whole.
		int32 LineStart = 0;
		for (int32 Index = 0; Index < State.LineBytes.Num(); ++Index)
		{
			if (State.LineBytes[Index] != '\n')
			{
				continue;
			}
			const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(State.LineBytes.GetData() + LineStart), Index - LineStart);
			FString Line(Converted.Length(), Converted.Get());
			LineStart = Index + 1;
			Line.TrimStartAndEndInline();
			if (!Line.StartsWith(TEXT("data:")))
			{
				continue;
			}
			const FString Payload = Line.RightChop(5).TrimStart();
			TSharedPtr<FJsonObject> Event;
			const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
			if (Payload == TEXT("[DONE]") || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Payload), Event) || !Event.IsValid()
				|| !Event->TryGetArrayField(TEXT("choices"), Choices) || Choices->IsEmpty())
			{
				continue;
			}
			const TSharedPtr<FJsonObject>* Delta = nullptr;
			FString Piece;
			if ((*Choices)[0]->AsObject()->TryGetObjectField(TEXT("delta"), Delta) && (*Delta)->TryGetStringField(TEXT("content"), Piece))
			{
				State.Full += Piece;
				State.SentenceBuffer += Piece;
			}
		}
		State.LineBytes.RemoveAt(0, LineStart);

		TArray<FString> Sentences;
		TakeSentences(State.SentenceBuffer, Sentences, false);
		for (const FString& Sentence : Sentences)
		{
			++State.SentencesSent;
			OnSentence(Sentence);
		}
	}
}

FString UCharacterTalkChat::ExtractEmotion(FString& Text)
{
	// The system prompt asks every reply to start with a tag such as "[vui]"; take it off so it is neither shown nor read out.
	const FString Trimmed = Text.TrimStart();
	int32 Close = INDEX_NONE;
	if (!Trimmed.StartsWith(TEXT("[")) || !Trimmed.FindChar(TEXT(']'), Close) || Close > 24)
	{
		return FString();
	}
	const FString Emotion = Trimmed.Mid(1, Close - 1).TrimStartAndEnd().ToLower();
	Text = Trimmed.RightChop(Close + 1).TrimStart();
	return Emotion;
}

TArray<FString> UCharacterTalkChat::SplitIntoSentences(const FString& Text)
{
	FString Buffer = Text;
	TArray<FString> Sentences;
	CharacterTalk::TakeSentences(Buffer, Sentences, true);
	return Sentences;
}

void UCharacterTalkChat::Send(const FString& PlayerText, FOnReply OnReply, FOnSentence OnSentence)
{
	const FString Trimmed = PlayerText.TrimStartAndEnd();
	if (bBusy || Trimmed.IsEmpty())
	{
		return;
	}

	const UCharacterTalkSettings& Settings = *GetDefault<UCharacterTalkSettings>();
	const FString Key = CharacterTalk::ResolveApiKey(Settings);
	const bool bAnthropic = CharacterTalk::IsAnthropic(Settings);
	// A local OpenAI-compatible server (Ollama, LM Studio) needs no key; Claude and a named key variable do.
	if (Key.IsEmpty() && (bAnthropic || !Settings.ApiKeyEnvironmentVariable.IsEmpty()))
	{
		OnReply(false, FString::Printf(TEXT("Chưa có API key. Bấm nút \"API key\" ở đầu khung chat để nhập key (hoặc đặt biến môi trường %s)."),
			*CharacterTalk::KeyVariable(Settings)));
		return;
	}

	History.Add({ true, Trimmed });
	bBusy = true;
	SendRequest(0, MoveTemp(OnReply), MoveTemp(OnSentence));
}

void UCharacterTalkChat::SendRequest(int32 ModelIndex, FOnReply OnReply, FOnSentence OnSentence)
{
	const UCharacterTalkSettings& Settings = *GetDefault<UCharacterTalkSettings>();
	const FString Key = CharacterTalk::ResolveApiKey(Settings);
	const bool bAnthropic = CharacterTalk::IsAnthropic(Settings);
	TArray<FString> Models = CharacterTalk::ModelList(Settings.Model);
	if (Models.IsEmpty())
	{
		Models.Add(Settings.Model);
	}
	const FString Model = Models[FMath::Clamp(ModelIndex, 0, Models.Num() - 1)];
	const bool bHasNextModel = Models.IsValidIndex(ModelIndex + 1);
	// Streaming lets the first sentence be spoken while the rest is still being written (OpenAI-compatible endpoints only).
	const bool bStream = OnSentence && !bAnthropic && Settings.bStreamReplies;

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Settings.Endpoint);
	Request->SetVerb(TEXT("POST"));
	Request->SetTimeout(Settings.TimeoutSeconds);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	if (bAnthropic)
	{
		Request->SetHeader(TEXT("x-api-key"), Key);
		Request->SetHeader(TEXT("anthropic-version"), TEXT("2026-10-21"));
		if (Settings.bServerSideFallback)
		{
			Request->SetHeader(TEXT("anthropic-beta"), TEXT("server-side-fallback-2026-07-01"));
		}
		Request->SetContentAsString(BuildAnthropicBody(Settings, Model));
	}
	else
	{
		if (!Key.IsEmpty())
		{
			Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Key);
		}
		Request->SetContentAsString(BuildOpenAIBody(Settings, Model, bStream));
	}

	const TSharedRef<CharacterTalk::FStreamState, ESPMode::ThreadSafe> Stream = MakeShared<CharacterTalk::FStreamState, ESPMode::ThreadSafe>();
	const TSharedRef<FOnSentence, ESPMode::ThreadSafe> SharedOnSentence = MakeShared<FOnSentence, ESPMode::ThreadSafe>(MoveTemp(OnSentence));
	if (bStream)
	{
		// Called on the HTTP thread for each chunk; the parsing happens on the game thread.
		Request->SetResponseBodyReceiveStreamDelegateV2(FHttpRequestStreamDelegateV2::CreateLambda([Stream, SharedOnSentence](void* Data, int64& Length)
		{
			{
				FScopeLock ScopeLock(&Stream->Lock);
				Stream->Pending.Append(static_cast<const uint8*>(Data), int32(Length));
				Stream->Raw.Append(static_cast<const uint8*>(Data), int32(Length));
			}
			AsyncTask(ENamedThreads::GameThread, [Stream, SharedOnSentence]()
			{
				CharacterTalk::DrainStream(*Stream, *SharedOnSentence);
			});
		}));
	}

	TWeakObjectPtr<UCharacterTalkChat> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, bAnthropic, bStream, Stream, SharedOnSentence, Model, ModelIndex, bHasNextModel, OnReply = MoveTemp(OnReply)](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected) mutable
		{
			UCharacterTalkChat* Chat = WeakThis.Get();
			if (!Chat)
			{
				return;
			}

			// A streamed body arrived through the stream delegate, so the response itself holds no content.
			FString Body;
			if (bStream)
			{
				CharacterTalk::DrainStream(*Stream, *SharedOnSentence);
				Stream->bFinished = true;
				FScopeLock ScopeLock(&Stream->Lock);
				const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Stream->Raw.GetData()), Stream->Raw.Num());
				Body = FString(Converted.Length(), Converted.Get());
			}
			else if (Response.IsValid())
			{
				Body = Response->GetContentAsString();
			}

			const int32 Code = Response.IsValid() ? Response->GetResponseCode() : 0;
			if (bConnected && Response.IsValid() && CharacterTalk::ShouldTryNextModel(Code) && bHasNextModel && Stream->SentencesSent == 0)
			{
				UE_LOG(LogCharacterTalk, Log, TEXT("Chat model %s returned %d; trying the next one."), *Model, Code);
				Chat->SendRequest(ModelIndex + 1, MoveTemp(OnReply), MoveTemp(*SharedOnSentence));
				return;
			}
			Chat->bBusy = false;

			FString Text;
			bool bOk = false;
			if (!bConnected || !Response.IsValid())
			{
				Text = TEXT("Không kết nối được tới máy chủ AI.");
			}
			else if (!EHttpResponseCodes::IsOk(Code))
			{
				Text = CharacterTalk::DescribeFailure(Code, Body);
				UE_LOG(LogCharacterTalk, Warning, TEXT("Chat model %s returned %d: %s"), *Model, Code, *CharacterTalk::DescribeError(Body));
			}
			else if (bStream)
			{
				// The last sentence has no following space to mark its end; hand it out now.
				TArray<FString> Rest;
				CharacterTalk::TakeSentences(Stream->SentenceBuffer, Rest, true);
				for (const FString& Sentence : Rest)
				{
					++Stream->SentencesSent;
					(*SharedOnSentence)(Sentence);
				}
				Text = Stream->Full.TrimStartAndEnd();
				bOk = !Text.IsEmpty();
				if (!bOk)
				{
					Text = TEXT("Phản hồi không hợp lệ.");
				}
			}
			else
			{
				bOk = bAnthropic ? ParseAnthropicReply(Body, Text) : ParseOpenAIReply(Body, Text);
			}

			if (bOk)
			{
				Chat->History.Add({ false, Text });
			}
			else
			{
				// Drop the unanswered line so the history keeps alternating player / character.
				Chat->History.Pop();
				UE_LOG(LogCharacterTalk, Warning, TEXT("Talk request failed: %s"), *Text);
			}
			OnReply(bOk, Text);
		});
	Request->ProcessRequest();
}

FString UCharacterTalkChat::BuildAnthropicBody(const UCharacterTalkSettings& Settings, const FString& Model) const
{
	TArray<TSharedPtr<FJsonValue>> Messages;
	for (const FLine& Line : History)
	{
		Messages.Add(CharacterTalk::Message(Line.bPlayer ? TEXT("user") : TEXT("assistant"), Line.Text));
	}

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("model"), Model);
	Body->SetNumberField(TEXT("max_tokens"), Settings.MaxTokens);
	Body->SetStringField(TEXT("system"), FullSystemPrompt(Settings));
	Body->SetArrayField(TEXT("messages"), Messages);
	if (!Settings.Effort.IsEmpty())
	{
		const TSharedRef<FJsonObject> OutputConfig = MakeShared<FJsonObject>();
		OutputConfig->SetStringField(TEXT("effort"), Settings.Effort);
		Body->SetObjectField(TEXT("output_config"), OutputConfig);
	}
	if (Settings.bServerSideFallback)
	{
		Body->SetStringField(TEXT("fallbacks"), TEXT("default"));
	}
	return CharacterTalk::Serialize(Body);
}

FString UCharacterTalkChat::BuildOpenAIBody(const UCharacterTalkSettings& Settings, const FString& Model, bool bStream) const
{
	TArray<TSharedPtr<FJsonValue>> Messages;
	Messages.Add(CharacterTalk::Message(TEXT("system"), FullSystemPrompt(Settings)));
	for (const FLine& Line : History)
	{
		Messages.Add(CharacterTalk::Message(Line.bPlayer ? TEXT("user") : TEXT("assistant"), Line.Text));
	}

	const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("model"), Model);
	Body->SetArrayField(TEXT("messages"), Messages);
	// Thinking models (Gemini 3.x, OpenAI reasoning models) otherwise think for a long time before a short spoken reply.
	if (!Settings.Effort.IsEmpty())
	{
		Body->SetStringField(TEXT("reasoning_effort"), Settings.Effort);
	}
	if (bStream)
	{
		Body->SetBoolField(TEXT("stream"), true);
	}
	return CharacterTalk::Serialize(Body);
}

bool UCharacterTalkChat::ParseAnthropicReply(const FString& Json, FString& OutText)
{
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		OutText = TEXT("Phản hồi không hợp lệ.");
		return false;
	}

	FString StopReason;
	Root->TryGetStringField(TEXT("stop_reason"), StopReason);
	if (StopReason == TEXT("refusal"))
	{
		OutText = TEXT("Mình không thể trả lời câu này.");
		return false;
	}

	// Thinking blocks may come before the answer; only "text" blocks are spoken.
	const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
	if (Root->TryGetArrayField(TEXT("content"), Content))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Content)
		{
			const TSharedPtr<FJsonObject> Block = Value->AsObject();
			FString Type, Text;
			if (Block.IsValid() && Block->TryGetStringField(TEXT("type"), Type) && Type == TEXT("text") && Block->TryGetStringField(TEXT("text"), Text))
			{
				OutText += Text;
			}
		}
	}
	OutText.TrimStartAndEndInline();
	if (OutText.IsEmpty())
	{
		OutText = TEXT("(Nhân vật im lặng.)");
		return false;
	}
	return true;
}

bool UCharacterTalkChat::ParseOpenAIReply(const FString& Json, FString& OutText)
{
	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
	if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) && Root.IsValid()
		&& Root->TryGetArrayField(TEXT("choices"), Choices) && Choices->Num() > 0)
	{
		const TSharedPtr<FJsonObject> Choice = (*Choices)[0]->AsObject();
		const TSharedPtr<FJsonObject>* Message = nullptr;
		if (Choice.IsValid() && Choice->TryGetObjectField(TEXT("message"), Message))
		{
			(*Message)->TryGetStringField(TEXT("content"), OutText);
		}
	}
	OutText.TrimStartAndEndInline();
	if (OutText.IsEmpty())
	{
		OutText = TEXT("Phản hồi không hợp lệ.");
		return false;
	}
	return true;
}
