#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RenderResource.h"
#include "NiagaraDataInterfaceBase.h"
#include "NiagaraDataInterfaceRW.h"
#include "GaussianSplatData.h"

// ── Per-splat GPU layout ──────────────────────────────────────────────────────
// We pack splat data into four float4 buffers.
// float4 wastes the W channel on position and scale but keeps
// buffer reads aligned on 16 bytes which is optimal on all GPU vendors.
//
// Buffer layout:
//   Positions   [float4]  xyz=position   w=unused
//   Scales      [float4]  xyz=scale      w=unused
//   Rotations   [float4]  xyzw=quaternion
//   ColorOpacity[float4]  xyz=color      w=opacity

// ── Shader parameter struct ───────────────────────────────────────────────────
BEGIN_SHADER_PARAMETER_STRUCT(FNiagaraGSShaderParameters, )
    SHADER_PARAMETER(int32, SplatCount)
    SHADER_PARAMETER_SRV(Buffer<float4>, Positions)
    SHADER_PARAMETER_SRV(Buffer<float4>, Scales)
    SHADER_PARAMETER_SRV(Buffer<float4>, Rotations)
    SHADER_PARAMETER_SRV(Buffer<float4>, ColorOpacity)
END_SHADER_PARAMETER_STRUCT()

// ── A single typed GPU buffer + its SRV ──────────────────────────────────────
struct FNiagaraGSSplatBuffer
{
    FBufferRHIRef                 Buffer;
    FShaderResourceViewRHIRef     SRV;

    bool IsValid() const { return Buffer.IsValid(); }

    void Release()
    {
        SRV.SafeRelease();
        Buffer.SafeRelease();
    }
};

// ── GPU Proxy ─────────────────────────────────────────────────────────────────
// One proxy exists per NDI instance on the render thread.
// It owns the four GPU buffers.
//
// IMPORTANT: UploadData_RenderThread must be called DIRECTLY on the render
// thread (e.g. from inside an ENQUEUE_RENDER_COMMAND lambda). It must NOT
// itself enqueue another render command — that causes double-enqueue bugs
// where buffers are created too late and bBuffersReady is set prematurely.

struct FNDIGaussianSplatProxy : public FNiagaraDataInterfaceProxyRW
{
    // ── GPU buffers ───────────────────────────────────────────────
    FNiagaraGSSplatBuffer PositionsBuffer;
    FNiagaraGSSplatBuffer ScalesBuffer;
    FNiagaraGSSplatBuffer RotationsBuffer;
    FNiagaraGSSplatBuffer ColorOpacityBuffer;

    // How many splats are in the buffers
    int32 SplatCount = 0;

    // Set to true only after all four buffers are fully created on the RT
    bool bBuffersReady = false;

    // ── Upload (MUST be called on the render thread) ───────────────
    // Creates GPU buffers synchronously within the calling render command.
    // All RHI work happens in the same command list as the caller.
    void UploadData_RenderThread(
        FRHICommandListImmediate& RHICmdList,
        const TArray<FGaussianSplatData>& Splats);

    // ── FNiagaraDataInterfaceProxyRW interface ────────────────────
    virtual int32 PerInstanceDataPassedToRenderThreadSize() const override
    {
        return 0;
    }

    // Release all GPU resources — must be called on the render thread
    void ReleaseBuffers_RenderThread()
    {
        PositionsBuffer.Release();
        ScalesBuffer.Release();
        RotationsBuffer.Release();
        ColorOpacityBuffer.Release();
        bBuffersReady = false;
        SplatCount = 0;
    }

    virtual ~FNDIGaussianSplatProxy()
    {
        // By the time the destructor runs UE has already flushed the
        // render thread so direct release is safe here.
        PositionsBuffer.Release();
        ScalesBuffer.Release();
        RotationsBuffer.Release();
        ColorOpacityBuffer.Release();
    }
};