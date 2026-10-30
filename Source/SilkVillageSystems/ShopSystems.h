#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameFramework/Actor.h"
#include "ShopSystems.generated.h"

class UTextBlock; class UBorder; class USkeletalMeshComponent; class UStaticMeshComponent; class UTextRenderComponent;

UCLASS()
class SILKVILLAGESYSTEMS_API USilkShopWidget : public UUserWidget {
 GENERATED_BODY()
public:
 virtual void NativeConstruct() override;
 virtual void NativeTick(const FGeometry& Geometry,float DeltaTime) override;
 virtual void NativeDestruct() override;
 virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& KeyEvent) override;
 void ShowNotice(const FString& Message,bool bError=false);
 UFUNCTION(BlueprintCallable,Category="Silk Village|Shop") void RefreshOwnedRows();
 UFUNCTION() void SelectBuy();
 UFUNCTION() void SelectSell();
 UFUNCTION() void CloseShop();
 bool IsSellingMode() const { return bSellingMode; }
private:
 UPROPERTY(Transient) TObjectPtr<UTextBlock> Notice;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> EmptyState;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> CoinBalanceText;
 UPROPERTY(Transient) TObjectPtr<UBorder> NoticeBox;
 double NoticeUntil=0;
 float RefreshElapsed=0;
 bool bSellingMode=false;
};

UCLASS()
class SILKVILLAGESYSTEMS_API USilkShopLibrary : public UBlueprintFunctionLibrary {
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintPure,Category="Silk Village|Shop",meta=(WorldContext="WorldContextObject")) static bool IsShopOpen(const UObject* WorldContextObject);
 UFUNCTION(BlueprintPure,Category="Silk Village|Shop",meta=(WorldContext="WorldContextObject")) static bool ShouldHideInteractionPrompt(const UObject* WorldContextObject);
 UFUNCTION(BlueprintCallable,Category="Silk Village|Shop",meta=(WorldContext="WorldContextObject")) static void HideInteractionPrompt(const UObject* WorldContextObject);
 UFUNCTION(BlueprintCallable,Category="Silk Village|Shop",meta=(WorldContext="WorldContextObject")) static UUserWidget* OpenShop(const UObject* WorldContextObject);
 UFUNCTION(BlueprintCallable,Category="Silk Village|Shop",meta=(WorldContext="WorldContextObject",AdvancedDisplay="2")) static void Notify(const UObject* WorldContextObject,const FString& InString,bool bPrintToScreen=true,bool bPrintToLog=true,FLinearColor TextColor=FLinearColor(0.0,0.66,1.0),float Duration=3.0,FName Key=NAME_None);
 UFUNCTION(BlueprintPure,Category="Silk Village|Shop",meta=(WorldContext="WorldContextObject")) static int32 GetOwnedQuantity(const UObject* WorldContextObject,FName ItemID);
 static UObject* GetInventory(const UObject* Context);
 static FProperty* FindField(const UStruct* Struct,const FString& Prefix);
};

UCLASS(Blueprintable)
class SILKVILLAGESYSTEMS_API ASilkShopNPC : public AActor {
 GENERATED_BODY()
public:
 ASilkShopNPC();
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Shop",meta=(ClampMin="100",ClampMax="600")) float InteractionDistance=360;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Shop") FText MerchantName;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Shop") TObjectPtr<USkeletalMeshComponent> MerchantMesh;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Shop") TObjectPtr<UStaticMeshComponent> Counter;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Shop") TObjectPtr<UTextRenderComponent> Sign;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Shop") TObjectPtr<UTextRenderComponent> Prompt;
 UFUNCTION(BlueprintPure,Category="Shop") bool CanInteract() const;
 UFUNCTION(BlueprintCallable,Category="Shop") void Interact();
private:
 bool bInputEnabled=false;
};
