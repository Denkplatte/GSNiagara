#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class FGaussianSplatAssetTypeActions : public FAssetTypeActions_Base
{
public:
    virtual FText    GetName()           const override;
    virtual FColor   GetTypeColor()      const override;
    virtual UClass* GetSupportedClass() const override;
    virtual uint32   GetCategories()           override;

    // Called when the user double-clicks the asset
    virtual void OpenAssetEditor(
        const TArray<UObject*>& InObjects,
        TSharedPtr<IToolkitHost>      EditWithinLevelEditor) override;
};

#endif