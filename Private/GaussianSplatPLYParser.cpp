#include "GaussianSplatPLYParser.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Math/UnrealMathUtility.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

float FGaussianSplatPLYParser::Sigmoid(float x)
{
    return 1.0f / (1.0f + FMath::Exp(-x));
}

// DC spherical harmonic coefficient → linear RGB
// This is the standard formula: color = 0.5 + SH_C0 * f_dc
// where SH_C0 = 0.28209479177387814 (1 / (2 * sqrt(pi)))
FVector3f FGaussianSplatPLYParser::SHDCToColor(float dc0, float dc1, float dc2)
{
    const float SH_C0 = 0.28209479177387814f;
    return FVector3f(
        FMath::Clamp(0.5f + SH_C0 * dc0, 0.0f, 1.0f),
        FMath::Clamp(0.5f + SH_C0 * dc1, 0.0f, 1.0f),
        FMath::Clamp(0.5f + SH_C0 * dc2, 0.0f, 1.0f)
    );
}

// PLY coordinate system:  X right, Y up, Z forward (right-handed)
// UE coordinate system:   X forward, Y right, Z up (left-handed)
// Mapping:  UE.X = PLY.X * 100   (also meters → centimeters)
//           UE.Y = -PLY.Z * 100
//           UE.Z = -PLY.Y * 100   (note: flipped — PLY Y is depth)
// Wait — actually the Magnopus derivation gives us:
//   UE position = (PLY.x, -PLY.z, -PLY.y) * 100
// which is what we use here.
FVector3f FGaussianSplatPLYParser::ConvertPosition(float x, float y, float z)
{
    return FVector3f(x * 100.0f, -z * 100.0f, -y * 100.0f);
}

// PLY scale is log-scale, needs sigmoid activation, then cm conversion
FVector3f FGaussianSplatPLYParser::ConvertScale(float sx, float sy, float sz)
{
    // exp() to un-log, then * 100 for meters → cm
    // Note: some loaders use exp() directly (3DGS paper), 
    // others use sigmoid. We use exp() to match the original 3DGS convention.
    return FVector3f(
        FMath::Exp(sx) * 100.0f,
        FMath::Exp(sy) * 100.0f,
        FMath::Exp(sz) * 100.0f
    );
}

// PLY quaternion is (rot_0=w, rot_1=x, rot_2=y, rot_3=z)
// UE FQuat4f is (X, Y, Z, W)
// We also need to apply the same axis flip as position.
// The correct transform for the PLY→UE axis flip on a quaternion is:
//   UE.quat = normalize( rot_1, -rot_3, -rot_2, rot_0 )
// (derived by composing the axis permutation matrix with the quaternion)
FQuat4f FGaussianSplatPLYParser::ConvertOrientation(float w, float x, float y, float z)
{
    FQuat4f Q(x, -z, -y, w);
    Q.Normalize();
    return Q;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Header parsing
// ─────────────────────────────────────────────────────────────────────────────

int32 FGaussianSplatPLYParser::FPLYHeader::OffsetOf(const FString& Name) const
{
    for (const FPLYProperty& P : Properties)
    {
        if (P.Name == Name) return P.ByteOffset;
    }
    return -1;
}

bool FGaussianSplatPLYParser::ParseHeader(
    const TArray<uint8>& FileBytes,
    FPLYHeader& OutHeader,
    int32& OutDataStartByte,
    FString& OutError)
{
    // PLY headers are always ASCII text ending with "end_header\n"
    // We scan byte by byte building lines until we hit that sentinel.

    int32 Pos = 0;
    const int32 FileSize = FileBytes.Num();

    auto ReadLine = [&](FString& OutLine) -> bool
        {
            OutLine.Empty();
            while (Pos < FileSize)
            {
                char C = (char)FileBytes[Pos++];
                if (C == '\n') return true;
                if (C != '\r') OutLine.AppendChar(C);
            }
            return false;
        };

    FString Line;

    // First line must be "ply"
    if (!ReadLine(Line) || Line.TrimStartAndEnd() != TEXT("ply"))
    {
        OutError = TEXT("Not a PLY file (missing 'ply' header)");
        return false;
    }

    bool bFoundFormat = false;
    bool bInVertexElement = false;
    int32 CurrentByteOffset = 0;

    while (ReadLine(Line))
    {
        Line = Line.TrimStartAndEnd();

        if (Line == TEXT("end_header"))
        {
            OutDataStartByte = Pos;  // byte immediately after "end_header\n"
            break;
        }

        TArray<FString> Tokens;
        Line.ParseIntoArrayWS(Tokens);

        if (Tokens.Num() == 0) continue;

        // format ascii 1.0  |  format binary_little_endian 1.0
        if (Tokens[0] == TEXT("format") && Tokens.Num() >= 2)
        {
            bFoundFormat = true;
            if (Tokens[1] == TEXT("ascii"))
            {
                OutHeader.bIsBinary = false;
            }
            else if (Tokens[1] == TEXT("binary_little_endian"))
            {
                OutHeader.bIsBinary = true;
                OutHeader.bIsLittleEndian = true;
            }
            else if (Tokens[1] == TEXT("binary_big_endian"))
            {
                // We could support this but almost no 3DGS tool produces it
                OutError = TEXT("Big-endian PLY not supported");
                return false;
            }
        }

        // element vertex 1234567
        else if (Tokens[0] == TEXT("element") && Tokens.Num() >= 3)
        {
            bInVertexElement = (Tokens[1] == TEXT("vertex"));
            if (bInVertexElement)
            {
                OutHeader.VertexCount = FCString::Atoi(*Tokens[2]);
            }
        }

        // property float x  |  property float scale_0  etc.
        else if (Tokens[0] == TEXT("property") && bInVertexElement && Tokens.Num() >= 3)
        {
            FPLYProperty Prop;
            Prop.Name = Tokens[2];

            const FString& TypeStr = Tokens[1];
            if (TypeStr == TEXT("float") || TypeStr == TEXT("float32"))
            {
                Prop.ByteSize = 4;
                Prop.IsFloat = true;
            }
            else if (TypeStr == TEXT("double") || TypeStr == TEXT("float64"))
            {
                // Rare but seen in some exporters
                Prop.ByteSize = 8;
                Prop.IsFloat = false; // we'll read but discard precision
            }
            else if (TypeStr == TEXT("int") || TypeStr == TEXT("int32") ||
                TypeStr == TEXT("uint") || TypeStr == TEXT("uint32"))
            {
                Prop.ByteSize = 4;
                Prop.IsFloat = false;
            }
            else
            {
                // Unknown type — record size 0 so we can warn but not crash
                Prop.ByteSize = 0;
                UE_LOG(LogTemp, Warning, TEXT("NiagaraGS: Unknown PLY property type '%s' for '%s', skipping"),
                    *TypeStr, *Prop.Name);
            }

            Prop.ByteOffset = CurrentByteOffset;
            CurrentByteOffset += Prop.ByteSize;
            OutHeader.Properties.Add(Prop);
        }
    }

    if (!bFoundFormat)
    {
        OutError = TEXT("PLY header missing format declaration");
        return false;
    }

    if (OutHeader.VertexCount <= 0)
    {
        OutError = TEXT("PLY header has no vertex element or zero vertices");
        return false;
    }

    OutHeader.ElementStride = CurrentByteOffset;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Single splat conversion
// ─────────────────────────────────────────────────────────────────────────────

// Reads a float from a raw byte pointer at a given offset.
// We cast directly — this is safe because PLY binary is always tightly packed
// and our platform is little-endian (x86/ARM).
static float ReadFloat(const uint8* Data, int32 Offset)
{
    float Val;
    FMemory::Memcpy(&Val, Data + Offset, 4);
    return Val;
}

FGaussianSplatData FGaussianSplatPLYParser::ConvertSplat(
    const uint8* ElementData,
    const FPLYHeader& Header)
{
    FGaussianSplatData Out;

    // Helper lambda: read a named property or return a default
    auto Get = [&](const FString& Name, float Default = 0.0f) -> float
        {
            int32 Off = Header.OffsetOf(Name);
            if (Off < 0) return Default;
            return ReadFloat(ElementData, Off);
        };

    // Position
    float px = Get(TEXT("x"));
    float py = Get(TEXT("y"));
    float pz = Get(TEXT("z"));
    Out.Position = ConvertPosition(px, py, pz);

    // Scale (log-space in PLY)
    float sx = Get(TEXT("scale_0"), 0.0f);
    float sy = Get(TEXT("scale_1"), 0.0f);
    float sz = Get(TEXT("scale_2"), 0.0f);
    Out.Scale = ConvertScale(sx, sy, sz);

    // Orientation (PLY stores as rot_0=w, rot_1=x, rot_2=y, rot_3=z)
    float rw = Get(TEXT("rot_0"), 1.0f);
    float rx = Get(TEXT("rot_1"), 0.0f);
    float ry = Get(TEXT("rot_2"), 0.0f);
    float rz = Get(TEXT("rot_3"), 0.0f);
    Out.Orientation = ConvertOrientation(rw, rx, ry, rz);

    // Opacity (logit-space in PLY, sigmoid to get [0,1])
    Out.Opacity = Sigmoid(Get(TEXT("opacity"), 0.0f));

    // Color from zero-order SH DC coefficients
    float dc0 = Get(TEXT("f_dc_0"), 0.0f);
    float dc1 = Get(TEXT("f_dc_1"), 0.0f);
    float dc2 = Get(TEXT("f_dc_2"), 0.0f);
    Out.Color = SHDCToColor(dc0, dc1, dc2);

    return Out;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main entry point
// ─────────────────────────────────────────────────────────────────────────────

bool FGaussianSplatPLYParser::ParsePLY(
    const FString& FilePath,
    TArray<FGaussianSplatData>& OutSplats,
    FString& OutError)
{
    // Load entire file into memory.
    // For multi-million-splat files this is 200–800 MB — UE's TArray
    // handles this fine. We'll add streaming in a later step if needed.
    TArray<uint8> FileBytes;
    if (!FFileHelper::LoadFileToArray(FileBytes, *FilePath))
    {
        OutError = FString::Printf(TEXT("Could not open file: %s"), *FilePath);
        return false;
    }

    FPLYHeader Header;
    int32 DataStart = 0;

    if (!ParseHeader(FileBytes, Header, DataStart, OutError))
    {
        return false;
    }

    // Validate we have the minimum required properties
    const TArray<FString> Required = {
        TEXT("x"), TEXT("y"), TEXT("z"),
        TEXT("rot_0"), TEXT("rot_1"), TEXT("rot_2"), TEXT("rot_3"),
        TEXT("scale_0"), TEXT("scale_1"), TEXT("scale_2"),
        TEXT("opacity"),
        TEXT("f_dc_0"), TEXT("f_dc_1"), TEXT("f_dc_2")
    };
    for (const FString& Req : Required)
    {
        if (Header.OffsetOf(Req) < 0)
        {
            OutError = FString::Printf(
                TEXT("PLY missing required property '%s'. Is this a 3DGS PLY?"), *Req);
            return false;
        }
    }

    OutSplats.Empty();
    OutSplats.Reserve(Header.VertexCount);

    if (Header.bIsBinary)
    {
        // ── Binary path (fast) ──────────────────────────────────────
        const int32 ExpectedBytes = DataStart + Header.VertexCount * Header.ElementStride;
        if (FileBytes.Num() < ExpectedBytes)
        {
            OutError = FString::Printf(
                TEXT("PLY binary data truncated. Expected %d bytes, got %d"),
                ExpectedBytes, FileBytes.Num());
            return false;
        }

        const uint8* DataPtr = FileBytes.GetData() + DataStart;

        for (int32 i = 0; i < Header.VertexCount; ++i)
        {
            OutSplats.Add(ConvertSplat(DataPtr, Header));
            DataPtr += Header.ElementStride;
        }
    }
    else
    {
        // ── ASCII path (slow, but correct for compatibility) ─────────
        // Re-scan from DataStart, parsing one line per vertex
        int32 Pos = DataStart;
        const int32 FileSize = FileBytes.Num();

        for (int32 i = 0; i < Header.VertexCount; ++i)
        {
            // Read one line
            FString Line;
            while (Pos < FileSize)
            {
                char C = (char)FileBytes[Pos++];
                if (C == '\n') break;
                if (C != '\r') Line.AppendChar(C);
            }

            TArray<FString> Tokens;
            Line.TrimStartAndEnd().ParseIntoArrayWS(Tokens);

            if (Tokens.Num() < Header.Properties.Num())
            {
                OutError = FString::Printf(
                    TEXT("ASCII PLY line %d has too few tokens"), i);
                return false;
            }

            // Build a fake binary block so ConvertSplat works identically
            TArray<uint8> FakeBinary;
            FakeBinary.SetNumZeroed(Header.ElementStride);

            for (int32 p = 0; p < Header.Properties.Num(); ++p)
            {
                const FPLYProperty& Prop = Header.Properties[p];
                if (Prop.ByteSize == 4 && Prop.IsFloat)
                {
                    float Val = FCString::Atof(*Tokens[p]);
                    FMemory::Memcpy(FakeBinary.GetData() + Prop.ByteOffset, &Val, 4);
                }
            }

            OutSplats.Add(ConvertSplat(FakeBinary.GetData(), Header));
        }
    }

    UE_LOG(LogTemp, Log, TEXT("NiagaraGS: Parsed %d splats from %s"),
        OutSplats.Num(), *FPaths::GetCleanFilename(FilePath));

    return true;
}