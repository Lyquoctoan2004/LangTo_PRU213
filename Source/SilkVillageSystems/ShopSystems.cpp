#include "ShopSystems.h"
#include "Modules/ModuleManager.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ActorComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/TextRenderComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Animation/AnimationAsset.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"
#include "UObject/ConstructorHelpers.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

// MutableSample is the project's primary game module; a second primary module breaks monolithic (game) links.
IMPLEMENT_MODULE(FDefaultGameModuleImpl,SilkVillageSystems);

FProperty* USilkShopLibrary::FindField(const UStruct* Struct,const FString& Prefix) {
 if(!Struct) return nullptr;
 for(TFieldIterator<FProperty> It(Struct);It;++It) if(It->GetName()==Prefix||It->GetName().StartsWith(Prefix+TEXT("_"))) return *It;
 return nullptr;
}
UObject* USilkShopLibrary::GetInventory(const UObject* Context) {
 APawn* Pawn=UGameplayStatics::GetPlayerPawn(Context,0); if(!Pawn) return nullptr;
 for(UActorComponent* Component:Pawn->GetComponents()) if(Component&&Component->GetClass()->GetName().StartsWith(TEXT("BPC_Inventory"))) return Component;
 return nullptr;
}
int32 USilkShopLibrary::GetOwnedQuantity(const UObject* Context,FName ItemID) {
 UObject* Inventory=GetInventory(Context); if(!Inventory||ItemID.IsNone()) return 0;
 for(TFieldIterator<FArrayProperty> It(Inventory->GetClass());It;++It) {
  FStructProperty* Inner=CastField<FStructProperty>(It->Inner); if(!Inner) continue;
  FNameProperty* ID=CastField<FNameProperty>(FindField(Inner->Struct,TEXT("ItemID")));
  FIntProperty* Quantity=CastField<FIntProperty>(FindField(Inner->Struct,TEXT("Quantity")));
  if(!ID||!Quantity) continue;
  FScriptArrayHelper Values(*It,It->ContainerPtrToValuePtr<void>(Inventory));
  int32 Total=0; for(int32 Index=0;Index<Values.Num();++Index) if(ID->GetPropertyValue_InContainer(Values.GetRawPtr(Index))==ItemID) Total+=FMath::Max(0,Quantity->GetPropertyValue_InContainer(Values.GetRawPtr(Index)));
  return Total;
 }
 return 0;
}
bool USilkShopLibrary::IsShopOpen(const UObject* Context) {
 if(!Context||!Context->GetWorld()) return false;
 TArray<UUserWidget*> Widgets; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Context,Widgets,USilkShopWidget::StaticClass(),true);
 for(UUserWidget* W:Widgets) if(W->IsInViewport()&&W->IsVisible()) return true;
 return false;
}
void USilkShopLibrary::HideInteractionPrompt(const UObject* Context) {
 APawn* Pawn=UGameplayStatics::GetPlayerPawn(Context,0);if(!Pawn) return;
 FObjectPropertyBase* Prop=FindFProperty<FObjectPropertyBase>(Pawn->GetClass(),TEXT("InteractionPromptRef"));
 if(Prop) if(UWidget* W=Cast<UWidget>(Prop->GetObjectPropertyValue_InContainer(Pawn))) W->SetVisibility(ESlateVisibility::Collapsed);
}
bool USilkShopLibrary::ShouldHideInteractionPrompt(const UObject* Context) {
 if(IsShopOpen(Context)) return true;
 UWorld* World=Context?Context->GetWorld():nullptr;if(!World) return false;
 for(TActorIterator<ASilkShopNPC> It(World);It;++It) if(It->CanInteract()) return true;
 return false;
}
UUserWidget* USilkShopLibrary::OpenShop(const UObject* Context) {
 APlayerController* PC=UGameplayStatics::GetPlayerController(Context,0);if(!PC) return nullptr;
 TArray<UUserWidget*> Existing;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Context,Existing,USilkShopWidget::StaticClass(),true);
 for(UUserWidget* W:Existing) if(W->IsInViewport()) return W;
 UClass* Class=LoadClass<UUserWidget>(nullptr,TEXT("/Game/SilkVillage/UI/WBP_Shop.WBP_Shop_C"));
 if(!Class) return nullptr;
 UUserWidget* Shop=CreateWidget<UUserWidget>(PC,Class); if(!Shop) return nullptr;
 Shop->AddToViewport(10); UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(PC,Shop,EMouseLockMode::DoNotLock,true);PC->bShowMouseCursor=true;
 HideInteractionPrompt(Context);return Shop;
}
void USilkShopLibrary::Notify(const UObject* Context,const FString& InString,bool bPrintToScreen,bool bPrintToLog,FLinearColor TextColor,float Duration,FName Key) {
 FString Message=InString; bool Error=false;
 if(InString.Contains(TEXT("Da mua"))) Message=TEXT("Mua thành công: đã thêm 1 vật phẩm vào túi.");
 else if(InString.Contains(TEXT("Da ban"))) Message=TEXT("Bán thành công: đã nhận xu.");
 else if(InString.Contains(TEXT("Khong du xu"))) {Message=TEXT("Không đủ xu để mua vật phẩm này.");Error=true;}
 else if(InString.Contains(TEXT("Khong du vat pham"))) {Message=TEXT("Không đủ vật phẩm để bán.");Error=true;}
 else if(InString.Contains(TEXT("hoan xu"))) {Message=TEXT("Không thể thêm vật phẩm. Đã hoàn lại xu.");Error=true;}
 else if(InString.Contains(TEXT("Gia phai"))) {Message=TEXT("Giá vật phẩm không hợp lệ.");Error=true;}
 else {Message=TEXT("Không thể giao dịch. Vui lòng kiểm tra dữ liệu hoặc túi đồ.");Error=true;}
 TArray<UUserWidget*> Widgets;UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Context,Widgets,USilkShopWidget::StaticClass(),true);
 for(UUserWidget* W:Widgets) if(USilkShopWidget* Shop=Cast<USilkShopWidget>(W)) {Shop->ShowNotice(Message,Error);Shop->RefreshOwnedRows();}
 if(bPrintToLog) UE_LOG(LogTemp,Log,TEXT("[SilkShop] %s"),*Message);
}
void USilkShopWidget::NativeConstruct() {
 SetIsFocusable(true); Super::NativeConstruct();
 if(UCanvasPanel* Canvas=Cast<UCanvasPanel>(GetRootWidget())) {
  NoticeBox=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("ShopNoticeBackground"));
  NoticeBox->SetBrushColor(FLinearColor(.06f,.035f,.015f,.97f));NoticeBox->SetPadding(FMargin(16,10));
  Notice=WidgetTree->ConstructWidget<UTextBlock>();Notice->SetAutoWrapText(true);Notice->SetJustification(ETextJustify::Center);
  FSlateFontInfo Font=Notice->GetFont();Font.Size=18;Notice->SetFont(Font);NoticeBox->SetContent(Notice);
  UCanvasPanelSlot* NoticeSlot=Canvas->AddChildToCanvas(NoticeBox);NoticeSlot->SetAnchors(FAnchors(.5f,.5f));NoticeSlot->SetAlignment(FVector2D(.5f,0));NoticeSlot->SetPosition(FVector2D(0,310));NoticeSlot->SetSize(FVector2D(850,65));NoticeSlot->SetZOrder(20);
  NoticeBox->SetVisibility(ESlateVisibility::Collapsed);
  EmptyState=WidgetTree->ConstructWidget<UTextBlock>();EmptyState->SetText(FText::FromString(TEXT("Túi đồ chưa có vật phẩm để bán.")));EmptyState->SetJustification(ETextJustify::Center);EmptyState->SetColorAndOpacity(FSlateColor(FLinearColor(.95f,.85f,.64f)));
  UCanvasPanelSlot* EmptySlot=Canvas->AddChildToCanvas(EmptyState);EmptySlot->SetAnchors(FAnchors(.5f,.5f));EmptySlot->SetAlignment(FVector2D(.5f,.5f));EmptySlot->SetPosition(FVector2D(0,0));EmptySlot->SetSize(FVector2D(750,40));EmptySlot->SetZOrder(15);EmptyState->SetVisibility(ESlateVisibility::Collapsed);
  UBorder* CoinBox=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("ShopCoinBadge"));
  CoinBox->SetBrushColor(FLinearColor(.06f,.09f,.075f,.96f));CoinBox->SetPadding(FMargin(18,8));
  CoinBalanceText=WidgetTree->ConstructWidget<UTextBlock>();CoinBalanceText->SetJustification(ETextJustify::Center);CoinBalanceText->SetColorAndOpacity(FSlateColor(FLinearColor(.98f,.84f,.42f,1.f)));
  FSlateFontInfo CoinFont=CoinBalanceText->GetFont();CoinFont.Size=17;CoinBalanceText->SetFont(CoinFont);CoinBox->SetContent(CoinBalanceText);
  UCanvasPanelSlot* CoinSlot=Canvas->AddChildToCanvas(CoinBox);CoinSlot->SetAnchors(FAnchors(.5f,.5f));CoinSlot->SetAlignment(FVector2D(.5f,1.f));CoinSlot->SetPosition(FVector2D(0,-308));CoinSlot->SetSize(FVector2D(360,42));CoinSlot->SetZOrder(20);
 }
 if(UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("BTN_BuyTab")))) B->OnClicked.AddUniqueDynamic(this,&USilkShopWidget::SelectBuy);
 if(UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("BTN_SellTab")))) B->OnClicked.AddUniqueDynamic(this,&USilkShopWidget::SelectSell);
 USilkShopLibrary::HideInteractionPrompt(this);RefreshOwnedRows();
}
void USilkShopWidget::SelectBuy(){bSellingMode=false;RefreshOwnedRows();}
void USilkShopWidget::SelectSell(){bSellingMode=true;RefreshOwnedRows();}
void USilkShopWidget::RefreshOwnedRows() {
 int32 CurrentCoins=0;
 if(UGameInstance* GI=UGameplayStatics::GetGameInstance(this)) {
  if(FIntProperty* CoinProp=FindFProperty<FIntProperty>(GI->GetClass(),TEXT("VillageCoin"))) CurrentCoins=CoinProp->GetPropertyValue_InContainer(GI);
 }
 if(CoinBalanceText) CoinBalanceText->SetText(FText::FromString(FString::Printf(TEXT("Số dư hiện có: %d Xu"),CurrentCoins)));
 UUserWidget* PrimaryCoinHud=nullptr;
 if(APawn* Pawn=UGameplayStatics::GetPlayerPawn(this,0)) {
  if(FObjectPropertyBase* HudProp=FindFProperty<FObjectPropertyBase>(Pawn->GetClass(),TEXT("HUDRef"))) {
   PrimaryCoinHud=Cast<UUserWidget>(HudProp->GetObjectPropertyValue_InContainer(Pawn));
  }
 }
 TArray<UUserWidget*> AllWidgets;
 UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this,AllWidgets,UUserWidget::StaticClass(),false);
 for(UUserWidget* W:AllWidgets) {
  if(!W||!W->GetClass()->GetName().StartsWith(TEXT("WBP_CoinHUD"))) continue;
  if(!PrimaryCoinHud&&W->IsInViewport()) PrimaryCoinHud=W;
  if(W!=PrimaryCoinHud) {W->SetVisibility(ESlateVisibility::Collapsed);W->RemoveFromParent();continue;}
  if(UTextBlock* CoinTxt=Cast<UTextBlock>(W->GetWidgetFromName(TEXT("TXT_CoinAmount")))) CoinTxt->SetText(FText::AsNumber(CurrentCoins));
 }
 UPanelWidget* List=Cast<UPanelWidget>(GetWidgetFromName(TEXT("VB_ShopItems")));if(!List)return;
 int32 Visible=0;
 for(UWidget* Child:List->GetAllChildren()) {
  UUserWidget* Row=Cast<UUserWidget>(Child);if(!Row)continue;
  FNameProperty* ID=FindFProperty<FNameProperty>(Row->GetClass(),TEXT("ItemID"));FBoolProperty* Sell=FindFProperty<FBoolProperty>(Row->GetClass(),TEXT("bIsSelling"));if(!ID||!Sell)continue;
  const bool Selling=Sell->GetPropertyValue_InContainer(Row);bSellingMode=Selling;
  const int32 Count=USilkShopLibrary::GetOwnedQuantity(this,ID->GetPropertyValue_InContainer(Row));
  const bool Show=!Selling||Count>0;Row->SetVisibility(Show?ESlateVisibility::SelfHitTestInvisible:ESlateVisibility::Collapsed);if(Show)++Visible;
  if(UTextBlock* Name=Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("TXT_ItemName")))) {
   FString Base=Name->GetText().ToString();int32 Pos=INDEX_NONE;if(Base.FindChar(TCHAR('['),Pos))Base=Base.Left(Pos).TrimEnd();
   Name->SetText(FText::FromString(Base+FString::Printf(TEXT("  [Có: %d]"),Count)));
  }
 }
 if(EmptyState)EmptyState->SetVisibility(bSellingMode&&Visible==0?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
 for(const TCHAR* Tab:{TEXT("BTN_BuyTab"),TEXT("BTN_SellTab")})if(UButton* Button=Cast<UButton>(GetWidgetFromName(Tab))){bool Active=(FString(Tab)==TEXT("BTN_SellTab"))==bSellingMode;Button->SetBackgroundColor(Active?FLinearColor(.72f,.47f,.17f):FLinearColor(.32f,.23f,.15f));}
}
void USilkShopWidget::ShowNotice(const FString& Message,bool Error) {
 if(!Notice||!NoticeBox)return;Notice->SetText(FText::FromString(Message));Notice->SetColorAndOpacity(FSlateColor(Error?FLinearColor(1,.42f,.32f):FLinearColor(.9f,.85f,.55f)));NoticeBox->SetVisibility(ESlateVisibility::HitTestInvisible);NoticeUntil=GetWorld()->GetTimeSeconds()+3.5;
}
void USilkShopWidget::NativeTick(const FGeometry& G,float Delta) {
 Super::NativeTick(G,Delta);USilkShopLibrary::HideInteractionPrompt(this);
 RefreshElapsed+=Delta;if(RefreshElapsed>=.15f){RefreshElapsed=0;RefreshOwnedRows();}
 if(NoticeBox&&GetWorld()->GetTimeSeconds()>NoticeUntil)NoticeBox->SetVisibility(ESlateVisibility::Collapsed);
}
void USilkShopWidget::CloseShop(){RemoveFromParent();}
void USilkShopWidget::NativeDestruct() {
 if(APlayerController* PC=GetOwningPlayer()){UWidgetBlueprintLibrary::SetInputMode_GameOnly(PC,true);PC->bShowMouseCursor=false;PC->ResetIgnoreLookInput();}
 Super::NativeDestruct();
}
FReply USilkShopWidget::NativeOnKeyDown(const FGeometry& G,const FKeyEvent& K){if(K.GetKey()==EKeys::Escape||K.GetKey()==EKeys::B){CloseShop();return FReply::Handled();}return Super::NativeOnKeyDown(G,K);}

ASilkShopNPC::ASilkShopNPC() {
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.1f;InputPriority=10;
 Tags.AddUnique(TEXT("NPC"));Tags.AddUnique(TEXT("Interactable"));Tags.AddUnique(TEXT("Interact_Shop"));
 RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
 MerchantName=FText::FromString(TEXT("NGƯỜI BÁN TƠ LỤA"));
 MerchantMesh=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("MerchantMesh"));MerchantMesh->SetupAttachment(RootComponent);MerchantMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);MerchantMesh->SetRelativeRotation(FRotator(0,-90,0));
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> Mesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"));if(Mesh.Succeeded())MerchantMesh->SetSkeletalMesh(Mesh.Object);
 static ConstructorHelpers::FObjectFinder<UAnimationAsset> Idle(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));if(Idle.Succeeded()){MerchantMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);MerchantMesh->AnimationData.AnimToPlay=Idle.Object;MerchantMesh->AnimationData.bSavedLooping=true;MerchantMesh->AnimationData.bSavedPlaying=true;}
 Counter=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Counter"));Counter->SetupAttachment(RootComponent);Counter->SetRelativeLocation(FVector(65,0,40));Counter->SetRelativeScale3D(FVector(.45,1.5,.8));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));if(Cube.Succeeded())Counter->SetStaticMesh(Cube.Object);
 Sign=CreateDefaultSubobject<UTextRenderComponent>(TEXT("MerchantSign"));Sign->SetupAttachment(RootComponent);Sign->SetRelativeLocation(FVector(0,0,210));Sign->SetHorizontalAlignment(EHTA_Center);Sign->SetWorldSize(20);Sign->SetTextRenderColor(FColor(244,228,202));Sign->SetText(FText::FromString(TEXT("CUA HANG TO LUA")));Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Prompt=CreateDefaultSubobject<UTextRenderComponent>(TEXT("InteractionHint"));Prompt->SetupAttachment(RootComponent);Prompt->SetRelativeLocation(FVector(0,0,185));Prompt->SetHorizontalAlignment(EHTA_Center);Prompt->SetWorldSize(17);Prompt->SetTextRenderColor(FColor(255,210,110));Prompt->SetText(FText::FromString(TEXT("[E] Mo cua hang")));Prompt->SetVisibility(false);Prompt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void ASilkShopNPC::BeginPlay(){Super::BeginPlay();}
bool ASilkShopNPC::CanInteract() const {
 APlayerController* PC=UGameplayStatics::GetPlayerController(this,0);APawn* Pawn=PC?PC->GetPawn():nullptr;if(!Pawn||USilkShopLibrary::IsShopOpen(this))return false;
 if(FVector::DistSquared(Pawn->GetActorLocation(),GetActorLocation()+FVector(0,0,90))>FMath::Square(InteractionDistance))return false;
 FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);FVector Target=GetActorLocation()+FVector(0,0,100);if(FVector::DotProduct((Target-Eye).GetSafeNormal(),View.Vector())<.15f)return false;
 return true;
}
void ASilkShopNPC::Tick(float Delta) {
 Super::Tick(Delta);APlayerController* PC=UGameplayStatics::GetPlayerController(this,0);if(!PC)return;
 FVector Eye;FRotator View;PC->GetPlayerViewPoint(Eye,View);const FRotator Face=(Eye-Sign->GetComponentLocation()).Rotation();Sign->SetWorldRotation(Face);Prompt->SetWorldRotation(Face);
 const bool Ready=CanInteract();Prompt->SetVisibility(Ready);
 if(Ready&&!bInputEnabled){EnableInput(PC);if(InputComponent){InputComponent->BindKey(EKeys::E,IE_Pressed,this,&ASilkShopNPC::Interact).bConsumeInput=true;}bInputEnabled=true;}
 else if(!Ready&&bInputEnabled){DisableInput(PC);if(InputComponent)InputComponent->ClearActionBindings();if(InputComponent)InputComponent->KeyBindings.Empty();bInputEnabled=false;}
 if(Ready)USilkShopLibrary::HideInteractionPrompt(this);
}
void ASilkShopNPC::Interact(){if(CanInteract()){USilkShopLibrary::OpenShop(this);Prompt->SetVisibility(false);}}

