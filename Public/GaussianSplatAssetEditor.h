#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/IToolkitHost.h"

class UGaussianSplatAsset;

/**
 * Minimal asset editor that opens when you double-click a
 * UGaussianSplatAsset in the Content Browser.
 * Shows a summary panel instead of trying to display 273k structs.
 */
class FGaussianSplatAssetEditor : public FAssetEditorToolkit
{
public:

    void InitEditor(
        const EToolkitMode::Type        Mode,
        const TSharedPtr<IToolkitHost>& InitToolkitHost,
        UGaussianSplatAsset* InAsset
    );

    // FAssetEditorToolkit interface
    virtual FName  GetToolkitFName()              const override;
    virtual FText  GetBaseToolkitName()           const override;
    virtual FText  GetToolkitName()               const override;
    virtual FText  GetToolkitToolTipText()        const override;
    virtual FString GetWorldCentricTabPrefix()    const override;
    virtual FLinearColor GetWorldCentricTabColorScale() const override;
    virtual void   RegisterTabSpawners(const TSharedRef<FTabManager>& TM) override;
    virtual void   UnregisterTabSpawners(const TSharedRef<FTabManager>& TM) override;

private:

    TSharedRef<SDockTab> SpawnSummaryTab(const FSpawnTabArgs& Args);
    TSharedPtr<SWidget>  BuildSummaryWidget();

    UGaussianSplatAsset* Asset = nullptr;

    static const FName SummaryTabId;
};

#endif // WITH_EDITOR