#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Factories/Factory.h"
#include "GaussianSplatAssetFactory.generated.h"

/**
 * Editor-only factory.
 * Unreal's import pipeline instantiates this when you drop a .ply
 * file into the Content Browser. FactoryCreateFile() does the work.
 */
UCLASS()
class UGaussianSplatAssetFactory : public UFactory
{
    GENERATED_BODY()

public:
    UGaussianSplatAssetFactory();

    // UFactory interface
    virtual UObject* FactoryCreateFile(
        UClass* InClass,
        UObject* InParent,
        FName               InName,
        EObjectFlags        Flags,
        const FString& Filename,
        const TCHAR* Parms,
        FFeedbackContext* Warn,
        bool& bOutOperationCanceled
    ) override;

    virtual bool FactoryCanImport(const FString& Filename) override;
};

#endif // WITH_EDITOR
