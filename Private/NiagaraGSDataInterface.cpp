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

const FName UNiagaraGSDataInterface::Name_GetSplatCount(TEXT("GetSplatCount"));
const FName UNiagaraGSDataInterface::Name_GetSplatPosition(TEXT("GetSplatPosition"));
const FName UNiagaraGSDataInterface::Name_GetSplatScale(TEXT("GetSplatScale"));
const FName UNiagaraGSDataInterface::Name_GetSplatOrientation(TEXT("GetSplatOrientation"));
const FName UNiagaraGSDataInterface::Name_GetSplatColor(TEXT("GetSplatColor"));
const FName UNiagaraGSDataInterface::Name_GetSplatOpacity(TEXT("GetSplatOpacity"));

bool UNiagaraGSDataInterface::CopyToInternal(UNiagaraDataInterface* Destination) const
{
    if (!Super::CopyToInternal(Destination)) return false;
    UNiagaraGSDataInterface* Dest = CastChecked<UNiagaraGSDataInterface>(Destination);
    Dest->SplatAsset = SplatAsset;

    UE_LOG(LogTemp, Log, TEXT("NiagaraGS: CopyToInternal called. Source DI: 0x%p, Dest DI: 0x%p, Asset: %s"),
        this, Dest, SplatAsset ? *SplatAsset->GetName() : TEXT("None"));

    Dest->UploadToGPU();
    return true;
}

bool UNiagaraGSDataInterface::Equals(const UNiagaraDataInterface* Other) const
{
    if (!Super::Equals(Other)) return false;
    const UNiagaraGSDataInterface* OtherDI = CastChecked<const UNiagaraGSDataInterface>(Other);
    return OtherDI->SplatAsset == SplatAsset;
}

int32 UNiagaraGSDataInterface::GetSplatCount() const
{
    return (SplatAsset && SplatAsset->SplatData.Num() > 0) ? SplatAsset->SplatData.Num() : 0;
}

void UNiagaraGSDataInterface::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
    auto NDISelf = [this]() -> FNiagaraVariable
        {
            return FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("GaussianSplatDI"));
        };

    // Splat Count
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = Name_GetSplatCount;
        Sig.bMemberFunction = true;
        Sig.bRequiresContext = false; // Fixed: Needs to be false to avoid Unreal GPU compiler profile errors
        Sig.bSupportsCPU = true;
        Sig.bSupportsGPU = true;
        Sig.Inputs.Add(NDISelf());
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Count")));
        OutFunctions.Add(Sig);
    }

    // Position
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = Name_GetSplatPosition;
        Sig.bMemberFunction = true;
        Sig.bRequiresContext = false;
        Sig.bSupportsCPU = true;
        Sig.bSupportsGPU = true;
        Sig.Inputs.Add(NDISelf());
        Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
        OutFunctions.Add(Sig);
    }

    // Scale
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = Name_GetSplatScale;
        Sig.bMemberFunction = true;
        Sig.bRequiresContext = false;
        Sig.bSupportsCPU = true;
        Sig.bSupportsGPU = true;
        Sig.Inputs.Add(NDISelf());
        Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Scale")));
        OutFunctions.Add(Sig);
    }

    // Orientation (Emitted as separate floats to bypass Scratchpad Quat representation limits)
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = Name_GetSplatOrientation;
        Sig.bMemberFunction = true;
        Sig.bRequiresContext = false;
        Sig.bSupportsCPU = true;
        Sig.bSupportsGPU = true;
        Sig.Inputs.Add(NDISelf());
        Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("QX")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("QY")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("QZ")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("QW")));
        OutFunctions.Add(Sig);
    }

    // Color
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = Name_GetSplatColor;
        Sig.bMemberFunction = true;
        Sig.bRequiresContext = false;
        Sig.bSupportsCPU = true;
        Sig.bSupportsGPU = true;
        Sig.Inputs.Add(NDISelf());
        Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), TEXT("Color")));
        OutFunctions.Add(Sig);
    }

    // Opacity
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = Name_GetSplatOpacity;
        Sig.bMemberFunction = true;
        Sig.bRequiresContext = false;
        Sig.bSupportsCPU = true;
        Sig.bSupportsGPU = true;
        Sig.Inputs.Add(NDISelf());
        Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Opacity")));
        OutFunctions.Add(Sig);
    }
}

DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatCount);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatPosition);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatScale);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatOrientation);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatColor);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatOpacity);

void UNiagaraGSDataInterface::GetVMExternalFunction(
    const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc)
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

void UNiagaraGSDataInterface::GetSplatCount(FVectorVMExternalFunctionContext& Context)
{
    FNDIOutputParam<int32> OutCount(Context);
    const int32 Count = GetSplatCount();
    for (int32 i = 0; i < Context.GetNumInstances(); ++i) { OutCount.SetAndAdvance(Count); }
}

void UNiagaraGSDataInterface::GetSplatPosition(FVectorVMExternalFunctionContext& Context)
{
    FNDIInputParam<int32> InIndex(Context);
    FNDIOutputParam<float> OutX(Context);
    FNDIOutputParam<float> OutY(Context);
    FNDIOutputParam<float> OutZ(Context);

    const TArray<FGaussianSplatData>* Splats = (SplatAsset ? &SplatAsset->SplatData : nullptr);
    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
        const int32 Index = InIndex.GetAndAdvance();
        if (Splats && Splats->IsValidIndex(Index))
        {
            const FVector3f& Pos = (*Splats)[Index].Position;
            OutX.SetAndAdvance(Pos.X);
            OutY.SetAndAdvance(Pos.Y);
            OutZ.SetAndAdvance(Pos.Z);
        }
        else { OutX.SetAndAdvance(0.f); OutY.SetAndAdvance(0.f); OutZ.SetAndAdvance(0.f); }
    }
}

void UNiagaraGSDataInterface::GetSplatScale(FVectorVMExternalFunctionContext& Context)
{
    FNDIInputParam<int32> InIndex(Context);
    FNDIOutputParam<float> OutX(Context);
    FNDIOutputParam<float> OutY(Context);
    FNDIOutputParam<float> OutZ(Context);

    const TArray<FGaussianSplatData>* Splats = (SplatAsset ? &SplatAsset->SplatData : nullptr);
    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
        const int32 Index = InIndex.GetAndAdvance();
        if (Splats && Splats->IsValidIndex(Index))
        {
            const FVector3f& S = (*Splats)[Index].Scale;
            OutX.SetAndAdvance(S.X); OutY.SetAndAdvance(S.Y); OutZ.SetAndAdvance(S.Z);
        }
        else { OutX.SetAndAdvance(1.f); OutY.SetAndAdvance(1.f); OutZ.SetAndAdvance(1.f); }
    }
}

void UNiagaraGSDataInterface::GetSplatOrientation(FVectorVMExternalFunctionContext& Context)
{
    FNDIInputParam<int32> InIndex(Context);
    FNDIOutputParam<float> OutQX(Context); FNDIOutputParam<float> OutQY(Context);
    FNDIOutputParam<float> OutQZ(Context); FNDIOutputParam<float> OutQW(Context);

    const TArray<FGaussianSplatData>* Splats = (SplatAsset ? &SplatAsset->SplatData : nullptr);
    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
        const int32 Index = InIndex.GetAndAdvance();
        if (Splats && Splats->IsValidIndex(Index))
        {
            const FQuat4f& Q = (*Splats)[Index].Orientation;
            OutQX.SetAndAdvance(Q.X); OutQY.SetAndAdvance(Q.Y); OutQZ.SetAndAdvance(Q.Z); OutQW.SetAndAdvance(Q.W);
        }
        else { OutQX.SetAndAdvance(0.f); OutQY.SetAndAdvance(0.f); OutQZ.SetAndAdvance(0.f); OutQW.SetAndAdvance(1.f); }
    }
}

void UNiagaraGSDataInterface::GetSplatColor(FVectorVMExternalFunctionContext& Context)
{
    FNDIInputParam<int32> InIndex(Context);
    FNDIOutputParam<float> OutR(Context);
    FNDIOutputParam<float> OutG(Context);
    FNDIOutputParam<float> OutB(Context);
    FNDIOutputParam<float> OutA(Context);

    const TArray<FGaussianSplatData>* Splats = (SplatAsset ? &SplatAsset->SplatData : nullptr);
    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
        const int32 Index = InIndex.GetAndAdvance();
        if (Splats && Splats->IsValidIndex(Index))
        {
            const FVector3f& C = (*Splats)[Index].Color;
            OutR.SetAndAdvance(C.X);
            OutG.SetAndAdvance(C.Y);
            OutB.SetAndAdvance(C.Z);
            OutA.SetAndAdvance((*Splats)[Index].Opacity);
        }
        else
        {
            OutR.SetAndAdvance(0.5f);
            OutG.SetAndAdvance(0.5f);
            OutB.SetAndAdvance(0.5f);
            OutA.SetAndAdvance(1.0f);
        }
    }
}

void UNiagaraGSDataInterface::GetSplatOpacity(FVectorVMExternalFunctionContext& Context)
{
    FNDIInputParam<int32> InIndex(Context);
    FNDIOutputParam<float> OutOpacity(Context);

    const TArray<FGaussianSplatData>* Splats = (SplatAsset ? &SplatAsset->SplatData : nullptr);
    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
        const int32 Index = InIndex.GetAndAdvance();
        if (Splats && Splats->IsValidIndex(Index)) { OutOpacity.SetAndAdvance((*Splats)[Index].Opacity); }
        else { OutOpacity.SetAndAdvance(0.0f); }
    }
}

void UNiagaraGSDataInterface::PostInitProperties()
{
    Super::PostInitProperties();
    if (HasAnyFlags(RF_ClassDefaultObject))
    {
        ENiagaraTypeRegistryFlags DIFlags = ENiagaraTypeRegistryFlags::AllowAnyVariable | ENiagaraTypeRegistryFlags::AllowParameter;
        FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), DIFlags);
    }
    if (!HasAnyFlags(RF_ClassDefaultObject))
    {
        Proxy = MakeUnique<FNDIGaussianSplatProxy>();
    }
}

void UNiagaraGSDataInterface::UploadToGPU()
{
    if (!SplatAsset)
    {
        UE_LOG(LogTemp, Warning, TEXT("NiagaraGS: UploadToGPU aborted. SplatAsset is null for DI: 0x%p"), this);
        return;
    }
    const int32 Count = SplatAsset->SplatData.Num();
    if (Count == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("NiagaraGS: UploadToGPU aborted. SplatAsset '%s' holds 0 splats for DI: 0x%p"), *SplatAsset->GetName(), this);
        return;
    }

    FNDIGaussianSplatProxy* ProxyPtr = GetProxyAs<FNDIGaussianSplatProxy>();
    if (!ProxyPtr)
    {
        UE_LOG(LogTemp, Log, TEXT("NiagaraGS: GetProxyAs returned null. This is normal during early asset loading or CDO init. Splats will be uploaded on-demand during rendering-stage callback. DI: 0x%p, Count: %d"), this, Count);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("NiagaraGS: Queueing GPU upload of %d splats for DI: 0x%p, Proxy: 0x%p"), Count, this, ProxyPtr);

    TArray<FGaussianSplatData> SplatsCopy = SplatAsset->SplatData;
    ENQUEUE_RENDER_COMMAND(NiagaraGS_Upload)(
        [ProxyPtr, SplatsCopy = MoveTemp(SplatsCopy)](FRHICommandListImmediate& RHICmdList) mutable
        {
            ProxyPtr->UploadData(SplatsCopy);
        });
}

void UNiagaraGSDataInterface::PostLoad()
{
    Super::PostLoad();
    UploadToGPU();
}

#if WITH_EDITOR
void UNiagaraGSDataInterface::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraGSDataInterface, SplatAsset))
    {
        UploadToGPU();
    }
}
#endif

void UNiagaraGSDataInterface::GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
    // URGENT COMPILER FIX:
    // To prevent Niagara shader compilation freezes/hangs, we define the helper functions 
    // dynamically as a C++ string here, instead of using a static #include file.
    // Why: Niagara's {Parameter} regex-replacement ONLY runs on the dynamic C++ string returned 
    // by this function. It does NOT crawl static .ush '#include' files pre-compilation. 
    // If the .ush file is included, DXC receives un-replaced curly braces (e.g. {Parameter}_Positions),
    // which results in catastrophic compiler loops/crashes in the editor.
    // We also omit redundant type declarations for our parameters because BuildShaderParameters() 
    // already automatically exposes flattened parameter bindings to the shader context!
    //
    // CRITICAL: We must manually replace "{Parameter}" in our custom C++ dynamic string with 
    // ParamInfo.DataInterfaceHLSLSymbol, as Niagara's translation pass does not automatically 
    // replace braces inside raw strings appended to OutHLSL from C++.
    //
    // VALUE VS RESOURCE ACCESS RULE:
    // 1. SplatCount is a value-type parameter in FNiagaraGSShaderParameters, meaning it is bundled 
    //    into a Constant Buffer. We access it using a DOT: {Parameter}.SplatCount
    // 2. Positions, Scales, Rotations, ColorOpacity are SRV resource-type parameters. Resources 
    //    cannot reside inside a constant buffer struct, so the shader parameters builder flattens 
    //    them to the global shader scope by joining the symbol with an UNDERSCORE: {Parameter}_Positions

    const FString Symbol = ParamInfo.DataInterfaceHLSLSymbol;

    FString TemplateCode =
        TEXT("void {Parameter}_GetSplatCount(out int OutCount)\n")
        TEXT("{\n")
        TEXT("    OutCount = {Parameter}.SplatCount;\n")
        TEXT("}\n\n")

        TEXT("void {Parameter}_GetSplatPosition(int Index, out float3 OutPosition)\n")
        TEXT("{\n")
        TEXT("    if (Index >= 0 && Index < {Parameter}.SplatCount)\n")
        TEXT("    {\n")
        TEXT("        OutPosition = {Parameter}_Positions[Index].xyz;\n")
        TEXT("    }\n")
        TEXT("    else\n")
        TEXT("    {\n")
        TEXT("        OutPosition = float3(0.0f, 0.0f, 0.0f);\n")
        TEXT("    }\n")
        TEXT("}\n\n")

        TEXT("void {Parameter}_GetSplatScale(int Index, out float3 OutScale)\n")
        TEXT("{\n")
        TEXT("    if (Index >= 0 && Index < {Parameter}.SplatCount)\n")
        TEXT("    {\n")
        TEXT("        OutScale = {Parameter}_Scales[Index].xyz;\n")
        TEXT("    }\n")
        TEXT("    else\n")
        TEXT("    {\n")
        TEXT("        OutScale = float3(1.0f, 1.0f, 1.0f);\n")
        TEXT("    }\n")
        TEXT("}\n\n")

        TEXT("void {Parameter}_GetSplatOrientation(int Index, out float OutQX, out float OutQY, out float OutQZ, out float OutQW)\n")
        TEXT("{\n")
        TEXT("    if (Index >= 0 && Index < {Parameter}.SplatCount)\n")
        TEXT("    {\n")
        TEXT("        float4 Q = {Parameter}_Rotations[Index];\n")
        TEXT("        OutQX = Q.x;\n")
        TEXT("        OutQY = Q.y;\n")
        TEXT("        OutQZ = Q.z;\n")
        TEXT("        OutQW = Q.w;\n")
        TEXT("    }\n")
        TEXT("    else\n")
        TEXT("    {\n")
        TEXT("        OutQX = 0.0f;\n")
        TEXT("        OutQY = 0.0f;\n")
        TEXT("        OutQZ = 0.0f;\n")
        TEXT("        OutQW = 1.0f;\n")
        TEXT("    }\n")
        TEXT("}\n\n")

        TEXT("void {Parameter}_GetSplatColor(int Index, out float4 OutColor)\n")
        TEXT("{\n")
        TEXT("    if (Index >= 0 && Index < {Parameter}.SplatCount)\n")
        TEXT("    {\n")
        TEXT("        OutColor = {Parameter}_ColorOpacity[Index];\n")
        TEXT("    }\n")
        TEXT("    else\n")
        TEXT("    {\n")
        TEXT("        OutColor = float4(0.5f, 0.5f, 0.5f, 1.0f);\n")
        TEXT("    }\n")
        TEXT("}\n\n")

        TEXT("void {Parameter}_GetSplatOpacity(int Index, out float OutOpacity)\n")
        TEXT("{\n")
        TEXT("    if (Index >= 0 && Index < {Parameter}.SplatCount)\n")
        TEXT("    {\n")
        TEXT("        OutOpacity = {Parameter}_ColorOpacity[Index].w;\n")
        TEXT("    }\n")
        TEXT("    else\n")
        TEXT("    {\n")
        TEXT("        OutOpacity = 0.0f;\n")
        TEXT("    }\n")
        TEXT("}\n\n");

    TemplateCode.ReplaceInline(TEXT("{Parameter}"), *Symbol);
    OutHLSL += TemplateCode;
}

bool UNiagaraGSDataInterface::GetFunctionHLSL(
    const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,
    const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo,
    int FunctionInstanceIndex,
    FString& OutHLSL)
{
    const FString& Sym = ParamInfo.DataInterfaceHLSLSymbol;

    if (FunctionInfo.DefinitionName == Name_GetSplatCount)
    {
        OutHLSL += FString::Printf(TEXT("void %s(out int OutCount)\\n{\\n    %s_GetSplatCount(OutCount);\\n}\\n"), *FunctionInfo.InstanceName, *Sym);
        return true;
    }
    if (FunctionInfo.DefinitionName == Name_GetSplatPosition)
    {
        OutHLSL += FString::Printf(TEXT("void %s(int Index, out float3 OutPosition)\\n{\\n    %s_GetSplatPosition(Index, OutPosition);\\n}\\n"), *FunctionInfo.InstanceName, *Sym);
        return true;
    }
    if (FunctionInfo.DefinitionName == Name_GetSplatScale)
    {
        OutHLSL += FString::Printf(TEXT("void %s(int Index, out float3 OutScale)\\n{\\n    %s_GetSplatScale(Index, OutScale);\\n}\\n"), *FunctionInfo.InstanceName, *Sym);
        return true;
    }
    if (FunctionInfo.DefinitionName == Name_GetSplatOrientation)
    {
        OutHLSL += FString::Printf(TEXT("void %s(int Index, out float OutQX, out float OutQY, out float OutQZ, out float OutQW)\\n{\\n    %s_GetSplatOrientation(Index, OutQX, OutQY, OutQZ, OutQW);\\n}\\n"), *FunctionInfo.InstanceName, *Sym);
        return true;
    }
    if (FunctionInfo.DefinitionName == Name_GetSplatColor)
    {
        OutHLSL += FString::Printf(TEXT("void %s(int Index, out float4 OutColor)\\n{\\n    %s_GetSplatColor(Index, OutColor);\\n}\\n"), *FunctionInfo.InstanceName, *Sym);
        return true;
    }
    if (FunctionInfo.DefinitionName == Name_GetSplatOpacity)
    {
        OutHLSL += FString::Printf(TEXT("void %s(int Index, out float OutOpacity)\\n{\\n    %s_GetSplatOpacity(Index, OutOpacity);\\n}\\n"), *FunctionInfo.InstanceName, *Sym);
        return true;
    }
    return false;
}

void UNiagaraGSDataInterface::BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
    ShaderParametersBuilder.AddNestedStruct<FNiagaraGSShaderParameters>();
}

void UNiagaraGSDataInterface::SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
    FNDIGaussianSplatProxy& SplatProxy = Context.GetProxy<FNDIGaussianSplatProxy>();

    // Safeguard / On-demand render thread upload if buffers are not ready but we have CPU data.
    // This is the thread-safest and most bulletproof way because SetShaderParameters is called on the render thread
    // right before binding, and our upload is guaranteed to target the exact proxy being rendered!
    if (!SplatProxy.bBuffersReady && SplatAsset && SplatAsset->SplatData.Num() > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("NiagaraGS: SetShaderParameters detected uninitialized proxy. Triggering on-demand rendering-thread upload of %d splats for DI: 0x%p, Proxy: 0x%p"),
            SplatAsset->SplatData.Num(), this, &SplatProxy);
        SplatProxy.UploadData(SplatAsset->SplatData);
    }

    // URGENT CRASH FIX: Guarantee the fallback buffer is initialized on the render thread 
    // before any parameters are retrieved/bound. This prevents null SRV bindings and editor crashes.
    SplatProxy.InitFallbackBuffer();

    FNiagaraGSShaderParameters* Params = Context.GetParameterNestedStruct<FNiagaraGSShaderParameters>();

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
        FRHIShaderResourceView* Fallback = SplatProxy.FallbackBuffer.SRV;
        Params->SplatCount = 0;
        Params->Positions = Fallback;
        Params->Scales = Fallback;
        Params->Rotations = Fallback;
        Params->ColorOpacity = Fallback;
    }
}

void FNDIGaussianSplatProxy::InitFallbackBuffer()
{
    if (FallbackBuffer.IsValid()) return;

    const int32 BufferSize = sizeof(FVector4f);
    FVector4f ZeroData(0.f, 0.f, 0.f, 0.f);

    FRHIBufferCreateDesc CreateInfo = FRHIBufferCreateDesc::Create(
        TEXT("GS_Fallback"),
        EBufferUsageFlags::Static | EBufferUsageFlags::ShaderResource)
        .SetSize(BufferSize)
        .SetStride(sizeof(FVector4f))
        .SetInitialState(ERHIAccess::SRVCompute);

    FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

    FallbackBuffer.Buffer = RHICmdList.CreateBuffer(CreateInfo);
    void* Dest = RHICmdList.LockBuffer(FallbackBuffer.Buffer, 0, BufferSize, RLM_WriteOnly);
    FMemory::Memcpy(Dest, &ZeroData, BufferSize);
    RHICmdList.UnlockBuffer(FallbackBuffer.Buffer);

    FShaderResourceViewInitializer ViewInit(FallbackBuffer.Buffer, PF_A32B32G32R32F);
    FallbackBuffer.SRV = RHICmdList.CreateShaderResourceView(ViewInit);
}

void FNDIGaussianSplatProxy::UploadData(const TArray<FGaussianSplatData>& Splats)
{
    InitFallbackBuffer();
    check(IsInRenderingThread());
    const int32 Count = Splats.Num();
    ReleaseBuffers();

    if (Count == 0) return;

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

    auto CreateBuffer = [](
        FRHICommandListImmediate& ImmediateRHICmdList,
        FNiagaraGSSplatBuffer& OutBuffer,
        const TArray<FVector4f>& Data,
        const TCHAR* DebugName)
        {
            const int32 BufferSize = Data.Num() * sizeof(FVector4f);

            FRHIBufferCreateDesc Desc = FRHIBufferCreateDesc::Create(DebugName, EBufferUsageFlags::Static | EBufferUsageFlags::ShaderResource)
                .SetSize(BufferSize)
                .SetStride(sizeof(FVector4f))
                .SetInitialState(ERHIAccess::SRVCompute);

            OutBuffer.Buffer = ImmediateRHICmdList.CreateBuffer(Desc);
            void* Dest = ImmediateRHICmdList.LockBuffer(OutBuffer.Buffer, 0, BufferSize, RLM_WriteOnly);
            FMemory::Memcpy(Dest, Data.GetData(), BufferSize);
            ImmediateRHICmdList.UnlockBuffer(OutBuffer.Buffer);

            FShaderResourceViewInitializer SRVInit(OutBuffer.Buffer, PF_A32B32G32R32F);
            OutBuffer.SRV = ImmediateRHICmdList.CreateShaderResourceView(SRVInit);
        };

    FRHICommandListImmediate& ExecutedRHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
    CreateBuffer(ExecutedRHICmdList, PositionsBuffer, PackedPositions, TEXT("GS_Positions"));
    CreateBuffer(ExecutedRHICmdList, ScalesBuffer, PackedScales, TEXT("GS_Scales"));
    CreateBuffer(ExecutedRHICmdList, RotationsBuffer, PackedRotations, TEXT("GS_Rotations"));
    CreateBuffer(ExecutedRHICmdList, ColorOpacityBuffer, PackedColorOpacity, TEXT("GS_ColorOpacity"));

    SplatCount = Count;
    bBuffersReady = true;

    UE_LOG(LogTemp, Log, TEXT("NiagaraGS: Uploaded %d splats (%d MB)"),
        Count, (Count * (int32)sizeof(FVector4f) * 4) / (1024 * 1024));
}