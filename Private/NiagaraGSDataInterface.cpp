#include "NiagaraGSDataInterface.h"
#include "NiagaraTypes.h"
#include "NiagaraCustomVersion.h"
#include "NiagaraShaderParametersBuilder.h"
#include "VectorVM.h"

// ── Static name definitions ───────────────────────────────────────────────────
// Defining them here means they're initialised exactly once and shared across
// all translation units that include the header.

const FName UNiagaraGSDataInterface::Name_GetSplatCount(TEXT("GetSplatCount"));
const FName UNiagaraGSDataInterface::Name_GetSplatPosition(TEXT("GetSplatPosition"));
const FName UNiagaraGSDataInterface::Name_GetSplatScale(TEXT("GetSplatScale"));
const FName UNiagaraGSDataInterface::Name_GetSplatOrientation(TEXT("GetSplatOrientation"));
const FName UNiagaraGSDataInterface::Name_GetSplatColor(TEXT("GetSplatColor"));
const FName UNiagaraGSDataInterface::Name_GetSplatOpacity(TEXT("GetSplatOpacity"));

// ── PostInitProperties ────────────────────────────────────────────────────────

void UNiagaraGSDataInterface::PostInitProperties()
{
    Super::PostInitProperties();

    // Register our NDI type with Niagara's type registry.
    // This must happen here (not in module startup) because the registry
    // is guaranteed to exist by the time PostInitProperties fires on the CDO.
    if (HasAnyFlags(RF_ClassDefaultObject))
    {
        ENiagaraTypeRegistryFlags DIFlags =
            ENiagaraTypeRegistryFlags::AllowAnyVariable |
            ENiagaraTypeRegistryFlags::AllowParameter;

        FNiagaraTypeRegistry::Register(
            FNiagaraTypeDefinition(GetClass()), DIFlags);
    }
}

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