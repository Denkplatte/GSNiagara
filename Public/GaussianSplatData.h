#pragma once

#include "CoreMinimal.h"
#include "GaussianSplatData.generated.h"

/**
 * Represents one fully parsed and UE-coordinate-converted Gaussian Splat.
 * Stored in a TArray on the CPU, then uploaded to GPU structured buffers
 * by the NDI in Step 4.
 *
 * Coordinate conversion from PLY space to UE space is done once at parse
 * time so the GPU never has to think about it.
 */
USTRUCT(BlueprintType)
struct NIAGARAGS_API FGaussianSplatData
{
    GENERATED_BODY()

    // World position in UE coordinates (cm), converted from PLY XYZ
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector3f Position = FVector3f::ZeroVector;

    // Orientation as a quaternion in UE space, converted from PLY rot_0..3
    // PLY stores WXYZ, UE FQuat is XYZW — we reorder on load
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FQuat4f Orientation = FQuat4f::Identity;

    // Scale in UE coordinates (cm), sigmoid-activated from PLY log-scale values
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector3f Scale = FVector3f::OneVector;

    // Opacity in [0,1], sigmoid-activated from raw PLY value
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Opacity = 1.0f;

    // Base color from zero-order Spherical Harmonics (f_dc_0, f_dc_1, f_dc_2)
    // Converted from SH DC coefficient to linear RGB [0,1]
    // Formula: color = 0.5 + 0.2820947918 * f_dc
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector3f Color = FVector3f(0.5f, 0.5f, 0.5f);
};
