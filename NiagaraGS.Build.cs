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
            "UnrealEd",   // For FAssetImportInfo, which we use to track source .ply files
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "RenderCore",
            "RHI",
            "Niagara",
            "NiagaraCore",
            "VectorVM",
            "NiagaraShader",
        });

        // Editor-only modules — stripped from packaged builds
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",       // UFactory base class, import framework
                "AssetTools",     // IAssetTypeActions, asset category registration
                "ContentBrowser", // Content Browser integration
                "Slate",       // ← add
                "SlateCore",   // ← add
                "WorkspaceMenuStructure", // ← add (for tab category)
            });
        }

        PrivateIncludePaths.AddRange(new string[]
        {
            "NiagaraGS/Private",
        });
    }
}