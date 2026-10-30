#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ShopEditorTools.generated.h"
UCLASS()
class SILKVILLAGEEDITOR_API UShopEditorTools : public UBlueprintFunctionLibrary {
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="Shop Editor") static void StartShopPIE();
 UFUNCTION(BlueprintCallable,Category="Shop Editor") static FString InspectBlueprint(const FString& AssetPath);
 UFUNCTION(BlueprintCallable,Category="Shop Editor") static FString ApplyShopUpgrade();
 UFUNCTION(BlueprintCallable,Category="Shop Editor") static FString RunShopTests();
 // Adds a Soft Object Reference field (e.g. ItemMesh -> StaticMesh) to a Blueprint struct such as S_ItemData; caller saves the struct.
 UFUNCTION(BlueprintCallable,Category="Shop Editor") static FString AddStructSoftObjectField(const FString& StructPath,const FString& FieldName,const FString& ObjectClassPath);
};
