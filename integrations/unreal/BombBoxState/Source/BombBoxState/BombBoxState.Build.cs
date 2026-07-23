using UnrealBuildTool;

public class BombBoxState : ModuleRules
{
    public BombBoxState(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        bEnableExceptions = false;
        bUseRTTI = false;

        PublicDependencyModuleNames.AddRange(new[] { "Core", "BombBoxCore" });
    }
}

