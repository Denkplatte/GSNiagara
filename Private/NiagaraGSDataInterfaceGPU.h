#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RenderResource.h"
#include "NiagaraDataInterfaceBase.h"
#include "NiagaraDataInterfaceRW.h"
#include "GaussianSplatData.h"

// GPU Memory layout. Structured buffers wrapped by RHI Shader Resource Views (SRV)
BEGIN_SHADER_PARAMETER_STRUCT(FNiagaraGSShaderParameters, )
    SHADER_PARAMETER(int32, SplatCount)
    SHADER_PARAMETER_SRV(Buffer<float4>, Positions)
    SHADER_PARAMETER_SRV(Buffer<float4>, Scales)
    SHADER_PARAMETER_SRV(Buffer<float4>, Rotations)
    SHADER_PARAMETER_SRV(Buffer<float4>, ColorOpacity)
END_SHADER_PARAMETER_STRUCT()

struct FNiagaraGSSplatBuffer
{
    FBufferRHIRef      Buffer;
    FShaderResourceViewRHIRef SRV;

    bool IsValid() const { return Buffer.IsValid(); }

    void Release()
    {
        SRV.SafeRelease();
        Buffer.SafeRelease();
    }
};

struct FNDIGaussianSplatProxy : public FNiagaraDataInterfaceProxyRW
{
    FNiagaraGSSplatBuffer PositionsBuffer;
    FNiagaraGSSplatBuffer ScalesBuffer;
    FNiagaraGSSplatBuffer RotationsBuffer;
    FNiagaraGSSplatBuffer ColorOpacityBuffer;

    // Fallback: a single zeroed float4 element bound when real data isn't ready.
    // Prevents Unreal's shader parameter validation from crashing on null SRV slots.
    FNiagaraGSSplatBuffer FallbackBuffer;

    int32 SplatCount = 0;
    bool bBuffersReady = false;

    void UploadData(const TArray<FGaussianSplatData>& Splats);
    void InitFallbackBuffer();

    virtual int32 PerInstanceDataPassedToRenderThreadSize() const override { return 0; }

    void ReleaseBuffers()
    {
        PositionsBuffer.Release();
        ScalesBuffer.Release();
        RotationsBuffer.Release();
        ColorOpacityBuffer.Release();
        bBuffersReady = false;
        SplatCount = 0;
    }

    // CRITICAL THREAD SAFETY FIX:
    // This destructor can execute on the GC sweep channel (Game Thread).
    // Direct RHI releases of buffer/SRV handles on the Game Thread trigger instant graphics engine driver assertions!
    // We safely package and command-enqueue the releases onto the Render Thread.
    virtual ~FNDIGaussianSplatProxy()
    {
        FNiagaraGSSplatBuffer TempPositions = PositionsBuffer;
        FNiagaraGSSplatBuffer TempScales = ScalesBuffer;
        FNiagaraGSSplatBuffer TempRotations = RotationsBuffer;
        FNiagaraGSSplatBuffer TempColorOpacity = ColorOpacityBuffer;
        FNiagaraGSSplatBuffer TempFallback = FallbackBuffer;

        ENQUEUE_RENDER_COMMAND(NiagaraGS_ProxyReleaseRHI)(
            [TempPositions, TempScales, TempRotations, TempColorOpacity, TempFallback](FRHICommandListImmediate& RHICmdList) mutable
            {
                // Execute SafeRelease safely under the correct thread!
                TempPositions.Release();
                TempScales.Release();
                TempRotations.Release();
                TempColorOpacity.Release();
                TempFallback.Release();
            }
            );
    }
};