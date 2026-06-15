#pragma once

#include "CoreMinimal.h"
#include "GaussianSplatData.h"

/**
 * Stateless utility class — call ParsePLY and get back an array of splats.
 * Supports both ASCII and binary little-endian PLY files.
 * All coordinate conversions (PLY → UE space) are applied here.
 */
class NIAGARAGS_API FGaussianSplatPLYParser
{
public:

    /**
     * Parse a .ply file at the given absolute path.
     *
     * @param FilePath   Absolute path to the .ply file
     * @param OutSplats  Filled with one FGaussianSplatData per splat on success
     * @param OutError   Human-readable error string on failure
     * @return           True on success, false on any parse error
     */
    static bool ParsePLY(
        const FString& FilePath,
        TArray<FGaussianSplatData>& OutSplats,
        FString& OutError
    );

private:

    // Internal header description of a single PLY property
    struct FPLYProperty
    {
        FString Name;
        int32   ByteOffset = 0;   // byte offset within one element's data block
        int32   ByteSize = 0;   // 4 = float/int32, 8 = double (we only use float)
        bool    IsFloat = true;
    };

    // Everything we need to know after reading the PLY header
    struct FPLYHeader
    {
        int32                  VertexCount = 0;
        int32                  ElementStride = 0;  // total bytes per vertex
        TArray<FPLYProperty>   Properties;
        bool                   bIsBinary = true;
        bool                   bIsLittleEndian = true;

        // Returns byte offset of a named property, -1 if not found
        int32 OffsetOf(const FString& Name) const;
    };

    static bool ParseHeader(
        const TArray<uint8>& FileBytes,
        FPLYHeader& OutHeader,
        int32& OutDataStartByte,
        FString& OutError
    );

    static FGaussianSplatData ConvertSplat(
        const uint8* ElementData,
        const FPLYHeader& Header
    );

    // PLY → UE coordinate conversions (see Magnopus article derivation)
    static FVector3f   ConvertPosition(float x, float y, float z);
    static FVector3f   ConvertScale(float sx, float sy, float sz);
    static FQuat4f     ConvertOrientation(float w, float x, float y, float z);
    static float       Sigmoid(float x);
    static FVector3f   SHDCToColor(float dc0, float dc1, float dc2);
};