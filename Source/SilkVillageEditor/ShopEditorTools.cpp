#include "ShopEditorTools.h"
#include "ShopSystems.h"
#include "Modules/ModuleManager.h"
#include "Editor.h"
#include "PlayInEditorDataTypes.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Blueprint.h"
#include "Engine/GameInstance.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphUtilities.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_IfThenElse.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/StructureEditorUtils.h"
#include "StructUtils/UserDefinedStruct.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "BlueprintEditorLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"
#include "Serialization/JsonSerializer.h"
#include "EngineUtils.h"

IMPLEMENT_MODULE(FDefaultModuleImpl,SilkVillageEditor);
void UShopEditorTools::StartShopPIE(){if(GEditor){FRequestPlaySessionParams Params;Params.WorldType=EPlaySessionWorldType::PlayInEditor;GEditor->RequestPlaySession(Params);}}
FString UShopEditorTools::AddStructSoftObjectField(const FString& StructPath,const FString& FieldName,const FString& ObjectClassPath){
 UUserDefinedStruct* Struct=LoadObject<UUserDefinedStruct>(nullptr,*StructPath);UClass* ObjectClass=LoadObject<UClass>(nullptr,*ObjectClassPath);
 if(!Struct||!ObjectClass)return TEXT("ERROR: struct or class missing");
 for(const FStructVariableDescription& Var:FStructureEditorUtils::GetVarDesc(Struct))if(Var.FriendlyName==FieldName)return TEXT("EXISTS");
 const FEdGraphPinType Type(UEdGraphSchema_K2::PC_SoftObject,NAME_None,ObjectClass,EPinContainerType::None,false,FEdGraphTerminalType());
 if(!FStructureEditorUtils::AddVariable(Struct,Type))return TEXT("ERROR: AddVariable failed");
 const FGuid Guid=FStructureEditorUtils::GetVarDesc(Struct).Last().VarGuid;
 if(!FStructureEditorUtils::RenameVariable(Struct,Guid,FieldName))return TEXT("ERROR: RenameVariable failed");
 return TEXT("ADDED");
}
static FString JsonString(TSharedRef<FJsonObject> Obj){FString Text;auto Writer=TJsonWriterFactory<>::Create(&Text);FJsonSerializer::Serialize(Obj,Writer);return Text;}
FString UShopEditorTools::InspectBlueprint(const FString& AssetPath){
 UBlueprint* BP=LoadObject<UBlueprint>(nullptr,*AssetPath);if(!BP)return TEXT("NOT_FOUND");
 FString Out;TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
 for(UEdGraph* G:Graphs){TSet<UObject*> Nodes;for(UEdGraphNode* N:G->Nodes)Nodes.Add(N);FString Text;FEdGraphUtilities::ExportNodesToText(Nodes,Text);Out+=TEXT("\nGRAPH ")+G->GetName()+TEXT("\n")+Text;}
 Out+=TEXT("\nPROPERTIES\n");if(BP->GeneratedClass)for(TFieldIterator<FProperty> It(BP->GeneratedClass);It;++It)if(It->GetOwnerClass()==BP->GeneratedClass){FString Value;It->ExportText_InContainer(0,Value,BP->GeneratedClass->GetDefaultObject(),nullptr,nullptr,PPF_None);Out+=It->GetName()+TEXT(" : ")+It->GetCPPType()+TEXT(" = ")+Value+TEXT("\n");}
 return Out;
}
FString UShopEditorTools::ApplyShopUpgrade(){
 FString Result;UBlueprint* Shop=LoadObject<UBlueprint>(nullptr,TEXT("/Game/SilkVillage/UI/WBP_Shop.WBP_Shop"));
 UBlueprint* Player=LoadObject<UBlueprint>(nullptr,TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter"));
 if(!Shop||!Player)return TEXT("ERROR: Blueprint missing");
 if(Shop->ParentClass!=USilkShopWidget::StaticClass())UBlueprintEditorLibrary::ReparentBlueprint(Shop,USilkShopWidget::StaticClass());
 TArray<UEdGraph*> Graphs;Shop->GetAllGraphs(Graphs);int32 Replaced=0;
 for(UEdGraph* G:Graphs)for(UEdGraphNode* N:G->Nodes)if(UK2Node_CallFunction* Call=Cast<UK2Node_CallFunction>(N))if(Call->FunctionReference.GetMemberName()==TEXT("PrintString")){
  Call->Modify();Call->SetFromFunction(USilkShopLibrary::StaticClass()->FindFunctionByName(TEXT("Notify")));Call->ReconstructNode();++Replaced;
 }
 Result+=FString::Printf(TEXT("Notify replacements=%d\n"),Replaced);
 Graphs.Reset();Player->GetAllGraphs(Graphs);
 for(UEdGraph* G:Graphs)if(G->GetName()==TEXT("RefreshInteractionPrompt")){
  bool Exists=false;for(UEdGraphNode* N:G->Nodes)if(UK2Node_CallFunction* C=Cast<UK2Node_CallFunction>(N))if(C->FunctionReference.GetMemberName()==TEXT("ShouldHideInteractionPrompt"))Exists=true;
  if(Exists){Result+=TEXT("Prompt guard already present\n");continue;}
  UK2Node_FunctionEntry* Entry=nullptr;for(UEdGraphNode* N:G->Nodes)if(Cast<UK2Node_FunctionEntry>(N))Entry=Cast<UK2Node_FunctionEntry>(N);
  if(!Entry||!Entry->FindPin(UEdGraphSchema_K2::PN_Then)){Result+=TEXT("ERROR: prompt entry missing\n");continue;}
  const UEdGraphSchema_K2* Schema=GetDefault<UEdGraphSchema_K2>();UEdGraphPin* Start=Entry->FindPin(UEdGraphSchema_K2::PN_Then);TArray<UEdGraphPin*> Old=Start->LinkedTo;Start->BreakAllPinLinks();
  UK2Node_CallFunction* Check=NewObject<UK2Node_CallFunction>(G);G->AddNode(Check);Check->CreateNewGuid();Check->SetFromFunction(USilkShopLibrary::StaticClass()->FindFunctionByName(TEXT("ShouldHideInteractionPrompt")));Check->AllocateDefaultPins();Check->NodePosX=Entry->NodePosX+220;Check->NodePosY=Entry->NodePosY+220;
  UK2Node_IfThenElse* Branch=NewObject<UK2Node_IfThenElse>(G);G->AddNode(Branch);Branch->CreateNewGuid();Branch->AllocateDefaultPins();Branch->NodePosX=Entry->NodePosX+220;Branch->NodePosY=Entry->NodePosY;
  UK2Node_CallFunction* Hide=NewObject<UK2Node_CallFunction>(G);G->AddNode(Hide);Hide->CreateNewGuid();Hide->SetFromFunction(USilkShopLibrary::StaticClass()->FindFunctionByName(TEXT("HideInteractionPrompt")));Hide->AllocateDefaultPins();Hide->NodePosX=Entry->NodePosX+500;Hide->NodePosY=Entry->NodePosY-180;
  bool OK=Schema->TryCreateConnection(Start,Branch->GetExecPin());OK&=Schema->TryCreateConnection(Check->GetReturnValuePin(),Branch->GetConditionPin());OK&=Schema->TryCreateConnection(Branch->GetThenPin(),Hide->GetExecPin());for(UEdGraphPin* P:Old)OK&=Schema->TryCreateConnection(Branch->GetElsePin(),P);
  Result+=OK?TEXT("Prompt guard connected\n"):TEXT("ERROR: prompt guard connection\n");
 }
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Shop);FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Player);FKismetEditorUtilities::CompileBlueprint(Shop);FKismetEditorUtilities::CompileBlueprint(Player);
 Result+=FString::Printf(TEXT("Shop status=%d; Player status=%d\n"),int32(Shop->Status),int32(Player->Status));return Result;
}

static bool Invoke(UObject* Object,const TCHAR* Function,const TMap<FString,FString>& Inputs={}){
 UFunction* F=Object?Object->FindFunction(FName(Function)):nullptr;if(!F)return false;
 FStructOnScope Params(F);for(const auto& Pair:Inputs){FProperty* P=FindFProperty<FProperty>(F,*Pair.Key);if(!P)return false;P->ImportText_Direct(*Pair.Value,P->ContainerPtrToValuePtr<void>(Params.GetStructMemory()),Object,PPF_None);}
 Object->ProcessEvent(F,Params.GetStructMemory());return true;
}
FString UShopEditorTools::RunShopTests(){
 TSharedRef<FJsonObject> Report=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Results;int32 Passed=0,Failed=0;
 auto Check=[&](const FString& Name,bool Good,const FString& Detail=TEXT("")){TSharedRef<FJsonObject> R=MakeShared<FJsonObject>();R->SetStringField(TEXT("test"),Name);R->SetBoolField(TEXT("passed"),Good);R->SetStringField(TEXT("detail"),Detail);Results.Add(MakeShared<FJsonValueObject>(R));if(Good)++Passed;else ++Failed;};
 UWorld* World=GEditor?GEditor->PlayWorld:nullptr;if(!World)return TEXT("ERROR: Start PIE first");
 UObject* GI=World->GetGameInstance();UObject* Inv=USilkShopLibrary::GetInventory(GI);FIntProperty* Coin=GI?FindFProperty<FIntProperty>(GI->GetClass(),TEXT("VillageCoin")):nullptr;
 if(!Inv||!Coin)return TEXT("ERROR: Inventory or coin reference missing");
 FArrayProperty* Array=nullptr;for(TFieldIterator<FArrayProperty> It(Inv->GetClass());It;++It){FStructProperty* S=CastField<FStructProperty>(It->Inner);if(S&&USilkShopLibrary::FindField(S->Struct,TEXT("ItemID"))&&USilkShopLibrary::FindField(S->Struct,TEXT("Quantity"))){Array=*It;break;}}
 if(!Array)return TEXT("ERROR: Inventory array not found");
 FString Snapshot;Array->ExportText_InContainer(0,Snapshot,Inv,nullptr,Inv,PPF_None);const int32 OriginalCoin=Coin->GetPropertyValue_InContainer(GI);
 auto Qty=[&](const TCHAR* ID){return USilkShopLibrary::GetOwnedQuantity(GI,FName(ID));};auto Coins=[&](){return Coin->GetPropertyValue_InContainer(GI);};
 UUserWidget* Widget=USilkShopLibrary::OpenShop(GI);USilkShopWidget* Shop=Cast<USilkShopWidget>(Widget);Check(TEXT("Open shop with native parent"),Shop&&Shop->IsInViewport());
 if(Shop){
  const TCHAR* Item=TEXT("MAT_SILK_THREAD_001");int32 Before=Qty(Item);Coin->SetPropertyValue_InContainer(GI,200);
  Check(TEXT("HandleTrade signature"),Invoke(Shop,TEXT("HandleTrade"),{{TEXT("ItemID"),Item},{TEXT("bIsSelling"),TEXT("False")}}));
  Check(TEXT("Buy one costs 10 and adds one"),Coins()==190&&Qty(Item)==Before+1,FString::Printf(TEXT("coins=%d;quantity=%d;before=%d"),Coins(),Qty(Item),Before));
  Invoke(Shop,TEXT("HandleTrade"),{{TEXT("ItemID"),Item},{TEXT("bIsSelling"),TEXT("True")}});Check(TEXT("Sell one refunds 10 and removes one"),Coins()==200&&Qty(Item)==Before);
  Coin->SetPropertyValue_InContainer(GI,5);Before=Qty(Item);Invoke(Shop,TEXT("HandleTrade"),{{TEXT("ItemID"),Item},{TEXT("bIsSelling"),TEXT("False")}});Check(TEXT("Insufficient coins is non-mutating"),Coins()==5&&Qty(Item)==Before);
  Invoke(Shop,TEXT("ExecuteShopTrade"),{{TEXT("ItemID"),Item},{TEXT("Price"),TEXT("0")},{TEXT("bIsSelling"),TEXT("False")}});Check(TEXT("Zero price rejected"),Coins()==5&&Qty(Item)==Before);
  Invoke(Shop,TEXT("ExecuteShopTrade"),{{TEXT("ItemID"),Item},{TEXT("Price"),TEXT("-10")},{TEXT("bIsSelling"),TEXT("False")}});Check(TEXT("Negative price rejected"),Coins()==5&&Qty(Item)==Before);
  Invoke(Shop,TEXT("HandleTrade"),{{TEXT("ItemID"),TEXT("DOES_NOT_EXIST")},{TEXT("bIsSelling"),TEXT("False")}});Check(TEXT("Unknown row is non-mutating"),Coins()==5&&Qty(Item)==Before);
  FScriptArrayHelper Entries(Array,Array->ContainerPtrToValuePtr<void>(Inv));Entries.EmptyValues();Before=Coins();Invoke(Shop,TEXT("HandleTrade"),{{TEXT("ItemID"),Item},{TEXT("bIsSelling"),TEXT("True")}});Check(TEXT("Sell absent item rejected"),Coins()==Before&&Qty(Item)==0);
  Invoke(Shop,TEXT("RefreshShop"),{{TEXT("IsSelling"),TEXT("True")}});Shop->SelectSell();Shop->RefreshOwnedRows();UPanelWidget* List=Cast<UPanelWidget>(Shop->GetWidgetFromName(TEXT("VB_ShopItems")));int32 Visible=0;if(List)for(UWidget* W:List->GetAllChildren())if(W->IsVisible())++Visible;Check(TEXT("Empty inventory has no sell rows"),List&&Visible==0);
  Invoke(Inv,TEXT("AddItem"),{{TEXT("ItemID"),Item},{TEXT("Quantity"),TEXT("2")}});Invoke(Shop,TEXT("RefreshShop"),{{TEXT("IsSelling"),TEXT("True")}});Shop->SelectSell();Shop->RefreshOwnedRows();Visible=0;if(List)for(UWidget* W:List->GetAllChildren())if(W->IsVisible())++Visible;Check(TEXT("Sell tab shows only owned type"),Visible==1&&Qty(Item)==2);
  Invoke(Shop,TEXT("HandleTrade"),{{TEXT("ItemID"),Item},{TEXT("bIsSelling"),TEXT("True")}});Invoke(Shop,TEXT("HandleTrade"),{{TEXT("ItemID"),Item},{TEXT("bIsSelling"),TEXT("True")}});Shop->RefreshOwnedRows();Visible=0;if(List)for(UWidget* W:List->GetAllChildren())if(W->IsVisible())++Visible;Check(TEXT("Last sold item disappears"),Visible==0&&Qty(Item)==0);
  Check(TEXT("Prompt suppressed during shop"),USilkShopLibrary::ShouldHideInteractionPrompt(GI));
  Check(TEXT("UI toast created"),Shop->GetWidgetFromName(TEXT("ShopNoticeBackground"))!=nullptr);
  Shop->CloseShop();APlayerController* PC=UGameplayStatics::GetPlayerController(GI,0);Check(TEXT("Close removes shop and hides cursor"),!USilkShopLibrary::IsShopOpen(GI)&&PC&&!PC->bShowMouseCursor);
 }
 Array->ImportText_Direct(*Snapshot,Array->ContainerPtrToValuePtr<void>(Inv),Inv,PPF_None);Coin->SetPropertyValue_InContainer(GI,OriginalCoin);Invoke(Inv,TEXT("SaveInventory"));Invoke(GI,TEXT("SaveCoin"));
 FString Restored;Array->ExportText_InContainer(0,Restored,Inv,nullptr,Inv,PPF_None);Check(TEXT("Test restores original player state"),Restored==Snapshot&&Coins()==OriginalCoin);
 Invoke(Inv,TEXT("LoadInventory"));Invoke(GI,TEXT("LoadCoin"));FString Loaded;Array->ExportText_InContainer(0,Loaded,Inv,nullptr,Inv,PPF_None);Check(TEXT("Save/load roundtrip preserves inventory and coins"),Loaded==Snapshot&&Coins()==OriginalCoin);
 int32 NPCs=0;for(TActorIterator<ASilkShopNPC> It(World);It;++It)++NPCs;Check(TEXT("Merchant exists in demo level"),NPCs>=1);
 Report->SetArrayField(TEXT("results"),Results);Report->SetNumberField(TEXT("passed"),Passed);Report->SetNumberField(TEXT("failed"),Failed);Report->SetStringField(TEXT("scope"),TEXT("PIE runtime; existing Blueprint transactions; test restores player state"));
 FString Text=JsonString(Report);FFileHelper::SaveStringToFile(Text,*(FPaths::ProjectSavedDir()/TEXT("ShopCompletion/functional-tests.json")));return Text;
}

