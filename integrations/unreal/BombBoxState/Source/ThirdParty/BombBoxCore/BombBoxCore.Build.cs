using System.IO;
using UnrealBuildTool;

public class BombBoxCore : ModuleRules
{
    public BombBoxCore(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        string IncludePath = Path.Combine(ModuleDirectory, "include");
        PublicSystemIncludePaths.Add(IncludePath);

        string LibraryPath;
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            LibraryPath = Path.Combine(ModuleDirectory, "lib", "Win64", "bomb_box_state.lib");
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            LibraryPath = Path.Combine(ModuleDirectory, "lib", "Mac", "libbomb_box_state.a");
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            LibraryPath = Path.Combine(ModuleDirectory, "lib", "Linux", "libbomb_box_state.a");
        }
        else
        {
            throw new BuildException("BombBoxCore has not been staged for {0}.", Target.Platform);
        }

        if (!File.Exists(LibraryPath))
        {
            throw new BuildException(
                "BombBoxCore is not staged. See integrations/unreal/README.md. Missing: {0}",
                LibraryPath
            );
        }

        PublicAdditionalLibraries.Add(LibraryPath);
        PublicDefinitions.Add("WITH_BOMB_BOX_CORE=1");
    }
}

