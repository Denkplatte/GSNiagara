#include "NiagaraGSModule.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"  // AddShaderSourceDirectoryMapping

#define LOCTEXT_NAMESPACE "FNiagaraGSModule"

void FNiagaraGSModule::StartupModule()
{
    // Register our shader directory so Unreal's shader compiler
    // can find .ush / .usf files we'll add in Step 5.
    // The virtual path "/NiagaraGS" maps to our plugin's Shaders/ folder.
    FString PluginShaderDir = FPaths::Combine(
        IPluginManager::Get().FindPlugin(TEXT("NiagaraGS"))->GetBaseDir(),
        TEXT("Shaders")
    );
    AddShaderSourceDirectoryMapping(TEXT("/NiagaraGS"), PluginShaderDir);
   
}

void FNiagaraGSModule::ShutdownModule()
{
    // Nothing to clean up yet.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FNiagaraGSModule, NiagaraGS)