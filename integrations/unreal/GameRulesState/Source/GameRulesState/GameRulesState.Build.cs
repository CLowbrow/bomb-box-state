using UnrealBuildTool;

public class GameRulesState : ModuleRules
{
    public GameRulesState(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        bEnableExceptions = false;
        bUseRTTI = false;

        PublicDependencyModuleNames.AddRange(new[] { "Core", "GameRulesCore" });
    }
}

