// Copyright Epic Games, Inc. All Rights Reserved.

#include "UiTranslationSubsystem.h"

#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"

namespace
{
	/**
	 * English label -> Vietnamese. Keys are matched after trimming spaces and a trailing ':'. Covers the main menu,
	 * the lobby HUD hints and every section, parameter (name and friendly name) and option of CO_Character.
	 * Add a row here when a new label shows up in English.
	 */
	const TMap<FString, FString>& Table()
	{
		static const TMap<FString, FString> Map = {
			// Main menu and settings
			{ TEXT("MAIN MENU"), TEXT("MENU CHÍNH") }, { TEXT("Start Game"), TEXT("Bắt đầu") }, { TEXT("Exit"), TEXT("Thoát") },
			{ TEXT("Settings"), TEXT("Cài đặt") }, { TEXT("SETTINGS"), TEXT("CÀI ĐẶT") }, { TEXT("Back"), TEXT("Quay lại") },
			{ TEXT("Brightness"), TEXT("Độ sáng") }, { TEXT("Adjust the screen brightness."), TEXT("Điều chỉnh độ sáng màn hình.") },
			{ TEXT("Drag left to dim, right to brighten."), TEXT("Kéo sang trái để tối, sang phải để sáng.") },
			{ TEXT("Confirm Configuration"), TEXT("Xác nhận tạo hình") }, { TEXT("Options"), TEXT("Tùy chọn") },
			{ TEXT("Wireframe"), TEXT("Khung lưới") }, { TEXT("Infinite Level"), TEXT("Màn Infinite") }, { TEXT("Showcase Level"), TEXT("Màn Showcase") },
			// Lobby HUD hints and the customization GUI's own words
			{ TEXT("Zoom in/out"), TEXT("Phóng to/thu nhỏ") }, { TEXT("Rotate Camera"), TEXT("Xoay camera") }, { TEXT("Navigate UI"), TEXT("Di chuyển giao diện") },
			{ TEXT("Modify Projector"), TEXT("Chỉnh hình dán") }, { TEXT("Apply Projection"), TEXT("Áp hình dán") },
			{ TEXT("Add Sticker +"), TEXT("Thêm hình dán +") }, { TEXT("Opacity"), TEXT("Độ trong suốt") }, { TEXT("Section"), TEXT("Mục") },
			{ TEXT("Sex"), TEXT("Giới tính") }, { TEXT("Female"), TEXT("Nữ") }, { TEXT("Male"), TEXT("Nam") },
			// Sections
			{ TEXT("General"), TEXT("Chung") }, { TEXT("Cloth"), TEXT("Trang phục") }, { TEXT("Jackets"), TEXT("Áo khoác") }, { TEXT("Shirts"), TEXT("Áo") },
			{ TEXT("Head Accessories"), TEXT("Phụ kiện đầu") }, { TEXT("Body Shape"), TEXT("Vóc dáng") }, { TEXT("Jaw"), TEXT("Cằm") },
			{ TEXT("Pants"), TEXT("Quần") }, { TEXT("Ears"), TEXT("Tai") }, { TEXT("Eyes"), TEXT("Mắt") }, { TEXT("Hair Male"), TEXT("Tóc") },
			{ TEXT("Arms"), TEXT("Tay") }, { TEXT("Mouth"), TEXT("Miệng") }, { TEXT("Nose"), TEXT("Mũi") }, { TEXT("Shoes"), TEXT("Giày") },
			{ TEXT("Tattoo"), TEXT("Hình xăm") }, { TEXT("Hair"), TEXT("Tóc") },
			// Parameters (names and friendly names)
			{ TEXT("Aged"), TEXT("Độ cũ") }, { TEXT("Wear"), TEXT("Độ sờn") }, { TEXT("Aged Clothes"), TEXT("Độ sờn quần áo") },
			{ TEXT("Anticto Sticker"), TEXT("Hình dán Anticto") }, { TEXT("Base Color"), TEXT("Màu chính") }, { TEXT("Beard"), TEXT("Râu") },
			{ TEXT("Beard Nasim"), TEXT("Râu") }, { TEXT("Body Blend"), TEXT("Pha trộn vóc dáng") }, { TEXT("Body Type"), TEXT("Vóc dáng") },
			{ TEXT("Body Type 1"), TEXT("Vóc dáng") }, { TEXT("Body Type 2"), TEXT("Vóc dáng") }, { TEXT("Chest ClawCut"), TEXT("Vết cào ngực") },
			{ TEXT("Shape Influence"), TEXT("Độ biến dạng") }, { TEXT("Chin Influence"), TEXT("Độ biến dạng cằm") },
			{ TEXT("Chinos Base Color"), TEXT("Màu quần kaki") }, { TEXT("Chinos Front Pockets"), TEXT("Túi trước") }, { TEXT("Shape"), TEXT("Hình dáng") },
			{ TEXT("ChinType"), TEXT("Dáng cằm") }, { TEXT("Circuit Spots"), TEXT("Đốm mạch") }, { TEXT("Secondary Color"), TEXT("Màu phụ") },
			{ TEXT("Color Cloth and Shoes"), TEXT("Màu phụ") }, { TEXT("Position"), TEXT("Vị trí") }, { TEXT("Size"), TEXT("Kích thước") },
			{ TEXT("EarPosition"), TEXT("Vị trí tai") }, { TEXT("EarSize"), TEXT("Cỡ tai") }, { TEXT("Eye Color"), TEXT("Màu mắt") },
			{ TEXT("Eye Influence"), TEXT("Độ biến dạng mắt") }, { TEXT("Brow Size"), TEXT("Độ dày lông mày") }, { TEXT("Eyebrow"), TEXT("Lông mày") },
			{ TEXT("EyeType"), TEXT("Dáng mắt") }, { TEXT("Freckles"), TEXT("Tàn nhang") }, { TEXT("Jacket"), TEXT("Áo khoác") },
			{ TEXT("Multilayer Stickers"), TEXT("Hình dán nhiều lớp") }, { TEXT("Back Pockets"), TEXT("Túi sau") }, { TEXT("Dirt"), TEXT("Độ bẩn") },
			{ TEXT("Front Pockets"), TEXT("Túi trước") }, { TEXT("Jeans Aged"), TEXT("Độ cũ") }, { TEXT("Jeans Dirt"), TEXT("Độ bẩn") },
			{ TEXT("Jeans Back Pockets"), TEXT("Túi sau") }, { TEXT("Jeans Front Pockets"), TEXT("Túi trước") },
			{ TEXT("Left Arm"), TEXT("Tay trái") }, { TEXT("Left Tattoo Color"), TEXT("Màu hình xăm tay trái") },
			{ TEXT("Lips Influence"), TEXT("Độ biến dạng môi") }, { TEXT("Lips Shape"), TEXT("Dáng môi") }, { TEXT("LipsType"), TEXT("Dáng môi") },
			{ TEXT("Logo Anticto Sticker"), TEXT("Logo Anticto") }, { TEXT("Make Up"), TEXT("Trang điểm") },
			{ TEXT("MouthPosition"), TEXT("Vị trí miệng") }, { TEXT("MouthSize"), TEXT("Cỡ miệng") }, { TEXT("Mustache"), TEXT("Ria mép") },
			{ TEXT("Mustache Nasim"), TEXT("Ria mép") }, { TEXT("Mutable Sticker"), TEXT("Hình dán Mutable") },
			{ TEXT("Nose Influence"), TEXT("Độ biến dạng mũi") }, { TEXT("NoseSize"), TEXT("Cỡ mũi") }, { TEXT("NoseType"), TEXT("Dáng mũi") },
			{ TEXT("Pants Style"), TEXT("Kiểu quần") }, { TEXT("Patience Sticker"), TEXT("Hình dán Patience") },
			{ TEXT("Pattern Color A"), TEXT("Màu hoa văn A") }, { TEXT("Pattern Color B"), TEXT("Màu hoa văn B") },
			{ TEXT("Pattern Color C"), TEXT("Màu hoa văn C") }, { TEXT("Pattern Color D"), TEXT("Màu hoa văn D") },
			{ TEXT("Patterns TShirt"), TEXT("Hoa văn áo thun") }, { TEXT("Right Arm"), TEXT("Tay phải") },
			{ TEXT("Right Tattoo Color"), TEXT("Màu hình xăm tay phải") }, { TEXT("Scifi  Dirt"), TEXT("Độ bẩn") },
			{ TEXT("Scifi Back Pockets"), TEXT("Túi sau") }, { TEXT("Scifi Base Color"), TEXT("Màu chính") }, { TEXT("Belt"), TEXT("Thắt lưng") },
			{ TEXT("Scifi Belt"), TEXT("Thắt lưng") }, { TEXT("Scifi Front Pockets"), TEXT("Túi trước") }, { TEXT("Pattern"), TEXT("Hoa văn") },
			{ TEXT("Scifi Patern"), TEXT("Hoa văn") }, { TEXT("Socks"), TEXT("Tất") }, { TEXT("Scifi Socks"), TEXT("Tất") },
			{ TEXT("Waist Buckles"), TEXT("Khóa thắt lưng") }, { TEXT("Scifi Waist Buckles"), TEXT("Khóa thắt lưng") },
			{ TEXT("Seam Trails"), TEXT("Đường chỉ may") }, { TEXT("Shoes Color 1"), TEXT("Màu giày 1") }, { TEXT("Shoes Color 2"), TEXT("Màu giày 2") },
			{ TEXT("Shoes Color 3"), TEXT("Màu giày 3") }, { TEXT("Shoes Color 4"), TEXT("Màu giày 4") }, { TEXT("Shoes Style"), TEXT("Kiểu giày") },
			{ TEXT("Skin Tone"), TEXT("Màu da") }, { TEXT("Sport Base Color"), TEXT("Màu chính") }, { TEXT("Sport Front Pockets"), TEXT("Túi trước") },
			{ TEXT("Star Sticker"), TEXT("Hình dán ngôi sao") }, { TEXT("Image"), TEXT("Hình in") }, { TEXT("Tanktop Image"), TEXT("Hình in") },
			{ TEXT("TanktTop Pattern"), TEXT("Hoa văn") }, { TEXT("Tattoo Arm L Picture"), TEXT("Hình xăm tay trái") },
			{ TEXT("Tattoo Arm L Position"), TEXT("Vị trí hình xăm tay trái") }, { TEXT("Tattoo Atomic"), TEXT("Hình xăm nguyên tử") },
			{ TEXT("Tattoo Forearm R Picture"), TEXT("Hình xăm cẳng tay phải") }, { TEXT("Tattoo Forearm R Position"), TEXT("Vị trí hình xăm cẳng tay phải") },
			{ TEXT("Tattoo Leg L"), TEXT("Hình xăm chân trái") }, { TEXT("Tattoo Leg L Picture"), TEXT("Mẫu hình xăm chân trái") },
			{ TEXT("Tattoo Leg L Position"), TEXT("Vị trí hình xăm chân trái") }, { TEXT("Tattoo Lightning"), TEXT("Hình xăm tia chớp") },
			{ TEXT("Tattoo Lightning Position"), TEXT("Vị trí hình xăm tia chớp") }, { TEXT("Tattoo Set 1"), TEXT("Bộ hình xăm") },
			{ TEXT("Tattoo Set 1 Color"), TEXT("Màu bộ hình xăm") }, { TEXT("Tattoo Set 1 Picture"), TEXT("Mẫu bộ hình xăm") },
			{ TEXT("Tattoo Set 1 Position"), TEXT("Vị trí bộ hình xăm") }, { TEXT("Pants Color"), TEXT("Màu quần") },
			{ TEXT("ThaiPants Color"), TEXT("Màu quần") }, { TEXT("Tunic Color"), TEXT("Màu áo") }, { TEXT("ThaiTunic Color"), TEXT("Màu áo") },
			{ TEXT("Trousers Color"), TEXT("Màu quần") }, { TEXT("Color"), TEXT("Màu") }, { TEXT("TShirt Colors"), TEXT("Màu áo") },
			{ TEXT("TShirt Stickers Set"), TEXT("Hình dán áo thun") }, { TEXT("TShirt Stickers Set Picture"), TEXT("Mẫu hình dán áo thun") },
			{ TEXT("TShirt Stickers Set Position"), TEXT("Vị trí hình dán áo thun") }, { TEXT("Jackets Projector Front"), TEXT("Hình dán nhiều lớp") },
			// Options
			{ TEXT("Slim"), TEXT("Mảnh khảnh") }, { TEXT("Regular"), TEXT("Cân đối") }, { TEXT("Strong"), TEXT("Vạm vỡ") },
			{ TEXT("Overweight"), TEXT("Đầy đặn") }, { TEXT("Original"), TEXT("Nguyên bản") }, { TEXT("Light Brown"), TEXT("Nâu nhạt") },
			{ TEXT("Brown"), TEXT("Nâu") }, { TEXT("Dark Brown"), TEXT("Nâu sẫm") }, { TEXT("Blue"), TEXT("Xanh dương") }, { TEXT("Off"), TEXT("Tắt") },
			{ TEXT("On"), TEXT("Bật") }, { TEXT("Black"), TEXT("Đen") }, { TEXT("BlueGrey"), TEXT("Xanh xám") }, { TEXT("BlueMarine"), TEXT("Xanh biển") },
			{ TEXT("BlueSky"), TEXT("Xanh da trời") }, { TEXT("BrownCooper"), TEXT("Nâu đồng") }, { TEXT("BrownHazel"), TEXT("Nâu hạt dẻ") },
			{ TEXT("BrownHoney"), TEXT("Nâu mật ong") }, { TEXT("DarkBrown"), TEXT("Nâu sẫm") }, { TEXT("Fuchsia"), TEXT("Hồng tím") },
			{ TEXT("GreenEmerald"), TEXT("Xanh lục bảo") }, { TEXT("GreenOlive"), TEXT("Xanh ô liu") }, { TEXT("GreenTurquoise"), TEXT("Xanh ngọc") },
			{ TEXT("Orange"), TEXT("Cam") }, { TEXT("PaleBlue"), TEXT("Xanh nhạt") }, { TEXT("Purple"), TEXT("Tím") }, { TEXT("Yellow"), TEXT("Vàng") },
			{ TEXT("None"), TEXT("Không") }, { TEXT("Hair Nasim UpdoMessyBun"), TEXT("Tóc búi") },
			{ TEXT("Hair Nasim NomadWaveUndercut"), TEXT("Tóc gợn sóng") }, { TEXT("Hair Nasim"), TEXT("Tóc ngắn") },
			{ TEXT("Jacket_B"), TEXT("Áo khoác B") }, { TEXT("Jacket_A"), TEXT("Áo khoác A") }, { TEXT("Owl"), TEXT("Cú mèo") },
			{ TEXT("Planets"), TEXT("Hành tinh") }, { TEXT("Gears"), TEXT("Bánh răng") }, { TEXT("Triangle A"), TEXT("Tam giác A") },
			{ TEXT("Triangle B"), TEXT("Tam giác B") }, { TEXT("Default pose"), TEXT("Tư thế mặc định") },
			{ TEXT("Back Pockets Off"), TEXT("Không túi sau") }, { TEXT("Back Pockets On"), TEXT("Có túi sau") },
			{ TEXT("Trousers Style"), TEXT("Kiểu quần tây") }, { TEXT("Jeans Style"), TEXT("Kiểu jean") }, { TEXT("Jeans Style + Pockets"), TEXT("Kiểu jean + túi") },
			{ TEXT("Tattoo Arm L"), TEXT("Hình xăm tay trái") }, { TEXT("Make Up Supreme"), TEXT("Trang điểm đậm") },
			{ TEXT("Make Up Black Eyes"), TEXT("Kẻ mắt đen") }, { TEXT("ThaiPants"), TEXT("Quần Thái") }, { TEXT("Scifi Pants"), TEXT("Quần viễn tưởng") },
			{ TEXT("Sport"), TEXT("Quần thể thao") }, { TEXT("Jeans"), TEXT("Quần jean") }, { TEXT("Chinos"), TEXT("Quần kaki") },
			{ TEXT("Inside Boots"), TEXT("Nhét trong ủng") }, { TEXT("Outside Boots"), TEXT("Phủ ngoài ủng") }, { TEXT("Sleeves"), TEXT("Tay áo") },
			{ TEXT("Stripe"), TEXT("Sọc") }, { TEXT("Triangle"), TEXT("Tam giác") }, { TEXT("Tattoo Forearm R"), TEXT("Hình xăm cẳng tay phải") },
			{ TEXT("Buckles"), TEXT("Khóa cài") }, { TEXT("Basic"), TEXT("Cơ bản") }, { TEXT("Grey"), TEXT("Xám") }, { TEXT("BiColor"), TEXT("Hai màu") },
			{ TEXT("TShirt"), TEXT("Áo thun") }, { TEXT("ThaiTunic"), TEXT("Áo Thái") }, { TEXT("TankTop"), TEXT("Áo ba lỗ") },
			{ TEXT("Sneakers"), TEXT("Giày thể thao") }, { TEXT("Boots"), TEXT("Ủng") }, { TEXT("Default"), TEXT("Mặc định") },
			{ TEXT("Custom"), TEXT("Tùy chỉnh") }, { TEXT("Red"), TEXT("Đỏ") }, { TEXT("White"), TEXT("Trắng") }, { TEXT("Green"), TEXT("Xanh lá") },
			{ TEXT("Front Pockets Off"), TEXT("Không túi trước") }, { TEXT("Front Pockets On"), TEXT("Có túi trước") },
			{ TEXT("No Image"), TEXT("Không hình") }, { TEXT("Shark"), TEXT("Cá mập") }, { TEXT("No Pattern"), TEXT("Không hoa văn") },
			{ TEXT("Geo"), TEXT("Hình học") }, { TEXT("Ramp"), TEXT("Chuyển màu") }, { TEXT("Stripes"), TEXT("Sọc") },
			{ TEXT("Frieze"), TEXT("Hoa văn diềm") }, { TEXT("Triangles"), TEXT("Tam giác") }, { TEXT("Native"), TEXT("Thổ dân") },
			{ TEXT("Geometric"), TEXT("Hình học") }, { TEXT("Precolumbian"), TEXT("Tiền Columbus") }, { TEXT("Indigo"), TEXT("Chàm") },
			{ TEXT("Crimson"), TEXT("Đỏ thẫm") }, { TEXT("Forest"), TEXT("Xanh rừng") }, { TEXT("Beige"), TEXT("Be") },
			{ TEXT("Triangle 1"), TEXT("Tam giác 1") }, { TEXT("Triangle 2"), TEXT("Tam giác 2") },
			// Option thumbnails seen in the lobby (not CO_Character parameter values)
			{ TEXT("Coin Pocket"), TEXT("Túi đồng xu") }, { TEXT("Cyborg"), TEXT("Người máy") }, { TEXT("Hair Old"), TEXT("Tóc kiểu cũ") },
		};
		return Map;
	}

	/**
	 * The Vietnamese for Source, if there is one. Ignores surrounding spaces and a trailing ':' (kept on the result);
	 * "Sticker 3" style layer names from the sticker editor become "Hình dán 3".
	 */
	TOptional<FString> Find(const FString& Source)
	{
		FString Key = Source.TrimStartAndEnd();
		const bool bColon = Key.EndsWith(TEXT(":"));
		if (bColon)
		{
			Key.LeftChopInline(1);
			Key.TrimEndInline();
		}
		if (Key.IsEmpty())
		{
			return {};
		}
		FString Result;
		if (const FString* Found = Table().Find(Key))
		{
			Result = *Found;
		}
		else if (Key.StartsWith(TEXT("Sticker ")) && Key.RightChop(8).IsNumeric())
		{
			Result = TEXT("Hình dán ") + Key.RightChop(8);
		}
		else
		{
			return {};
		}
		return bColon ? Result + TEXT(":") : Result;
	}
}

void UUiTranslationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UUiTranslationSubsystem::Tick), 0.2f);
}

void UUiTranslationSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	Super::Deinitialize();
}

FText UUiTranslationSubsystem::TranslateLabel(const FText& English)
{
	const TOptional<FString> Vietnamese = Find(English.ToString());
	return Vietnamese ? FText::FromString(*Vietnamese) : English;
}

bool UUiTranslationSubsystem::Tick(float DeltaTime)
{
	// Only text blocks of this game instance's worlds (the editor's own UI is Slate, never touched).
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* ActiveWorld = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!ActiveWorld || !ActiveWorld->GetName().Contains(TEXT("Lobby")))
	{
		return true;
	}
	for (TObjectIterator<UTextBlock> It; It; ++It)
	{
		UTextBlock* Block = *It;
		const UWorld* World = Block ? Block->GetWorld() : nullptr;
		if (!World || !World->IsGameWorld() || World->GetGameInstance() != GameInstance)
		{
			continue;
		}
		if (const TOptional<FString> Vietnamese = Find(Block->GetText().ToString()))
		{
			Block->SetText(FText::FromString(*Vietnamese));
		}
	}
	// The preset picker (Q/E between ready-made characters, e.g. "Người máy") is not part of this game: the player builds
	// their own character. It is collapsed in DynamicallyGeneratedGUI_DGGUI; keep it collapsed if the GUI shows it again.
	for (TObjectIterator<UWidget> It; It; ++It)
	{
		UWidget* Widget = *It;
		const UWorld* World = Widget ? Widget->GetWorld() : nullptr;
		if (World && World->IsGameWorld() && World->GetGameInstance() == GameInstance && Widget->GetFName() == TEXT("InstanceSelector")
			&& Widget->GetVisibility() != ESlateVisibility::Collapsed)
		{
			Widget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
	return true;
}
