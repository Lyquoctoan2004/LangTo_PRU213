using UnrealBuildTool;

public class LangToLuaEditorTarget : TargetRules
{
    public LangToLuaEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        ExtraModuleNames.AddRange(new string[] { "MutableSample", "SilkVillageSystems", "SilkVillageEditor" });
    }
}
