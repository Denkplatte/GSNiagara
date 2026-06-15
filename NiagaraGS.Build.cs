using UnrealBuildTool;

public class NiagaraGS : ModuleRules
{
    public NiagaraGS(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        // ── Core engine modules ──────────────────────────────────────
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "RenderCore",      // FRenderCommandFence, render thread utilities
            "RHI",             // RHI buffers, SRV/UAV, structured buffers
            "Renderer",        // FSceneRenderer, custom render passes
            "Projects",        // IPluginManager (for shader directory registration)
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "RenderCore",
            "RHI",
            "Niagara",                  // UNiagaraDataInterface base class
            "NiagaraCore",              // FNiagaraTypeRegistry, FNiagaraVariable
            "VectorVM",                 // FVectorVMExternalFunctionContext (CPU NDI)
            "NiagaraShader",            // Niagara HLSL injection infrastructure
        });

        // ── Shader include paths ─────────────────────────────────────
        // We will put our .ush / .usf shader files here later.
        // Unreal needs to know about them at build time.
        PrivateIncludePaths.AddRange(new string[]
        {
            "NiagaraGS/Private",
        });
        PublicIncludePaths.AddRange(new string[]
        {
            "NiagaraGS/Public",
        });
    }
}