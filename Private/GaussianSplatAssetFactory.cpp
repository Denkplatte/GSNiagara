#include "GaussianSplatAssetFactory.h"
#include "Misc/FeedbackContext.h"

#if WITH_EDITOR

#include "GaussianSplatAsset.h"
#include "GaussianSplatPLYParser.h"
#include "Misc/Paths.h"
#include "EditorFramework/AssetImportData.h"


UGaussianSplatAssetFactory::UGaussianSplatAssetFactory()
{
    // Tell Unreal what this factory produces
    SupportedClass = UGaussianSplatAsset::StaticClass();

    // Register .ply as an importable extension
    Formats.Add(TEXT("ply;Gaussian Splat PLY File"));

    // This is an import factory (creates assets from external files),
    // not a creation factory (creates blank assets)
    bCreateNew = false;
    bEditorImport = true;
    bText = false;  // binary file
}

bool UGaussianSplatAssetFactory::FactoryCanImport(const FString& Filename)
{
    // Only handle .ply files — leave all other formats alone
    return FPaths::GetExtension(Filename).ToLower() == TEXT("ply");
}

UObject* UGaussianSplatAssetFactory::FactoryCreateFile(
    UClass* InClass,
    UObject* InParent,
    FName            InName,
    EObjectFlags     Flags,
    const FString& Filename,
    const TCHAR* Parms,
    FFeedbackContext* Warn,
    bool& bOutOperationCanceled)
{
    bOutOperationCanceled = false;

    // Parse the PLY
    TArray<FGaussianSplatData> ParsedSplats;
    FString ParseError;

    if (!FGaussianSplatPLYParser::ParsePLY(Filename, ParsedSplats, ParseError))
    {

        // Show error in editor notification
        UE_LOG(LogTemp, Error, TEXT("NiagaraGS: Failed to import '%s': %s"), *Filename, *ParseError);
        return nullptr;
    }

    // Create the asset object inside the Content Browser package
    UGaussianSplatAsset* NewAsset = NewObject<UGaussianSplatAsset>(
        InParent, InClass, InName, Flags);

    NewAsset->SplatData = MoveTemp(ParsedSplats);
    NewAsset->SourceFilePath = Filename;
    NewAsset->OnImportFinished();

    UE_LOG(LogTemp, Log,
        TEXT("NiagaraGS: Imported '%s' → %d splats"),
        *FPaths::GetCleanFilename(Filename),
        NewAsset->SplatCount);

    return NewAsset;
}

#endif // WITH_EDITOR