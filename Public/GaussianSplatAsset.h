#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GaussianSplatData.h"
#include "GaussianSplatAsset.generated.h"

/**
 * added this line before commiting since this was almost working
 * A UAsset that holds all parsed splat data for one PLY file.
 * Drag a .ply into the Content Browser → this asset is created.
 * At runtime, the Niagara Data Interface reads directly from SplatData.
 */
UCLASS(BlueprintType)
class NIAGARAGS_API UGaussianSplatAsset : public UObject
{
    GENERATED_BODY()

public:
    // All splats loaded from the source PLY, already converted to UE space.
    UPROPERTY(BlueprintReadOnly, Category = "Gaussian Splats")
    TArray<FGaussianSplatData> SplatData;

    // Path to the original .ply file (useful for re-importing)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Splats")
    FString SourceFilePath;

    // Total splat count, shown in the asset details panel
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Splats")
    int32 SplatCount = 0;

    // ── UObject interface ──────────────────────────────────────────
    virtual void PostLoad() override;

#if WITH_EDITOR
    // Called after import or reimport to sync and flag dirty
    void OnImportFinished();

    // Re-parse from SourceFilePath, replacing SplatData in place
    bool ReimportFromSource(FString& OutError);
#endif
};