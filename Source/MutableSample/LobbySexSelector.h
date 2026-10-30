#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "LobbySexSelector.generated.h"

class UCustomizableObject;
class UCustomizableObjectInstance;
class UCustomizableSkeletalComponent;
class UTextBlock;
class USkeletalMeshComponent;
class USkinnedMeshComponent;
class UGroomComponent;

/** Sex selection for the lobby's existing Mutable customization screen. */
UCLASS()
class MUTABLESAMPLE_API ULobbySexSelector : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Character|Editor")
	static ULobbySexSelector* AddToCustomizationTree(UObject* TreeObject, UObject* PanelObject);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Character")
	TObjectPtr<UCustomizableObject> MaleObject;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Character")
	TObjectPtr<UCustomizableObject> FemaleObject;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FSlateBrush RowBrush;
	UFUNCTION(BlueprintCallable, Category="Character")
	void SelectFemale(bool bFemale);
	UFUNCTION(BlueprintPure, Category="Character")
	bool IsFemaleSelected() const { return bFemaleSelected; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;

private:
	UFUNCTION() void ToggleSex();
	UUserWidget* FindCustomizationWidget() const;
	UCustomizableSkeletalComponent* FindCustomizationComponent(UUserWidget* Editor) const;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SelectedText;
	UPROPERTY(Transient) TObjectPtr<UCustomizableObjectInstance> MaleInstance;
	UPROPERTY(Transient) TObjectPtr<UCustomizableObjectInstance> FemaleInstance;
	bool bFemaleSelected = false;
	bool bSwitching = false;
	struct FOriginalAnimation
	{
		TWeakObjectPtr<USkeletalMeshComponent> Component;
		TWeakObjectPtr<UClass> Class;
		TWeakObjectPtr<USkinnedMeshComponent> Leader;
		uint8 Mode = 0;
	};
	TArray<FOriginalAnimation> OriginalAnimations;
	TMap<TWeakObjectPtr<UGroomComponent>, bool> OriginalGroomVisibility;
};
