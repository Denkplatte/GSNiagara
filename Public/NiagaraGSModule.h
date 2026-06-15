#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "IAssetTypeActions.h"
#endif

class FNiagaraGSModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
#if WITH_EDITOR
    TSharedPtr<class FGaussianSplatAssetTypeActions> AssetTypeActions;
#endif
};