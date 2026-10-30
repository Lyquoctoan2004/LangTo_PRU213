using UnrealBuildTool;
public class SilkVillageSystems : ModuleRules { public SilkVillageSystems(ReadOnlyTargetRules Target):base(Target) { PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs; PublicIncludePaths.Add(ModuleDirectory); PublicDependencyModuleNames.AddRange(new string[]{"Core","CoreUObject","Engine","InputCore","UMG","Slate","SlateCore"}); PrivateDependencyModuleNames.AddRange(new string[]{"Json","JsonUtilities"}); } }

