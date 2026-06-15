#include "GaussianSplatAssetTypeActions.h"

#if WITH_EDITOR

#include "GaussianSplatAsset.h"
#include "GaussianSplatAssetEditor.h"

FText FGaussianSplatAssetTypeActions::GetName() const
{
    return FText::FromString(TEXT("Gaussian Splat Asset"));
}

FColor FGaussianSplatAssetTypeActions::GetTypeColor() const
{
    return FColor(0, 188, 188);
}

UClass* FGaussianSplatAssetTypeActions::GetSupportedClass() const
{
    return UGaussianSplatAsset::StaticClass();
}

uint32 FGaussianSplatAssetTypeActions::GetCategories()
{
    return EAssetTypeCategories::Misc;
}

void FGaussianSplatAssetTypeActions::OpenAssetEditor(
    const TArray<UObject*>& InObjects,
    TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
    const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid()
        ? EToolkitMode::WorldCentric
        : EToolkitMode::Standalone;

    for (UObject* Obj : InObjects)
    {
        if (UGaussianSplatAsset* Asset = Cast<UGaussianSplatAsset>(Obj))
        {
            TSharedRef<FGaussianSplatAssetEditor> Editor =
                MakeShareable(new FGaussianSplatAssetEditor());
            Editor->InitEditor(Mode, EditWithinLevelEditor, Asset);
        }
    }
}

#endif