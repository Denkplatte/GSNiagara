#include "GaussianSplatAsset.h"
#include "GaussianSplatPLYParser.h"

void UGaussianSplatAsset::PostLoad()
{
    Super::PostLoad();

    // Sync count in case asset was hand-edited or partially corrupted
    SplatCount = SplatData.Num();

    UE_LOG(LogTemp, Log,
        TEXT("NiagaraGS: Asset '%s' loaded with %d splats"),
        *GetName(), SplatCount);
}

#if WITH_EDITOR

void UGaussianSplatAsset::OnImportFinished()
{
    SplatCount = SplatData.Num();
    MarkPackageDirty();  // flags the .uasset as needing save
}

bool UGaussianSplatAsset::ReimportFromSource(FString& OutError)
{
    if (SourceFilePath.IsEmpty())
    {
        OutError = TEXT("No source file path stored on this asset.");
        return false;
    }

    TArray<FGaussianSplatData> NewSplats;
    if (!FGaussianSplatPLYParser::ParsePLY(SourceFilePath, NewSplats, OutError))
    {
        return false;
    }

    SplatData = MoveTemp(NewSplats);
    OnImportFinished();
    return true;
}

#endif