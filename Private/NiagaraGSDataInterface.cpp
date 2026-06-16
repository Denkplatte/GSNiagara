#include "NiagaraGSDataInterface.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RHIDefinitions.h"
#include "NiagaraDataInterfaceRW.h"

#include "NiagaraTypes.h"
#include "NiagaraCustomVersion.h"
#include "NiagaraShaderParametersBuilder.h"
#include "VectorVM.h"

#include "NiagaraGSDataInterfaceGPU.h"
#include "RHICommandList.h"
#include "RenderingThread.h"

// ── Static name definitions ───────────────────────────────────────────────────
// Defining them here means they're initialised exactly once and shared across
// all translation units that include the header.

const FName UNiagaraGSDataInterface::Name_GetSplatCount(TEXT("GetSplatCount"));
const FName UNiagaraGSDataInterface::Name_GetSplatPosition(TEXT("GetSplatPosition"));
const FName UNiagaraGSDataInterface::Name_GetSplatScale(TEXT("GetSplatScale"));
const FName UNiagaraGSDataInterface::Name_GetSplatOrientation(TEXT("GetSplatOrientation"));
const FName UNiagaraGSDataInterface::Name_GetSplatColor(TEXT("GetSplatColor"));
const FName UNiagaraGSDataInterface::Name_GetSplatOpacity(TEXT("GetSplatOpacity"));


// ── CopyToInternal / Equals ───────────────────────────────────────────────────
// Niagara calls these when duplicating or comparing NDI instances.
// We only need to sync the asset reference — the splat data lives on the asset.

bool UNiagaraGSDataInterface::CopyToInternal(UNiagaraDataInterface* Destination) const
{
    if (!Super::CopyToInternal(Destination)) return false;

    UNiagaraGSDataInterface* Dest = CastChecked<UNiagaraGSDataInterface>(Destination);
    Dest->SplatAsset = SplatAsset;
    return true;
}

bool UNiagaraGSDataInterface::Equals(const UNiagaraDataInterface* Other) const
{
    if (!Super::Equals(Other)) return false;

    const UNiagaraGSDataInterface* OtherDI =
        CastChecked<const UNiagaraGSDataInterface>(Other);

    return OtherDI->SplatAsset == SplatAsset;
}

// ── Splat count helper ────────────────────────────────────────────────────────

int32 UNiagaraGSDataInterface::GetSplatCount() const
{
    if (SplatAsset && SplatAsset->SplatData.Num() > 0)
    {
        return SplatAsset->SplatData.Num();
    }
    return 0;
}

// ── GetFunctions ──────────────────────────────────────────────────────────────
// This is the manifest Niagara reads to know what scratch pad nodes exist.
// Every function declared here needs a CPU binding below AND an HLSL
// definition in Step 5.

void UNiagaraGSDataInterface::GetFunctions(
    TArray<FNiagaraFunctionSignature>& OutFunctions)
{
    // ── Helper to build the standard NDI self-reference input ──────
    // Every NDI function's first input must be the NDI itself.
    // This is how Niagara routes the call to the right instance.
    auto NDISelf = [this]() -> FNiagaraVariable
        {
            return FNiagaraVariable(
                FNiagaraTypeDefinition(GetClass()), TEXT("GaussianSplatDI"));
        };

    // ── GetSplatCount ──────────────────────────────────────────────
    // Returns total number of splats. Use this to drive emitter SpawnCount.
    // Inputs:  (NDI self)
    // Outputs: int Count
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = Name_GetSplatCount;
        Sig.bMemberFunction = true;
        Sig.bRequiresContext = false;
        Sig.Inputs.Add(NDISelf());
        Sig.Outputs.Add(FNiagaraVariable(
            FNiagaraTypeDefinition::GetIntDef(), TEXT("Count")));
        OutFunctions.Add(Sig);
    }

    // ── GetSplatPosition ───────────────────────────────────────────
    // Returns world position (cm) for splat at Index.
    // Inputs:  (NDI self), int Index
    // Outputs: vec3 Position
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = Name_GetSplatPosition;
        Sig.bMemberFunction = true;
        Sig.bRequiresContext = false;
        Sig.Inputs.Add(NDISelf());
        Sig.Inputs.Add(FNiagaraVariable(
            FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
        Sig.Outputs.Add(FNiagaraVariable(
            FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
        OutFunctions.Add(Sig);
    }

    // ── GetSplatScale ──────────────────────────────────────────────
    // Returns per-axis scale (cm) for splat at Index.
    // Inputs:  (NDI self), int Index
    // Outputs: vec3 Scale
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = Name_GetSplatScale;
        Sig.bMemberFunction = true;
        Sig.bRequiresContext = false;
        Sig.Inputs.Add(NDISelf());
        Sig.Inputs.Add(FNiagaraVariable(
            FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
        Sig.Outputs.Add(FNiagaraVariable(
            FNiagaraTypeDefinition::GetVec3Def(), TEXT("Scale")));
        OutFunctions.Add(Sig);
    }

    // ── GetSplatOrientation ────────────────────────────────────────
    // Returns orientation quaternion for splat at Index.
    // Niagara doesn't have a native Quat type in scratch pads so we
    // expose it as four floats (X Y Z W) that the scratch pad reassembles.
    // Inputs:  (NDI self), int Index
    // Outputs: float QX, float QY, float QZ, float QW
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = Name_GetSplatOrientation;
        Sig.bMemberFunction = true;
        Sig.bRequiresContext = false;
        Sig.Inputs.Add(NDISelf());
        Sig.Inputs.Add(FNiagaraVariable(
            FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
        Sig.Outputs.Add(FNiagaraVariable(
            FNiagaraTypeDefinition::GetFloatDef(), TEXT("QX")));
        Sig.Outputs.Add(FNiagaraVariable(
            FNiagaraTypeDefinition::GetFloatDef(), TEXT("QY")));
        Sig.Outputs.Add(FNiagaraVariable(
            FNiagaraTypeDefinition::GetFloatDef(), TEXT("QZ")));
        Sig.Outputs.Add(FNiagaraVariable(
            FNiagaraTypeDefinition::GetFloatDef(), TEXT("QW")));
        OutFunctions.Add(Sig);
    }

    // ── GetSplatColor ──────────────────────────────────────────────
    // Returns linear RGB color from zero-order SH for splat at Index.
    // Inputs:  (NDI self), int Index
    // Outputs: vec3 Color
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = Name_GetSplatColor;
        Sig.bMemberFunction = true;
        Sig.bRequiresContext = false;
        Sig.Inputs.Add(NDISelf());
        Sig.Inputs.Add(FNiagaraVariable(
            FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
        Sig.Outputs.Add(FNiagaraVariable(
            FNiagaraTypeDefinition::GetVec3Def(), TEXT("Color")));
        OutFunctions.Add(Sig);
    }

    // ── GetSplatOpacity ────────────────────────────────────────────
    // Returns opacity [0,1] for splat at Index.
    // Inputs:  (NDI self), int Index
    // Outputs: float Opacity
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = Name_GetSplatOpacity;
        Sig.bMemberFunction = true;
        Sig.bRequiresContext = false;
        Sig.Inputs.Add(NDISelf());
        Sig.Inputs.Add(FNiagaraVariable(
            FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
        Sig.Outputs.Add(FNiagaraVariable(
            FNiagaraTypeDefinition::GetFloatDef(), TEXT("Opacity")));
        OutFunctions.Add(Sig);
    }
}

// ── GetVMExternalFunction ─────────────────────────────────────────────────────
// Binds function names → CPU implementations.
// The DEFINE_NDI_DIRECT_FUNC_BINDER macro generates a small trampoline
// struct that lets Niagara call our member functions through a stable
// function pointer without virtual dispatch overhead per-particle.

DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatCount);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatPosition);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatScale);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatOrientation);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatColor);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatOpacity);

void UNiagaraGSDataInterface::GetVMExternalFunction(
    const FVMExternalFunctionBindingInfo& BindingInfo,
    void* InstanceData,
    FVMExternalFunction& OutFunc)
{
    if (BindingInfo.Name == Name_GetSplatCount)
        NDI_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatCount)
        ::Bind(this, OutFunc);

    else if (BindingInfo.Name == Name_GetSplatPosition)
        NDI_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatPosition)
        ::Bind(this, OutFunc);

    else if (BindingInfo.Name == Name_GetSplatScale)
        NDI_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatScale)
        ::Bind(this, OutFunc);

    else if (BindingInfo.Name == Name_GetSplatOrientation)
        NDI_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatOrientation)
        ::Bind(this, OutFunc);

    else if (BindingInfo.Name == Name_GetSplatColor)
        NDI_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatColor)
        ::Bind(this, OutFunc);

    else if (BindingInfo.Name == Name_GetSplatOpacity)
        NDI_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatOpacity)
        ::Bind(this, OutFunc);
}

// ── CPU function implementations ──────────────────────────────────────────────
// These run inside the VectorVM when the emitter is CPU-based.
// The pattern is always:
//   1. Declare VectorVM::FUserPtrHandler for the NDI self-reference
//   2. Declare FNDIInputParam for each input (after the NDI self)
//   3. Declare FNDIOutputParam for each output
//   4. Loop over Context.GetNumInstances() — one iteration per particle
//
// GetAndAdvance() reads the current value and moves to the next particle.
// SetAndAdvance() writes the output and moves to the next particle.

void UNiagaraGSDataInterface::GetSplatCount(
    FVectorVMExternalFunctionContext& Context)
{
    VectorVM::FUserPtrHandler<UNiagaraGSDataInterface> InstData(Context);
    FNDIOutputParam<int32> OutCount(Context);

    const int32 Count = GetSplatCount();
    const int32 NumInstances = Context.GetNumInstances();

    for (int32 i = 0; i < NumInstances; ++i)
    {
        OutCount.SetAndAdvance(Count);
    }
}

void UNiagaraGSDataInterface::GetSplatPosition(
    FVectorVMExternalFunctionContext& Context)
{
    VectorVM::FUserPtrHandler<UNiagaraGSDataInterface> InstData(Context);
    FNDIInputParam<int32>  InIndex(Context);
    FNDIOutputParam<float> OutX(Context);
    FNDIOutputParam<float> OutY(Context);
    FNDIOutputParam<float> OutZ(Context);

    const TArray<FGaussianSplatData>* Splats =
        (SplatAsset ? &SplatAsset->SplatData : nullptr);

    const int32 NumInstances = Context.GetNumInstances();
    for (int32 i = 0; i < NumInstances; ++i)
    {
        const int32 Index = InIndex.GetAndAdvance();

        if (Splats && Splats->IsValidIndex(Index))
        {
            const FVector3f& Pos = (*Splats)[Index].Position;
            OutX.SetAndAdvance(Pos.X);
            OutY.SetAndAdvance(Pos.Y);
            OutZ.SetAndAdvance(Pos.Z);
        }
        else
        {
            OutX.SetAndAdvance(0.0f);
            OutY.SetAndAdvance(0.0f);
            OutZ.SetAndAdvance(0.0f);
        }
    }
}

void UNiagaraGSDataInterface::GetSplatScale(
    FVectorVMExternalFunctionContext& Context)
{
    VectorVM::FUserPtrHandler<UNiagaraGSDataInterface> InstData(Context);
    FNDIInputParam<int32>  InIndex(Context);
    FNDIOutputParam<float> OutX(Context);
    FNDIOutputParam<float> OutY(Context);
    FNDIOutputParam<float> OutZ(Context);

    const TArray<FGaussianSplatData>* Splats =
        (SplatAsset ? &SplatAsset->SplatData : nullptr);

    const int32 NumInstances = Context.GetNumInstances();
    for (int32 i = 0; i < NumInstances; ++i)
    {
        const int32 Index = InIndex.GetAndAdvance();

        if (Splats && Splats->IsValidIndex(Index))
        {
            const FVector3f& S = (*Splats)[Index].Scale;
            OutX.SetAndAdvance(S.X);
            OutY.SetAndAdvance(S.Y);
            OutZ.SetAndAdvance(S.Z);
        }
        else
        {
            OutX.SetAndAdvance(1.0f);
            OutY.SetAndAdvance(1.0f);
            OutZ.SetAndAdvance(1.0f);
        }
    }
}

void UNiagaraGSDataInterface::GetSplatOrientation(
    FVectorVMExternalFunctionContext& Context)
{
    VectorVM::FUserPtrHandler<UNiagaraGSDataInterface> InstData(Context);
    FNDIInputParam<int32>  InIndex(Context);
    FNDIOutputParam<float> OutQX(Context);
    FNDIOutputParam<float> OutQY(Context);
    FNDIOutputParam<float> OutQZ(Context);
    FNDIOutputParam<float> OutQW(Context);

    const TArray<FGaussianSplatData>* Splats =
        (SplatAsset ? &SplatAsset->SplatData : nullptr);

    const int32 NumInstances = Context.GetNumInstances();
    for (int32 i = 0; i < NumInstances; ++i)
    {
        const int32 Index = InIndex.GetAndAdvance();

        if (Splats && Splats->IsValidIndex(Index))
        {
            const FQuat4f& Q = (*Splats)[Index].Orientation;
            OutQX.SetAndAdvance(Q.X);
            OutQY.SetAndAdvance(Q.Y);
            OutQZ.SetAndAdvance(Q.Z);
            OutQW.SetAndAdvance(Q.W);
        }
        else
        {
            OutQX.SetAndAdvance(0.0f);
            OutQY.SetAndAdvance(0.0f);
            OutQZ.SetAndAdvance(0.0f);
            OutQW.SetAndAdvance(1.0f);
        }
    }
}

void UNiagaraGSDataInterface::GetSplatColor(
    FVectorVMExternalFunctionContext& Context)
{
    VectorVM::FUserPtrHandler<UNiagaraGSDataInterface> InstData(Context);
    FNDIInputParam<int32>  InIndex(Context);
    FNDIOutputParam<float> OutR(Context);
    FNDIOutputParam<float> OutG(Context);
    FNDIOutputParam<float> OutB(Context);

    const TArray<FGaussianSplatData>* Splats =
        (SplatAsset ? &SplatAsset->SplatData : nullptr);

    const int32 NumInstances = Context.GetNumInstances();
    for (int32 i = 0; i < NumInstances; ++i)
    {
        const int32 Index = InIndex.GetAndAdvance();

        if (Splats && Splats->IsValidIndex(Index))
        {
            const FVector3f& C = (*Splats)[Index].Color;
            OutR.SetAndAdvance(C.X);
            OutG.SetAndAdvance(C.Y);
            OutB.SetAndAdvance(C.Z);
        }
        else
        {
            OutR.SetAndAdvance(0.5f);
            OutG.SetAndAdvance(0.5f);
            OutB.SetAndAdvance(0.5f);
        }
    }
}

void UNiagaraGSDataInterface::GetSplatOpacity(
    FVectorVMExternalFunctionContext& Context)
{
    VectorVM::FUserPtrHandler<UNiagaraGSDataInterface> InstData(Context);
    FNDIInputParam<int32>  InIndex(Context);
    FNDIOutputParam<float> OutOpacity(Context);

    const TArray<FGaussianSplatData>* Splats =
        (SplatAsset ? &SplatAsset->SplatData : nullptr);

    const int32 NumInstances = Context.GetNumInstances();
    for (int32 i = 0; i < NumInstances; ++i)
    {
        const int32 Index = InIndex.GetAndAdvance();

        if (Splats && Splats->IsValidIndex(Index))
        {
            OutOpacity.SetAndAdvance((*Splats)[Index].Opacity);
        }
        else
        {
            OutOpacity.SetAndAdvance(0.0f);
        }
    }
}

/*void FNDIGaussianSplatProxy::UploadData(const TArray<FGaussianSplatData>& Splats)
{
    const int32 Count = Splats.Num();
    if (Count == 0)
    {
        ReleaseBuffers();
        return;
    }

    // 1. Pack CPU data
    TArray<FVector4f> PackedPositions;
    TArray<FVector4f> PackedScales;
    TArray<FVector4f> PackedRotations;
    TArray<FVector4f> PackedColorOpacity;

    PackedPositions.SetNumUninitialized(Count);
    PackedScales.SetNumUninitialized(Count);
    PackedRotations.SetNumUninitialized(Count);
    PackedColorOpacity.SetNumUninitialized(Count);

    for (int32 i = 0; i < Count; ++i)
    {
        const FGaussianSplatData& S = Splats[i];
        PackedPositions[i] = FVector4f(S.Position.X, S.Position.Y, S.Position.Z, 0.0f);
        PackedScales[i] = FVector4f(S.Scale.X, S.Scale.Y, S.Scale.Z, 0.0f);
        PackedRotations[i] = FVector4f(S.Orientation.X, S.Orientation.Y, S.Orientation.Z, S.Orientation.W);
        PackedColorOpacity[i] = FVector4f(S.Color.X, S.Color.Y, S.Color.Z, S.Opacity);
    }

    // 2. Safely defer buffer creation and allocation to the Render Thread
    struct FBufferInitData
    {
        TArray<FVector4f> Data;
        FNiagaraGSSplatBuffer* TargetBuffer;
        const TCHAR* DebugName;
    };

    // Construct an array of references to fill asynchronously 
    TSharedPtr<TArray<FBufferInitData>, ESPMode::ThreadSafe> InitArray = MakeShared<TArray<FBufferInitData>, ESPMode::ThreadSafe>();
    InitArray->Add({ MoveTemp(PackedPositions), &PositionsBuffer, TEXT("GS_Positions") });
    InitArray->Add({ MoveTemp(PackedScales), &ScalesBuffer, TEXT("GS_Scales") });
    InitArray->Add({ MoveTemp(PackedRotations), &RotationsBuffer, TEXT("GS_Rotations") });
    InitArray->Add({ MoveTemp(PackedColorOpacity), &ColorOpacityBuffer, TEXT("GS_ColorOpacity") });

    ReleaseBuffers();

    // Enqueue the work to run on the Render Thread
    ENQUEUE_RENDER_COMMAND(FNiagaraGSUploadBufferData)(
        [InitArray](FRHICommandListImmediate& RHICmdList) // UE provides RHICmdList automatically here
        {
            for (const FBufferInitData& InitData : *InitArray)
            {
                if (InitData.Data.Num() == 0) continue;

                const int32 BufferSize = InitData.Data.Num() * sizeof(FVector4f);

                // 1. Descriptor setup (UE 5.6 chained syntax)
                FRHIBufferCreateDesc CreateInfo = FRHIBufferCreateDesc::Create(InitData.DebugName, EBufferUsageFlags::Static | EBufferUsageFlags::ShaderResource)
                    .SetSize(BufferSize)
                    .SetStride(sizeof(FVector4f))
                    .SetInitialState(ERHIAccess::Unknown);

                // 2. Create the buffer using the render thread's command list
                InitData.TargetBuffer->Buffer = RHICmdList.CreateBuffer(CreateInfo);

                // 3. Lock, write, and unlock
                void* Dest = RHICmdList.LockBuffer(InitData.TargetBuffer->Buffer, 0, BufferSize, RLM_WriteOnly);
                FMemory::Memcpy(Dest, InitData.Data.GetData(), BufferSize);
                RHICmdList.UnlockBuffer(InitData.TargetBuffer->Buffer);

                // 4. Generate the Shader Resource View
                // 4. Generate the Shader Resource View directly without a descriptor
               // 4. Generate the Shader Resource View via explicit initializer
                FShaderResourceViewInitializer ViewInitializer(InitData.TargetBuffer->Buffer, PF_A32B32G32R32F);
                InitData.TargetBuffer->SRV = RHICmdList.CreateShaderResourceView(ViewInitializer);


            }
        });

    SplatCount = Count;
    bBuffersReady = true;

    const int32 TotalBytes = Count * sizeof(FVector4f) * 4;
    UE_LOG(LogTemp, Log, TEXT("NiagaraGS: Enqueued %d splats for GPU upload (%d MB)"), Count, TotalBytes / (1024 * 1024));
}
*/
void FNDIGaussianSplatProxy::UploadData(const TArray<FGaussianSplatData>& Splats)
{
    // Must be called on the render thread (via ENQUEUE_RENDER_COMMAND in UploadToGPU)
    check(IsInRenderingThread());

    const int32 Count = Splats.Num();
    ReleaseBuffers();  // safe — we're on the render thread, no race

    if (Count == 0) return;

    // Pack CPU data
    TArray<FVector4f> PackedPositions, PackedScales, PackedRotations, PackedColorOpacity;
    PackedPositions.SetNumUninitialized(Count);
    PackedScales.SetNumUninitialized(Count);
    PackedRotations.SetNumUninitialized(Count);
    PackedColorOpacity.SetNumUninitialized(Count);

    for (int32 i = 0; i < Count; ++i)
    {
        const FGaussianSplatData& S = Splats[i];
        PackedPositions[i] = FVector4f(S.Position.X, S.Position.Y, S.Position.Z, 0.f);
        PackedScales[i] = FVector4f(S.Scale.X, S.Scale.Y, S.Scale.Z, 0.f);
        PackedRotations[i] = FVector4f(S.Orientation.X, S.Orientation.Y, S.Orientation.Z, S.Orientation.W);
        PackedColorOpacity[i] = FVector4f(S.Color.X, S.Color.Y, S.Color.Z, S.Opacity);
    }

    // Helper — create one buffer+SRV directly on the render thread
    // No nested ENQUEUE needed; we ARE on the render thread already.
    auto CreateBuffer = [](
        FRHICommandListImmediate& RHICmdList,
        FNiagaraGSSplatBuffer& OutBuffer,
        const TArray<FVector4f>& Data,
        const TCHAR* DebugName)
        {
            const int32 BufferSize = Data.Num() * sizeof(FVector4f);

            FRHIBufferCreateDesc Desc =
                FRHIBufferCreateDesc::Create(DebugName,
                    EBufferUsageFlags::Static | EBufferUsageFlags::ShaderResource)
                .SetSize(BufferSize)
                .SetStride(sizeof(FVector4f))
                .SetInitialState(ERHIAccess::SRVCompute);

            OutBuffer.Buffer = RHICmdList.CreateBuffer(Desc);

            void* Dest = RHICmdList.LockBuffer(OutBuffer.Buffer, 0, BufferSize, RLM_WriteOnly);
            FMemory::Memcpy(Dest, Data.GetData(), BufferSize);
            RHICmdList.UnlockBuffer(OutBuffer.Buffer);

            FShaderResourceViewInitializer SRVInit(OutBuffer.Buffer, PF_A32B32G32R32F);
            OutBuffer.SRV = RHICmdList.CreateShaderResourceView(SRVInit);
        };

    // We need an RHICmdList — get the immediate one since we're on the render thread
    FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

    CreateBuffer(RHICmdList, PositionsBuffer, PackedPositions, TEXT("GS_Positions"));
    CreateBuffer(RHICmdList, ScalesBuffer, PackedScales, TEXT("GS_Scales"));
    CreateBuffer(RHICmdList, RotationsBuffer, PackedRotations, TEXT("GS_Rotations"));
    CreateBuffer(RHICmdList, ColorOpacityBuffer, PackedColorOpacity, TEXT("GS_ColorOpacity"));

    SplatCount = Count;
    bBuffersReady = true;  // ← set AFTER buffers are actually created

    UE_LOG(LogTemp, Log, TEXT("NiagaraGS: Uploaded %d splats (%d MB)"),
        Count, (Count * (int32)sizeof(FVector4f) * 4) / (1024 * 1024));
}

// ─────────────────────────────────────────────────────────────────────────────
//  NDI GPU interface implementations
// ─────────────────────────────────────────────────────────────────────────────

void UNiagaraGSDataInterface::PostInitProperties()
{
    Super::PostInitProperties();

    if (HasAnyFlags(RF_ClassDefaultObject))
    {
        ENiagaraTypeRegistryFlags DIFlags =
            ENiagaraTypeRegistryFlags::AllowAnyVariable |
            ENiagaraTypeRegistryFlags::AllowParameter;

        FNiagaraTypeRegistry::Register(
            FNiagaraTypeDefinition(GetClass()), DIFlags);
    }

    Proxy = MakeUnique<FNDIGaussianSplatProxy>();
}

void UNiagaraGSDataInterface::UploadToGPU()
{
    if (!SplatAsset || SplatAsset->SplatData.Num() == 0) return;

    FNDIGaussianSplatProxy* ProxyPtr = GetProxyAs<FNDIGaussianSplatProxy>();
    if (!ProxyPtr) return;

    TArray<FGaussianSplatData> SplatsCopy = SplatAsset->SplatData;

    ENQUEUE_RENDER_COMMAND(NiagaraGS_Upload)(
        [ProxyPtr, SplatsCopy = MoveTemp(SplatsCopy)]
        (FRHICommandListImmediate& RHICmdList) mutable
        {
            // UploadData now uses GetImmediateCommandList() internally,
            // or you can pass RHICmdList through — either works since
            // we're already on the render thread here.
            ProxyPtr->UploadData(SplatsCopy);
        });
}

void UNiagaraGSDataInterface::PostLoad()
{
    Super::PostLoad();

    // Asset is fully loaded by PostLoad time, so we can safely upload.
    // This covers the case where a saved Niagara system is reopened
    // with an already-assigned SplatAsset.
    UploadToGPU();
}

#if WITH_EDITOR
void UNiagaraGSDataInterface::PostEditChangeProperty(
    FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    // Re-upload whenever the asset reference changes in the Details panel
    if (PropertyChangedEvent.Property &&
        PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(
            UNiagaraGSDataInterface, SplatAsset))
    {
        UploadToGPU();
    }
}
#endif

// ── GetParameterDefinitionHLSL ────────────────────────────────────────────────
// Niagara calls this once when compiling the emitter shader.
// We append an #include of our .ush file — that file contains all
// buffer declarations and function bodies with {DataInterfaceHLSLSymbol}
// as a placeholder that Niagara replaces with a unique prefix.

void UNiagaraGSDataInterface::GetParameterDefinitionHLSL(
    const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,
    FString& OutHLSL)
{
    // The virtual path "/NiagaraGS/NiagaraGSDataInterface.ush" maps to
    // Plugins/NiagaraGS/Shaders/NiagaraGSDataInterface.ush via the
    // directory mapping we registered in StartupModule (Step 1).
    OutHLSL += TEXT("#include \"/NiagaraGS/NiagaraGSDataInterface.ush\"\n");
}

// ── GetFunctionHLSL ───────────────────────────────────────────────────────────
// Called for each function in GetFunctions().
// We emit a call-forwarding stub that delegates to the function defined
// in our .ush file, substituting the correct HLSL symbol prefix.
// The stub signature must exactly match what GetFunctions() declared.

bool UNiagaraGSDataInterface::GetFunctionHLSL(
    const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,
    const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo,
    int                                           FunctionInstanceIndex,
    FString& OutHLSL)
{
    const FString& Sym = ParamInfo.DataInterfaceHLSLSymbol;

    // ── GetSplatCount ──────────────────────────────────────────────
    if (FunctionInfo.DefinitionName == Name_GetSplatCount)
    {
        OutHLSL += FString::Printf(TEXT(
            "void %s(out int OutCount)\n"
            "{\n"
            "    %s_GetSplatCount(OutCount);\n"
            "}\n"),
            *FunctionInfo.InstanceName, *Sym);
        return true;
    }

    // ── GetSplatPosition ───────────────────────────────────────────
    if (FunctionInfo.DefinitionName == Name_GetSplatPosition)
    {
        OutHLSL += FString::Printf(TEXT(
            "void %s(int Index, out float3 OutPosition)\n"
            "{\n"
            "    %s_GetSplatPosition(Index, OutPosition);\n"
            "}\n"),
            *FunctionInfo.InstanceName, *Sym);
        return true;
    }

    // ── GetSplatScale ──────────────────────────────────────────────
    if (FunctionInfo.DefinitionName == Name_GetSplatScale)
    {
        OutHLSL += FString::Printf(TEXT(
            "void %s(int Index, out float3 OutScale)\n"
            "{\n"
            "    %s_GetSplatScale(Index, OutScale);\n"
            "}\n"),
            *FunctionInfo.InstanceName, *Sym);
        return true;
    }

    // ── GetSplatOrientation ────────────────────────────────────────
    if (FunctionInfo.DefinitionName == Name_GetSplatOrientation)
    {
        OutHLSL += FString::Printf(TEXT(
            "void %s(int Index,"
            " out float OutQX, out float OutQY,"
            " out float OutQZ, out float OutQW)\n"
            "{\n"
            "    %s_GetSplatOrientation(Index,"
            " OutQX, OutQY, OutQZ, OutQW);\n"
            "}\n"),
            *FunctionInfo.InstanceName, *Sym);
        return true;
    }

    // ── GetSplatColor ──────────────────────────────────────────────
    if (FunctionInfo.DefinitionName == Name_GetSplatColor)
    {
        OutHLSL += FString::Printf(TEXT(
            "void %s(int Index, out float3 OutColor)\n"
            "{\n"
            "    %s_GetSplatColor(Index, OutColor);\n"
            "}\n"),
            *FunctionInfo.InstanceName, *Sym);
        return true;
    }

    // ── GetSplatOpacity ────────────────────────────────────────────
    if (FunctionInfo.DefinitionName == Name_GetSplatOpacity)
    {
        OutHLSL += FString::Printf(TEXT(
            "void %s(int Index, out float OutOpacity)\n"
            "{\n"
            "    %s_GetSplatOpacity(Index, OutOpacity);\n"
            "}\n"),
            *FunctionInfo.InstanceName, *Sym);
        return true;
    }

    return false;
}

// ── BuildShaderParameters ─────────────────────────────────────────────────────
// Tells Niagara the memory layout of our parameter struct so it can
// allocate the right amount of space in the shader parameter buffer.

void UNiagaraGSDataInterface::BuildShaderParameters(
    FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
    ShaderParametersBuilder.AddNestedStruct<FNiagaraGSShaderParameters>();
}

// ── SetShaderParameters ───────────────────────────────────────────────────────
// Called every frame on the render thread before Niagara dispatches
// the GPU simulation compute shader.
// We bind our GPU buffers into the shader parameter struct slots.

void UNiagaraGSDataInterface::SetShaderParameters(
    const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
    // Fix: Renamed local reference from Proxy to SplatProxy to avoid hiding the class member
    FNDIGaussianSplatProxy& SplatProxy =
        Context.GetProxy<FNDIGaussianSplatProxy>();

    FNiagaraGSShaderParameters* Params =
        Context.GetParameterNestedStruct<FNiagaraGSShaderParameters>();

    if (!Params) return;

    if (SplatProxy.bBuffersReady)
    {
        Params->SplatCount = SplatProxy.SplatCount;
        Params->Positions = SplatProxy.PositionsBuffer.SRV;
        Params->Scales = SplatProxy.ScalesBuffer.SRV;
        Params->Rotations = SplatProxy.RotationsBuffer.SRV;
        Params->ColorOpacity = SplatProxy.ColorOpacityBuffer.SRV;
    }
    else
    {
        // Buffers not ready yet — bind zero count so the shader
        // safely skips all reads rather than reading garbage memory
        Params->SplatCount = 0;
        Params->Positions = nullptr;
        Params->Scales = nullptr;
        Params->Rotations = nullptr;
        Params->ColorOpacity = nullptr;
    }
}
