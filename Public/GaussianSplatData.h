#pragma once

#include "CoreMinimal.h"
#include "GaussianSplatData.generated.h"

/**
 * Represents one fully parsed and UE-coordinate-converted Gaussian Splat.
 * Stored in a TArray on the CPU, then uploaded to GPU structured buffers.
 * Coordinate conversion is applied on import to prevent runtime math overhead.
 */
USTRUCT(BlueprintType)
struct NIAGARAGS_API FGaussianSplatData
{
    GENERATED_BODY()

    // World position in UE coordinates (cm), converted from PLY XYZ (scaled)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaussian Splat")
    FVector3f Position = FVector3f::ZeroVector;

    // Orientation as a quaternion in UE space, converted from PLY rot_0..3
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaussian Splat")
    FQuat4f Orientation = FQuat4f::Identity;

    // Scale in UE coordinates (cm), exp-activated from PLY log-scale values
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaussian Splat")
    FVector3f Scale = FVector3f::OneVector;

    // Opacity [0,1], sigmoid-activated from raw PLY logit value
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaussian Splat")
    float Opacity = 1.0f;

    // Base color from zero-order Spherical Harmonics (f_dc_0, f_dc_1, f_dc_2)
    // Converted to linear RGB [0,1]
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaussian Splat")
    FVector3f Color = FVector3f(0.5f, 0.5f, 0.5f);
};