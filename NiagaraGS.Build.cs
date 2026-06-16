using UnrealBuildTool;

public class NiagaraGS : ModuleRules
{
    public NiagaraGS(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "RenderCore",
            "RHI",
            "Renderer",
            "Projects",
            "InputCore",
            "Niagara",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "RenderCore",
            "RHI",
            "Niagara",
            "NiagaraCore",
            "VectorVM",
            "NiagaraShader",
            "RenderCore", // Shader compiler mapping support
        });

        // Editor-only modules — stripped from packaged builds
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",               // UFactory base class, import framework
                "AssetTools",             // IAssetTypeActions, asset category registration
                "ContentBrowser",         // Content Browser integration
                "Slate",                  // Layout and UI
                "SlateCore",              // Core widget and style types
                "WorkspaceMenuStructure", // Tab category structures
            });
        }

        PrivateIncludePaths.AddRange(new string[]
        {
            "NiagaraGS/Private",
        });
    }
}