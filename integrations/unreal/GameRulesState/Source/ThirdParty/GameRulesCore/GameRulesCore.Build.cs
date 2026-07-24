using System.IO;
using UnrealBuildTool;

public class GameRulesCore : ModuleRules
{
    public GameRulesCore(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        string IncludePath = Path.Combine(ModuleDirectory, "include");
        PublicSystemIncludePaths.Add(IncludePath);

        string LibraryPath;
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            LibraryPath = Path.Combine(ModuleDirectory, "lib", "Win64", "game_rules_state.lib");
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            LibraryPath = Path.Combine(ModuleDirectory, "lib", "Mac", "libgame_rules_state.a");
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            LibraryPath = Path.Combine(ModuleDirectory, "lib", "Linux", "libgame_rules_state.a");
        }
        else
        {
            throw new BuildException("GameRulesCore has not been staged for {0}.", Target.Platform);
        }

        if (!File.Exists(LibraryPath))
        {
            throw new BuildException(
                "GameRulesCore is not staged. See integrations/unreal/README.md. Missing: {0}",
                LibraryPath
            );
        }

        PublicAdditionalLibraries.Add(LibraryPath);
        PublicDefinitions.Add("WITH_GAME_RULES_CORE=1");
    }
}

