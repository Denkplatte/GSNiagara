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
// This macro generates a struct that Unreal's RHI knows how to bind
// to a shader. Each SHADER_PARAMETER_SRV maps to a Buffer<float4>
// in our HLSL (read-only structured buffer view).

BEGIN_SHADER_PARAMETER_STRUCT(FNiagaraGSShaderParameters, )
    SHADER_PARAMETER(int32, SplatCount)
    SHADER_PARAMETER_SRV(Buffer<float4>, Positions)
    SHADER_PARAMETER_SRV(Buffer<float4>, Scales)
    SHADER_PARAMETER_SRV(Buffer<float4>, Rotations)
    SHADER_PARAMETER_SRV(Buffer<float4>, ColorOpacity)
END_SHADER_PARAMETER_STRUCT()

// ── A single typed GPU buffer + its SRV ──────────────────────────────────────
// We wrap these together because they're always created and destroyed
// as a pair. FVertexBuffer gives us a properly RHI-tracked buffer object.

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

// ── GPU Proxy ─────────────────────────────────────────────────────────────────
// One proxy exists per NDI instance on the render thread.
// It owns the four GPU buffers and is responsible for uploading
// data from the CPU exactly once (on first SetShaderParameters call).
//
// Lifecycle:
//   NDI::PostInitProperties  → creates the proxy (on game thread)
//   NDI::SetShaderParameters → proxy uploads data if not yet uploaded
//   NDI destroyed            → proxy released, buffers freed

struct FNDIGaussianSplatProxy : public FNiagaraDataInterfaceProxyRW
{
    // ── GPU buffers ───────────────────────────────────────────────
    FNiagaraGSSplatBuffer PositionsBuffer;
    FNiagaraGSSplatBuffer ScalesBuffer;
    FNiagaraGSSplatBuffer RotationsBuffer;
    FNiagaraGSSplatBuffer ColorOpacityBuffer;

    // How many splats are in the buffers
    int32 SplatCount = 0;

    // Set to true after first successful upload
    bool bBuffersReady = false;

    // ── Upload ────────────────────────────────────────────────────
    // Called from the render thread via ENQUEUE_RENDER_COMMAND.
    // Takes a snapshot of the CPU splat array and pushes it to VRAM.
    void UploadData(const TArray<FGaussianSplatData>& Splats);

    // ── FNiagaraDataInterfaceProxyRW interface ────────────────────
    virtual int32 PerInstanceDataPassedToRenderThreadSize() const override
    {
        return 0; // we don't use per-instance game thread data
    }

    // Release all GPU resources — called on render thread during cleanup
    void ReleaseBuffers()
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
        // Buffers must be released on the render thread
        // By the time this destructor runs UE has already
        // flushed the render thread so direct release is safe here
        ReleaseBuffers();
    }
};