using UnrealBuildTool;

public class LangToLuaTarget : TargetRules
{
    public LangToLuaTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        ExtraModuleNames.AddRange(new string[] { "MutableSample", "SilkVillageSystems" });
    }
}
