#include "NiagaraGSModule.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "GaussianSplatAssetTypeActions.h"
#endif

#define LOCTEXT_NAMESPACE "FNiagaraGSModule"

void FNiagaraGSModule::StartupModule()
{
    // Register shader directory (from Step 1)
    FString PluginShaderDir = FPaths::Combine(
        IPluginManager::Get().FindPlugin(TEXT("NiagaraGS"))->GetBaseDir(),
        TEXT("Shaders")
    );
    AddShaderSourceDirectoryMapping(TEXT("/NiagaraGS"), PluginShaderDir);

#if WITH_EDITOR
    // Register asset type actions so the Content Browser
    // knows how to display and handle UGaussianSplatAsset
    IAssetTools& AssetTools =
        FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools")
        .Get();

    AssetTypeActions = MakeShareable(new FGaussianSplatAssetTypeActions());
    AssetTools.RegisterAssetTypeActions(AssetTypeActions.ToSharedRef());
#endif
}

void FNiagaraGSModule::ShutdownModule()
{
#if WITH_EDITOR
    // Unregister on shutdown to avoid dangling pointers
    if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
    {
        IAssetTools& AssetTools =
            FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools")
            .Get();
        AssetTools.UnregisterAssetTypeActions(AssetTypeActions.ToSharedRef());
    }
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FNiagaraGSModule, NiagaraGS)