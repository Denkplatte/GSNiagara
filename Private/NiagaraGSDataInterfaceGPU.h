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

    // ── Fallback: a single zeroed float4 element bound when real
    //    data isn't ready. Prevents Unreal's shader parameter
    //    validation from crashing on null SRV slots.
    FNiagaraGSSplatBuffer FallbackBuffer;

    int32 SplatCount = 0;
    bool  bBuffersReady = false;

    void UploadData(const TArray<FGaussianSplatData>& Splats);
    void InitFallbackBuffer();   // ← new

    virtual int32 PerInstanceDataPassedToRenderThreadSize() const override
    {
        return 0;
    }

    void ReleaseBuffers()
    {
        PositionsBuffer.Release();
        ScalesBuffer.Release();
        RotationsBuffer.Release();
        ColorOpacityBuffer.Release();
        // Note: do NOT release FallbackBuffer here —
        // it must stay valid for the lifetime of the proxy.
        bBuffersReady = false;
        SplatCount = 0;
    }

    virtual ~FNDIGaussianSplatProxy()
    {
        ReleaseBuffers();
        FallbackBuffer.Release();
    }
};