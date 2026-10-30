// Copyright Epic Games, Inc. All Rights Reserved.

#include "meshy.h"
#include "MeshyBridge.h"
#include "LevelEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ToolMenus.h"
#include "meshyStyle.h"
#include "Styling/SlateStyle.h"
#include "Misc/EngineVersionComparison.h"
#include "HAL/IConsoleManager.h"

// UE 5.6/5.7 版本兼容性宏
#ifndef UE_5_07_OR_LATER
    #if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
        #define UE_5_07_OR_LATER 1
    #else
        #define UE_5_07_OR_LATER 0
    #endif
#endif

#define LOCTEXT_NAMESPACE "FMeshyModule"

// 定义全局指针，用于访问模块实例
static FMeshyModule* ModuleInstance = nullptr;

void FMeshyModule::StartupModule()
{
    // 存储模块实例指针
    ModuleInstance = this;
    
    // 初始化样式
    FmeshyStyle::Initialize();
    
    // 初始化命令列表
    PluginCommands = MakeShareable(new FUICommandList);

    // Download Only mode and its directory (EditorPerProjectUserSettings [MeshyBridge])
    FMeshyBridge::Get().LoadSettings();

    // 注册菜单
    RegisterMenus();
}

void FMeshyModule::ShutdownModule()
{
    // 清理样式
    FmeshyStyle::Shutdown();
    
    // 停止服务
    if (FMeshyBridge::Get().IsRunning())
    {
        FMeshyBridge::Get().StopServer();
    }
    
    // 清除模块实例指针
    ModuleInstance = nullptr;
}

void FMeshyModule::ToggleMeshyBridge()
{
    if (FMeshyBridge::Get().IsRunning())
    {
        FMeshyBridge::Get().StopServer();
        UE_LOG(LogTemp, Log, TEXT("Meshy Bridge stop"));
    }
    else
    {
        FMeshyBridge::Get().StartServer();
        UE_LOG(LogTemp, Log, TEXT("Meshy Bridge start"));
    }
}

bool FMeshyModule::IsMeshyBridgeRunning() const
{
    return FMeshyBridge::Get().IsRunning();
}

// 用于菜单项的静态委托函数
static void ExecuteToggleBridge()
{
    if (ModuleInstance)
    {
        ModuleInstance->ToggleMeshyBridge();
    }
}

// 用于菜单项的静态检查函数
static bool IsRunningBridge()
{
    if (ModuleInstance)
    {
        return ModuleInstance->IsMeshyBridgeRunning();
    }
    return false;
}

static void ExecuteToggleDownloadOnly()
{
    FMeshyBridge::Get().SetDownloadOnly(!FMeshyBridge::Get().IsDownloadOnly());
}

static bool IsDownloadOnly()
{
    return FMeshyBridge::Get().IsDownloadOnly();
}

// Same controls as the Window menu, for automation: Meshy.Bridge Start | Stop | DownloadOnly <0|1> | Status
static void ExecuteBridgeCommand(const TArray<FString>& Args)
{
    FMeshyBridge& Bridge = FMeshyBridge::Get();
    const FString Verb = Args.Num() > 0 ? Args[0] : FString(TEXT("Status"));
    if (Verb.Equals(TEXT("Start"), ESearchCase::IgnoreCase))
    {
        Bridge.StartServer();
    }
    else if (Verb.Equals(TEXT("Stop"), ESearchCase::IgnoreCase))
    {
        Bridge.StopServer();
    }
    else if (Verb.Equals(TEXT("DownloadOnly"), ESearchCase::IgnoreCase) && Args.Num() > 1)
    {
        Bridge.SetDownloadOnly(Args[1].ToBool());
    }
    else if (!Verb.Equals(TEXT("Status"), ESearchCase::IgnoreCase))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Meshy Bridge] Usage: Meshy.Bridge Start | Stop | DownloadOnly <0|1> | Status"));
    }
    UE_LOG(LogTemp, Display, TEXT("[Meshy Bridge] Server %s, Download Only %s, directory: %s"),
        Bridge.IsRunning() ? TEXT("running") : TEXT("stopped"),
        Bridge.IsDownloadOnly() ? TEXT("ON") : TEXT("OFF"),
        *Bridge.GetDownloadOnlyDirectory());
}

static FAutoConsoleCommand GMeshyBridgeCommand(
    TEXT("Meshy.Bridge"),
    TEXT("Meshy Bridge: Start | Stop | DownloadOnly <0|1> | Status"),
    FConsoleCommandWithArgsDelegate::CreateStatic(&ExecuteBridgeCommand));

void FMeshyModule::RegisterMenus()
{
    // 获取主编辑器菜单
    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
    TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());
    
    // 将菜单项添加到Window菜单中
    MenuExtender->AddMenuExtension(
        "WindowLayout", // 在"WindowLayout"部分之后添加
        EExtensionHook::After,
        nullptr,
        FMenuExtensionDelegate::CreateLambda([](FMenuBuilder& Builder) {
            Builder.AddSeparator(); // 添加分隔符使其更清晰
            
            // 添加带勾选状态和图标的菜单项
            Builder.AddMenuEntry(
                LOCTEXT("MeshyBridge", "Meshy Bridge"),
                LOCTEXT("MeshyBridgeTooltip", "Enable/Disable Meshy Bridge Service"),
                FSlateIcon(FmeshyStyle::GetStyleSetName(), "MeshyBridge.Icon"),
                FUIAction(
                    FExecuteAction::CreateStatic(&ExecuteToggleBridge),
                    FCanExecuteAction(),
                    FIsActionChecked::CreateStatic(&IsRunningBridge)
                ),
                NAME_None,
                EUserInterfaceActionType::ToggleButton
            );

            Builder.AddMenuEntry(
                LOCTEXT("MeshyDownloadOnly", "Meshy Bridge: Download Only"),
                FText::Format(
                    LOCTEXT("MeshyDownloadOnlyTooltip", "Save files sent from Meshy to {0} without importing them (no extraction, materials or actors). Turn off to import into /Game/MeshyImports as before."),
                    FText::FromString(FMeshyBridge::Get().GetDownloadOnlyDirectory())),
                FSlateIcon(),
                FUIAction(
                    FExecuteAction::CreateStatic(&ExecuteToggleDownloadOnly),
                    FCanExecuteAction(),
                    FIsActionChecked::CreateStatic(&IsDownloadOnly)
                ),
                NAME_None,
                EUserInterfaceActionType::ToggleButton
            );
        })
    );
    
    // 获取Window菜单的扩展点管理器并添加我们的扩展
    LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FMeshyModule, meshy)