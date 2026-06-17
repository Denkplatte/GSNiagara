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

const FName UNiagaraGSDataInterface::Name_GetSplatCount(TEXT("GetSplatCount"));
const FName UNiagaraGSDataInterface::Name_GetSplatPosition(TEXT("GetSplatPosition"));
const FName UNiagaraGSDataInterface::Name_GetSplatScale(TEXT("GetSplatScale"));
const FName UNiagaraGSDataInterface::Name_GetSplatOrientation(TEXT("GetSplatOrientation"));
const FName UNiagaraGSDataInterface::Name_GetSplatColor(TEXT("GetSplatColor"));
const FName UNiagaraGSDataInterface::Name_GetSplatOpacity(TEXT("GetSplatOpacity"));


// ── CopyToInternal / Equals ───────────────────────────────────────────────────

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

void UNiagaraGSDataInterface::GetFunctions(
    TArray<FNiagaraFunctionSignature>& OutFunctions)
{
    auto NDISelf = [this]() -> FNiagaraVariable
        {
            return FNiagaraVariable(
                FNiagaraTypeDefinition(GetClass()), TEXT("GaussianSplatDI"));
        };

    // ── GetSplatCount ──────────────────────────────────────────────
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
        NDI_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatCount)::Bind(this, OutFunc);

    else if (BindingInfo.Name == Name_GetSplatPosition)
        NDI_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatPosition)::Bind(this, OutFunc);

    else if (BindingInfo.Name == Name_GetSplatScale)
        NDI_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatScale)::Bind(this, OutFunc);

    else if (BindingInfo.Name == Name_GetSplatOrientation)
        NDI_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatOrientation)::Bind(this, OutFunc);

    else if (BindingInfo.Name == Name_GetSplatColor)
        NDI_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatColor)::Bind(this, OutFunc);

    else if (BindingInfo.Name == Name_GetSplatOpacity)
        NDI_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatOpacity)::Bind(this, OutFunc);
}

// ── CPU function implementations ──────────────────────────────────────────────

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

// ── FNDIGaussianSplatProxy::UploadData_RenderThread ───────────────────────────
//
// KEY FIX: This function is now called DIRECTLY inside an
// ENQUEUE_RENDER_COMMAND lambda (not nested inside another render command).
// All RHI work — buffer creation, lock/write/unlock, SRV creation — happens
// synchronously in the same render command, so bBuffersReady is set AFTER
// the buffers truly exist.
//
// The old code had two bugs:
//   1. UploadToGPU() enqueued a render command that called UploadData(),
//      which itself enqueued ANOTHER render command for the actual RHI work.
//      The inner command ran one frame later, so bBuffersReady was true but
//      the buffers were still null.
//   2. ReleaseBuffers() and bBuffersReady = true were called on the game
//      thread, racing with the render thread.

void FNDIGaussianSplatProxy::UploadData_RenderThread(
    FRHICommandListImmediate& RHICmdList,
    const TArray<FGaussianSplatData>& Splats)
{
    // Must be on the render thread
    check(IsInRenderingThread());

    // Release any previously allocated buffers (we're already on RT)
    ReleaseBuffers_RenderThread();

    const int32 Count = Splats.Num();
    if (Count == 0)
    {
        return;
    }

    // ── 1. Pack CPU arrays ────────────────────────────────────────
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

    // ── 2. Upload each buffer synchronously ───────────────────────
    struct FUploadEntry
    {
        const TArray<FVector4f>* Data;
        FNiagaraGSSplatBuffer* Target;
        const TCHAR* DebugName;
    };

    const FUploadEntry Entries[] =
    {
        { &PackedPositions,    &PositionsBuffer,    TEXT("GS_Positions")    },
        { &PackedScales,       &ScalesBuffer,       TEXT("GS_Scales")       },
        { &PackedRotations,    &RotationsBuffer,    TEXT("GS_Rotations")    },
        { &PackedColorOpacity, &ColorOpacityBuffer, TEXT("GS_ColorOpacity") },
    };

    for (const FUploadEntry& Entry : Entries)
    {
        const int32 BufferSize = Entry.Data->Num() * sizeof(FVector4f);

        // ERHIAccess::Unknown is explicitly banned as an initial state by the
        // RHI transition validator (RHICoreTransitions.h:32).
        // We declare SRVCompute because:
        //   • the buffer is write-once / static  (no UAV needed)
        //   • Niagara's GPU sim reads it from a compute shader
        //   • LockBuffer/UnlockBuffer internally transitions through
        //     CopyDest and back to this declared state before returning
        FRHIBufferCreateDesc CreateInfo =
            FRHIBufferCreateDesc::Create(
                Entry.DebugName,
                EBufferUsageFlags::Static | EBufferUsageFlags::ShaderResource)
            .SetSize(BufferSize)
            .SetStride(sizeof(FVector4f))
            .SetInitialState(ERHIAccess::SRVCompute);

        Entry.Target->Buffer = RHICmdList.CreateBuffer(CreateInfo);

        void* Dest = RHICmdList.LockBuffer(Entry.Target->Buffer, 0, BufferSize, RLM_WriteOnly);
        FMemory::Memcpy(Dest, Entry.Data->GetData(), BufferSize);
        RHICmdList.UnlockBuffer(Entry.Target->Buffer);

        FShaderResourceViewInitializer ViewInit(Entry.Target->Buffer, PF_A32B32G32R32F);
        Entry.Target->SRV = RHICmdList.CreateShaderResourceView(ViewInit);
    }

    // ── 3. Mark ready — safe because we're on the render thread ──
    SplatCount = Count;
    bBuffersReady = true;

    const int32 TotalMB = (Count * sizeof(FVector4f) * 4) / (1024 * 1024);
    UE_LOG(LogTemp, Log,
        TEXT("NiagaraGS: Uploaded %d splats to GPU (%d MB)"), Count, TotalMB);
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

// ── UploadToGPU ───────────────────────────────────────────────────────────────
//
// KEY FIX: We enqueue exactly ONE render command. Inside that command we call
// UploadData_RenderThread() directly — no further enqueuing.  This means:
//   • Buffer creation happens in the same command as the upload
//   • bBuffersReady is set after buffers exist, not before
//   • No game-thread / render-thread race on bBuffersReady

void UNiagaraGSDataInterface::UploadToGPU()
{
    if (!SplatAsset || SplatAsset->SplatData.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("NiagaraGS: UploadToGPU called with no splat data"));
        return;
    }

    // Copy on the game thread — UObjects are not render-thread safe
    TArray<FGaussianSplatData> SplatsCopy = SplatAsset->SplatData;
    FNDIGaussianSplatProxy* ProxyPtr = GetProxyAs<FNDIGaussianSplatProxy>();

    UE_LOG(LogTemp, Log,
        TEXT("NiagaraGS: Enqueueing GPU upload for %d splats"), SplatsCopy.Num());

    ENQUEUE_RENDER_COMMAND(NiagaraGS_Upload)(
        [ProxyPtr, SplatsCopy = MoveTemp(SplatsCopy)]
        (FRHICommandListImmediate& RHICmdList) mutable
        {
            // UploadData_RenderThread does ALL RHI work synchronously here.
            // No further enqueuing — buffers are ready when this lambda returns.
            ProxyPtr->UploadData_RenderThread(RHICmdList, SplatsCopy);
        });
}

void UNiagaraGSDataInterface::PostLoad()
{
    Super::PostLoad();
    UploadToGPU();
}

#if WITH_EDITOR
void UNiagaraGSDataInterface::PostEditChangeProperty(
    FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (PropertyChangedEvent.Property &&
        PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(
            UNiagaraGSDataInterface, SplatAsset))
    {
        UploadToGPU();
    }
}
#endif

// ── GetParameterDefinitionHLSL ────────────────────────────────────────────────

void UNiagaraGSDataInterface::GetParameterDefinitionHLSL(
    const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,
    FString& OutHLSL)
{
    OutHLSL += TEXT("#include \"/NiagaraGS/NiagaraGSDataInterface.ush\"\n");
}

// ── GetFunctionHLSL ───────────────────────────────────────────────────────────

bool UNiagaraGSDataInterface::GetFunctionHLSL(
    const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,
    const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo,
    int                                           FunctionInstanceIndex,
    FString& OutHLSL)
{
    const FString& Sym = ParamInfo.DataInterfaceHLSLSymbol;

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

void UNiagaraGSDataInterface::BuildShaderParameters(
    FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
    ShaderParametersBuilder.AddNestedStruct<FNiagaraGSShaderParameters>();
}

// ── SetShaderParameters ───────────────────────────────────────────────────────

void UNiagaraGSDataInterface::SetShaderParameters(
    const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
    FNDIGaussianSplatProxy& SplatProxy =
        Context.GetProxy<FNDIGaussianSplatProxy>();

    FNiagaraGSShaderParameters* Params =
        Context.GetParameterNestedStruct<FNiagaraGSShaderParameters>();

    if (!Params) return;

    if (SplatProxy.bBuffersReady
        && SplatProxy.PositionsBuffer.IsValid()
        && SplatProxy.ScalesBuffer.IsValid()
        && SplatProxy.RotationsBuffer.IsValid()
        && SplatProxy.ColorOpacityBuffer.IsValid())
    {
        Params->SplatCount = SplatProxy.SplatCount;
        Params->Positions = SplatProxy.PositionsBuffer.SRV;
        Params->Scales = SplatProxy.ScalesBuffer.SRV;
        Params->Rotations = SplatProxy.RotationsBuffer.SRV;
        Params->ColorOpacity = SplatProxy.ColorOpacityBuffer.SRV;
    }
    else
    {
        Params->SplatCount = 0;
        Params->Positions = nullptr;
        Params->Scales = nullptr;
        Params->Rotations = nullptr;
        Params->ColorOpacity = nullptr;
    }
}