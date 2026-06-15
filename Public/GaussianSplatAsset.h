#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GaussianSplatData.h"
#include "GaussianSplatAsset.generated.h"

/**
 * A UAsset that holds all parsed splat data for one PLY file.
 * Drag a .ply into the Content Browser → this asset is created.
 * At runtime the NDI reads directly from SplatData.
 *
 * The raw data is stored as a TArray so it serialises cleanly
 * into the .uasset binary. On first use the NDI uploads it to
 * GPU structured buffers (Step 4).
 */
UCLASS(BlueprintType)
class NIAGARAGS_API UGaussianSplatAsset : public UObject
{
    GENERATED_BODY()

public:

    // All splats loaded from the source PLY, already converted to UE space.
    // This is what everything downstream reads from.
    UPROPERTY(BlueprintReadOnly, Category = "Gaussian Splats")
    TArray<FGaussianSplatData> SplatData;

    // Path to the original .ply file — stored so you can re-import later
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Splats")
    FString SourceFilePath;

    // Filled at import time, shown in the asset details panel
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Gaussian Splats")
    int32 SplatCount = 0;

    // ── UObject interface ──────────────────────────────────────────
    virtual void PostLoad() override;

#if WITH_EDITOR
    // Called after (re)import — syncs SplatCount from array length
    void OnImportFinished();

    // Re-parse from SourceFilePath, replacing SplatData in place
    bool ReimportFromSource(FString& OutError);
#endif
};